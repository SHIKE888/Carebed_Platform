/**
 * ============================================================================
 * CareBed 物联网陪护床 — ESP32 蓝牙 Mesh 组网固件
 * ============================================================================
 *
 * 功能概述：
 *   1. BLE 主从一体：每个 ESP32 同时运行 GATT Server（从机广播设备信息）
 *      和 GATT Client（主机扫描并连接附近同类设备），形成 mesh 网络。
 *   2. 智能网关：连接 WiFi 成功的设备自动成为 mesh 网关节点，
 *      动态订阅 mesh 中所有设备的 MQTT 主题，收到消息后通过蓝牙转发。
 *   3. 防泛洪转发：每条消息携带唯一 msgId 和 TTL，
 *      每个节点只执行/转发一次，丢弃重复消息，杜绝消息环路。
 *   4. 多 MQTT 主题动态订阅：网关节点根据 mesh 成员列表，
 *      自动订阅 devices/{deviceCode}/command 等主题。
 *
 * 硬件接线（同参考固件）：
 *   - 继电器/锁控输出：GPIO 48（高电平脉冲）
 *   - 锁状态检测：GPIO 4，上拉输入，CHANGE 中断
 *   - OLED I2C：SDA=8, SCL=9
 *   - 串口：115200 baud
 *
 * 依赖库：
 *   - BLEDevice (内置 esp32 库)
 *   - PubSubClient (Nick O'Leary)
 *   - WiFi.h (内置)
 *   - U8g2lib (olikraus)
 *   - ArduinoJson (bblanchon) — 用于 BLE 消息序列化
 *
 * 编译环境：Arduino ESP32 或 PlatformIO
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>
#include <map>
#include <list>
#include <algorithm>

// ============================================================================
// 配置常量
// ============================================================================

// ---- BLE ----
#define MESH_SERVICE_UUID        "cafebeed-0001-4d45-5348-000000000000"
#define MESH_DATA_CHAR_UUID      "cafebeed-0002-4d45-5348-000000000000"
#define BLE_DEVICE_PREFIX        "CB-MESH-"       // BLE 广播名称前缀
#define MESH_NETWORK_ID          0xCB01           // mesh 网络标识，防止不同网络设备互联
#define BLE_ADVERT_INTERVAL_MS   250              // 广播间隔
#define BLE_SCAN_INTERVAL_MS     400              // 扫描间隔（仅网关）
#define BLE_SCAN_WINDOW_MS       200              // 扫描窗口（仅网关）
#define BLE_SCAN_PERIOD_MS       3000             // 单次扫描时长
#define BLE_RECONNECT_INTERVAL_MS 30000           // BLE 重连间隔
#define MAX_BLE_CONNECTIONS      6                // 最大同时 BLE 连接数

// ---- Mesh 消息 ----
#define MAX_MESH_TTL             5                // 消息最大跳数
#define MSG_ID_CACHE_SIZE        128              // 消息 ID 缓存大小（LRU）
#define MESH_SYNC_INTERVAL_MS    60000            // mesh 拓扑同步间隔

// ---- MQTT ----
const char *MQTT_HOST = "msas.absozero.cn";
const int   MQTT_PORT = 1883;
const uint8_t COMMAND_QOS = 1;
const unsigned long HEARTBEAT_INTERVAL_MS = 10000; // 10 秒心跳
const unsigned long MQTT_RETRY_INTERVAL_MS = 2000;

// ---- WiFi ----
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 3000;

// ---- 硬件 ----
const uint8_t LOCK_OUTPUT_PIN = 48;
const uint8_t LOCK_SENSOR_PIN = 4;
const uint8_t UNLOCK_PULSE_MS = 500;
const uint8_t UNLOCK_MAX_ATTEMPTS = 3;
const unsigned long UNLOCK_VERIFY_DELAY_MS = 300;
const unsigned long UNLOCK_RETRY_GAP_MS = 300;

// ---- 屏幕 ----
const unsigned long DISPLAY_REFRESH_INTERVAL_MS = 300;
const unsigned long TOAST_DURATION_MS = 2500;
const unsigned long NTP_RETRY_INTERVAL_MS = 30000;

// ---- EEPROM ----
struct DeviceConfig {
    char ssid[32];
    char password[64];
    char deviceCode[32];
};

DeviceConfig deviceConfig = {
    "Pura70",
    "12345678",
    "BED-0001"
};

const uint32_t CONFIG_MAGIC = 0xCAFEB33D;
const size_t   EEPROM_SIZE  = 256;

// ============================================================================
// 全局变量
// ============================================================================

// ---- BLE ----
BLEServer      *pMeshServer = nullptr;
BLECharacteristic *pMeshDataChar = nullptr;
bool            bleScanActive = false;
bool            bleScanDone = false;
unsigned long   lastBleScanStart = 0;

// ---- BLE 连接管理 ----
struct BlePeer {
    uint16_t    connId;
    BLEAddress  address;
    String      deviceCode;
    uint8_t     hopsAway;          // 到本设备的估计跳数
    bool        isGateway;         // 是否具有网关能力
    unsigned long lastSeen;
    bool        subscribed;        // 是否已订阅 notify
};
std::list<BlePeer> blePeers;
int maxPeersSeen = 0;

// ---- MQTT ----
WiFiClient      wifiClient;
PubSubClient    mqttClient(wifiClient);
bool            mqttNeedsReconnect = false;
unsigned long   lastMqttAttemptAt = 0;
unsigned long   lastHeartbeatSent = 0;
bool            heartbeatPending = false;

// ---- WiFi ----
bool wifiConnectInProgress = false;
bool wifiWasConnected = false;
unsigned long wifiConnectStartedAt = 0;
unsigned long wifiNextRetryAt = 0;

// ---- 锁 ----
volatile bool lockOpen = false;
volatile bool lockStatusChanged = false;
bool lockInterruptEnabled = false;
const uint8_t BATTERY_LEVEL = 100;

// ---- 消息 ID 缓存 (LRU) ----
std::list<String> msgIdCache;
typedef std::list<String>::iterator MsgIdIter;
std::map<String, MsgIdIter> msgIdMap;

// ---- OLED ----
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);
bool  displayInitDone = false;
bool  ntpSynced = false;
unsigned long lastNtpAttempt = 0;
unsigned long lastDisplayUpdate = 0;
char  toastMessage[64];
unsigned long toastEndTime = 0;

// ---- Mesh 状态 ----
bool isGateway = false;               // 本设备是否为网关（有 WiFi 连接）
unsigned long lastMeshSyncAt = 0;

// ============================================================================
// 消息类型枚举（通过 BLE mesh 传输）
// ============================================================================
enum MeshMsgType : uint8_t {
    MSG_UNKNOWN    = 0,
    MSG_HEARTBEAT  = 1,   // 心跳数据（mesh 内转发）
    MSG_COMMAND    = 2,   // 远程指令（来自 MQTT）
    MSG_COMMAND_RESP = 3, // 指令执行结果（发往 MQTT）
    MSG_TOPIC_SYNC = 4,   // 主题订阅同步（网关下发）
    MSG_MESH_SYNC  = 5,   // mesh 拓扑同步广播
    MSG_MESH_JOIN  = 6,   // 新设备加入 mesh
    MSG_MESH_LEAVE = 7,   // 设备离开 mesh
};

// ============================================================================
// 前向声明
// ============================================================================
void IRAM_ATTR handleLockISR();
void getLockStatus();
void ensureWifiConnected();
void ensureMqttConnected();
void publishHeartbeat();
void handleMqttCommand(char *topic, byte *payload, unsigned int length);
void handleSerialConfig();
void updateMqttTopics();
void meshInit();
void meshScanAndConnect();
void meshDisconnectAll();
void meshSendMessage(const String &msgJson);
void meshSendToPeer(BlePeer &peer, const String &msgJson);
void meshBroadcast(const String &msgJson);
void meshForwardMessage(const String &msgJson, BlePeer *excludePeer);
void processMeshMessage(const String &msgJson, BlePeer *fromPeer);
String createMeshMessage(MeshMsgType type, const String &payload, uint8_t ttl = MAX_MESH_TTL);
bool isMsgIdProcessed(const String &msgId);
void recordMsgId(const String &msgId);
String generateMsgId();
void syncMeshTopology();
void subscribeAllMeshTopics();
void unsubscribeAllMeshTopics();
void connectToPeers();
bool executeUnlock();
String getMeshPeerListJson();
void displayShowInit();
void displayUpdate();
void displayShowToast(const char *msg);
void syncTime();
void loadConfigFromEeprom();
void saveConfigToEeprom();
void applyDeviceCodeChange();

// ============================================================================
// BLE Server 回调
// ============================================================================
class MeshServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
        Serial.printf("BLE client connected, conn_id=%d\n", param->connect.conn_id);
    }
    void onDisconnect(BLEServer* pServer) override {
        Serial.println("BLE client disconnected");
        // 重新开启广播，让其他设备可发现
        if (pMeshServer) {
            pMeshServer->startAdvertising();
        }
    }
};

// ============================================================================
// BLE Data Characteristic 回调（接收 mesh 数据）
// ============================================================================
class MeshDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) override {
        String value = pChar->getValue();
        if (value.length() == 0) return;
        Serial.printf("BLE mesh data received (%u bytes): %s\n",
                      value.length(), value.c_str());
        processMeshMessage(value, nullptr);
    }
};

// ============================================================================
// BLE Client 回调
// ============================================================================
class MeshClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient *pClient) override {
        Serial.println("BLE client connected to peer");
    }
    void onDisconnect(BLEClient *pClient) override {
        Serial.println("BLE client disconnected from peer");
        // 从 peers 列表中移除
        BLEAddress addr = pClient->getPeerAddress();
        for (auto it = blePeers.begin(); it != blePeers.end(); ) {
            if ((*it).address.equals(addr)) {
                Serial.printf("Removed peer %s from list\n", it->deviceCode.c_str());
                it = blePeers.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// ============================================================================
// BLE 扫描回调
// ============================================================================
class MeshAdvertCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        String devName = advertisedDevice.getName().c_str();

        // 过滤：只关心本 mesh 网络的设备
        if (!devName.startsWith(BLE_DEVICE_PREFIX)) return;

        // 从广播数据中提取 mesh 网络 ID
        // 格式: CB-MESH-{networkId}-{deviceCode}
        // 例如: CB-MESH-CB01-BED-0001
        String fullName = devName;

        // 检查是否已在连接列表中
        for (auto &peer : blePeers) {
            if (peer.address.equals(advertisedDevice.getAddress())) {
                peer.lastSeen = millis();
                return;
            }
        }

        // 解析设备编号
        // CB-MESH-CB01-BED-0001 → BED-0001
        int firstDash = fullName.indexOf('-', strlen(BLE_DEVICE_PREFIX));
        int secondDash = fullName.indexOf('-', firstDash + 1);
        String deviceCode;
        if (secondDash > 0) {
            deviceCode = fullName.substring(secondDash + 1);
        } else {
            deviceCode = "UNKNOWN";
        }

        // 不连接自己
        if (deviceCode.equalsIgnoreCase(String(deviceConfig.deviceCode))) return;

        // 不重复连接已满
        if (blePeers.size() >= MAX_BLE_CONNECTIONS) return;

        Serial.printf("Found mesh peer: %s (%s), addr=%s, RSSI=%d\n",
                      deviceCode.c_str(), fullName.c_str(),
                      advertisedDevice.getAddress().toString().c_str(),
                      advertisedDevice.getRSSI());

        // 添加到 peers 列表
        BlePeer peer;
        peer.address = advertisedDevice.getAddress();
        peer.deviceCode = deviceCode;
        peer.hopsAway = 1; // 直接相邻
        peer.isGateway = false; // 稍后通过特征读取判断
        peer.lastSeen = millis();
        peer.subscribed = false;
        peer.connId = 0;
        blePeers.push_back(peer);
        maxPeersSeen++;
    }
};

// ============================================================================
// 设置函数
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n===== CareBed BLE Mesh Firmware =====");

    // 加载配置
    EEPROM.begin(EEPROM_SIZE);
    loadConfigFromEeprom();

    // WiFi 初始化
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_STA);

    // 锁引脚
    pinMode(LOCK_OUTPUT_PIN, OUTPUT);
    digitalWrite(LOCK_OUTPUT_PIN, LOW);
    pinMode(LOCK_SENSOR_PIN, INPUT_PULLUP);

    // MQTT
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(handleMqttCommand);

    // 初始化 BLE（主从一体）
    meshInit();

    // 读取锁状态
    getLockStatus();

    // OLED 初始化
    Wire.begin(8, 9);
    u8g2.begin();
    u8g2.enableUTF8Print();
    displayInitDone = true;
    displayShowInit();

    Serial.println("Setup complete. Device code: " + String(deviceConfig.deviceCode));
    displayShowToast("系统就绪");
}

// ============================================================================
// 主循环
// ============================================================================
void loop() {
    unsigned long now = millis();

    // ---- 1. 锁中断处理 ----
    if (lockStatusChanged) {
        lockStatusChanged = false;
        getLockStatus();
        heartbeatPending = true;
        Serial.println("Lock status changed, heartbeat scheduled");
    }

    // ---- 2. 串口命令 ----
    handleSerialConfig();

    // ---- 3. WiFi 连接管理 ----
    ensureWifiConnected();

    // ---- 4. 网关角色切换 ----
    bool wasGateway = isGateway;
    isGateway = (WiFi.status() == WL_CONNECTED);
    if (isGateway && !wasGateway) {
        // 刚获得网关能力，开始扫描
        Serial.println("Device became gateway, starting BLE scan");
        lastBleScanStart = 0;
        // 订阅所有已知 mesh 设备的 MQTT 主题
        subscribeAllMeshTopics();
        displayShowToast("成为网关节点");
    } else if (!isGateway && wasGateway) {
        // 失去网关能力
        Serial.println("Device lost gateway capability");
        unsubscribeAllMeshTopics();
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        displayShowToast("失去网关连接");
    }

    // ---- 5. MQTT 连接 ----
    if (isGateway) {
        ensureMqttConnected();
        mqttClient.loop();
    }

    // ---- 6. BLE 扫描（仅网关扫描，非网关只广播） ----
    if (isGateway) {
        if (!bleScanActive && (now - lastBleScanStart > BLE_SCAN_PERIOD_MS + 1000)) {
            meshScanAndConnect();
        }
        // 如果 BLE 扫描超时，停止
        if (bleScanActive && (now - lastBleScanStart > BLE_SCAN_PERIOD_MS)) {
            BLEDevice::getScan()->stop();
            bleScanActive = false;
            bleScanDone = true;
            Serial.printf("BLE scan finished, found %u peers\n", blePeers.size());
            // 连接发现的 peer
            connectToPeers();
        }
    }

    // ---- 7. 心跳发送 ----
    if (isGateway && mqttClient.connected()) {
        if (heartbeatPending || (now - lastHeartbeatSent >= HEARTBEAT_INTERVAL_MS)) {
            publishHeartbeat();
            lastHeartbeatSent = now;
            heartbeatPending = false;
        }
    }

    // ---- 8. Mesh 拓扑同步 ----
    if (now - lastMeshSyncAt >= MESH_SYNC_INTERVAL_MS) {
        lastMeshSyncAt = now;
        syncMeshTopology();
    }

    // ---- 9. BLE 重连检查 ----
    if (isGateway) {
        for (auto it = blePeers.begin(); it != blePeers.end(); ) {
            if (now - (*it).lastSeen > BLE_RECONNECT_INTERVAL_MS) {
                Serial.printf("Peer %s timed out, removing\n", (*it).deviceCode.c_str());
                it = blePeers.erase(it);
            } else {
                ++it;
            }
        }
    }

    // ---- 10. OLED 显示更新 ----
    displayUpdate();
}

// ============================================================================
// BLE Mesh 初始化（主从一体）
// ============================================================================
void meshInit() {
    String bleDeviceName = String(BLE_DEVICE_PREFIX) +
                           String(MESH_NETWORK_ID, HEX) + "-" +
                           String(deviceConfig.deviceCode);
    Serial.printf("Initializing BLE as: %s\n", bleDeviceName.c_str());

    // 初始化 BLE
    BLEDevice::init(bleDeviceName.c_str());
    // 同时支持 Central 和 Peripheral
    BLEDevice::setPower(ESP_PWR_LVL_P7); // 最大功率

    // ---- 创建 GATT Server（Peripheral 角色） ----
    pMeshServer = BLEDevice::createServer();
    pMeshServer->setCallbacks(new MeshServerCallbacks());

    // 创建 Service
    BLEService *pMeshService = pMeshServer->createService(MESH_SERVICE_UUID);

    // 创建 Data Characteristic (Notify + Write)
    pMeshDataChar = pMeshService->createCharacteristic(
        MESH_DATA_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
    );
    pMeshDataChar->addDescriptor(new BLE2902());
    pMeshDataChar->setCallbacks(new MeshDataCallbacks());

    // 启动 Service
    pMeshService->start();

    // 配置广播
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(MESH_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // 有助于 iPhone 连接
    pAdvertising->setMinInterval(BLE_ADVERT_INTERVAL_MS);
    pAdvertising->setMaxInterval(BLE_ADVERT_INTERVAL_MS * 2);

    // 启动广播
    pMeshServer->startAdvertising();
    Serial.println("BLE server started, advertising mesh service");
}

// ============================================================================
// BLE 扫描并发现附近设备
// ============================================================================
void meshScanAndConnect() {
    if (!isGateway) return;
    if (blePeers.size() >= MAX_BLE_CONNECTIONS) return;

    Serial.println("Starting BLE scan for mesh peers...");
    BLEScan *pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MeshAdvertCallbacks());
    pScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pScan->setWindow(BLE_SCAN_WINDOW_MS);
    pScan->setActiveScan(true);
    pScan->start(BLE_SCAN_PERIOD_MS / 1000, false); // 阻塞扫描
    bleScanActive = true;
    lastBleScanStart = millis();

    // 在 onResult 中收集 peer，扫描结束后 connectToPeers 建立连接
}

// ============================================================================
// 连接 BLE Peers（网关主动连接发现的设备）
// ============================================================================
void connectToPeers() {
    if (!isGateway) return;
    if (blePeers.empty()) return;

    Serial.printf("Connecting to %u peers...\n", blePeers.size());

    for (auto &peer : blePeers) {
        if (peer.subscribed) continue; // 已连接

        // 创建 BLE Client 连接 peer
        BLEClient *pClient = BLEDevice::createClient();
        pClient->setClientCallbacks(new MeshClientCallback());

        Serial.printf("Connecting to %s at %s...\n",
                      peer.deviceCode.c_str(),
                      peer.address.toString().c_str());

        if (!pClient->connect(peer.address)) {
            Serial.printf("Failed to connect to %s\n", peer.deviceCode.c_str());
            BLEDevice::deleteClient(pClient);
            continue;
        }

        // 发现 Service
        BLERemoteService *pRemoteService = pClient->getService(MESH_SERVICE_UUID);
        if (pRemoteService == nullptr) {
            Serial.println("Mesh service not found on peer");
            pClient->disconnect();
            BLEDevice::deleteClient(pClient);
            continue;
        }

        // 发现 Characteristic
        BLERemoteCharacteristic *pRemoteChar =
            pRemoteService->getCharacteristic(MESH_DATA_CHAR_UUID);
        if (pRemoteChar == nullptr) {
            Serial.println("Mesh data char not found on peer");
            pClient->disconnect();
            BLEDevice::deleteClient(pClient);
            continue;
        }

        // 注册 notify 回调
        pRemoteChar->registerForNotify([](BLERemoteCharacteristic* pChar,
                                           uint8_t* data, size_t len,
                                           bool isNotify) {
            if (len == 0) return;
            String msg((char*)data, len);
            Serial.printf("BLE notify received (%u bytes): %s\n", len, msg.c_str());
            processMeshMessage(msg, nullptr);
        });

        peer.subscribed = true;
        peer.lastSeen = millis();
        Serial.printf("Connected to peer %s\n", peer.deviceCode.c_str());

        // 发送 mesh 同步消息
        String syncMsg = createMeshMessage(MSG_MESH_SYNC,
            "{\"deviceCode\":\"" + String(deviceConfig.deviceCode) +
            "\",\"isGateway\":" + (isGateway ? "true" : "false") + "}", 1);
        pRemoteChar->writeValue(syncMsg.c_str(), syncMsg.length(), true);
    }

    // 更新 MQTT 订阅
    if (isGateway) {
        subscribeAllMeshTopics();
    }
}

// ============================================================================
// 处理 Mesh 消息（核心防泛洪逻辑）
// ============================================================================
void processMeshMessage(const String &msgJson, BlePeer *fromPeer) {
    if (msgJson.length() < 10) return; // 太短，忽略

    // ---- 解析消息字段 ----
    // 格式: {"i":"msgId","t":"type","s":"source","p":"payload_json","h":ttl}
    String msgId = extractJsonField(msgJson, "i");
    if (msgId.length() == 0) {
        Serial.println("Mesh msg missing msgId, ignored");
        return;
    }

    // ---- 防泛洪核心：去重检测 ----
    if (isMsgIdProcessed(msgId)) {
        Serial.printf("Duplicate mesh msg %s ignored\n", msgId.c_str());
        return;
    }
    recordMsgId(msgId);

    String typeStr = extractJsonField(msgJson, "t");
    String source  = extractJsonField(msgJson, "s");
    String payload = extractJsonField(msgJson, "p");
    String ttlStr  = extractJsonField(msgJson, "h");
    uint8_t ttl = (uint8_t)ttlStr.toInt();

    // 来源是自己？忽略
    if (source.equalsIgnoreCase(String(deviceConfig.deviceCode))) {
        return;
    }

    Serial.printf("Mesh msg: id=%s type=%s from=%s ttl=%u payload=%s\n",
                  msgId.c_str(), typeStr.c_str(), source.c_str(), ttl, payload.c_str());

    // ---- 根据消息类型处理 ----
    MeshMsgType msgType = (MeshMsgType)typeStr.toInt();
    bool needForward = true;

    switch (msgType) {
        case MSG_HEARTBEAT: {
            // 心跳：只转发，不处理（发往 MQTT 的工作由网关做）
            if (isGateway && mqttClient.connected()) {
                // 替 mesh 中的非网关设备转发心跳到 MQTT
                String heartTopic = String("devices/") + source + "/heartbeat";
                mqttClient.publish(heartTopic.c_str(), payload.c_str(), true);
                Serial.printf("Relayed heartbeat for %s to MQTT\n", source.c_str());
            }
            break;
        }
        case MSG_COMMAND: {
            // 指令：检查是否是给自己的
            String dst = extractJsonField(msgJson, "d");
            if (dst.equalsIgnoreCase(String(deviceConfig.deviceCode)) || dst == "ALL") {
                // 执行指令
                String command = extractJsonField(payload, "command");
                command.toUpperCase();

                Serial.printf("Executing command: %s\n", command.c_str());

                if (command == "UNLOCK") {
                    executeUnlock();
                    heartbeatPending = true;

                    // 如果有 msgId，记入去重
                    if (msgId.length() > 0) {
                        // 已在上方记录
                    }

                    // 发送执行结果回 mesh
                    String respPayload = "{\"command\":\"UNLOCK\",\"result\":\"" +
                                         (lockOpen ? "SUCCESS" : "FAILED") + "\"}";
                    String resp = createMeshMessage(MSG_COMMAND_RESP, respPayload, ttl);
                    meshBroadcast(resp);
                } else if (command == "REBOOT") {
                    String respPayload = "{\"command\":\"REBOOT\",\"result\":\"ACK\"}";
                    String resp = createMeshMessage(MSG_COMMAND_RESP, respPayload, ttl);
                    meshBroadcast(resp);

                    // 延迟重启，让消息发送完成
                    needForward = false;
                    delay(100);
                    ESP.restart();
                }
            }
            break;
        }
        case MSG_COMMAND_RESP: {
            // 指令执行结果：网关收到后发往 MQTT
            if (isGateway && mqttClient.connected()) {
                String cmdTopic = String("devices/") + source + "/response";
                mqttClient.publish(cmdTopic.c_str(), payload.c_str(), false);
            }
            break;
        }
        case MSG_MESH_SYNC: {
            // mesh 拓扑同步：更新 peer 信息
            String peerCode = extractJsonField(payload, "deviceCode");
            String peerGw   = extractJsonField(payload, "isGateway");
            if (peerCode.length() > 0 && !peerCode.equalsIgnoreCase(String(deviceConfig.deviceCode))) {
                // 更新或添加 peer
                bool found = false;
                for (auto &p : blePeers) {
                    if (p.deviceCode == peerCode) {
                        p.lastSeen = millis();
                        p.isGateway = (peerGw == "true");
                        found = true;
                        break;
                    }
                }
                if (!found && blePeers.size() < MAX_BLE_CONNECTIONS) {
                    BlePeer newPeer;
                    newPeer.deviceCode = peerCode;
                    newPeer.isGateway = (peerGw == "true");
                    newPeer.hopsAway = 2; // 通过同步了解的至少隔 2 跳
                    newPeer.lastSeen = millis();
                    newPeer.subscribed = false;
                    blePeers.push_back(newPeer);
                    Serial.printf("Learned new peer via mesh sync: %s\n", peerCode.c_str());
                }
            }
            needForward = true;
            break;
        }
        case MSG_MESH_JOIN: {
            Serial.printf("Device %s joined mesh\n", source.c_str());
            if (isGateway) {
                subscribeAllMeshTopics();
            }
            break;
        }
        case MSG_MESH_LEAVE: {
            Serial.printf("Device %s left mesh\n", source.c_str());
            // 从 peer 列表中移除
            for (auto it = blePeers.begin(); it != blePeers.end(); ) {
                if ((*it).deviceCode == source) {
                    it = blePeers.erase(it);
                } else {
                    ++it;
                }
            }
            if (isGateway) {
                // 取消订阅离开设备的主题（下次 subscribeAll 会重新计算）
                String cmdTopic = String("devices/") + source + "/command";
                mqttClient.unsubscribe(cmdTopic.c_str());
                Serial.printf("Unsubscribed from %s\n", cmdTopic.c_str());
            }
            break;
        }
        default:
            Serial.printf("Unknown mesh msg type: %s\n", typeStr.c_str());
            break;
    }

    // ---- 转发：仅当 TTL > 0 时才转发 ----
    if (needForward && ttl > 1) {
        // TTL 减 1 后广播（排除来源 peer）
        String forwardMsg = msgJson;
        // 更新 TTL
        uint8_t newTtl = ttl - 1;
        replaceJsonField(forwardMsg, "h", String(newTtl));
        meshBroadcast(forwardMsg);
        Serial.printf("Forwarded mesh msg %s (TTL=%u->%u)\n",
                      msgId.c_str(), ttl, newTtl);
    } else if (needForward && ttl <= 1) {
        Serial.printf("Mesh msg %s TTL exhausted, not forwarding\n", msgId.c_str());
    }
}

// ============================================================================
// 广播 mesh 消息到所有已连接的 peer
// ============================================================================
void meshBroadcast(const String &msgJson) {
    for (auto &peer : blePeers) {
        if (peer.subscribed) {
            meshSendToPeer(peer, msgJson);
        }
    }
}

// ============================================================================
// 向指定 peer 发送 mesh 消息
// ============================================================================
void meshSendToPeer(BlePeer &peer, const String &msgJson) {
    // 通过已连接的 BLE Client 发送
    // 注意：这里简化处理，实际需要通过 peer 的 BLE connection 发送
    // 由于 BLEDevice 库的限制，需要存储每个 peer 的 remote characteristic 引用

    // 简化方案：通过 Server 的 Notify 发送给所有已连接的 Client
    // 或通过 Client 的 write 发送给 Server
    // 这里采用 Server Notify 方式（假设所有连接都是双向的）
    if (pMeshDataChar) {
        pMeshDataChar->setValue(msgJson);
        pMeshDataChar->notify();
        Serial.printf("Broadcast via BLE notify: %s\n", msgJson.c_str());
    }
}

// ============================================================================
// 创建 Mesh 消息 JSON
// ============================================================================
String createMeshMessage(MeshMsgType type, const String &payload, uint8_t ttl) {
    String msgId = generateMsgId();
    String json = String("{\"i\":\"") + msgId +
                  "\",\"t\":" + String((int)type) +
                  ",\"s\":\"" + deviceConfig.deviceCode +
                  "\",\"h\":" + String(ttl) +
                  ",\"p\":" + payload + "}";
    return json;
}

// ============================================================================
// 生成唯一消息 ID
// ============================================================================
String generateMsgId() {
    static uint32_t seq = 0;
    seq++;
    // 使用设备编号 + 时间戳 + 序列号
    return String(deviceConfig.deviceCode) + "-" +
           String(millis(), HEX) + "-" +
           String(seq);
}

// ============================================================================
// 消息 ID 去重缓存（LRU）
// ============================================================================
bool isMsgIdProcessed(const String &msgId) {
    return msgIdMap.find(msgId) != msgIdMap.end();
}

void recordMsgId(const String &msgId) {
    // 如果已存在，移到前面
    auto it = msgIdMap.find(msgId);
    if (it != msgIdMap.end()) {
        msgIdCache.splice(msgIdCache.begin(), msgIdCache, it->second);
        return;
    }

    // 如果缓存满了，淘汰最旧的
    while (msgIdCache.size() >= MSG_ID_CACHE_SIZE) {
        String oldest = msgIdCache.back();
        msgIdCache.pop_back();
        msgIdMap.erase(oldest);
    }

    // 插入新消息 ID
    msgIdCache.push_front(msgId);
    msgIdMap[msgId] = msgIdCache.begin();
}

// ============================================================================
// 替换 JSON 字段值
// ============================================================================
void replaceJsonField(String &json, const String &field, const String &newValue) {
    // 查找 "field": 并替换值
    String search = String("\"") + field + "\":";
    int pos = json.indexOf(search);
    if (pos < 0) return;

    int valStart = pos + search.length();
    // 找到值结束位置（逗号或 }）
    int valEnd = json.indexOf(',', valStart);
    if (valEnd < 0) valEnd = json.indexOf('}', valStart);
    if (valEnd < 0) return;

    // 替换
    String before = json.substring(0, valStart);
    String after = json.substring(valEnd);
    json = before + newValue + after;
}

// ============================================================================
// 同步 Mesh 拓扑信息
// ============================================================================
void syncMeshTopology() {
    String payload = "{\"deviceCode\":\"" + String(deviceConfig.deviceCode) +
                     "\",\"isGateway\":" + (isGateway ? "true" : "false") +
                     ",\"peerCount\":" + String(blePeers.size()) + "}";
    String msg = createMeshMessage(MSG_MESH_SYNC, payload, MAX_MESH_TTL);
    meshBroadcast(msg);
    Serial.println("Mesh topology sync sent");
}

// ============================================================================
// MQTT 主题订阅管理（动态订阅 mesh 中所有设备）
// ============================================================================
void subscribeAllMeshTopics() {
    if (!isGateway || !mqttClient.connected()) return;

    Serial.println("Subscribing to all mesh device topics...");

    // 订阅自己的命令主题
    String myCmdTopic = String("devices/") + deviceConfig.deviceCode + "/command";
    mqttClient.subscribe(myCmdTopic.c_str(), COMMAND_QOS);
    Serial.printf("Subscribed: %s\n", myCmdTopic.c_str());

    // 订阅所有已知 mesh 设备的命令主题
    for (auto &peer : blePeers) {
        String cmdTopic = String("devices/") + peer.deviceCode + "/command";
        mqttClient.subscribe(cmdTopic.c_str(), COMMAND_QOS);
        Serial.printf("Subscribed: %s\n", cmdTopic.c_str());
    }

    // 订阅通配符主题（接收新设备指令）
    // 注意：有些 MQTT Broker 不支持 + 通配符订阅
    // 我们通过维护 peer 列表来管理
}

void unsubscribeAllMeshTopics() {
    Serial.println("Unsubscribing from all mesh device topics...");

    // 取消订阅所有已知设备的主题
    String myCmdTopic = String("devices/") + deviceConfig.deviceCode + "/command";
    mqttClient.unsubscribe(myCmdTopic.c_str());

    for (auto &peer : blePeers) {
        String cmdTopic = String("devices/") + peer.deviceCode + "/command";
        mqttClient.unsubscribe(cmdTopic.c_str());
    }
}

// ============================================================================
// MQTT 命令回调
// ============================================================================
void handleMqttCommand(char *topic, byte *payload, unsigned int length) {
    if (!isGateway) return;

    String body;
    for (unsigned int i = 0; i < length; i++) {
        body += static_cast<char>(payload[i]);
    }
    Serial.printf("MQTT command on %s: %s\n", topic, body.c_str());

    // 提取目标设备编号
    // topic 格式: devices/{deviceCode}/command
    String topicStr = String(topic);
    int devStart = topicStr.indexOf('/') + 1;
    int devEnd = topicStr.indexOf('/', devStart);
    if (devStart <= 0 || devEnd <= devStart) return;
    String targetDevice = topicStr.substring(devStart, devEnd);

    Serial.printf("Command target: %s\n", targetDevice.c_str());

    // 如果是给自己的命令
    if (targetDevice.equalsIgnoreCase(String(deviceConfig.deviceCode))) {
        // 直接处理
        String commandId = extractJsonField(body, "commandId");
        String command = extractJsonField(body, "command");
        command.toUpperCase();

        if (command == "UNLOCK") {
            executeUnlock();
            heartbeatPending = true;
        } else if (command == "REBOOT") {
            delay(50);
            ESP.restart();
        }
        return;
    }

    // 检查目标设备是否在 mesh 中
    bool inMesh = false;
    for (auto &peer : blePeers) {
        if (peer.deviceCode.equalsIgnoreCase(targetDevice)) {
            inMesh = true;
            break;
        }
    }

    if (inMesh) {
        // 通过 BLE mesh 转发命令
        // 构造 mesh 消息
        String meshPayload = body;
        String meshMsg = createMeshMessage(MSG_COMMAND, meshPayload, MAX_MESH_TTL);
        // 在 msgJson 中添加目标字段
        // 因为我们使用固定格式，需要将目标信息编码到 payload 中
        // 实际上目标设备信息已在 topic 中，mesh 消息需要包含目标
        // 重新构造带目标的 mesh 消息
        String directedMsg = String("{\"i\":\"") + generateMsgId() +
                             "\",\"t\":" + String((int)MSG_COMMAND) +
                             ",\"s\":\"" + deviceConfig.deviceCode +
                             "\",\"d\":\"" + targetDevice +
                             "\",\"h\":" + String(MAX_MESH_TTL) +
                             ",\"p\":" + body + "}";
        meshBroadcast(directedMsg);
        Serial.printf("Forwarded command to mesh device %s\n", targetDevice.c_str());
    } else {
        Serial.printf("Target device %s not in mesh\n", targetDevice.c_str());
    }
}

// ============================================================================
// MQTT 心跳发布（同时广播到 mesh）
// ============================================================================
void publishHeartbeat() {
    if (!isGateway || !mqttClient.connected()) return;

    getLockStatus();
    const char *lockStatus = lockOpen ? "UNLOCKED" : "LOCKED";
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"battery\":%u,\"lockStatus\":\"%s\"}",
             BATTERY_LEVEL, lockStatus);

    // 发往 MQTT
    String heartTopic = String("devices/") + deviceConfig.deviceCode + "/heartbeat";
    bool ok = mqttClient.publish(heartTopic.c_str(), payload, true);
    Serial.printf("Heartbeat sent to MQTT: %s (ok=%d)\n", payload, ok);

    // 广播到 mesh（让 mesh 中的其他设备知道本设备状态）
    String meshMsg = createMeshMessage(MSG_HEARTBEAT, String(payload), MAX_MESH_TTL);
    meshBroadcast(meshMsg);
}

// ============================================================================
// 开锁执行
// ============================================================================
bool executeUnlock() {
    bool opened = false;
    for (uint8_t attempt = 1; attempt <= UNLOCK_MAX_ATTEMPTS; attempt++) {
        Serial.printf("Unlock attempt %u\n", attempt);
        digitalWrite(LOCK_OUTPUT_PIN, HIGH);
        delay(UNLOCK_PULSE_MS);
        digitalWrite(LOCK_OUTPUT_PIN, LOW);
        delay(UNLOCK_VERIFY_DELAY_MS);
        getLockStatus();
        if (lockOpen) {
            Serial.println("Lock opened");
            opened = true;
            break;
        }
        if (attempt < UNLOCK_MAX_ATTEMPTS) {
            delay(UNLOCK_RETRY_GAP_MS);
        }
    }
    if (!opened) {
        Serial.println("Unlock failed after retries");
    }
    delay(UNLOCK_VERIFY_DELAY_MS);
    getLockStatus();
    return lockOpen;
}

// ============================================================================
// WiFi 连接管理
// ============================================================================
void ensureWifiConnected() {
    unsigned long now = millis();
    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        if (!wifiWasConnected) {
            if (!lockInterruptEnabled) {
                attachInterrupt(digitalPinToInterrupt(LOCK_SENSOR_PIN),
                                handleLockISR, CHANGE);
                lockInterruptEnabled = true;
                Serial.println("Lock interrupt enabled");
            }
            Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
            syncTime();
            heartbeatPending = true;
            // 成为网关，开始扫描 mesh 设备
            isGateway = true;
            lastBleScanStart = 0;
        }
        wifiWasConnected = true;
        wifiConnectInProgress = false;
        return;
    }

    if (wifiWasConnected) {
        if (lockInterruptEnabled) {
            detachInterrupt(digitalPinToInterrupt(LOCK_SENSOR_PIN));
            lockInterruptEnabled = false;
            lockStatusChanged = false;
        }
        if (mqttClient.connected()) {
            mqttClient.disconnect();
        }
        mqttNeedsReconnect = true;
        wifiWasConnected = false;
        isGateway = false;
    }

    if (wifiConnectInProgress) {
        bool stillConnecting = (status == WL_IDLE_STATUS || status == WL_DISCONNECTED);
        if (stillConnecting && (now - wifiConnectStartedAt < WIFI_CONNECT_TIMEOUT_MS)) {
            return;
        }
        wifiConnectInProgress = false;
        wifiNextRetryAt = now + WIFI_RETRY_INTERVAL_MS;
        Serial.printf("WiFi reconnect scheduled, status=%d\n", (int)status);
        return;
    }

    if (strlen(deviceConfig.ssid) == 0 || strlen(deviceConfig.password) == 0) {
        wifiNextRetryAt = now + WIFI_CONNECT_TIMEOUT_MS;
        return;
    }

    if (now < wifiNextRetryAt) return;

    WiFi.mode(WIFI_STA);
    WiFi.begin(deviceConfig.ssid, deviceConfig.password);
    wifiConnectInProgress = true;
    wifiConnectStartedAt = now;
    Serial.printf("WiFi connecting to %s...\n", deviceConfig.ssid);
}

// ============================================================================
// MQTT 连接管理
// ============================================================================
void ensureMqttConnected() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (mqttNeedsReconnect && mqttClient.connected()) {
        mqttClient.disconnect();
    }
    if (mqttClient.connected()) return;

    unsigned long now = millis();
    if (now - lastMqttAttemptAt < MQTT_RETRY_INTERVAL_MS) return;

    lastMqttAttemptAt = now;
    Serial.print("Connecting MQTT...");
    String clientId = String("mesh-") + deviceConfig.deviceCode;
    if (mqttClient.connect(clientId.c_str())) {
        Serial.println(" connected");
        mqttNeedsReconnect = false;
        // 订阅所有 mesh 设备主题
        subscribeAllMeshTopics();
    } else {
        Serial.printf(" failed, rc=%d\n", mqttClient.state());
    }
}

// ============================================================================
// 锁中断服务
// ============================================================================
void IRAM_ATTR handleLockISR() {
    lockStatusChanged = true;
}

void getLockStatus() {
    lockOpen = (digitalRead(LOCK_SENSOR_PIN) == HIGH);
}

// ============================================================================
// ============================================================================
// JSON 字段提取（轻量级，不依赖 ArduinoJson 库）
// ============================================================================
String extractJsonField(const String &body, const char *field) {
    String needle = String("\"") + field + String("\":");
    int start = body.indexOf(needle);
    if (start < 0) return "";

    start += needle.length();

    // 跳过空白
    while (start < (int)body.length() &&
           (body[start] == ' ' || body[start] == '\t')) start++;

    if (start >= (int)body.length()) return "";

    // 判断值类型
    if (body[start] == '"') {
        // 字符串
        start++;
        int end = body.indexOf('"', start);
        if (end < 0) return "";
        return body.substring(start, end);
    } else if (body[start] == '{' || body[start] == '[') {
        // 对象或数组 — 需要匹配括号
        char openBracket = body[start];
        char closeBracket = (openBracket == '{') ? '}' : ']';
        int depth = 1;
        int end = start + 1;
        while (end < (int)body.length() && depth > 0) {
            if (body[end] == openBracket) depth++;
            else if (body[end] == closeBracket) depth--;
            if (depth > 0) end++;
        }
        return body.substring(start, end + 1);
    } else if (body[start] == 't' || body[start] == 'f' || body[start] == 'n') {
        // true/false/null
        int end = start;
        while (end < (int)body.length() &&
               (isAlphaNumeric(body[end]) || body[end] == '+' || body[end] == '-' || body[end] == '.'))
            end++;
        return body.substring(start, end);
    } else {
        // 数字
        int end = start;
        while (end < (int)body.length() &&
               (isDigit(body[end]) || body[end] == '+' || body[end] == '-' || body[end] == '.' ||
                body[end] == 'e' || body[end] == 'E'))
            end++;
        return body.substring(start, end);
    }
}

// ============================================================================
// EEPROM 配置管理
// ============================================================================
void loadConfigFromEeprom() {
    uint32_t magic = 0;
    EEPROM.get(0, magic);
    if (magic != CONFIG_MAGIC) {
        Serial.println("EEPROM config missing, using defaults");
        saveConfigToEeprom();
        return;
    }
    DeviceConfig stored;
    EEPROM.get(sizeof(uint32_t), stored);
    stored.ssid[sizeof(stored.ssid) - 1] = '\0';
    stored.password[sizeof(stored.password) - 1] = '\0';
    stored.deviceCode[sizeof(stored.deviceCode) - 1] = '\0';
    deviceConfig = stored;
    Serial.println("Config loaded from EEPROM");
}

void saveConfigToEeprom() {
    EEPROM.put(0, CONFIG_MAGIC);
    EEPROM.put(sizeof(uint32_t), deviceConfig);
    EEPROM.commit();
    Serial.println("Config saved to EEPROM");
}

void applyDeviceCodeChange() {
    // 设备编号修改后，重启 BLE（重新广播新名称）
    Serial.println("Device code changed, restarting BLE...");
    if (pMeshServer) {
        pMeshServer->stopAdvertising();
    }
    BLEDevice::deinit(true);
    delay(200);
    meshInit();
}

// ============================================================================
// 串口命令处理
// ============================================================================
void handleSerialConfig() {
    if (!Serial.available()) return;

    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line.equalsIgnoreCase("HELP")) {
        Serial.println("Commands: HELP | SHOW | SCAN | PEERS | SET WIFI <ssid> <password> | SET ID <deviceCode>");
        return;
    }

    if (line.equalsIgnoreCase("SHOW")) {
        Serial.printf("SSID: %s\n", deviceConfig.ssid);
        Serial.printf("PASSWORD: %s\n", deviceConfig.password);
        Serial.printf("DEVICE: %s\n", deviceConfig.deviceCode);
        Serial.printf("GATEWAY: %s\n", isGateway ? "YES" : "NO");
        Serial.printf("PEERS: %u\n", blePeers.size());
        Serial.printf("MQTT: %s\n", mqttClient.connected() ? "CONNECTED" : "DISCONNECTED");
        return;
    }

    if (line.equalsIgnoreCase("PEERS")) {
        Serial.printf("Known peers (%u):\n", blePeers.size());
        for (auto &peer : blePeers) {
            Serial.printf("  %s | hops=%u | gw=%s | seen=%lu\n",
                          peer.deviceCode.c_str(),
                          peer.hopsAway,
                          peer.isGateway ? "Y" : "N",
                          peer.lastSeen);
        }
        return;
    }

    if (line.equalsIgnoreCase("SCAN")) {
        if (isGateway) {
            meshScanAndConnect();
        } else {
            Serial.println("Not a gateway (WiFi not connected)");
        }
        return;
    }

    if (line.startsWith("SET WIFI ")) {
        String rest = line.substring(9);
        int spacePos = rest.indexOf(' ');
        if (spacePos <= 0) {
            Serial.println("Invalid: SET WIFI <ssid> <password>");
            return;
        }
        String ssid = rest.substring(0, spacePos);
        String password = rest.substring(spacePos + 1);
        ssid.trim();
        password.trim();
        if (ssid.length() == 0 || password.length() == 0) {
            Serial.println("SSID or password empty");
            return;
        }
        strlcpy(deviceConfig.ssid, ssid.c_str(), sizeof(deviceConfig.ssid));
        strlcpy(deviceConfig.password, password.c_str(), sizeof(deviceConfig.password));
        saveConfigToEeprom();
        Serial.println("WiFi credentials updated, reconnecting...");
        // 强制 WiFi 重连
        WiFi.disconnect();
        wifiConnectInProgress = false;
        wifiWasConnected = false;
        wifiNextRetryAt = 0;
        return;
    }

    if (line.startsWith("SET ID ")) {
        String code = line.substring(7);
        code.trim();
        if (code.length() == 0) {
            Serial.println("Device ID empty");
            return;
        }
        strlcpy(deviceConfig.deviceCode, code.c_str(), sizeof(deviceConfig.deviceCode));
        saveConfigToEeprom();
        applyDeviceCodeChange();
        Serial.println("Device ID updated, BLE restarted");
        return;
    }

    if (line.equalsIgnoreCase("UNLOCK")) {
        executeUnlock();
        heartbeatPending = true;
        return;
    }

    Serial.println("Unknown command. Type HELP");
}

// ============================================================================
// OLED 显示
// ============================================================================
void displayShowInit() {
    if (!displayInitDone) return;
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);

    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.setCursor(16, 24);
    u8g2.print("物联网陪护床");
    u8g2.setCursor(16, 40);
    u8g2.print("Mesh 组网启动...");
    u8g2.drawFrame(14, 48, 100, 10);
    u8g2.sendBuffer();
    delay(400);

    const char *steps[] = {"BLE 初始化...", "扫描邻居...", "Mesh 就绪"};
    const uint8_t progresses[] = {30, 65, 100};
    for (uint8_t i = 0; i < 3; i++) {
        u8g2.setCursor(16, 40);
        u8g2.print("            ");
        u8g2.setCursor(16, 40);
        u8g2.print(steps[i]);
        u8g2.drawBox(15, 49, progresses[i], 8);
        u8g2.sendBuffer();
        delay(400);
    }
    u8g2.setCursor(16, 40);
    u8g2.print("启动完成  ");
    u8g2.sendBuffer();
    delay(500);
}

void displayUpdate() {
    if (!displayInitDone) return;

    unsigned long now = millis();
    if (now - lastDisplayUpdate < DISPLAY_REFRESH_INTERVAL_MS) return;
    lastDisplayUpdate = now;

    if (!ntpSynced && WiFi.status() == WL_CONNECTED && now - lastNtpAttempt > NTP_RETRY_INTERVAL_MS) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            ntpSynced = true;
            Serial.println("NTP synced");
        } else {
            lastNtpAttempt = now;
        }
    }

    // Toast 覆盖
    if (now < toastEndTime) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
        u8g2.drawBox(0, 22, 128, 24);
        u8g2.setDrawColor(0);
        u8g2.setCursor(10, 38);
        u8g2.print(toastMessage);
        u8g2.setDrawColor(1);
        u8g2.sendBuffer();
        return;
    }

    // 正常显示
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);

    // 顶部：设备编号 + 时间
    u8g2.drawStr(2, 11, deviceConfig.deviceCode);
    if (ntpSynced) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[10];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
            int tw = u8g2.getStrWidth(timeStr);
            u8g2.drawStr(128 - tw - 2, 11, timeStr);
        }
    }
    u8g2.drawHLine(2, 16, 124);

    // WiFi + Mesh 状态
    int y = 28;
    if (WiFi.status() == WL_CONNECTED) {
        u8g2.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print("WiFi 已连接");
        // 显示 mesh peer 数量
        char peerStr[16];
        snprintf(peerStr, sizeof(peerStr), "Mesh: %u", blePeers.size());
        int pw = u8g2.getStrWidth(peerStr);
        u8g2.drawStr(128 - pw - 2, y, peerStr);
    } else {
        u8g2.drawCircle(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print("WiFi 已断开 (BLE广播)");
    }

    // MQTT 状态
    y = 44;
    if (mqttClient.connected()) {
        u8g2.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print("网关模式 已连接");
        char gwStr[16];
        snprintf(gwStr, sizeof(gwStr), "%d台", blePeers.size());
        u8g2.drawStr(100, y, gwStr);
    } else {
        u8g2.drawCircle(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print(isGateway ? "服务器 连接中" : "终端模式 (等待网关)");
    }

    // 锁状态
    y = 58;
    if (lockOpen) {
        u8g2.drawCircle(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print("门锁 已打开");
    } else {
        u8g2.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
        u8g2.setCursor(16, y);
        u8g2.print("门锁 已关闭");
    }

    u8g2.sendBuffer();
}

void displayShowToast(const char *msg) {
    if (!displayInitDone) return;
    strncpy(toastMessage, msg, sizeof(toastMessage) - 1);
    toastMessage[sizeof(toastMessage) - 1] = '\0';
    toastEndTime = millis() + TOAST_DURATION_MS;
}

void syncTime() {
    if (WiFi.status() != WL_CONNECTED) return;
    Serial.println("Starting NTP sync...");
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org", "time.nist.gov");
    ntpSynced = false;
    lastNtpAttempt = millis();
}
