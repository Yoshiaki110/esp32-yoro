# esp32-yoro

$ pwd
/Users/itouyoshiaki/Documents/Arduino/hardware/espressif/esp32
$ git diff
diff --git a/boards.txt b/boards.txt
index 6a78f4e..5b3aa7e 100644
--- a/boards.txt
+++ b/boards.txt
@@ -10,7 +10,7 @@ menu.DebugLevel=Core Debug Level
 esp32.name=ESP32 Dev Module
 
 esp32.upload.tool=esptool
-esp32.upload.maximum_size=1310720
+esp32.upload.maximum_size=3407872
 esp32.upload.maximum_data_size=294912
 esp32.upload.wait_for_upload_port=true
 
diff --git a/cores/esp32/main.cpp b/cores/esp32/main.cpp
index 0cba93e..bf5e023 100644
--- a/cores/esp32/main.cpp
+++ b/cores/esp32/main.cpp
@@ -22,7 +22,7 @@ void loopTask(void *pvParameters)
 extern "C" void app_main()
 {
     initArduino();
-    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
+    xTaskCreatePinnedToCore(loopTask, "loopTask", 8192*2, NULL, 1, NULL, ARDUINO_RUNNING_CORE);
 }
 
 #endif
diff --git a/tools/partitions/default.csv b/tools/partitions/default.csv
index 3e4235d..9f27fb9 100644
--- a/tools/partitions/default.csv
+++ b/tools/partitions/default.csv
@@ -1,7 +1,6 @@
-# Name,   Type, SubType, Offset,  Size, Flags
-nvs,      data, nvs,     0x9000,  0x5000,
-otadata,  data, ota,     0xe000,  0x2000,
-app0,     app,  ota_0,   0x10000, 0x140000,
-app1,     app,  ota_1,   0x150000,0x140000,
-eeprom,   data, 0x99,    0x290000,0x1000,
-spiffs,   data, spiffs,  0x291000,0x16F000,
+# Name,   Type, SubType, Offset,   Size, Flags
+nvs,      data, nvs,     0x9000,   0x5000,
+otadata,  data, ota,     0xe000,   0x2000,
+app0,     app,  ota_0,   0x10000,  0x340000,
+eeprom,   data, 0x99,    0x350000, 0x1000,
+spiffs,   data, spiffs,  0x351000, 0xAF000,
diff --git a/tools/sdk/sdkconfig b/tools/sdk/sdkconfig
index b2ca2f6..73be49d 100644
--- a/tools/sdk/sdkconfig
+++ b/tools/sdk/sdkconfig
@@ -261,7 +261,7 @@ CONFIG_ESP_ERR_TO_NAME_LOOKUP=y
 #
 CONFIG_SW_COEXIST_ENABLE=y
 CONFIG_SW_COEXIST_PREFERENCE_WIFI=
-CONFIG_SW_COEXIST_PREFERENCE_BT=
+CONFIG_SW_COEXIST_PREFERENCE_BT=y
 CONFIG_SW_COEXIST_PREFERENCE_BALANCE=y
 CONFIG_SW_COEXIST_PREFERENCE_VALUE=2
 CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=10
$ 

