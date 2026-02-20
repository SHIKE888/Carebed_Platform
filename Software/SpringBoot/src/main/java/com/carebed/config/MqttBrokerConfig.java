package com.carebed.config;

import io.moquette.BrokerConstants;
import io.moquette.broker.Server;
import io.moquette.broker.config.MemoryConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.boot.context.properties.EnableConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Properties;

@Configuration
@EnableConfigurationProperties(MqttProperties.class)
@ConditionalOnProperty(prefix = "carebed.mqtt", name = "enabled", havingValue = "true", matchIfMissing = true)
public class MqttBrokerConfig {

    private static final Logger log = LoggerFactory.getLogger(MqttBrokerConfig.class);

    @Bean(destroyMethod = "stopServer")
    public Server mqttBroker(MqttProperties properties) throws IOException {
        Server server = new Server();
        Properties config = new Properties();
        config.setProperty(BrokerConstants.HOST_PROPERTY_NAME, properties.getBroker().getHost());
        config.setProperty(BrokerConstants.PORT_PROPERTY_NAME, Integer.toString(properties.getBroker().getPort()));
        config.setProperty(BrokerConstants.ALLOW_ANONYMOUS_PROPERTY_NAME, Boolean.TRUE.toString());
        config.setProperty(BrokerConstants.PERSISTENT_STORE_PROPERTY_NAME, resolveStorePath(properties));
        server.startServer(new MemoryConfig(config));
        log.info("Embedded MQTT broker started on {}:{}", properties.getBroker().getHost(),
                properties.getBroker().getPort());
        return server;
    }

    private String resolveStorePath(MqttProperties properties) throws IOException {
        Path dir = Paths.get(properties.getBroker().getPersistencePath()).toAbsolutePath();
        Files.createDirectories(dir);
        return dir.resolve("moquette_store.mapdb").toString();
    }
}
