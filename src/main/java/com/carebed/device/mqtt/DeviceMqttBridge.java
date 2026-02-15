package com.carebed.device.mqtt;

import com.carebed.common.exception.BadRequestException;
import com.carebed.common.model.OperationResult;
import com.carebed.config.MqttProperties;
import com.carebed.device.DeviceLockStatus;
import com.carebed.device.DeviceService;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import io.moquette.broker.Server;
import org.eclipse.paho.client.mqttv3.IMqttClient;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.DependsOn;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.boot.context.event.ApplicationReadyEvent;

import jakarta.annotation.PreDestroy;

import java.io.IOException;
import java.time.Duration;
import java.time.Instant;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.UUID;

@Service
@ConditionalOnProperty(prefix = "carebed.mqtt", name = "enabled", havingValue = "true", matchIfMissing = true)
@DependsOn("mqttBroker")
public class DeviceMqttBridge implements DeviceCommandGateway, MqttCallbackExtended {

    private static final Logger log = LoggerFactory.getLogger(DeviceMqttBridge.class);
    private static final String HEARTBEAT_TOPIC_PATTERN = "devices/+/heartbeat";
    private static final int COMMAND_QOS = 2;

    private final DeviceService deviceService;
    private final MqttProperties properties;
    private final ObjectMapper objectMapper;
    private final com.carebed.mqttlog.MqttLogService mqttLogService;

    private IMqttClient client;

        public DeviceMqttBridge(DeviceService deviceService, MqttProperties properties, ObjectMapper objectMapper,
            com.carebed.mqttlog.MqttLogService mqttLogService, Server broker) {
        this.deviceService = deviceService;
        this.properties = properties;
        this.objectMapper = objectMapper;
        this.mqttLogService = mqttLogService;
        Objects.requireNonNull(broker, "Embedded MQTT broker must be initialized before bridge");
    }

    @EventListener(ApplicationReadyEvent.class)
    public void onApplicationReady() {
        connectAndSubscribe();
    }

    @Override
    public OperationResult sendUnlockCommand(UUID deviceId) {
        return publishCommand(deviceId, "UNLOCK", "下发开锁指令", "开锁指令已发送", "开锁指令发送失败");
    }

    @Override
    public OperationResult sendRebootCommand(UUID deviceId) {
        return publishCommand(deviceId, "REBOOT", "下发重启指令", "重启指令已发送", "重启指令发送失败");
    }

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
        log.info("MQTT bridge connected to {} (reconnect={})", serverURI, reconnect);
        try {
            client.subscribe(HEARTBEAT_TOPIC_PATTERN, properties.getBridge().getQos());
        } catch (MqttException e) {
            log.error("Failed to subscribe to heartbeat topics", e);
        }
    }

    @Override
    public void connectionLost(Throwable cause) {
        log.warn("MQTT bridge connection lost", cause);
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
        handleHeartbeat(topic, message);
    }

    @Override
    public void deliveryComplete(org.eclipse.paho.client.mqttv3.IMqttDeliveryToken token) {
        // no-op
    }

    @Scheduled(fixedDelayString = "${carebed.mqtt.offline-check-interval-ms:30000}")
    public void evaluateConnectivity() {
        deviceService.markOfflineDevices(Duration.ofMillis(properties.getHeartbeatTimeoutMs()));
    }

    @PreDestroy
    public void shutdown() {
        if (client != null) {
            try {
                if (client.isConnected()) {
                    client.disconnect();
                }
                client.close();
            } catch (MqttException e) {
                log.warn("Error disconnecting MQTT client", e);
            }
            client = null;
        }
    }

    private synchronized void connectAndSubscribe() {
        ensureConnected();
    }

    private synchronized void ensureConnected() {
        if (client != null && client.isConnected()) {
            return;
        }
        String brokerURI = String.format("tcp://%s:%d", properties.getBridge().getHost(),
                properties.getBridge().getPort());
        try {
            if (client == null) {
                String clientId = "carebed-bridge-" + UUID.randomUUID();
                client = new MqttClient(brokerURI, clientId, new MemoryPersistence());
            }
            client.setCallback(this);
            if (!client.isConnected()) {
                client.connect(buildConnectOptions());
            }
            client.subscribe(HEARTBEAT_TOPIC_PATTERN, properties.getBridge().getQos());
            log.info("MQTT bridge subscribed to {}", HEARTBEAT_TOPIC_PATTERN);
        } catch (MqttException e) {
            cleanupClient();
            log.error("Failed to connect to MQTT broker {}", brokerURI, e);
            throw new BadRequestException("无法连接 MQTT Broker: " + e.getMessage());
        }
    }

    private MqttConnectOptions buildConnectOptions() {
        MqttConnectOptions options = new MqttConnectOptions();
        options.setAutomaticReconnect(true);
        options.setCleanSession(true);
        if (properties.getBridge().getUsername() != null && !properties.getBridge().getUsername().isBlank()) {
            options.setUserName(properties.getBridge().getUsername());
            char[] password = properties.getBridge().getPassword() == null ? new char[0]
                    : properties.getBridge().getPassword().toCharArray();
            options.setPassword(password);
        }
        return options;
    }

    private void handleHeartbeat(String topic, MqttMessage message) {
        String[] segments = topic.split("/");
        if (segments.length < 3) {
            log.warn("Ignore heartbeat with invalid topic: {}", topic);
            return;
        }
        String deviceCode = segments[1];
        try {
            JsonNode node = objectMapper.readTree(message.getPayload());
            int battery = node.path("battery").asInt(-1);
            if (battery < 0 || battery > 100) {
                log.warn("Heartbeat battery value invalid for {}", deviceCode);
                return;
            }
            String lockNode = node.path("lockStatus").asText(null);
            DeviceLockStatus lockStatus = parseLockStatus(lockNode);
            deviceService.refreshHeartbeat(deviceCode, battery, lockStatus);
            mqttLogService.recordIn(topic, new String(message.getPayload()), deviceCode);
            log.debug("Heartbeat processed for {} (battery={}, lock={})", deviceCode, battery, lockStatus);
        } catch (Exception e) {
            log.error("Failed to process heartbeat from {}", deviceCode, e);
        }
    }

    private void cleanupClient() {
        if (client != null) {
            try {
                client.close();
            } catch (MqttException ex) {
                log.debug("Error closing MQTT client after failure", ex);
            } finally {
                client = null;
            }
        }
    }

    private DeviceLockStatus parseLockStatus(String value) {
        if (value == null) {
            return DeviceLockStatus.LOCKED;
        }
        try {
            return DeviceLockStatus.valueOf(value.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException ex) {
            log.warn("Unknown lock status {}; default to LOCKED", value);
            return DeviceLockStatus.LOCKED;
        }
    }

    private OperationResult publishCommand(UUID deviceId, String command, String logDescription,
            String successMessage, String failureMessage) {
        ensureConnected();
        try {
            var device = deviceService.getDevice(deviceId);
            String topic = "devices/" + device.deviceCode() + "/command";
            Map<String, Object> payload = Map.of(
                    "command", command,
                    "issuedAt", Instant.now().toString(),
                    "commandId", UUID.randomUUID().toString());
            MqttMessage message = new MqttMessage(objectMapper.writeValueAsBytes(payload));
            message.setQos(COMMAND_QOS);
            message.setRetained(false);
            client.publish(topic, message);
            deviceService.recordRemoteCommand(deviceId, logDescription);
            mqttLogService.recordOut(topic, new String(message.getPayload()), device.deviceCode());
            log.info("MQTT {} command published to {}", command, topic);
            return OperationResult.of(successMessage);
        } catch (IOException | MqttException e) {
            log.error("Failed to send {} command", command, e);
            throw new BadRequestException(failureMessage + ": " + e.getMessage());
        }
    }
}
