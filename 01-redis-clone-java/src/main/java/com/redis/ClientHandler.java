package com.redis;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.List;

public class ClientHandler implements Runnable {
    private final Socket socket;
    private final StorageEngine storage;

    public ClientHandler(Socket socket, StorageEngine storage) {
        this.socket = socket;
        this.storage = storage;
    }

    @Override
    public void run() {
        try (
            BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
            OutputStream out = socket.getOutputStream()
        ) {
            while (!socket.isClosed()) {
                List<String> command = RespParser.parseRequest(reader);
                if (command == null || command.isEmpty()) {
                    break;
                }

                String action = command.get(0).toUpperCase();
                String response;

                switch (action) {
                    case "PING":
                        response = RespParser.toSimpleString("PONG");
                        break;
                    case "ECHO":
                        response = command.size() > 1 ? RespParser.toBulkString(command.get(1)) : RespParser.toError("wrong arguments for echo");
                        break;
                    case "SET":
                        if (command.size() >= 3) {
                            String key = command.get(1);
                            String val = command.get(2);
                            Long ttl = null;
                            if (command.size() >= 5 && command.get(3).equalsIgnoreCase("EX")) {
                                try {
                                    ttl = Long.parseLong(command.get(4));
                                } catch (NumberFormatException ignored) {}
                            }
                            storage.set(key, val, ttl);
                            response = RespParser.toSimpleString("OK");
                        } else {
                            response = RespParser.toError("wrong arguments for set");
                        }
                        break;
                    case "GET":
                        if (command.size() >= 2) {
                            String val = storage.get(command.get(1));
                            response = RespParser.toBulkString(val);
                        } else {
                            response = RespParser.toError("wrong arguments for get");
                        }
                        break;
                    case "DEL":
                        if (command.size() >= 2) {
                            boolean deleted = storage.del(command.get(1));
                            response = RespParser.toInteger(deleted ? 1 : 0);
                        } else {
                            response = RespParser.toError("wrong arguments for del");
                        }
                        break;
                    default:
                        response = RespParser.toError("unknown command '" + action + "'");
                }

                out.write(response.getBytes(StandardCharsets.UTF_8));
                out.flush();
            }
        } catch (IOException ignored) {
        } finally {
            try {
                socket.close();
            } catch (IOException ignored) {}
        }
    }
}
