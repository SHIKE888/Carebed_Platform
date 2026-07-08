#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>

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
    "Pura70",   // default WiFi SSID
    "12345678", // default WiFi password
    "BED-0001"  // default device code
};

const uint32_t CONFIG_MAGIC = 0xCAFEB33D;
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
bool lockInterruptEnabled = false;
bool mqttNeedsReconnect = false;
bool wifiForceReconnect = false;
bool wifiConnectInProgress = false;
bool wifiWasConnected = false;
unsigned long wifiConnectStartedAt = 0;
unsigned long wifiNextRetryAt = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 3000;
unsigned long lastMqttAttemptAt = 0;
const unsigned long MQTT_RETRY_INTERVAL_MS = 2000;

// === OLED Display (I2C: SDA=8, SCL=9) ===
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
bool ntpSynced = false;
unsigned long lastNtpAttempt = 0;
const unsigned long NTP_RETRY_INTERVAL_MS = 30000;
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_REFRESH_INTERVAL_MS = 300;
bool displayInitDone = false;

// === Toast notification on OLED ===
char toastMessage[64];
unsigned long toastEndTime = 0;
const unsigned long TOAST_DURATION_MS = 2500;

void IRAM_ATTR handleLockStatus();
void getLockStatus();
void ensureWifiConnected();
void ensureMqttConnected();
void publishHeartbeat();
void handleCommand(char *topic, byte *payload, unsigned int length);
String extractJsonField(const String &body, const char *field);
bool executeUnlock();
void executeReboot();
bool attemptUnlockOnce();
void updateTopics();
void loadConfigFromEeprom();
void saveConfigToEeprom();
void handleSerialConfig();
void applyDeviceCodeChange();
const char *wifiAuthText(uint8_t mode);
void scanNearbyWifi();
void printTextHex(const char *label, const char *text);
void displayShowInit();
void displayUpdate();
void displayShowToast(const char *msg);
void syncTime();

void setup()
{
    Serial.begin(115200);
    delay(100);

    EEPROM.begin(EEPROM_SIZE);
    loadConfigFromEeprom();
    updateTopics();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_STA);

    pinMode(48, OUTPUT);
    digitalWrite(48, LOW);
    pinMode(4, INPUT_PULLUP);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(handleCommand);
    getLockStatus();

    // Initialize OLED (I2C: SDA=8, SCL=9)
    Wire.begin(8, 9);
    u8g2.begin();
    u8g2.enableUTF8Print();
    displayInitDone = true;
    displayShowInit();
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
    displayUpdate();

    if (heartbeatPending && mqttClient.connected())
    {
        publishHeartbeat();
        lastHeartbeat = millis();
        heartbeatPending = false;
    }

    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED && mqttClient.connected() && now - lastHeartbeat >= HEARTBEAT_INTERVAL)
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
    if (digitalRead(4) == HIGH)
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
    unsigned long now = millis();
    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED)
    {
        if (!wifiWasConnected)
        {
            if (!lockInterruptEnabled)
            {
                attachInterrupt(
                    digitalPinToInterrupt(4),
                    handleLockStatus,
                    CHANGE);
                lockInterruptEnabled = true;
                Serial.println("Lock interrupt enabled after WiFi connected");
            }
            Serial.print("WiFi connected, IP: ");
            Serial.println(WiFi.localIP());
            syncTime();
            heartbeatPending = true;
        }
        wifiWasConnected = true;
        wifiConnectInProgress = false;
        return;
    }

    if (wifiWasConnected)
    {
        if (lockInterruptEnabled)
        {
            detachInterrupt(digitalPinToInterrupt(4));
            lockInterruptEnabled = false;
            lockStatusChanged = false;
            Serial.println("Lock interrupt disabled on WiFi disconnected");
        }
        if (mqttClient.connected())
        {
            mqttClient.disconnect();
        }
        mqttNeedsReconnect = true;
        wifiWasConnected = false;
    }

    if (wifiForceReconnect)
    {
        WiFi.disconnect();
        wifiForceReconnect = false;
        wifiConnectInProgress = false;
        wifiNextRetryAt = 0;
    }

    if (wifiConnectInProgress)
    {
        bool stillConnecting = (status == WL_IDLE_STATUS || status == WL_DISCONNECTED);
        if (stillConnecting && (now - wifiConnectStartedAt < WIFI_CONNECT_TIMEOUT_MS))
        {
            return;
        }
        wifiConnectInProgress = false;
        wifiNextRetryAt = now + WIFI_RETRY_INTERVAL_MS;
        Serial.printf("WiFi reconnect scheduled, status=%d\n", (int)status);
        return;
    }

    if (strlen(deviceConfig.ssid) == 0 || strlen(deviceConfig.password) == 0)
    {
        wifiNextRetryAt = now + WIFI_CONNECT_TIMEOUT_MS;
        Serial.println("WiFi config empty, waiting for SET WIFI");
        return;
    }

    if (now < wifiNextRetryAt)
    {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(deviceConfig.ssid, deviceConfig.password);
    wifiConnectInProgress = true;
    wifiConnectStartedAt = now;
    Serial.printf("WiFi connecting to SSID=%s, PASSWORD=%s, PASS_LEN=%u\n",
                  deviceConfig.ssid,
                  deviceConfig.password,
                  (unsigned int)strlen(deviceConfig.password));
    printTextHex("SSID_HEX", deviceConfig.ssid);
    printTextHex("PASS_HEX", deviceConfig.password);
}

void printTextHex(const char *label, const char *text)
{
    if (label == nullptr || text == nullptr)
    {
        return;
    }
    Serial.print(label);
    Serial.print(":");
    for (size_t i = 0; text[i] != '\0'; i++)
    {
        Serial.printf(" %02X", (unsigned char)text[i]);
    }
    Serial.println();
}

const char *wifiAuthText(uint8_t mode)
{
    // Numeric fallback mapping for Arduino-ESP32 variants where enum macros may differ.
    switch (mode)
    {
    case 0:
        return "OPEN";
    case 1:
        return "WEP";
    case 2:
        return "WPA_PSK";
    case 3:
        return "WPA2_PSK";
    case 4:
        return "WPA_WPA2_PSK";
    case 5:
        return "WPA2_ENTERPRISE";
    case 6:
        return "WPA3_PSK";
    case 7:
        return "WPA2_WPA3_PSK";
    case 8:
        return "WAPI_PSK";
    default:
        break;
    }

    switch (mode)
    {
#ifdef WIFI_AUTH_OPEN
    case WIFI_AUTH_OPEN:
        return "OPEN";
#endif
#ifdef WIFI_AUTH_WEP
    case WIFI_AUTH_WEP:
        return "WEP";
#endif
#ifdef WIFI_AUTH_WPA_PSK
    case WIFI_AUTH_WPA_PSK:
        return "WPA_PSK";
#endif
#ifdef WIFI_AUTH_WPA2_PSK
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2_PSK";
#endif
#ifdef WIFI_AUTH_WPA_WPA2_PSK
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA_WPA2_PSK";
#endif
#ifdef WIFI_AUTH_WPA2_ENTERPRISE
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2_ENTERPRISE";
#endif
#ifdef WIFI_AUTH_WPA3_PSK
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3_PSK";
#endif
#ifdef WIFI_AUTH_WPA2_WPA3_PSK
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2_WPA3_PSK";
#endif
#ifdef WIFI_AUTH_WAPI_PSK
    case WIFI_AUTH_WAPI_PSK:
        return "WAPI_PSK";
#endif
    default:
        return "UNKNOWN";
    }
}

void scanNearbyWifi()
{
    Serial.println("Scanning nearby WiFi...");
    WiFi.mode(WIFI_STA);
    int found = WiFi.scanNetworks(false, true);
    if (found < 0)
    {
        Serial.println("WiFi scan failed");
        return;
    }
    if (found == 0)
    {
        Serial.println("No WiFi found");
        return;
    }

    Serial.printf("Found %d WiFi network(s):\n", found);
    for (int i = 0; i < found; i++)
    {
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        int32_t channel = WiFi.channel(i);
        uint8_t auth = WiFi.encryptionType(i);
        bool hidden = (ssid.length() == 0);
        Serial.printf("[%d] SSID=%s RSSI=%lddBm CH=%ld AUTH=%s(%u)%s\n",
                      i + 1,
                      hidden ? "<hidden>" : ssid.c_str(),
                      (long)rssi,
                      (long)channel,
                      wifiAuthText(auth),
                      (unsigned int)auth,
                      hidden ? " HIDDEN" : "");
    }
    WiFi.scanDelete();
}

void ensureMqttConnected()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (mqttNeedsReconnect && mqttClient.connected())
    {
        mqttClient.disconnect();
    }

    if (mqttClient.connected())
    {
        return;
    }

    unsigned long now = millis();
    if (now - lastMqttAttemptAt < MQTT_RETRY_INTERVAL_MS)
    {
        return;
    }

    lastMqttAttemptAt = now;
    Serial.print("Connecting MQTT");
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
    }
}

void publishHeartbeat()
{
    if (WiFi.status() != WL_CONNECTED || !mqttClient.connected())
    {
        return;
    }
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
        // Send heartbeat with the verified lock state (after settling delay)
        heartbeatPending = true;
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

bool executeUnlock()
{
    bool opened = false;
    for (uint8_t attempt = 1; attempt <= UNLOCK_MAX_ATTEMPTS; attempt++)
    {
        Serial.printf("Unlock attempt %u\n", attempt);
        opened = attemptUnlockOnce();
        if (opened)
        {
            Serial.println("Lock opened");
            break;
        }

        if (attempt < UNLOCK_MAX_ATTEMPTS)
        {
            Serial.println("Lock still closed, retrying...");
            delay(UNLOCK_RETRY_GAP_MS);
        }
    }
    if (!opened)
    {
        Serial.println("Unlock failed after retries");
    }

    // Wait for electrical noise to settle, then verify the TRUE lock state
    delay(UNLOCK_VERIFY_DELAY_MS);
    getLockStatus();
    Serial.printf("Verified lock state after unlock: %s\n", lockOpen ? "OPEN" : "CLOSED");
    return lockOpen;
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
    digitalWrite(48, HIGH);
    delay(UNLOCK_PULSE_MS);
    digitalWrite(48, LOW);

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
        Serial.println("Commands: HELP | SHOW | SCAN | SET WIFI <ssid> <password> | SET ID <deviceCode>");
        displayShowToast("HELP: 查看命令列表");
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
        displayShowToast("已显示配置信息");
        return;
    }

    if (line.equalsIgnoreCase("SCAN"))
    {
        scanNearbyWifi();
        displayShowToast("WiFi 扫描中...");
        return;
    }

    if (line.startsWith("SET WIFI "))
    {
        String rest = line.substring(9);
        int spacePos = rest.indexOf(' ');
        if (spacePos <= 0)
        {
            Serial.println("Invalid format. Use: SET WIFI <ssid> <password>");
            displayShowToast("格式错误: SET WIFI SSID PWD");
            return;
        }
        String ssid = rest.substring(0, spacePos);
        String password = rest.substring(spacePos + 1);
        ssid.trim();
        password.trim();
        if (ssid.length() == 0 || password.length() == 0)
        {
            Serial.println("SSID or password empty");
            displayShowToast("SSID 或密码为空");
            return;
        }
        if (ssid.length() >= sizeof(deviceConfig.ssid))
        {
            Serial.printf("SSID too long: %u bytes, max %u bytes\n",
                          (unsigned int)ssid.length(),
                          (unsigned int)(sizeof(deviceConfig.ssid) - 1));
            displayShowToast("SSID 过长");
            return;
        }
        if (password.length() >= sizeof(deviceConfig.password))
        {
            Serial.printf("Password too long: %u bytes, max %u bytes\n",
                          (unsigned int)password.length(),
                          (unsigned int)(sizeof(deviceConfig.password) - 1));
            displayShowToast("密码过长");
            return;
        }
        strlcpy(deviceConfig.ssid, ssid.c_str(), sizeof(deviceConfig.ssid));
        strlcpy(deviceConfig.password, password.c_str(), sizeof(deviceConfig.password));
        saveConfigToEeprom();
        Serial.println("WiFi credentials updated");
        displayShowToast("WiFi 已更新，重新连接中");
        heartbeatPending = true;
        wifiForceReconnect = true;
        wifiConnectInProgress = false;
        wifiWasConnected = false;
        wifiNextRetryAt = 0;
        return;
    }

    if (line.startsWith("SET ID "))
    {
        String code = line.substring(7);
        code.trim();
        if (code.length() == 0)
        {
            Serial.println("Device ID empty");
            displayShowToast("设备编号为空");
            return;
        }
        strlcpy(deviceConfig.deviceCode, code.c_str(), sizeof(deviceConfig.deviceCode));
        saveConfigToEeprom();
        applyDeviceCodeChange();
        Serial.println("Device ID updated");
        displayShowToast("设备编号已更新");
        return;
    }

    Serial.println("Unknown command. Type HELP");
    displayShowToast("未知命令，输入 HELP");
}

void applyDeviceCodeChange()
{
    updateTopics();
    mqttNeedsReconnect = true;
    heartbeatPending = true;
}

// ─────────────────────────────────────────────
// OLED Display Functions
// ─────────────────────────────────────────────

void displayShowInit()
{
    if (!displayInitDone)
        return;
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);

    // ── Step 1: frame + title ──
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.setCursor(16, 24);
    u8g2.print("物联网陪护床");
    u8g2.setCursor(28, 40);
    u8g2.print("系统启动中...");
    // Empty progress bar track
    u8g2.drawFrame(14, 48, 100, 10);
    u8g2.sendBuffer();
    delay(400);

    const char *steps[] = {"读取配置...", "初始化外设...", "OLED 就绪"};
    const uint8_t progresses[] = {30, 65, 100};
    const uint8_t stepCount = 3;

    for (uint8_t i = 0; i < stepCount; i++)
    {
        // Update progress text
        u8g2.setCursor(28, 40);
        u8g2.print("            "); // clear previous text
        u8g2.setCursor(28, 40);
        u8g2.print(steps[i]);

        // Draw filled progress bar
        u8g2.drawBox(15, 49, progresses[i], 8);

        u8g2.sendBuffer();
        delay(350);
    }

    // Brief hold on "启动完成"
    u8g2.setCursor(28, 40);
    u8g2.print("启动完成  ");
    u8g2.sendBuffer();
    delay(500);
}

void syncTime()
{
    if (WiFi.status() != WL_CONNECTED)
        return;
    Serial.println("Starting NTP time sync...");
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.nist.gov");
    ntpSynced = false;
    lastNtpAttempt = millis();
}

void displayShowToast(const char *msg)
{
    if (!displayInitDone)
        return;
    strncpy(toastMessage, msg, sizeof(toastMessage) - 1);
    toastMessage[sizeof(toastMessage) - 1] = '\0';
    toastEndTime = millis() + TOAST_DURATION_MS;
}

void displayUpdate()
{
    if (!displayInitDone)
        return;

    unsigned long now = millis();
    if (now - lastDisplayUpdate < DISPLAY_REFRESH_INTERVAL_MS)
        return;
    lastDisplayUpdate = now;

    // Retry NTP if not synced yet
    if (!ntpSynced && WiFi.status() == WL_CONNECTED && now - lastNtpAttempt > NTP_RETRY_INTERVAL_MS)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            ntpSynced = true;
            Serial.println("NTP time synced");
        }
        else
        {
            lastNtpAttempt = now;
        }
    }

    // ── If toast is active, show overlay ──
    if (now < toastEndTime)
    {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
        // Dark overlay background
        u8g2.drawBox(0, 22, 128, 24);
        u8g2.setDrawColor(0); // white bg → we want white text on black box
        u8g2.setCursor(10, 38);
        u8g2.print(toastMessage);
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
        return;
    }

    // ── Normal display ──
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);

    // ── Top bar: device code + time ──
    u8g2.drawStr(2, 11, deviceConfig.deviceCode);

    if (ntpSynced)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            char timeStr[10];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            int tw = u8g2.getStrWidth(timeStr);
            u8g2.drawStr(128 - tw - 2, 11, timeStr);
        }
    }

    // Separator line
    u8g2.drawHLine(2, 16, 124);

    // ── WiFi status ──
    if (WiFi.status() == WL_CONNECTED)
    {
        u8g2.drawDisc(6, 26, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 28);
        u8g2.print("WiFi 已连接");
        // Show RSSI level on the right
        int rssi = WiFi.RSSI();
        char rssiStr[8];
        snprintf(rssiStr, sizeof(rssiStr), "%d dBm", rssi);
        int rw = u8g2.getStrWidth(rssiStr);
        if (rw < 60)
        {
            u8g2.drawStr(128 - rw - 2, 28, rssiStr);
        }
    }
    else
    {
        u8g2.drawCircle(6, 26, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 28);
        u8g2.print("WiFi 已断开");
    }

    // ── MQTT / Server status ──
    if (mqttClient.connected())
    {
        u8g2.drawDisc(6, 40, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 42);
        u8g2.print("服务器 已连接");
    }
    else
    {
        u8g2.drawCircle(6, 40, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 42);
        u8g2.print("服务器 已断开");
    }

    // ── Lock status ──
    if (lockOpen)
    {
        u8g2.drawCircle(6, 54, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 56);
        u8g2.print("门锁 已打开");
    }
    else
    {
        u8g2.drawDisc(6, 54, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, 56);
        u8g2.print("门锁 已关闭");
    }

    u8g2.sendBuffer();
}
