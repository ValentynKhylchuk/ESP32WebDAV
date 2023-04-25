# 1 "C:\\Users\\Valentyn\\AppData\\Local\\Temp\\tmpw_8mji7l"
#include <Arduino.h>
# 1 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"



#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPWebDAV.h>
# 20 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
#define DBG_INIT(...) { Serial.begin(__VA_ARGS__); }
#define DBG_PRINT(...) { Serial.print(__VA_ARGS__); }
#define DBG_PRINTLN(...) { Serial.println(__VA_ARGS__); }



#define INIT_LED {pinMode(2, OUTPUT);}
#define LED_ON {digitalWrite(2, LOW);}
#define LED_OFF {digitalWrite(2, HIGH);}

#define HOSTNAME "Rigidbot"
#define SERVER_PORT 80
#define SPI_BLOCKOUT_PERIOD 20000UL

#define CS_SENSE 13
#define SD_CS 4

const char *ssid = "Buba";
const char *password = "88887777";

ESPWebDAV dav;
String statusMessage;
bool initFailed = false;

volatile long spiBlockoutTime = 0;
bool weHaveBus = false;
bool clientWiting = false;
void setup();
void loop();
void takeBusControl();
void relenquishBusControl();
void blink();
void errorBlink();
#line 50 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
void setup() {
# 64 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
 DBG_INIT(115200);
 Serial.begin(115200);

 DBG_PRINTLN("");
 DBG_PRINTLN(" ==============");
 DBG_PRINTLN(" ==  Hello!  ==");
 DBG_PRINTLN(" ==============");
 DBG_PRINTLN(CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);

 INIT_LED;
 blink();


 delay(SPI_BLOCKOUT_PERIOD);



 WiFi.hostname(HOSTNAME);

 WiFi.setAutoConnect(false);
 WiFi.mode(WIFI_STA);
 WiFi.begin(ssid, password);


 while(WiFi.status() != WL_CONNECTED) {
  blink();
  DBG_PRINT("WIFI: Connecting...");
 }

 DBG_PRINTLN("");
 DBG_PRINT("Connected to "); DBG_PRINTLN(ssid);
 DBG_PRINT("IP address: "); DBG_PRINTLN(WiFi.localIP());
 DBG_PRINT("RSSI: "); DBG_PRINTLN(WiFi.RSSI());
# 105 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
 takeBusControl();


 if(!dav.init(SD_CS, SERVER_PORT)) {
  statusMessage = "Failed to initialize SD Card";
  DBG_PRINT("ERROR: "); DBG_PRINTLN(statusMessage);

  errorBlink();
  initFailed = true;
 }
 else
  blink();


 DBG_PRINTLN("WebDAV server started");
}




void loop() {

 if(millis() < spiBlockoutTime)
  blink();
# 150 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
 if(dav.isClientWaiting()) {
  if(initFailed)
   return dav.rejectClient(statusMessage);


  if(millis() < spiBlockoutTime)
   return dav.rejectClient("Marlin is reading from SD card");



  dav.handleClient();

 }
}




void takeBusControl() {

 weHaveBus = true;





 pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);





}




void relenquishBusControl() {
# 197 "M:/Devices/WIFI-SD/ESP32WebDAV-saved/src/Printer_3D.ino"
 LED_OFF;
 weHaveBus = false;
}





void blink() {

 LED_ON;
 delay(100);
 LED_OFF;
 delay(400);
}




void errorBlink() {

 for(int i = 0; i < 100; i++) {
  LED_ON;
  delay(50);
  LED_OFF;
  delay(50);
 }
}