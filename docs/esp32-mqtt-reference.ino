#include <WiFi.h>
#include <PubSubClient.h>

// === User configuration ===
const char *WIFI_SSID = "Pura 70";
const char *WIFI_PASSWORD = "12345678.";
const char *MQTT_HOST = "msas.absozero.cn"; // 后端所在服务器地址
const int MQTT_PORT = 1883;
const char *DEVICE_CODE = "BED-0001"; // 与后端设备编码一致

// === MQTT topics ===
char heartbeatTopic[64];
char commandTopic[64];
// PubSubClient 仅支持 QoS 0/1，订阅用 1 可与后端 QoS2 发布协商为 QoS1
const uint8_t COMMAND_QOS = 1;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool lockOpen = false;
const uint8_t BATTERY_LEVEL = 100;
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 30UL * 1000UL;
String lastProcessedCommandId = "";

void ensureWifiConnected();
void ensureMqttConnected();
void publishHeartbeat();
void handleCommand(char *topic, byte *payload, unsigned int length);
String extractJsonField(const String &body, const char *field);
void executeUnlock();
void executeReboot();

void setup()
{
    Serial.begin(115200);
    delay(100);

    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "devices/%s/heartbeat", DEVICE_CODE);
    snprintf(commandTopic, sizeof(commandTopic), "devices/%s/command", DEVICE_CODE);

    ensureWifiConnected();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(handleCommand);
}

void loop()
{
    ensureWifiConnected();
    ensureMqttConnected();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL)
    {
        publishHeartbeat();
        lastHeartbeat = now;
    }
}

void ensureWifiConnected()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected");
}

void ensureMqttConnected()
{
    if (mqttClient.connected())
    {
        return;
    }
    Serial.print("Connecting MQTT");
    while (!mqttClient.connected())
    {
        String clientId = String("esp32-") + DEVICE_CODE;
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println(" connected");
            mqttClient.subscribe(commandTopic, COMMAND_QOS);
        }
        else
        {
            Serial.print(" failed, rc=");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

void publishHeartbeat()
{
    const char *lockStatus = lockOpen ? "UNLOCKED" : "LOCKED";
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"battery\":%u,\"lockStatus\":\"%s\"}", BATTERY_LEVEL, lockStatus);
    bool ok = mqttClient.publish(heartbeatTopic, payload, true);
    Serial.print("Heartbeat sent: ");
    Serial.println(payload);
    if (!ok)
    {
        Serial.println("Heartbeat publish failed");
    }
}

void handleCommand(char *topic, byte *payload, unsigned int length)
{
    if (strcmp(topic, commandTopic) != 0)
    {
        return;
    }
    String body;
    for (unsigned int i = 0; i < length; i++)
    {
        body += static_cast<char>(payload[i]);
    }
    Serial.print("Command received: ");
    Serial.println(body);

    String commandId = extractJsonField(body, "commandId");
    if (commandId.length() > 0 && commandId == lastProcessedCommandId)
    {
        Serial.println("Duplicate command ignored");
        return;
    }

    String command = extractJsonField(body, "command");
    command.toUpperCase();
    if (command == "UNLOCK")
    {
        executeUnlock();
    }
    else if (command == "REBOOT")
    {
        executeReboot();
    }
    else
    {
        Serial.println("Unsupported command");
        return;
    }

    if (commandId.length() > 0)
    {
        lastProcessedCommandId = commandId;
    }
}

String extractJsonField(const String &body, const char *field)
{
    String needle = String("\"") + field + String("\":\"");
    int start = body.indexOf(needle);
    if (start < 0)
    {
        return "";
    }
    start += needle.length();
    int end = body.indexOf("\"", start);
    if (end < 0)
    {
        return "";
    }
    return body.substring(start, end);
}

void executeUnlock()
{
    // TODO: 在此处调用开锁驱动逻辑
    lockOpen = true;
    Serial.println("Lock opened");
}

void executeReboot()
{
    Serial.println("Reboot command acknowledged");
    delay(50);
    ESP.restart();
}
