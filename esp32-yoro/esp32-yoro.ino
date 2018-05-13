#include "BLEDevice.h"
//#include "BLEScan.h"
#include <ESP8266WebServer.h>
#include <HTTPClient.h>
#include "FS.h"
#include "SPIFFS.h"

// The remote device we wish to connect to.
static BLEUUID        deviceUUID("0000aa80-0000-1000-8000-00805f9b34fb");
// The remote service we wish to connect to.
//static BLEUUID   notificationUUID("00002902-0000-1000-8000-00805f9b34fb");
static BLEUUID        serviceUUID("f000aa80-0451-4000-b000-000000000000");
static BLEUUID           dataUUID("f000aa81-0451-4000-b000-000000000000");
static BLEUUID  configurationUUID("f000aa82-0451-4000-b000-000000000000");
static BLEUUID         periodUUID("f000aa83-0451-4000-b000-000000000000");
static BLEUUID batteryServiceUUID("0000180f-0000-1000-8000-00805f9b34fb");
static BLEUUID    batteryDataUUID("00002a19-0000-1000-8000-00805f9b34fb");

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* pConfigurationCharacteristic;
static BLERemoteCharacteristic* pPeriodCharacteristic;
static BLERemoteCharacteristic* pBatteryCharacteristic;
int rssi = -10000;

// モード切り替えピン
const int MODE_PIN = 0; // GPIO0
// Wi-Fi設定保存ファイル
const char* settings = "/wifi_settings.txt";
// サーバモードでのパスワード
const String pass = "password1234";

//WebServer server(80);
WebServer *pServer;


void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- frite failed");
    }
}

/**
 * WiFi設定
 */
void handleRootGet() {
  Serial.println("handleRootGet");
  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += "<form method='post'>";
  html += "  <input type='text' name='ssid' placeholder='ssid'><br>";
  html += "  <input type='text' name='pass' placeholder='pass'><br>";
  html += "  <input type='submit'><br>";
  html += "</form>";
  pServer->send(200, "text/html", html);
}

void handleRootPost() {
  Serial.println("handleRootPost");
  String ssid = pServer->arg("ssid");
  String pass = pServer->arg("pass");

  File f = SPIFFS.open(settings, "w");
  f.println(ssid);
  f.println(pass);
  f.close();

  String html = "";
  html += "<h1>WiFi Settings</h1>";
  html += ssid + "<br>";
  html += pass + "<br>";
  pServer->send(200, "text/html", html);
}

/**
 * 初期化(クライアントモード)
 */
void setup_client() {
//  File f0 = SPIFFS.open(settings, "w");
//  f0.println("aaaa");
//  f0.println("bbbb");
//  f0.close();

  File f = SPIFFS.open(settings, "r");
  String ssid = f.readStringUntil('\n');
  String pass = f.readStringUntil('\n');
  f.close();

  ssid.trim();
  pass.trim();

  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);

  WiFi.begin(ssid.c_str(), pass.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * 初期化(サーバモード)
 */
void setup_server() {
  WebServer server(80);
  pServer = &server;
  byte mac[6];
  WiFi.macAddress(mac);
  String ssid = "AccessPoint";
//  for (int i = 0; i < 6; i++) {
//    ssid += String(mac[i], HEX);
//  }
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);

  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid.c_str(), pass.c_str());

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

/**
 * 初期化
 */
void _setup() {
  Serial.begin(115200);
  Serial.println();

  // 1秒以内にMODEを切り替える
  //  0 : Server
  //  1 : Client
  delay(1000);

  // ファイルシステム初期化
  SPIFFS.begin(true);

  pinMode(MODE_PIN, INPUT);
  if (digitalRead(MODE_PIN) == 0) {
    // サーバモード初期化
    setup_server();
  } else {
    // クライアントモード初期化
    setup_client();
  }
}

void _loop() {
  HTTPClient http;
  http.begin("http://canon.jp/");
  int httpCode = http.GET();

  Serial.printf("Response: %d", httpCode);
  Serial.println();
  if (httpCode == HTTP_CODE_OK) {
    String body = http.getString();
    Serial.print("Response Body: ");
    Serial.println(body);
  }

  delay(5000);
}





static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
}

bool connectToServer(BLEAddress pAddress) {
    Serial.print("Forming a connection to ");
    Serial.println(pAddress.toString().c_str());

    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our service");
    Serial.println(pRemoteService->toString().c_str());

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(dataUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(dataUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found our characteristic");

    //  Enable Accelerometer
    pConfigurationCharacteristic = pRemoteService->getCharacteristic(configurationUUID);
    if (pConfigurationCharacteristic == nullptr) {
      Serial.print("Failed to find our Configuration: ");
      Serial.println(configurationUUID.toString().c_str());
      return false;
    }
    // Gyroscope must be enabled to get Accelerometer due to unknown reason..
    uint8_t accelerometerEnabled[2] = {B01111111, B00000000};
    pConfigurationCharacteristic->writeValue( accelerometerEnabled, 2);
    // Read the value of the characteristic.
    uint16_t valueInt16 = pConfigurationCharacteristic->readUInt16();
    Serial.print(" - Enabled accelerometer: ");
    Serial.println(valueInt16, BIN);

    // Set update period.
    //Resolution 10 ms. Range 100 ms (0x0A) to 2.55 sec (0xFF). Default 1 second (0x64).
    uint8_t accelerometerPeriod = 10;
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

    BLERemoteService* pBatteryService = pClient->getService(batteryServiceUUID);
    if (pBatteryService == nullptr) {
      Serial.print("Failed to find battery service UUID: ");
      Serial.println(batteryServiceUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found battery service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pBatteryCharacteristic = pBatteryService->getCharacteristic(batteryDataUUID);
    if (pBatteryCharacteristic == nullptr) {
      Serial.print("Failed to find battery level UUID: ");
      Serial.println(batteryDataUUID.toString().c_str());
      return false;
    }
    Serial.println(" - Found battery level");

    pRemoteCharacteristic->registerForNotify(notifyCallback);
}
/**
 * Scan for BLE servers and find the strongest RSSI one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(deviceUUID)) {

      //
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

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

//short toShort(String s, int n) {
short toShort(std::string s, int n) {
  short ret = 0;
//  short a = s.charAt(n);
//  short b = s.charAt(n + 1);
  short a = s[n];
  short b = s[n + 1];
  //Serial.println(a, HEX);
  //Serial.println(b, HEX);
  ret = b * 256 + a;
  return ret/45;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

//  String s1 = "1234567890";
//  int n = toShort(s1, 0);
//  Serial.println(n);
//  n = toShort(s1, 2);
//  Serial.println(n);
  
  scanBle();
} // End of setup.

void scanBle() {
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
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
//    std::string value = pRemoteCharacteristic->readValue();
//    String strings = value.c_str();
//    int len = strings.length();
//    for ( int i = 0; i < len; i++) {
//      short a = strings.charAt(i);
//      Serial.print(a, HEX);
//      Serial.print(" ");
//    }
    std::string strings = pRemoteCharacteristic->readValue();
    Serial.print(strings.length());
    Serial.print(" ");
    Serial.print(strings.size());

//    printGyroscope(strings);
    printAccelerometer(strings);
//    printMagnetometer(strings);
    //std::string battery = pBatteryCharacteristic->readValue();
    //printBatteryLevel(battery.c_str());
    Serial.println("");
  }

  delay(100); // Delay a second between loops.
} // End of loop

void printBatteryLevel(String strings) {
    Serial.print(" Battery: ");
    Serial.print(strings.charAt( 0), DEC);
}

//void printGyroscope(String strings) {
void printGyroscope(std::string strings) {
  Serial.print("  Gyroscope X: ");
  Serial.print(toShort(strings, 0));
  Serial.print(" Y: ");
  Serial.print(toShort(strings, 2));
  Serial.print(" Z: ");
  Serial.print(toShort(strings, 4));
}

//void printAccelerometer(String strings) {
void printAccelerometer(std::string strings) {
  Serial.print("  Accelerometer X: ");
  Serial.print(toShort(strings, 6));
  Serial.print(" Y: ");
  Serial.print(toShort(strings, 8));
  Serial.print(" Z: ");
  Serial.print(toShort(strings, 10));
}
//void printMagnetometer(String strings) {
void printMagnetometer(std::string strings) {
  Serial.print("  Magnetometer X: ");
  Serial.print(toShort(strings, 12));
  Serial.print(" Y: ");
  Serial.print(toShort(strings, 14));
  Serial.print(" Z: ");
  Serial.print(toShort(strings, 16));
}
//void _printAccelerometer(String strings) {
//  Serial.print("  Accelerometer Z: ");
//  long n = toShort(strings, 10) /(256*256/360);
//  Serial.print(n);
//}

