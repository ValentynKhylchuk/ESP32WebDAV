// Using the WebDAV server with Rigidbot 3D printer.
// Printer controller is a variation of Rambo running Marlin firmware

#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPWebDAV.h>
/*
#ifdef DBG_PRINTLN
	#undef DBG_INIT
	#undef DBG_PRINT
	#undef DBG_PRINTLN
	// #define DBG_INIT(...)		{ Serial1.begin(__VA_ARGS__); }
	// #define DBG_PRINT(...) 		{ Serial1.print(__VA_ARGS__); }
	// #define DBG_PRINTLN(...) 	{ Serial1.println(__VA_ARGS__); }
	#define DBG_INIT(...)		{}
	#define DBG_PRINT(...) 		{}
	#define DBG_PRINTLN(...) 	{}
#endif
*/
#define DBG_INIT(...)		{ Serial.begin(__VA_ARGS__); }
#define DBG_PRINT(...) 		{ Serial.print(__VA_ARGS__); }
#define DBG_PRINTLN(...) 	{ Serial.println(__VA_ARGS__); }


// LED is connected to GPIO2 on this board
#define INIT_LED			{pinMode(2, OUTPUT);}
#define LED_ON				{digitalWrite(2, LOW);}
#define LED_OFF				{digitalWrite(2, HIGH);}

#define HOSTNAME		"Rigidbot"
#define SERVER_PORT		80
#define SPI_BLOCKOUT_PERIOD	20000UL

#define CS_SENSE	13
#define SD_CS		4

const char *ssid = 		"Buba";
const char *password = 	"88887777";

ESPWebDAV dav;
String statusMessage;
bool initFailed = false;

volatile long spiBlockoutTime = 0;
bool weHaveBus = false;
bool clientWiting = false;


// ------------------------
void setup() {
// ------------------------
	// ----- GPIO -------
	// Detect when other master uses SPI bus
	// pinMode(CS_SENSE, INPUT);
	
    /*
	attachInterrupt(CS_SENSE, []() {
		if(!weHaveBus)
			spiBlockoutTime = millis() + SPI_BLOCKOUT_PERIOD;
	}, FALLING);
	
    */

	DBG_INIT(115200);
	Serial.begin(115200);

	DBG_PRINTLN("");
	DBG_PRINTLN(" ==============");
	DBG_PRINTLN(" ==  Hello!  ==");
	DBG_PRINTLN(" ==============");
	DBG_PRINTLN("");
	INIT_LED;
	blink();
	
	// wait for other master to assert SPI bus first
	delay(SPI_BLOCKOUT_PERIOD);

	// ----- WIFI -------
	// Set hostname first
	WiFi.hostname(HOSTNAME);
	// Reduce startup surge current
	WiFi.setAutoConnect(false);
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	// Wait for connection
	while(WiFi.status() != WL_CONNECTED) {
		blink();
		DBG_PRINT("WIFI: Connecting...");
	}

	DBG_PRINTLN("");
	DBG_PRINT("Connected to "); DBG_PRINTLN(ssid);
	DBG_PRINT("IP address: "); DBG_PRINTLN(WiFi.localIP());
	DBG_PRINT("RSSI: "); DBG_PRINTLN(WiFi.RSSI());
	//DBG_PRINT("Mode: "); DBG_PRINTLN(WiFi.getMode());


	// ----- SD Card and Server -------
	// Check to see if other master is using the SPI bus
	//while(millis() < spiBlockoutTime)
	//	blink();
	
	takeBusControl();
	
	// start the SD DAV server
	if(!dav.init(SD_CS, SERVER_PORT))		{
		statusMessage = "Failed to initialize SD Card";
		DBG_PRINT("ERROR: "); DBG_PRINTLN(statusMessage);
		// indicate error on LED
		errorBlink();
		initFailed = true;
	}
	else
		blink();

	//relenquishBusControl();
	DBG_PRINTLN("WebDAV server started");
}



// ------------------------
void loop() {
// ------------------------
	if(millis() < spiBlockoutTime)
		blink();

	/*
	if( clientWiting != dav.isClientWaiting())
	{
		clientWiting = dav.isClientWaiting();
		
		time_t now = time(&now);
		Serial.println(ctime(&now));
		if(clientWiting)
		{
			Serial.println("isClientWaiting: true");
		}
		else
		{
			Serial.println("isClientWaiting: false");
		}
		Serial.println("...");
	}
	*/
	
	// do it only if there is a need to read FS
	if(dav.isClientWaiting())	{
		if(initFailed)
			return dav.rejectClient(statusMessage);
		
		// has other master been using the bus in last few seconds
		if(millis() < spiBlockoutTime)
			return dav.rejectClient("Marlin is reading from SD card");
		
		// a client is waiting and FS is ready and other SPI master is not using the bus
		// takeBusControl();
		dav.handleClient();
		// relenquishBusControl();
	}
}



// ------------------------
void takeBusControl()	{
// ------------------------
	weHaveBus = true;
	//LED_ON;

	// Pullup pin for MMC mode.
    //digitalWrite(27, HIGH);

	pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);

	//pinMode(MISO, SPECIAL);	
	//pinMode(MOSI, SPECIAL);	
	//pinMode(SCLK, SPECIAL);	
	//pinMode(SD_CS, OUTPUT);
}



// ------------------------
void relenquishBusControl()	{
// ------------------------
	//pinMode(MISO, INPUT);	
	//pinMode(MOSI, INPUT);	
	//pinMode(SCLK, INPUT);	
	//pinMode(SD_CS, INPUT);



	LED_OFF;
	weHaveBus = false;
}




// ------------------------
void blink()	{
// ------------------------
	LED_ON; 
	delay(100); 
	LED_OFF; 
	delay(400);
}



// ------------------------
void errorBlink()	{
// ------------------------
	for(int i = 0; i < 100; i++)	{
		LED_ON; 
		delay(50); 
		LED_OFF; 
		delay(50);
	}
}



