#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>

// === User configuration (persisted) ===
const char *MQTT_HOST = "msas.absozero.cn"; // 后端所在服务器地址
const int MQTT_PORT = 1883;

struct DeviceConfig
{
    char ssid[32];
    char password[64];
    char deviceCode[32];
};

DeviceConfig deviceConfig = {
    "CMCC-EAy3", // default WiFi SSID
    "cdfe4923",  // default WiFi password
    "BED-0002"   // default device code
};

const uint32_t CONFIG_MAGIC = 0xCAREB33D;
const size_t EEPROM_SIZE = 256;

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
const unsigned long UNLOCK_PULSE_MS = 500;        // how long to power the lock
const uint8_t UNLOCK_MAX_ATTEMPTS = 3;            // how many times to retry unlock
const unsigned long UNLOCK_VERIFY_DELAY_MS = 300; // wait time before checking lock state
const unsigned long UNLOCK_RETRY_GAP_MS = 300;    // gap between retries
volatile bool lockStatusChanged = false;          // ISR flag only
bool mqttNeedsReconnect = false;

void IRAM_ATTR handleLockStatus();
void getLockStatus();
void ensureWifiConnected();
void ensureMqttConnected();
void publishHeartbeat();
void handleCommand(char *topic, byte *payload, unsigned int length);
String extractJsonField(const String &body, const char *field);
void executeUnlock();
void executeReboot();
bool attemptUnlockOnce();
void updateTopics();
void loadConfigFromEeprom();
void saveConfigToEeprom();
void handleSerialConfig();
void applyDeviceCodeChange();

void setup()
{
    Serial.begin(115200);
    delay(100);

    EEPROM.begin(EEPROM_SIZE);
    loadConfigFromEeprom();
    updateTopics();

    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    pinMode(34, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(34), // 获取中断编号
        handleLockStatus,          // 回调函数
        FALLING                    // 下降沿触发
    );
    getLockStatus();
    ensureWifiConnected();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(handleCommand);
}

void loop()
{
    handleSerialConfig();

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
    WiFi.begin(deviceConfig.ssid, deviceConfig.password);
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
    if (mqttNeedsReconnect && mqttClient.connected())
    {
        mqttClient.disconnect();
    }

    if (mqttClient.connected())
    {
        return;
    }
    Serial.print("Connecting MQTT");
    while (!mqttClient.connected())
    {
        String clientId = String("esp32-") + deviceConfig.deviceCode;
        if (mqttClient.connect(clientId.c_str()))
        {
            Serial.println(" connected");
            mqttClient.subscribe(commandTopic, COMMAND_QOS);
            mqttNeedsReconnect = false;
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
    for (uint8_t attempt = 1; attempt <= UNLOCK_MAX_ATTEMPTS; attempt++)
    {
        Serial.printf("Unlock attempt %u\n", attempt);
        bool opened = attemptUnlockOnce();
        if (opened)
        {
            Serial.println("Lock opened");
            return;
        }

        if (attempt < UNLOCK_MAX_ATTEMPTS)
        {
            Serial.println("Lock still closed, retrying...");
            delay(UNLOCK_RETRY_GAP_MS);
        }
    }
    Serial.println("Unlock failed after retries");
}

void executeReboot()
{
    Serial.println("Reboot command acknowledged");
    delay(50);
    ESP.restart();
}

bool attemptUnlockOnce()
{
    // Drive the lock line high for a short pulse, then verify sensor state
    digitalWrite(4, HIGH);
    delay(UNLOCK_PULSE_MS);
    digitalWrite(4, LOW);

    delay(UNLOCK_VERIFY_DELAY_MS); // allow hardware to settle
    getLockStatus();
    return lockOpen;
}

void updateTopics()
{
    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "devices/%s/heartbeat", deviceConfig.deviceCode);
    snprintf(commandTopic, sizeof(commandTopic), "devices/%s/command", deviceConfig.deviceCode);
}

void loadConfigFromEeprom()
{
    uint32_t magic = 0;
    EEPROM.get(0, magic);
    if (magic != CONFIG_MAGIC)
    {
        Serial.println("EEPROM config missing, using defaults");
        saveConfigToEeprom();
        return;
    }

    DeviceConfig stored;
    EEPROM.get(sizeof(uint32_t), stored);
    // ensure null-termination
    stored.ssid[sizeof(stored.ssid) - 1] = '\0';
    stored.password[sizeof(stored.password) - 1] = '\0';
    stored.deviceCode[sizeof(stored.deviceCode) - 1] = '\0';
    deviceConfig = stored;
    Serial.println("Config loaded from EEPROM");
}

void saveConfigToEeprom()
{
    EEPROM.put(0, CONFIG_MAGIC);
    EEPROM.put(sizeof(uint32_t), deviceConfig);
    EEPROM.commit();
    Serial.println("Config saved to EEPROM");
}

void handleSerialConfig()
{
    if (!Serial.available())
    {
        return;
    }

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
    {
        return;
    }

    if (line.equalsIgnoreCase("HELP"))
    {
        Serial.println("Commands: HELP | SHOW | SET WIFI <ssid> <password> | SET ID <deviceCode>");
        return;
    }

    if (line.equalsIgnoreCase("SHOW"))
    {
        Serial.print("SSID: ");
        Serial.println(deviceConfig.ssid);
        Serial.print("PASSWORD: ");
        Serial.println(deviceConfig.password);
        Serial.print("DEVICE: ");
        Serial.println(deviceConfig.deviceCode);
        return;
    }

    if (line.startsWith("SET WIFI "))
    {
        String rest = line.substring(9);
        int spacePos = rest.indexOf(' ');
        if (spacePos <= 0)
        {
            Serial.println("Invalid format. Use: SET WIFI <ssid> <password>");
            return;
        }
        String ssid = rest.substring(0, spacePos);
        String password = rest.substring(spacePos + 1);
        ssid.trim();
        password.trim();
        if (ssid.length() == 0 || password.length() == 0)
        {
            Serial.println("SSID or password empty");
            return;
        }
        strlcpy(deviceConfig.ssid, ssid.c_str(), sizeof(deviceConfig.ssid));
        strlcpy(deviceConfig.password, password.c_str(), sizeof(deviceConfig.password));
        saveConfigToEeprom();
        Serial.println("WiFi credentials updated");
        heartbeatPending = true;
        WiFi.disconnect();
        return;
    }

    if (line.startsWith("SET ID "))
    {
        String code = line.substring(7);
        code.trim();
        if (code.length() == 0)
        {
            Serial.println("Device ID empty");
            return;
        }
        strlcpy(deviceConfig.deviceCode, code.c_str(), sizeof(deviceConfig.deviceCode));
        saveConfigToEeprom();
        applyDeviceCodeChange();
        Serial.println("Device ID updated");
        return;
    }

    Serial.println("Unknown command. Type HELP");
}

void applyDeviceCodeChange()
{
    updateTopics();
    mqttNeedsReconnect = true;
    heartbeatPending = true;
}
