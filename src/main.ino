#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPWebDAV.h>

// Some loggings methods.
#define DBG_INIT(...)        { Serial.begin(__VA_ARGS__); }
#define DBG_PRINT(...)       { Serial.print(__VA_ARGS__); }
#define DBG_PRINTLN(...)     { Serial.println(__VA_ARGS__); }

// WiFi settings
#define HOSTNAME "3D Printer"
#define SERVER_PORT 80

// How long should we wait since last interaction between SD and printer to consider it safe.
#define SPI_BLOCKOUT_PERIOD 10000UL
// How long should we wait since last interaction with SD to consider it safe to disconnect it.
#define SD_RELEASE_DELAY 4000

// Pins
#define CS_SENSE 13

// Enter your WiFi access data here:
const char * SSID = "Buba";
const char * PASSWORD = "88887777";

/// @brief WebDav server.
ESPWebDAV dav;

/// @brief safety delay.
volatile long spiBlockoutTime = 0;
/// @brief safety delay.
volatile long sdReleaseTime = 0;
/// @brief flag that indicates that we have controll over SD.
bool weHaveBus = false;

void setup()
{
    // Set GPIO pins to netral position to not intefire with 3D Printer.
    pinMode(2, INPUT);
    pinMode(4, INPUT);
    pinMode(13, INPUT);
    pinMode(14, INPUT);
    pinMode(15, INPUT);

	// Some logs.
    DBG_INIT(115200);
    DBG_PRINTLN("");
    DBG_PRINTLN("==============");
    DBG_PRINTLN("==  Hello!  ==");
    DBG_PRINTLN("==============");
	DBG_PRINT("= CPU: ");DBG_PRINT(CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);DBG_PRINTLN(" MHz.");

    // Detect when other master uses SPI bus.
    pinMode(CS_SENSE, INPUT);

    // Turn on power for SD.
    powerOn();
    // Connect SD to printer.
    dataOn();
    // Listening printer accessing SD card.
    listenPrinter(true);

    // Set hostname first
    WiFi.hostname(HOSTNAME);
    // Reduce startup surge current
    WiFi.setAutoConnect(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);

    // Wait for connection.
    while (WiFi.status() != WL_CONNECTED)
    {
        DBG_PRINTLN("WIFI: Connecting...");
        delay(200);
    }

    // Log WiFi info.
    DBG_PRINTLN("");
    DBG_PRINT("Connected to ");DBG_PRINTLN(SSID);
    DBG_PRINT("IP address: ");DBG_PRINTLN(WiFi.localIP());

    // Start the WebDAV server.
    dav.init(SERVER_PORT);
    DBG_PRINTLN("WebDAV server started");
}


void loop()
{
    // Check if web client wants something.
    if (dav.isClientWaiting())
    {
        // Check if printer was not using SD card for some time.
        if (millis() < spiBlockoutTime)
        {
			return dav.rejectClient("Marlin is reading from SD card...");
		}

		// Take controll on SD card, if needed.
        if (!weHaveBus && !takeBusControl())
        {
            relenquishBusControl();
            return dav.rejectClient("Can not take SD.");
        }

		// Do process client request.
        dav.handleClient();
        sdReleaseTime = millis() + SD_RELEASE_DELAY;
    }

    // Check and release SD, if no actions for some time.
    if (weHaveBus && (sdReleaseTime < millis()))
    {
        DBG_PRINTLN("Time to release SD.");
        relenquishBusControl();
    }

	// Just delay in loop.
    delay(100);
}

/// @brief Takes control on SD card.
/// @return True if successful.
bool takeBusControl()
{
    DBG_PRINTLN("Takeing controll on SD...");
    weHaveBus = true;
    dataOff();
    powerReset(100);

    // Pullup pin for MMC mode.
    pinMode(27, OUTPUT);
    digitalWrite(27, HIGH);

    if (dav.takeSD())
    {
        DBG_PRINTLN("Taking controll on SD. Done.");
        weHaveBus = true;
        listenPrinter(false);
        return true;
    }

    DBG_PRINTLN("Can not take controll on SD.");
    weHaveBus = false;
    return false;
}

/// @brief Release and reset SD.
void relenquishBusControl()
{
    if (!weHaveBus) return;

    DBG_PRINTLN("Releasing controll on SD...");
    dav.releaseSD();
    pinMode(27, PULLDOWN);
    powerReset(200);
    dataOn();
	delay(10);
    listenPrinter(true);
    DBG_PRINTLN("Releasing controll on SD. Done.");
    weHaveBus = false;
}

/// @brief Turns of power for SD and pulls connected pins down.
/// @param drain Delay for SD card discharging.
void powerReset(int drain)
{
    if (drain < 0) drain = 10;

    powerOff();

    DBG_PRINTLN("Drain...");
    pinMode(2, PULLDOWN);
    pinMode(4, PULLDOWN);
    pinMode(14, PULLDOWN);
    pinMode(15, PULLDOWN);

    delay(drain);

    DBG_PRINTLN("Drain. End.");
    pinMode(2, INPUT);
    pinMode(4, INPUT);
    pinMode(14, INPUT);
    pinMode(15, INPUT);

    powerOn();
}

/// @brief Disables power for SD.
void powerOff()
{
    pinMode(26, PULLDOWN);
    DBG_PRINTLN("SD Power OFF.");
}

/// @brief Enables power for SD.
void powerOn()
{
    pinMode(26, OUTPUT);
    digitalWrite(26, HIGH);
    DBG_PRINTLN("SD Power ON.");
}

/// @brief Disconnects SPI lines between SD and printer.
void dataOff()
{
    pinMode(25, PULLDOWN);
    DBG_PRINTLN("SD - Printer data OFF.");
}

/// @brief Connects SPI lines between SD and printer.
void dataOn()
{
    pinMode(25, OUTPUT);
    digitalWrite(25, HIGH);
    DBG_PRINTLN("SD - Printer data ON.");
}

/// @brief Listen for commands between Sd and printer.
/// @param value True to enable listening.
void listenPrinter(bool value)
{
    if (!value)
    {
        detachInterrupt(CS_SENSE);
        return;
    }

    attachInterrupt(
        CS_SENSE, []()
        {
        if(!weHaveBus)
        {
            spiBlockoutTime = millis() + SPI_BLOCKOUT_PERIOD;
        } },
        FALLING);
}