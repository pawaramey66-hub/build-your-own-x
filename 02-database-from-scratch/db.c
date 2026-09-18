#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

const uint32_t PAGE_SIZE = 4096;
#define ROWS_PER_PAGE (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS (ROWS_PER_PAGE * TABLE_MAX_PAGES)

typedef struct {
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void* pages[TABLE_MAX_PAGES];
} Pager;

typedef struct {
    uint32_t num_rows;
    Pager* pager;
} Table;

void serialize_row(Row* source, void* destination) {
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void* get_page(Pager* pager, uint32_t page_num) {
    if (page_num > TABLE_MAX_PAGES) {
        printf("Page number out of bounds.\n");
        exit(EXIT_FAILURE);
    }
    if (pager->pages[page_num] == NULL) {
        void* page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }

        if (page_num <= num_pages) {
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            read(pager->file_descriptor, page, PAGE_SIZE);
        }
        pager->pages[page_num] = page;
        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
    }
    return pager->pages[page_num];
}

void* row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = get_page(table->pager, page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = sizeof(uint32_t) + (row_offset * ROW_SIZE);
    return page + byte_offset;
}

Pager* pager_open(const char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }
    off_t file_length = lseek(fd, 0, SEEK_END);
    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;
    pager->num_pages = (file_length / PAGE_SIZE);

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = NULL;
    }
    return pager;
}

Table* db_open(const char* filename) {
    Pager* pager = pager_open(filename);
    Table* table = malloc(sizeof(Table));
    table->pager = pager;

    if (pager->file_length == 0) {
        table->num_rows = 0;
    } else {
        void* page0 = get_page(pager, 0);
        memcpy(&(table->num_rows), page0, sizeof(uint32_t));
    }
    return table;
}

void pager_flush(Pager* pager, uint32_t page_num) {
    if (pager->pages[page_num] == NULL) return;
    lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
    write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
}

void db_close(Table* table) {
    Pager* pager = table->pager;
    void* page0 = get_page(pager, 0);
    memcpy(page0, &(table->num_rows), sizeof(uint32_t));

    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;
    for (uint32_t i = 0; i <= num_full_pages; i++) {
        if (pager->pages[i] == NULL) continue;
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    close(pager->file_descriptor);
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        if (pager->pages[i]) free(pager->pages[i]);
    }
    free(pager);
    free(table);
}

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./db <db_filename>\n");
        exit(EXIT_FAILURE);
    }
    Table* table = db_open(argv[1]);
    char input_buffer[512];

    while (true) {
        printf("mydb > ");
        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) break;
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        if (strcmp(input_buffer, ".exit") == 0) {
            db_close(table);
            printf("Saved and exited.\n");
            exit(EXIT_SUCCESS);
        } else if (strncmp(input_buffer, "insert", 6) == 0) {
            Row row;
            int args_assigned = sscanf(input_buffer, "insert %d %s %s", &(row.id), row.username, row.email);
            if (args_assigned < 3) {
                printf("Syntax error. Use: insert <id> <username> <email>\n");
                continue;
            }
            if (table->num_rows >= TABLE_MAX_ROWS) {
                printf("Error: Table full.\n");
                continue;
            }
            serialize_row(&row, row_slot(table, table->num_rows));
            table->num_rows += 1;
            printf("Executed.\n");
        } else if (strcmp(input_buffer, "select") == 0) {
            Row row;
            for (uint32_t i = 0; i < table->num_rows; i++) {
                deserialize_row(row_slot(table, i), &row);
                print_row(&row);
            }
            printf("Executed.\n");
        } else {
            printf("Unrecognized command '%s'.\n", input_buffer);
        }
    }
    return 0;
}
