package com.redis;

import java.util.concurrent.ConcurrentHashMap;

public class StorageEngine {

    private static class ValueEntry {
        final String value;
        final Long expiryTimeMs;

        ValueEntry(String value, Long expiryTimeMs) {
            this.value = value;
            this.expiryTimeMs = expiryTimeMs;
        }

        boolean isExpired() {
            if (expiryTimeMs == null) return false;
            return System.currentTimeMillis() > expiryTimeMs;
        }
    }

    private final ConcurrentHashMap<String, ValueEntry> store = new ConcurrentHashMap<>();

    public void set(String key, String value, Long ttlSeconds) {
        Long expiryTime = (ttlSeconds != null) ? System.currentTimeMillis() + (ttlSeconds * 1000) : null;
        store.put(key, new ValueEntry(value, expiryTime));
    }

    public String get(String key) {
        ValueEntry entry = store.get(key);
        if (entry == null) return null;

        if (entry.isExpired()) {
            store.remove(key);
            return null;
        }
        return entry.value;
    }

    public boolean del(String key) {
        return store.remove(key) != null;
    }
}
