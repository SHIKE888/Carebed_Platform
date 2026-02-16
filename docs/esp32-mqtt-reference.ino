#include <WiFi.h>
#include <PubSubClient.h>

// === User configuration ===
const char *WIFI_SSID = "CMCC-EAy3";
const char *WIFI_PASSWORD = "cdfe4923";
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

volatile bool lockOpen = false;
const uint8_t BATTERY_LEVEL = 100;
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 10UL * 1000UL; // 10s heartbeat interval
bool heartbeatPending = false;                          // trigger immediate heartbeat on WiFi connect or unlock
String lastProcessedCommandId = "";
const unsigned long UNLOCK_PULSE_MS = 500; // how long to power the lock
bool unlocking = false;
unsigned long unlockOffAt = 0;
volatile bool lockStatusChanged = false; // ISR flag only

void IRAM_ATTR handleLockStatus();
void getLockStatus();
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
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    pinMode(34, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(34), // 获取中断编号
        handleLockStatus,          // 回调函数
        FALLING                    // 下降沿触发
    );
    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "devices/%s/heartbeat", DEVICE_CODE);
    snprintf(commandTopic, sizeof(commandTopic), "devices/%s/command", DEVICE_CODE);

    getLockStatus();
    ensureWifiConnected();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(handleCommand);
}

void loop()
{
    if (unlocking && millis() >= unlockOffAt)
    {
        digitalWrite(4, LOW);
        unlocking = false;
        Serial.println("Lock power off after pulse");
    }

    if (lockStatusChanged)
    {
        lockStatusChanged = false;
        getLockStatus();
        heartbeatPending = true;
        Serial.println("Lock status changed, heartbeat scheduled");
    }
    ensureWifiConnected();
    ensureMqttConnected();
    mqttClient.loop();

    if (heartbeatPending && mqttClient.connected())
    {
        publishHeartbeat();
        lastHeartbeat = millis();
        heartbeatPending = false;
    }

    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL)
    {
        publishHeartbeat();
        lastHeartbeat = now;
    }
}

void IRAM_ATTR handleLockStatus()
{
    lockStatusChanged = true;
}

void getLockStatus()
{
    if (digitalRead(34) == HIGH)
    {
        lockOpen = true;
    }
    else
    {
        lockOpen = false;
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
    heartbeatPending = true; // send heartbeat immediately after WiFi connects
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
    getLockStatus();
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
        heartbeatPending = true; // report state immediately after unlock
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
    // Pulse the lock line then auto-off in loop to avoid holding power
    digitalWrite(4, HIGH);
    unlocking = true;
    unlockOffAt = millis() + UNLOCK_PULSE_MS;
    Serial.println("Lock opened (pulse started)");
}

void executeReboot()
{
    Serial.println("Reboot command acknowledged");
    delay(50);
    ESP.restart();
}
