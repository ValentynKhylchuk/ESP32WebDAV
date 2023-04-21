#include "SD_MMC.h"
#include <FS.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <time.h>

// debugging
#define DBG_PRINT(...) 		{ Serial.print(__VA_ARGS__); }
#define DBG_PRINTLN(...) 	{ Serial.println(__VA_ARGS__); }
// production
//#define DBG_PRINT(...) 		{ }
//#define DBG_PRINTLN(...) 	{ }

// constants for WebServer
#define CONTENT_LENGTH_UNKNOWN ((size_t) -1)
#define CONTENT_LENGTH_NOT_SET ((size_t) -2)
#define HTTP_MAX_POST_WAIT 		5000 

enum ResourceType { RESOURCE_NONE, RESOURCE_FILE, RESOURCE_DIR };
enum DepthType { DEPTH_NONE, DEPTH_CHILD, DEPTH_ALL };



class ESPWebDAV	{
public:
	bool init(int chipSelectPin, int serverPort);
	bool isClientWaiting();
	void handleClient(String blank = "");
	void rejectClient(String rejectMessage);

	SemaphoreHandle_t S_buffer_access_1 = NULL;
	SemaphoreHandle_t S_buffer_access_2 = NULL;
	SemaphoreHandle_t S_reading = NULL;
	SemaphoreHandle_t S_writing = NULL;

	uint8_t S_buffer_1[1024];
	uint8_t S_buffer_2[1024];
	volatile size_t S_buffer_1_readNum = 0;
	volatile size_t S_buffer_2_readNum = 0;
	volatile int S_buffer_1_partNum = -1;
	volatile int S_buffer_2_partNum = -1;
	String S_writeErrorMesage;
	
	QueueHandle_t

	TaskHandle_t S_WriteTask;
	File S_WriteFile;
	void handlePutPP();

	
protected:
	typedef void (ESPWebDAV::*THandlerFunction)(String);
	
	void processClient(THandlerFunction handler, String message);
	void handleNotFound();
	void handleReject(String rejectMessage);
	void handleRequest(String blank);
	void handleOptions(ResourceType resource);
	void handleLock(ResourceType resource);
	void handleUnlock(ResourceType resource);
	void handlePropPatch(ResourceType resource);
	void handleProp(ResourceType resource);
	//void sendPropResponse(boolean recursing, FatFile *curFile);File
	void sendPropResponse(boolean recursing, File *curFile);
	void handleGet(ResourceType resource, bool isGet);
	void handlePut(ResourceType resource);
	//void handleWriteError(String message, FatFile *wFile);
	void handleWriteError(String message, File *wFile);
	void handleDirectoryCreate(ResourceType resource);
	void handleMove(ResourceType resource);
	void handleDelete(ResourceType resource);

	// Sections are copied from ESP8266Webserver
	String getMimeType(String path);
	String urlDecode(const String& text);
	String urlToUri(String url);
	bool parseRequest();
	void sendHeader(const String& name, const String& value, bool first = false);
	void send(String code, const char* content_type, const String& content);
	void _prepareHeader(String& response, String code, const char* content_type, size_t contentLength);
	void sendContent(const String& content);
	void sendContent_P(PGM_P content);
	void setContentLength(size_t len);
	size_t readBytesWithTimeout(uint8_t *buf, size_t bufSize);
	size_t readBytesWithTimeout(uint8_t *buf, size_t bufSize, size_t numToRead);
	
	
	// variables pertaining to current most HTTP request being serviced
	WiFiServer *server;
	//SdFat sd;

	WiFiClient 	client;
	String 		method;
	String 		uri;
	String 		contentLengthHeader;
	String 		depthHeader;
	String 		hostHeader;
	String		destinationHeader;

	String 		_responseHeaders;
	bool		_chunked;
	int			_contentLength;
};


void handlePutTrigger(void * pvParameters);