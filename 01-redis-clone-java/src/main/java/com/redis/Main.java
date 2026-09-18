package com.redis;

public class Main {
    public static void main(String[] args) {
        int port = 6379;
        Server server = new Server(port);
        server.start();
    }
}
