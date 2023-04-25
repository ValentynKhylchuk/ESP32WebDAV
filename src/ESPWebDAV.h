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
#define RW_BUF_SIZE		 		1024 * 2
#define RW_Q_SIZE		 		4

enum ResourceType { RESOURCE_NONE, RESOURCE_FILE, RESOURCE_DIR };
enum DepthType { DEPTH_NONE, DEPTH_CHILD, DEPTH_ALL };

struct DataPortin
{
	uint8_t buffer[RW_BUF_SIZE];
	size_t readNum = -1;
	int partNum = -1;
};

class ESPWebDAV	{
public:
	bool init(int chipSelectPin, int serverPort);
	bool isClientWaiting();
	void handleClient(String blank = "");
	void rejectClient(String rejectMessage);


	QueueHandle_t S_dataQueue;

	volatile bool S_reading = NULL;
	volatile bool S_writing = NULL;

	volatile bool S_readingHasError = false;
	volatile bool S_writingHasError = false;

	String S_readingError;
	String S_writingError;

	TaskHandle_t S_WriteTask;
	TaskHandle_t S_ReadTask;

	File S_WriteFile;

	void WriteTask();
	void ReadTask();

	
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


void StartWriteTask(void * pvParameters);
void StartReadTask(void * pvParameters);