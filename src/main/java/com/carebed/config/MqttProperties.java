package com.carebed.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "carebed.mqtt")
public class MqttProperties {

    private boolean enabled = true;
    private final Broker broker = new Broker();
    private final Bridge bridge = new Bridge();
    private long heartbeatTimeoutMs = 90_000L;
    private long offlineCheckIntervalMs = 30_000L;

    public boolean isEnabled() {
        return enabled;
    }

    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
    }

    public Broker getBroker() {
        return broker;
    }

    public Bridge getBridge() {
        return bridge;
    }

    public long getHeartbeatTimeoutMs() {
        return heartbeatTimeoutMs;
    }

    public void setHeartbeatTimeoutMs(long heartbeatTimeoutMs) {
        this.heartbeatTimeoutMs = heartbeatTimeoutMs;
    }

    public long getOfflineCheckIntervalMs() {
        return offlineCheckIntervalMs;
    }

    public void setOfflineCheckIntervalMs(long offlineCheckIntervalMs) {
        this.offlineCheckIntervalMs = offlineCheckIntervalMs;
    }

    public static class Broker {
        private String host = "0.0.0.0";
        private int port = 1883;
        private String persistencePath = "mqtt-data";
        private String username;
        private String password;

        public String getHost() {
            return host;
        }

        public void setHost(String host) {
            this.host = host;
        }

        public int getPort() {
            return port;
        }

        public void setPort(int port) {
            this.port = port;
        }

        public String getPersistencePath() {
            return persistencePath;
        }

        public void setPersistencePath(String persistencePath) {
            this.persistencePath = persistencePath;
        }

        public String getUsername() {
            return username;
        }

        public void setUsername(String username) {
            this.username = username;
        }

        public String getPassword() {
            return password;
        }

        public void setPassword(String password) {
            this.password = password;
        }
    }

    public static class Bridge {
        private String host = "127.0.0.1";
        private int port = 1883;
        private String username;
        private String password;
        private int qos = 1;

        public String getHost() {
            return host;
        }

        public void setHost(String host) {
            this.host = host;
        }

        public int getPort() {
            return port;
        }

        public void setPort(int port) {
            this.port = port;
        }

        public String getUsername() {
            return username;
        }

        public void setUsername(String username) {
            this.username = username;
        }

        public String getPassword() {
            return password;
        }

        public void setPassword(String password) {
            this.password = password;
        }

        public int getQos() {
            return qos;
        }

        public void setQos(int qos) {
            this.qos = qos;
        }
    }
}
