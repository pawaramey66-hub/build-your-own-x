package com.redis;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public class RespParser {

    public static List<String> parseRequest(BufferedReader reader) throws IOException {
        String line = reader.readLine();
        if (line == null || line.isEmpty()) {
            return null;
        }

        if (line.startsWith("*")) {
            int numElements = Integer.parseInt(line.substring(1));
            List<String> commandTokens = new ArrayList<>();

            for (int i = 0; i < numElements; i++) {
                String lengthLine = reader.readLine();
                if (lengthLine != null && lengthLine.startsWith("$")) {
                    String argument = reader.readLine();
                    commandTokens.add(argument);
                }
            }
            return commandTokens;
        }

        String[] tokens = line.trim().split("\\s+");
        return List.of(tokens);
    }

    public static String toSimpleString(String message) {
        return "+" + message + "\r\n";
    }

    public static String toBulkString(String value) {
        if (value == null) {
            return "$-1\r\n";
        }
        return "$" + value.length() + "\r\n" + value + "\r\n";
    }

    public static String toError(String message) {
        return "-ERR " + message + "\r\n";
    }

    public static String toInteger(long value) {
        return ":" + value + "\r\n";
    }
}
