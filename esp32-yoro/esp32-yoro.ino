#include "BLEDevice.h"
#include <ESP8266WebServer.h>
#include <HTTPClient.h>
#include <WiFiClient.h> 
#include "FS.h"
#include "SPIFFS.h"

static BLEUUID        deviceUUID("0000aa80-0000-1000-8000-00805f9b34fb");
static BLEUUID        serviceUUID("f000aa80-0451-4000-b000-000000000000");
static BLEUUID           dataUUID("f000aa81-0451-4000-b000-000000000000");
static BLEUUID  configurationUUID("f000aa82-0451-4000-b000-000000000000");
static BLEUUID         periodUUID("f000aa83-0451-4000-b000-000000000000");
static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* pConfigurationCharacteristic;
static BLERemoteCharacteristic* pPeriodCharacteristic;
int rssi = -10000;

const int MODE_PIN = 0;                 // GPIO0 モード切り替えピン
const char* settings = "/wifi_settings.txt";  // Wi-Fi設定保存ファイル
const char* AP_SSID = "AccessPoint";   // サーバモードでのパスワード
const char* AP_PASS = "password1234";  // サーバモードでのパスワード
WebServer *pServer;
WiFiClient client;
char HOST[256];
int PORT = 443;
int MYID = 1;
int DISTID = 1;

int lastValue = 0;

// WiFi設定
void handleRootGet() {
  Serial.println("handleRootGet");
  File f = SPIFFS.open(settings, "r");
  String ssid = f.readStringUntil('\n');
  String pass = f.readStringUntil('\n');
  String host = f.readStringUntil('\n');
  String myid = f.readStringUntil('\n');
  String distid = f.readStringUntil('\n');
  f.close();

  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "<form method='post'>";
  html += "  <input type='text' value='" + ssid + "' name='ssid' placeholder='ssid'><br>";
  html += "  <input type='text' value='" + pass + "' name='pass' placeholder='password'><br>";
  html += "  <input type='text' value='" + host + "' name='host' placeholder='host'><br>";
  html += "  <input type='text' value='" + myid + "' name='myid' placeholder='my id'><br>";
  html += "  <input type='text' value='" + distid + "' name='distid' placeholder='dist id'><br>";
  html += "  <input type='submit'><br>";
  html += "</form>";
  pServer->send(200, "text/html", html);
}

// POST結果の表示
void handleRootPost() {
  Serial.println("handleRootPost");
  String ssid = pServer->arg("ssid");
  String pass = pServer->arg("pass");
  String host = pServer->arg("host");
  String myid = pServer->arg("myid");
  String distid = pServer->arg("distid");
  File f = SPIFFS.open(settings, "w");
  f.println(ssid);
  f.println(pass);
  f.println(host);
  f.println(myid);
  f.println(distid);
  f.close();
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += ssid + "<br>";
  html += pass + "<br>";
  html += host + "<br>";
  html += myid + "<br>";
  html += distid + "<br>";
  pServer->send(200, "text/html", html);  
}

// 初期化(サーバモード)
void setup_server() {
  WebServer server(80);
  pServer = &server;
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("PASS: ");
  Serial.println(AP_PASS);
  WiFi.softAP(AP_SSID, AP_PASS);
  pServer->on("/", HTTP_GET, handleRootGet);
  pServer->on("/", HTTP_POST, handleRootPost);
  pServer->begin();
  Serial.println("HTTP server started.");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  while (true) {
    Serial.println("while");
    pServer->handleClient();
    delay(100);
  }
}

// 初期化(クライアントモード)
void setup_client() {
//  File f = SPIFFS.open(settings, "r");
//  String ssid = f.readStringUntil('\n');
//  String pass = f.readStringUntil('\n');
//  String host = f.readStringUntil('\n');
//  String myid = f.readStringUntil('\n');
//  String distid = f.readStringUntil('\n');
  String ssid = "jq145602";
  String pass = "jq-145602";
  String host = "yoro.southeastasia.cloudapp.azure.com";
  String myid = "10";
  String distid = "10";
//  f.close();
  ssid.trim();
  pass.trim();
  host.trim();
  myid.trim();
  distid.trim();
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);
  Serial.println("HOST: " + host);
  Serial.println("MY ID: " + myid);
  Serial.println("DIST ID: " + distid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  strcpy(HOST, host.c_str());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// サーバから受信
void receive() {
  if (!client.connected()) {
    Serial.println("> not connected");
    if (!client.connect(HOST, PORT)) {
      Serial.print("> connection failed:");
    } else {
      Serial.print("> connection success:");
    }
    Serial.print(HOST);
    Serial.print(":");
    Serial.println(PORT);
  }
  if (client.available() >= 3) {
    int c1 = client.read();
    int c2 = client.read();
    int c3 = client.read();
    Serial.print("> ");
    Serial.print(c1);
    Serial.print(" ");
    Serial.print(c2);
    Serial.print(" ");
    Serial.println(c3);
//    if (c1 == 0xff && c2 == id && c3 <= 180) {
//      dist_angle = c3;
//    }
  }
}

// サーバ送信
void send(unsigned char rid, unsigned char val) {
  unsigned char msg[] = {255, rid, val};
  client.write(msg, 3);
  client.flush();
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
      uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Notify callback for characteristic ");
  if (pBLERemoteCharacteristic == nullptr) {
    Serial.println("**pBLERemoteCharacteristic is null");
  }
  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.print(" of data length ");
  Serial.println(length);
}

bool connectToServer(BLEAddress pAddress) {
  Serial.print("Forming a connection to ");
  Serial.println(pAddress.toString().c_str());
  BLEClient*  pClient  = BLEDevice::createClient();
  Serial.println(" - Created client");
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    return false;
  }
  Serial.println(" - Found our service");
  Serial.println(pRemoteService->toString().c_str());
  pRemoteCharacteristic = pRemoteService->getCharacteristic(dataUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(dataUUID.toString().c_str());
    return false;
  }
  Serial.println(" - Found our characteristic");
  pConfigurationCharacteristic = pRemoteService->getCharacteristic(configurationUUID);
  if (pConfigurationCharacteristic == nullptr) {
    Serial.print("Failed to find our Configuration: ");
    Serial.println(configurationUUID.toString().c_str());
    return false;
  }
  uint8_t accelerometerEnabled[2] = {B01111111, B00000000};
  pConfigurationCharacteristic->writeValue( accelerometerEnabled, 2);
  uint16_t valueInt16 = pConfigurationCharacteristic->readUInt16();
  Serial.print(" - Enabled accelerometer: ");
  Serial.println(valueInt16, BIN);
//  uint8_t accelerometerPeriod = 10;
  uint8_t accelerometerPeriod = 100;
  pPeriodCharacteristic = pRemoteService->getCharacteristic(periodUUID);
  if (pPeriodCharacteristic == nullptr) {
    Serial.print("Failed to find our period: ");
    Serial.println(periodUUID.toString().c_str());
    return false;
  }
  pPeriodCharacteristic->writeValue( accelerometerPeriod, 1);
  Serial.print(" - Accelerometer priod: ");
  uint8_t valueInt8 = pPeriodCharacteristic->readUInt8();
  Serial.print(valueInt8);
  Serial.println("*10ms");
  pRemoteCharacteristic->registerForNotify(notifyCallback);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(deviceUUID)) {
      int thisRssi = advertisedDevice.getRSSI();
      BLEAddress *thisAddress;
      thisAddress = new BLEAddress(advertisedDevice.getAddress());
      Serial.print("Found our device!  address: ");
      Serial.print(thisAddress->toString().c_str());
      //advertisedDevice.getScan()->stop();
      Serial.print("  RSSI: ");
      Serial.println(thisRssi);
      if ( thisRssi > rssi ) {
        rssi = thisRssi;
        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      }
      doConnect = true;
    }
  }
};

void scanBle() {
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  if (pBLEScan == nullptr) {
    Serial.println("**pBLEScan is null");
  }
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}
/*
short toShort(std::string s, int n) {
  short ret = 0;
  short a = s[n];
  short b = s[n + 1];
  ret = b * 256 + a;
  return ret/45;
}*/

int printAccelerometer(std::string s) {
//  Serial.print(" Z: ");
//  Serial.print(toShort(s, 10));
  short data = 0;
  short a = s[10];
  short b = s[11];
  data = b * 256 + a;
  int ret = (int)data / 45;
  Serial.print(ret);
  return ret;
}

void loop() {
  if (doConnect == true) {
    if (pServerAddress == nullptr) {
      Serial.println("**pServerAddress is null");
    }
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }
  if (connected) {
    Serial.print("Connected...");
    if (pRemoteCharacteristic == nullptr) {
      Serial.println("**pRemoteCharacteristic is null");
    }
    std::string strings = pRemoteCharacteristic->readValue();
    Serial.print(" * ");
    int val = printAccelerometer(strings);
//      int val = 99;
    if (abs(lastValue - val) > 2) {
      Serial.print(" send");
      send(DISTID, val);
      lastValue = val;
    }
    Serial.println("");
  }
  receive();
  delay(100);
//  delay(1000);
}
// 初期化
void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(500);
  btStop();
//  SPIFFS.begin(true);     // ファイルシステム初期化
  pinMode(MODE_PIN, INPUT);
  delay(1000);            // 1秒以内にMODEを切り替える
//  if (digitalRead(MODE_PIN) == 0) {
//    setup_server();       // サーバモード
//  } else {
    setup_client();       // クライアントモード初期化
//  }
  scanBle();
}


