/**
 * ============================================================================
 * CareBed 物联网陪护床 — ESP32 蓝牙 Mesh 组网固件 v3.0
 * ============================================================================
 *
 * 多跳转发的核心改进：
 *   1. 邻居感知的路由表：每个设备维护 {deviceCode, hopsAway, lastSeen} 路由表
 *   2. 排除来源转发：meshSend() 不重发给来源 peer，避免一跳回流
 *   3. TTL 递减：每转发一次 TTL-1，到达 0 停止，防止无限循环
 *   4. msgId LRU 去重：256 条目缓存，已处理消息不再重复执行/转发
 *   5. 路由宣告：设备定期广播设备邻居信息，构建多跳路由表
 *   6. 通配广播 + 定向写入：Server Notify + Client Write（排除来源）
 *
 * 多跳转发示例 (A 网关 -- B 中继 -- C 终端):
 *   服务器发指令给 C:
 *     MQTT → A → BLE(MSG_COMMAND, TTL=7) → B
 *     B 收到 → msgId 不重复 → 不是目标 → TTL=6
 *     B 转发(排除来源 A) → C
 *     C 收到 → 是目标 → 执行开锁 → TTL=5 → 广播结果
 *     C → B(MSG_RSP, TTL=5) → B → A(TTL=4) → MQTT → 服务器
 *
 * 硬件接线:
 *   继电器/锁控输出: GPIO 4  (高电平脉冲 500ms)
 *   锁状态检测:      GPIO 48 (上拉输入, CHANGE 中断)
 *   OLED I2C: SDA=8, SCL=9
 *   串口: 115200 baud
 *
 * 依赖库: BLEDevice, PubSubClient, U8g2lib
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
#include <vector>

// ============================================================================
// 配置常量
// ============================================================================
#define MESH_SERVICE_UUID "cafebeed-0001-4d45-5348-000000000000"
#define MESH_DATA_CHAR_UUID "cafebeed-0002-4d45-5348-000000000000"
#define BLE_DEVICE_PREFIX "CB-M-"
#define MESH_NETWORK_ID 0xCB01
#define MAX_BLE_CONNECTIONS 5
#define BLE_SCAN_INTERVAL_MS 400
#define BLE_SCAN_WINDOW_MS 200
#define BLE_SCAN_DURATION_MS 3000
#define BLE_SCAN_COOLDOWN_MS 20000

#define MAX_MESH_TTL 7
#define MSG_ID_CACHE_SIZE 256
#define MESH_SYNC_INTERVAL_MS 30000
#define MESH_HB_INTERVAL_MS 10000
#define PEER_TIMEOUT_MS 180000
#define ROUTE_ENTRY_TIMEOUT_MS 300000

const char *MQTT_HOST = "msas.absozero.cn";
const int MQTT_PORT = 1883;
const uint8_t COMMAND_QOS = 1;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 3000;

const uint8_t LOCK_RELAY_PIN = 4;
const uint8_t LOCK_SENSE_PIN = 48;
const uint8_t UNLOCK_PULSE_MS = 500;
const uint8_t UNLOCK_MAX_ATTEMPTS = 3;
const uint8_t BATTERY_LEVEL = 100;

const unsigned long DISPLAY_REFRESH_MS = 300;
const unsigned long TOAST_DURATION_MS = 2500;
const unsigned long NTP_RETRY_MS = 30000;

struct DeviceConfig
{
    char ssid[32];
    char password[64];
    char deviceCode[32];
};
DeviceConfig devCfg = {"Pura70", "12345678", "BED-0001"};
const uint32_t CFG_MAGIC = 0xCAFEB33D;
const size_t EEPROM_SZ = 256;

// ============================================================================
// 全局
// ============================================================================
BLEServer *srv = nullptr;
BLECharacteristic *chr = nullptr;
bool scanning = false;
unsigned long lastScanAt = 0;

struct BlePeer
{
    String dc;
    bool gw;
    uint8_t hops;
    unsigned long seen;
    BLEClient *cli;
    BLERemoteCharacteristic *rch;
    bool con;
    BlePeer() : hops(1), gw(false), seen(0), cli(nullptr), rch(nullptr), con(false) {}
};
std::list<BlePeer> peers;

// ---- 路由表 ----
struct RouteEntry
{
    String peerDc;
    uint8_t hops;
    unsigned long seen;
};
std::map<String, RouteEntry> routeTable; // key=目标 deviceCode, value=下一跳

WiFiClient wc;
PubSubClient mq(wc);
bool mqNeedsReconnect = false;
unsigned long lastMqttTry = 0;
bool wifiBusy = false, wifiWas = false;
unsigned long wifiStartAt = 0, wifiNextAt = 0;
volatile bool lockOpen = false, lockChanged = false;
bool intEn = false;
std::list<String> idCache;
typedef std::list<String>::iterator IdIt;
std::map<String, IdIt> idMap;
bool isGateway = false;
unsigned long lastSyncAt = 0, lastHbAt = 0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R2, U8X8_PIN_NONE);
bool oledOk = false, ntpOk = false;
unsigned long lastNtpAt = 0, lastOledAt = 0;
char toast[64];
unsigned long toastEnd = 0;

enum MsgT : uint8_t
{
    MT_HB = 1,
    MT_CMD = 2,
    MT_RSP = 3,
    MT_SYNC = 5,
    MT_JOIN = 6,
    MT_LEAVE = 7,
    MT_RT_ANNOUNCE = 8
};

void IRAM_ATTR onLockISR();
void getLock();
bool unlock();
void doWiFi();
void doMQTT();
void onMQTT(char *, byte *, unsigned);
void serialCmd();
void meshInit();
void meshScan();
void meshConn(BlePeer &);
void meshDisconn(BlePeer &);
void meshSend(const String &, BlePeer *exclude = nullptr);
void meshSendTo(BlePeer &, const String &);
void meshRecv(const String &);
String mkMsg(MsgT, const String &, uint8_t);
String genId();
bool seen(const String &);
void mark(const String &);
String jget(const String &, const char *);
void jset(String &, const String &, const String &);
void syncTopo();
void announceRoutes();
void subAll();
void unsubAll();
void ldCfg();
void svCfg();
void updCode();
void oledInit();
void oledTick();
void oledToast(const char *);
void syncNTP();

// ============================================================================
// BLE 回调
// ============================================================================
class SrvCB : public BLEServerCallbacks
{
    void onConnect(BLEServer *, esp_ble_gatts_cb_param_t *p) override { Serial.printf("BLE client conn=%d\n", p->connect.conn_id); }
    void onDisconnect(BLEServer *) override
    {
        if (srv)
            srv->startAdvertising();
    }
};
class DatCB : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *c) override
    {
        String v = c->getValue();
        if (v.length())
        {
            Serial.printf("BLE<=S %s\n", v.c_str());
            meshRecv(v);
        }
    }
};
class CliCB : public BLEClientCallbacks
{
    void onConnect(BLEClient *) override {}
    void onDisconnect(BLEClient *c) override
    {
        for (auto &p : peers)
        {
            if (p.cli && p.cli->getPeerAddress().equals(c->getPeerAddress()))
            {
                p.con = false;
                p.seen = 0;
                break;
            }
        }
    }
};
class AdvCB : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice d) override
    {
        String n = d.getName().c_str();
        if (!n.startsWith(BLE_DEVICE_PREFIX))
            return;
        int p1 = n.indexOf('-', 5), p2 = n.indexOf('-', p1 + 1);
        String c = (p2 > 0) ? n.substring(p2 + 1) : "?";
        if (c.equalsIgnoreCase(String(devCfg.deviceCode)))
            return;
        for (auto &p : peers)
        {
            if (p.dc == c)
            {
                p.seen = millis();
                return;
            }
        }
        if (peers.size() >= MAX_BLE_CONNECTIONS)
            return;
        BlePeer bp;
        bp.dc = c;
        bp.seen = millis();
        peers.push_back(bp);
    }
};

// ============================================================================
// setup
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("\n===== CareBed BLE Mesh v3.0 (Multi-hop) =====");
    EEPROM.begin(EEPROM_SZ);
    ldCfg();
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_STA);
    pinMode(LOCK_RELAY_PIN, OUTPUT);
    digitalWrite(LOCK_RELAY_PIN, LOW);
    pinMode(LOCK_SENSE_PIN, INPUT_PULLUP);
    mq.setServer(MQTT_HOST, MQTT_PORT);
    mq.setCallback(onMQTT);
    meshInit();
    getLock();
    Wire.begin(8, 9);
    oled.begin();
    oled.enableUTF8Print();
    oledOk = true;
    oledInit();
    Serial.printf("Ready: %s\n", devCfg.deviceCode);
    oledToast("Mesh v3 就绪");
    lastScanAt = 0;
}

// ============================================================================
// loop
// ============================================================================
void loop()
{
    unsigned long n = millis();
    if (lockChanged)
    {
        lockChanged = false;
        getLock();
    }
    serialCmd();
    doWiFi();

    bool gw = (WiFi.status() == WL_CONNECTED);
    if (gw != isGateway)
    {
        isGateway = gw;
        if (isGateway)
        {
            subAll();
            oledToast("成为网关");
        }
        else
        {
            unsubAll();
            if (mq.connected())
                mq.disconnect();
            oledToast("纯BLE模式");
        }
    }
    if (isGateway)
    {
        doMQTT();
        mq.loop();
    }

    // BLE 扫描
    if (!scanning && (n - lastScanAt > BLE_SCAN_COOLDOWN_MS))
        meshScan();
    if (scanning && (n - lastScanAt > BLE_SCAN_DURATION_MS))
    {
        BLEDevice::getScan()->stop();
        scanning = false;
        for (auto &p : peers)
        {
            if (!p.con && !p.cli)
                meshConn(p);
        }
    }

    // 重连
    for (auto it = peers.begin(); it != peers.end();)
    {
        if (n - it->seen > PEER_TIMEOUT_MS)
        {
            meshDisconn(*it);
            it = peers.erase(it);
        }
        else
            ++it;
    }
    for (auto &p : peers)
    {
        if (!p.con && !p.cli)
            meshConn(p);
    }

    // 心跳 + 路由宣告
    if (n - lastHbAt >= MESH_HB_INTERVAL_MS)
    {
        lastHbAt = n;
        getLock();
        String pl = String("{\"b\":") + BATTERY_LEVEL + ",\"l\":\"" + (lockOpen ? "U" : "L") + "\"}";
        meshSend(mkMsg(MT_HB, pl, MAX_MESH_TTL));
    }
    if (n - lastSyncAt >= MESH_SYNC_INTERVAL_MS)
    {
        lastSyncAt = n;
        syncTopo();
        announceRoutes();
    }

    // 路由表清理
    for (auto it = routeTable.begin(); it != routeTable.end();)
    {
        if (n - it->second.seen > ROUTE_ENTRY_TIMEOUT_MS)
            it = routeTable.erase(it);
        else
            ++it;
    }

    oledTick();
}

// ============================================================================
// BLE
// ============================================================================
void meshInit()
{
    String bn = String(BLE_DEVICE_PREFIX) + String(MESH_NETWORK_ID, HEX) + "-" + String(devCfg.deviceCode);
    BLEDevice::init(bn.c_str());
    BLEDevice::setPower(ESP_PWR_LVL_P7);
    srv = BLEDevice::createServer();
    srv->setCallbacks(new SrvCB());
    auto *svc = srv->createService(MESH_SERVICE_UUID);
    chr = svc->createCharacteristic(MESH_DATA_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    chr->addDescriptor(new BLE2902());
    chr->setCallbacks(new DatCB());
    svc->start();
    auto *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(MESH_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMinInterval(200);
    adv->setMaxInterval(400);
    srv->startAdvertising();
}

void meshScan()
{
    if (peers.size() >= MAX_BLE_CONNECTIONS)
        return;
    auto *s = BLEDevice::getScan();
    s->setAdvertisedDeviceCallbacks(new AdvCB());
    s->setInterval(BLE_SCAN_INTERVAL_MS);
    s->setWindow(BLE_SCAN_WINDOW_MS);
    s->setActiveScan(true);
    s->start(BLE_SCAN_DURATION_MS / 1000, false);
    scanning = true;
    lastScanAt = millis();
}

void meshConn(BlePeer &p)
{
    if (p.con)
        return;
    auto *c = BLEDevice::createClient();
    c->setClientCallbacks(new CliCB());
    auto *s = BLEDevice::getScan();
    s->setActiveScan(true);
    s->setInterval(200);
    s->setWindow(100);
    int nd = s->start(1, false);
    for (int i = 0; i < nd; i++)
    {
        auto d = s->getResults().getDevice(i);
        String nn = d.getName().c_str();
        if (nn.startsWith(BLE_DEVICE_PREFIX) && nn.indexOf(p.dc.c_str()) > 0)
        {
            if (c->connect(d.getAddress()))
            {
                auto *rs = c->getService(MESH_SERVICE_UUID);
                if (!rs)
                {
                    c->disconnect();
                    break;
                }
                auto *rc = rs->getCharacteristic(MESH_DATA_CHAR_UUID);
                if (!rc)
                {
                    c->disconnect();
                    break;
                }
                rc->registerForNotify([](BLERemoteCharacteristic *, uint8_t *d, size_t l, bool)
                                      {
                    if(l==0) return; String m((char*)d,l); meshRecv(m); });
                p.cli = c;
                p.rch = rc;
                p.con = true;
                p.seen = millis();
                String j = mkMsg(MT_JOIN, "{\"d\":\"" + String(devCfg.deviceCode) + "\",\"g\":" + (isGateway ? "1" : "0") + "}", 2);
                rc->writeValue(j.c_str(), j.length(), true);
            }
            break;
        }
    }
    s->clearResults();
    if (!p.con)
        BLEDevice::deleteClient(c);
}

void meshDisconn(BlePeer &p)
{
    if (p.cli)
    {
        if (p.cli->isConnected())
            p.cli->disconnect();
        BLEDevice::deleteClient(p.cli);
    }
    p.cli = nullptr;
    p.rch = nullptr;
    p.con = false;
    for (auto it = routeTable.begin(); it != routeTable.end();)
    {
        if (it->second.peerDc == p.dc)
            it = routeTable.erase(it);
        else
            ++it;
    }
}

// ============================================================================
// 发送 — 排除来源的多跳广播
// ============================================================================
void meshSend(const String &j, BlePeer *exclude)
{
    if (chr)
    {
        chr->setValue(j);
        chr->notify();
    }
    for (auto &p : peers)
    {
        if (!p.con || !p.rch)
            continue;
        if (exclude && p.dc == exclude->dc)
            continue;
        meshSendTo(p, j);
    }
}
void meshSendTo(BlePeer &p, const String &j)
{
    if (!p.con || !p.rch)
        return;
    if (!p.rch->writeValue(j.c_str(), j.length(), true))
        meshDisconn(const_cast<BlePeer &>(p));
}

// ============================================================================
// meshRecv — 多跳 + 防泛洪 + 路由学习
// ============================================================================
void meshRecv(const String &j)
{
    if (j.length() < 10)
        return;
    String id = jget(j, "i");
    if (!id.length())
        return;
    String ts = jget(j, "t"), ss = jget(j, "s"), pl = jget(j, "p");
    uint8_t ttl = (uint8_t)jget(j, "h").toInt(), mt = (uint8_t)ts.toInt();
    if (ss.equalsIgnoreCase(String(devCfg.deviceCode)))
        return;
    if (seen(id))
        return;
    mark(id);

    bool fwd = true, isTarget = false;
    bool srcIsPeer = false;
    for (auto &p : peers)
    {
        if (p.dc == ss)
        {
            p.seen = millis();
            srcIsPeer = true;
            break;
        }
    }

    switch (mt)
    {
    case MT_HB:
        if (isGateway && mq.connected())
            mq.publish((String("devices/") + ss + "/heartbeat").c_str(), pl.c_str(), true);
        break;
    case MT_CMD:
    {
        String dst = jget(j, "d");
        if (dst.equalsIgnoreCase(String(devCfg.deviceCode)) || dst == "ALL")
        {
            isTarget = true;
            String cmd = jget(pl, "command");
            cmd.toUpperCase();
            if (cmd == "UNLOCK")
            {
                bool ok = unlock();
                meshSend(mkMsg(MT_RSP, "{\"cmd\":\"UNLOCK\",\"r\":\"" + String(ok ? "OK" : "FAIL") + "\"}", ttl > 1 ? ttl - 1 : 1));
            }
            else if (cmd == "REBOOT")
            {
                meshSend(mkMsg(MT_RSP, "{\"cmd\":\"REBOOT\",\"r\":\"ACK\"}", 1));
                fwd = false;
                delay(100);
                ESP.restart();
            }
        }
        break;
    }
    case MT_RSP:
        if (isGateway && mq.connected())
            mq.publish((String("devices/") + ss + "/response").c_str(), pl.c_str(), false);
        break;
    case MT_SYNC:
    {
        String dc = jget(pl, "d"), gw = jget(pl, "g");
        if (dc.length() && !dc.equalsIgnoreCase(String(devCfg.deviceCode)))
        {
            bool found = false;
            for (auto &p : peers)
            {
                if (p.dc == dc)
                {
                    p.seen = millis();
                    p.gw = (gw == "1");
                    found = true;
                    break;
                }
            }
            if (!found && peers.size() < MAX_BLE_CONNECTIONS)
            {
                // 非直连设备加入路由表
                RouteEntry r;
                r.peerDc = ss;
                r.hops = 2;
                r.seen = millis();
                auto it = routeTable.find(dc);
                if (it == routeTable.end() || it->second.hops > r.hops)
                    routeTable[dc] = r;
            }
        }
        break;
    }
    case MT_JOIN:
        if (isGateway)
            subAll();
        break;
    case MT_LEAVE:
        for (auto it = peers.begin(); it != peers.end();)
        {
            if (it->dc == ss)
            {
                meshDisconn(*it);
                it = peers.erase(it);
            }
            else
                ++it;
        }
        if (isGateway)
            subAll();
        break;
    default:
        break;
    }

    // ---- 多跳转发(排除来源) ----
    if (fwd && ttl > 1)
    {
        BlePeer *from = nullptr;
        if (srcIsPeer)
        {
            for (auto &p : peers)
            {
                if (p.con && p.dc == ss)
                {
                    from = &p;
                    break;
                }
            }
        }
        else
        {
            auto rt = routeTable.find(ss);
            if (rt != routeTable.end())
            {
                for (auto &p : peers)
                {
                    if (p.con && p.dc == rt->second.peerDc)
                    {
                        from = &p;
                        break;
                    }
                }
            }
        }
        String fj = j;
        jset(fj, "h", String(ttl - 1));
        meshSend(fj, from);
    }
}

// ============================================================================
// 路由宣告
// ============================================================================
void announceRoutes()
{
    String pl = "{\"d\":\"" + String(devCfg.deviceCode) + "\",\"g\":" + (isGateway ? "1" : "0") +
                ",\"p\":" + String(peers.size()) + "}";
    meshSend(mkMsg(MT_RT_ANNOUNCE, pl, 2));
}

// ============================================================================
// 工具
// ============================================================================
String mkMsg(MsgT t, const String &pl, uint8_t ttl)
{
    return String("{\"i\":\"") + genId() + "\",\"t\":" + String((int)t) +
           ",\"s\":\"" + devCfg.deviceCode + "\",\"h\":" + String(ttl) +
           ",\"p\":" + pl + "}";
}
String genId()
{
    static uint32_t seq = 0;
    return String(devCfg.deviceCode) + "-" + String(millis(), HEX) + "-" + String(++seq);
}
bool seen(const String &id) { return idMap.find(id) != idMap.end(); }
void mark(const String &id)
{
    auto it = idMap.find(id);
    if (it != idMap.end())
    {
        idCache.splice(idCache.begin(), idCache, it->second);
        return;
    }
    while (idCache.size() >= MSG_ID_CACHE_SIZE)
    {
        String o = idCache.back();
        idCache.pop_back();
        idMap.erase(o);
    }
    idCache.push_front(id);
    idMap[id] = idCache.begin();
}
String jget(const String &b, const char *f)
{
    String n = String("\"") + f + "\":";
    int s = b.indexOf(n);
    if (s < 0)
        return "";
    s += n.length();
    while (s < (int)b.length() && (b[s] == ' ' || b[s] == '\t'))
        s++;
    if (s >= (int)b.length())
        return "";
    if (b[s] == '"')
    {
        s++;
        int e = b.indexOf('"', s);
        return (e < 0) ? "" : b.substring(s, e);
    }
    if (b[s] == '{' || b[s] == '[')
    {
        char ob = b[s], cb = (ob == '{') ? '}' : ']';
        int d = 1, e = s + 1;
        while (e < (int)b.length() && d > 0)
        {
            if (b[e] == ob)
                d++;
            else if (b[e] == cb)
                d--;
            if (d > 0)
                e++;
        }
        return b.substring(s, e + 1);
    }
    int e = s;
    while (e < (int)b.length() && (isAlphaNumeric(b[e]) || b[e] == '+' || b[e] == '-' || b[e] == '.'))
        e++;
    return b.substring(s, e);
}
void jset(String &j, const String &f, const String &v)
{
    String s = String("\"") + f + "\":";
    int p = j.indexOf(s);
    if (p < 0)
        return;
    int vs = p + s.length(), ve = j.indexOf(',', vs);
    if (ve < 0)
        ve = j.indexOf('}', vs);
    if (ve < 0)
        return;
    j = j.substring(0, vs) + v + j.substring(ve);
}
void syncTopo()
{
    String pl = "{\"d\":\"" + String(devCfg.deviceCode) + "\",\"g\":" + (isGateway ? "1" : "0") + "}";
    meshSend(mkMsg(MT_SYNC, pl, MAX_MESH_TTL));
}

void subAll()
{
    if (!isGateway || !mq.connected())
        return;
    mq.subscribe((String("devices/") + devCfg.deviceCode + "/command").c_str(), COMMAND_QOS);
    for (auto &p : peers)
        mq.subscribe((String("devices/") + p.dc + "/command").c_str(), COMMAND_QOS);
}
void unsubAll()
{
    mq.unsubscribe((String("devices/") + devCfg.deviceCode + "/command").c_str());
    for (auto &p : peers)
        mq.unsubscribe((String("devices/") + p.dc + "/command").c_str());
}

void onMQTT(char *t, byte *pl, unsigned len)
{
    if (!isGateway)
        return;
    String b;
    for (unsigned i = 0; i < len; i++)
        b += (char)pl[i];
    String ts = String(t);
    int ds = ts.indexOf('/') + 1, de = ts.indexOf('/', ds);
    if (ds <= 0 || de <= ds)
        return;
    String tgt = ts.substring(ds, de);
    if (tgt.equalsIgnoreCase(String(devCfg.deviceCode)))
    {
        String cmd = jget(b, "command");
        cmd.toUpperCase();
        if (cmd == "UNLOCK")
            unlock();
        else if (cmd == "REBOOT")
        {
            delay(50);
            ESP.restart();
        }
        return;
    }
    meshSend(String("{\"i\":\"") + genId() + "\",\"t\":" + String((int)MT_CMD) +
             ",\"s\":\"" + devCfg.deviceCode + "\",\"d\":\"" + tgt +
             "\",\"h\":" + String(MAX_MESH_TTL) + ",\"p\":" + b + "}");
}

bool unlock()
{
    for (uint8_t i = 1; i <= UNLOCK_MAX_ATTEMPTS; i++)
    {
        digitalWrite(LOCK_RELAY_PIN, HIGH);
        delay(UNLOCK_PULSE_MS);
        digitalWrite(LOCK_RELAY_PIN, LOW);
        delay(300);
        getLock();
        if (lockOpen)
            break;
        if (i < UNLOCK_MAX_ATTEMPTS)
            delay(300);
    }
    return lockOpen;
}

void doWiFi()
{
    unsigned long n = millis();
    auto st = WiFi.status();
    if (st == WL_CONNECTED)
    {
        if (!wifiWas)
        {
            if (!intEn)
            {
                attachInterrupt(digitalPinToInterrupt(LOCK_SENSE_PIN), onLockISR, CHANGE);
                intEn = true;
            }
            syncNTP();
            wifiWas = true;
            wifiBusy = false;
        }
        return;
    }
    if (wifiWas)
    {
        wifiWas = false;
        if (intEn)
        {
            detachInterrupt(digitalPinToInterrupt(LOCK_SENSE_PIN));
            intEn = false;
        }
    }
    if (wifiBusy)
    {
        bool b = (st == WL_IDLE_STATUS || st == WL_DISCONNECTED);
        if (b && (n - wifiStartAt < WIFI_CONNECT_TIMEOUT_MS))
            return;
        wifiBusy = false;
        wifiNextAt = n + WIFI_RETRY_INTERVAL_MS;
        return;
    }
    if (!strlen(devCfg.ssid))
    {
        wifiNextAt = n + 10000;
        return;
    }
    if (n < wifiNextAt)
        return;
    WiFi.begin(devCfg.ssid, devCfg.password);
    wifiBusy = true;
    wifiStartAt = n;
}
void doMQTT()
{
    if (WiFi.status() != WL_CONNECTED || mq.connected())
        return;
    unsigned long n = millis();
    if (n - lastMqttTry < 2000)
        return;
    lastMqttTry = n;
    if (mq.connect((String("mesh-") + devCfg.deviceCode).c_str()))
        subAll();
}
void IRAM_ATTR onLockISR() { lockChanged = true; }
void getLock() { lockOpen = (digitalRead(LOCK_SENSE_PIN) == HIGH); }
void ldCfg()
{
    uint32_t m;
    EEPROM.get(0, m);
    if (m != CFG_MAGIC)
    {
        svCfg();
        return;
    }
    DeviceConfig s;
    EEPROM.get(sizeof(uint32_t), s);
    s.ssid[sizeof(s.ssid) - 1] = 0;
    s.password[sizeof(s.password) - 1] = 0;
    s.deviceCode[sizeof(s.deviceCode) - 1] = 0;
    devCfg = s;
}
void svCfg()
{
    EEPROM.put(0, CFG_MAGIC);
    EEPROM.put(sizeof(uint32_t), devCfg);
    EEPROM.commit();
}
void updCode()
{
    if (srv)
        srv->stopAdvertising();
    BLEDevice::deinit(true);
    delay(200);
    meshInit();
}

void serialCmd()
{
    if (!Serial.available())
        return;
    String l = Serial.readStringUntil('\n');
    l.trim();
    if (!l.length())
        return;
    if (l.equalsIgnoreCase("HELP"))
    {
        Serial.println("HELP|SHOW|PEERS|ROUTE|SCAN|UNLOCK|REBOOT");
        return;
    }
    if (l.equalsIgnoreCase("SHOW"))
    {
        Serial.printf("%s GW=%s PEERS=%u ROUTES=%u MQ=%s\n", devCfg.deviceCode, isGateway ? "Y" : "N", peers.size(), routeTable.size(), mq.connected() ? "UP" : "DOWN");
        return;
    }
    if (l.equalsIgnoreCase("PEERS"))
    {
        for (auto &p : peers)
            Serial.printf("  %s gw=%s con=%s\n", p.dc.c_str(), p.gw ? "Y" : "N", p.con ? "Y" : "N");
        return;
    }
    if (l.equalsIgnoreCase("ROUTE"))
    {
        Serial.printf("Route table (%u):\n", routeTable.size());
        for (auto &r : routeTable)
            Serial.printf("  %s -> via %s (hops=%u)\n", r.first.c_str(), r.second.peerDc.c_str(), r.second.hops);
        return;
    }
    if (l.equalsIgnoreCase("SCAN"))
    {
        meshScan();
        return;
    }
    if (l.equalsIgnoreCase("UNLOCK"))
    {
        unlock();
        return;
    }
    if (l.equalsIgnoreCase("REBOOT"))
    {
        delay(50);
        ESP.restart();
    }
    if (l.startsWith("SET WIFI "))
    {
        String r = l.substring(9);
        int sp = r.indexOf(' ');
        if (sp <= 0)
            return;
        strlcpy(devCfg.ssid, r.substring(0, sp).c_str(), sizeof(devCfg.ssid));
        strlcpy(devCfg.password, r.substring(sp + 1).c_str(), sizeof(devCfg.password));
        svCfg();
        WiFi.disconnect();
        wifiBusy = false;
        wifiWas = false;
        wifiNextAt = 0;
        return;
    }
    if (l.startsWith("SET ID "))
    {
        String c = l.substring(7);
        c.trim();
        if (!c.length())
            return;
        strlcpy(devCfg.deviceCode, c.c_str(), sizeof(devCfg.deviceCode));
        svCfg();
        updCode();
        return;
    }
    Serial.println("? HELP");
}

void oledInit()
{
    oled.setFont(u8g2_font_wqy12_t_gb2312b);
    oled.clearBuffer();
    oled.drawFrame(0, 0, 128, 64);
    oled.setCursor(16, 24);
    oled.print("物联网陪护床");
    oled.setCursor(16, 40);
    oled.print("Mesh v3 多跳");
    oled.drawFrame(14, 48, 100, 10);
    oled.sendBuffer();
    delay(500);
    for (int i = 0; i < 3; i++)
    {
        oled.setCursor(16, 40);
        oled.print("        ");
        oled.setCursor(16, 40);
        oled.print(i == 0 ? "BLE..." : i == 1 ? "扫描中..."
                                              : "就绪");
        oled.drawBox(15, 49, i == 0 ? 30 : i == 1 ? 65
                                                  : 100,
                     8);
        oled.sendBuffer();
        delay(400);
    }
    oled.setCursor(16, 40);
    oled.print("启动完成");
    oled.sendBuffer();
    delay(500);
}

void oledTick()
{
    if (!oledOk)
        return;
    unsigned long n = millis();
    if (n - lastOledAt < DISPLAY_REFRESH_MS)
        return;
    lastOledAt = n;
    if (!ntpOk && WiFi.status() == WL_CONNECTED && n - lastNtpAt > NTP_RETRY_MS)
    {
        struct tm ti;
        if (getLocalTime(&ti))
            ntpOk = true;
        else
            lastNtpAt = n;
    }
    if (n < toastEnd)
    {
        oled.clearBuffer();
        oled.setFont(u8g2_font_wqy12_t_gb2312b);
        oled.drawBox(0, 22, 128, 24);
        oled.setDrawColor(0);
        oled.setCursor(10, 38);
        oled.print(toast);
        oled.setDrawColor(1);
        oled.sendBuffer();
        return;
    }
    oled.clearBuffer();
    oled.setFont(u8g2_font_wqy12_t_gb2312b);
    oled.setCursor(2, 11);
    oled.print(devCfg.deviceCode);
    if (ntpOk)
    {
        struct tm ti;
        if (getLocalTime(&ti))
        {
            char ts[10];
            snprintf(ts, sizeof(ts), "%02d:%02d", ti.tm_hour, ti.tm_min);
            oled.drawStr(128 - oled.getStrWidth(ts) - 2, 11, ts);
        }
    }
    oled.drawHLine(2, 16, 124);
    int y = 28;
    oled.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
    oled.setCursor(16, y);
    oled.print("Peer ");
    char ms[16];
    snprintf(ms, sizeof(ms), "%u", peers.size());
    oled.drawStr(70, y, ms);
    oled.setCursor(90, y);
    oled.print("Rt");
    char rs[8];
    snprintf(rs, sizeof(rs), "%u", routeTable.size());
    oled.drawStr(126 - oled.getStrWidth(rs), y, rs);
    y = 44;
    if (isGateway)
    {
        oled.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
        oled.setCursor(16, y);
        oled.print("网关");
        if (!mq.connected())
            oled.print("(MQTT...)");
    }
    else
    {
        oled.drawCircle(6, y - 2, 3, U8G2_DRAW_ALL);
        oled.setCursor(16, y);
        oled.print("终端(纯BLE)");
    }
    y = 58;
    if (lockOpen)
    {
        oled.drawCircle(6, y - 2, 3, U8G2_DRAW_ALL);
        oled.setCursor(16, y);
        oled.print("开");
    }
    else
    {
        oled.drawDisc(6, y - 2, 3, U8G2_DRAW_ALL);
        oled.setCursor(16, y);
        oled.print("关");
    }
    oled.sendBuffer();
}
void oledToast(const char *msg)
{
    if (!oledOk)
        return;
    strncpy(toast, msg, sizeof(toast) - 1);
    toastEnd = millis() + TOAST_DURATION_MS;
}
void syncNTP()
{
    if (WiFi.status() != WL_CONNECTED)
        return;
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
    ntpOk = false;
    lastNtpAt = millis();
}
