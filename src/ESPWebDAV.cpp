// WebDAV server using ESP8266 and SD card filesystem
// Targeting Windows 7 Explorer WebDav

#include "SD_MMC.h"
#include <FS.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <time.h>
#include <Hash.h>
#include "ESPWebDAV.h"

// define cal constants
const char *months[]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *wdays[]  = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};


// ------------------------
bool ESPWebDAV::init(int chipSelectPin, int serverPort) {
// ------------------------

	//_canRunOnSecond = xSemaphoreCreateMutex();

	// start the wifi server
	server = new WiFiServer(serverPort);
	server->begin();

	// Get time ???
	WiFi.waitForConnectResult();
  	configTime(0, 0, "pool.ntp.org");
	// https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  	setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  	tzset();
  	while (time(NULL) < 1E6) delay(1000);

  	//Serial.printf("\n\nsystime: %d\n", time(NULL));
  	time_t now = time(&now);
	Serial.println(ctime(&now));
	// --- ?
	
	// initialize the SD card
	//return sd.begin(chipSelectPin, spiSettings);
	return SD_MMC.begin();
}



// ------------------------
void ESPWebDAV::handleNotFound() {
// ------------------------
	Serial.println("handleNotFound");
	String message = "Not found\n";
	message += "URI: ";
	message += uri;
	message += " Method: ";
	message += method;
	message += "\n";

	sendHeader("Allow", "OPTIONS,MKCOL,POST,PUT");
	send("404 Not Found", "text/plain", message);
	DBG_PRINTLN("404 Not Found");
}



// ------------------------
void ESPWebDAV::handleReject(String rejectMessage)	{
// ------------------------
	Serial.println("handleReject");
	DBG_PRINT("Rejecting request: "); DBG_PRINTLN(rejectMessage);

	// handle options
	if(method.equals("OPTIONS"))
		return handleOptions(RESOURCE_NONE);
	
	// handle properties
	if(method.equals("PROPFIND"))	{
		sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");
		setContentLength(CONTENT_LENGTH_UNKNOWN);
		send("207 Multi-Status", "application/xml;charset=utf-8", "");
		sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:multistatus xmlns:D=\"DAV:\"><D:response><D:href>/</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>Fri, 30 Nov 1979 00:00:00 GMT</D:getlastmodified><D:getetag>\"3333333333333333333333333333333333333333\"</D:getetag><D:resourcetype><D:collection/></D:resourcetype></D:prop></D:propstat></D:response>"));
		
		if(depthHeader.equals("1"))	{
			sendContent(F("<D:response><D:href>/"));
			sendContent(rejectMessage);
			sendContent(F("</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>Fri, 01 Apr 2016 16:07:40 GMT</D:getlastmodified><D:getetag>\"2222222222222222222222222222222222222222\"</D:getetag><D:resourcetype/><D:getcontentlength>0</D:getcontentlength><D:getcontenttype>application/octet-stream</D:getcontenttype></D:prop></D:propstat></D:response>"));
		}
		
		sendContent(F("</D:multistatus>"));
		return;
	}
	else
		// if reached here, means its a 404
		handleNotFound();
}




// set http_proxy=http://localhost:36036
// curl -v -X PROPFIND -H "Depth: 1" http://Rigidbot/Old/PipeClip.gcode
// Test PUT a file: curl -v -T c.txt -H "Expect:" http://Rigidbot/c.txt
// C:\Users\gsbal>curl -v -X LOCK http://Rigidbot/EMA_CPP_TRCC_Tutorial/Consumer.cpp -d "<?xml version=\"1.0\" encoding=\"utf-8\" ?><D:lockinfo xmlns:D=\"DAV:\"><D:lockscope><D:exclusive/></D:lockscope><D:locktype><D:write/></D:locktype><D:owner><D:href>CARBON2\gsbal</D:href></D:owner></D:lockinfo>"
// ------------------------
void ESPWebDAV::handleRequest(String blank)	{
// ------------------------
	Serial.println("handleRequest");
	ResourceType resource = RESOURCE_NONE;

	// does uri refer to a file or directory or a null?
	/*
	FatFile tFile;
	if(tFile.open(sd.vwd(), uri.c_str(), O_READ))	{
		resource = tFile.isDir() ? RESOURCE_DIR : RESOURCE_FILE;
		tFile.close();
	}
	*/

	DBG_PRINT("Attempt to open:"); DBG_PRINTLN(uri);
	File file = SD_MMC.open(uri.c_str());
    if(file){
		resource = file.isDirectory() ? RESOURCE_DIR : RESOURCE_FILE;
		file.close();
    }

	DBG_PRINT("\r\nm: "); DBG_PRINT(method);
	DBG_PRINT(" r: "); DBG_PRINT(resource);
	DBG_PRINT(" u: "); DBG_PRINTLN(uri);

	// add header that gets sent everytime
	sendHeader("DAV", "2");

	// handle properties
	if(method.equals("PROPFIND"))
		return handleProp(resource);
	
	if(method.equals("GET"))
		return handleGet(resource, true);

	if(method.equals("HEAD"))
		return handleGet(resource, false);

	// handle options
	if(method.equals("OPTIONS"))
		return handleOptions(resource);

	// handle file create/uploads
	if(method.equals("PUT"))
		return handlePut(resource);
	
	// handle file locks
	if(method.equals("LOCK"))
		return handleLock(resource);
	
	if(method.equals("UNLOCK"))
		return handleUnlock(resource);
	
	if(method.equals("PROPPATCH"))
		return handlePropPatch(resource);
	
	// directory creation
	if(method.equals("MKCOL"))
		return handleDirectoryCreate(resource);

	// move a file or directory
	if(method.equals("MOVE"))
		return handleMove(resource);
	
	// delete a file or directory
	if(method.equals("DELETE"))
		return handleDelete(resource);

	// if reached here, means its a 404
	handleNotFound();
}



// ------------------------
void ESPWebDAV::handleOptions(ResourceType resource)	{
// ------------------------
	Serial.println("handleOptions");
	DBG_PRINTLN("Processing OPTION");
	sendHeader("Allow", "PROPFIND,GET,DELETE,PUT,COPY,MOVE");
	send("200 OK", NULL, "");
}



// ------------------------
void ESPWebDAV::handleLock(ResourceType resource)	{
// ------------------------
	Serial.println("handleLock");
	
	DBG_PRINTLN("Processing LOCK");
	
	// does URI refer to an existing resource
	if(resource == RESOURCE_NONE)
		return handleNotFound();
	
	sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,LOCK,MOVE,HEAD,POST,PUT,GET");
	sendHeader("Lock-Token", "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");

	size_t contentLen = contentLengthHeader.toInt();
	uint8_t buf[1024];
	size_t numRead = readBytesWithTimeout(buf, sizeof(buf), contentLen);
	
	if(numRead == 0)
		return handleNotFound();

	buf[contentLen] = 0;
	String inXML = String((char*) buf);
	int startIdx = inXML.indexOf("<D:href>");
	int endIdx = inXML.indexOf("</D:href>");
	if(startIdx < 0 || endIdx < 0)
		return handleNotFound();
		
	String lockUser = inXML.substring(startIdx + 8, endIdx);
	String resp1 = F("<?xml version=\"1.0\" encoding=\"utf-8\"?><D:prop xmlns:D=\"DAV:\"><D:lockdiscovery><D:activelock><D:locktype><write/></D:locktype><D:lockscope><exclusive/></D:lockscope><D:locktoken><D:href>urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9</D:href></D:locktoken><D:lockroot><D:href>");
	String resp2 = F("</D:href></D:lockroot><D:depth>infinity</D:depth><D:owner><a:href xmlns:a=\"DAV:\">");
	String resp3 = F("</a:href></D:owner><D:timeout>Second-3600</D:timeout></D:activelock></D:lockdiscovery></D:prop>");

	send("200 OK", "application/xml;charset=utf-8", resp1 + uri + resp2 + lockUser + resp3);
}



// ------------------------
void ESPWebDAV::handleUnlock(ResourceType resource)	{
// ------------------------
	Serial.println("handleUnlock");
	DBG_PRINTLN("Processing UNLOCK");
	sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,LOCK,MOVE,HEAD,POST,PUT,GET");
	sendHeader("Lock-Token", "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");
	send("204 No Content", NULL, "");
	
}



// ------------------------
void ESPWebDAV::handlePropPatch(ResourceType resource)	{
// ------------------------
	Serial.println("handlePropPatch");
	DBG_PRINTLN("PROPPATCH forwarding to PROPFIND");
	handleProp(resource);
}



// ------------------------
void ESPWebDAV::handleProp(ResourceType resource)	{
// ------------------------
	Serial.println("handleProp");
	DBG_PRINTLN("Processing PROPFIND");
	// check depth header
	DepthType depth = DEPTH_NONE;
	if(depthHeader.equals("1"))
		depth = DEPTH_CHILD;
	else if(depthHeader.equals("infinity"))
		depth = DEPTH_ALL;
	
	DBG_PRINT("Depth: "); DBG_PRINTLN(depth);

	// does URI refer to an existing resource
	if(resource == RESOURCE_NONE)
		return handleNotFound();

	if(resource == RESOURCE_FILE)
		sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
	else
		sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE");

	setContentLength(CONTENT_LENGTH_UNKNOWN);
	send("207 Multi-Status", "application/xml;charset=utf-8", "");
	sendContent(F("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
	sendContent(F("<D:multistatus xmlns:D=\"DAV:\">"));

	// open this resource
	//SdFile baseFile;
	//baseFile.open(uri.c_str(), FILE_READ);
	File baseFile = SD_MMC.open(uri.c_str(), FILE_READ);
	sendPropResponse(false, &baseFile);

	if((resource == RESOURCE_DIR) && (depth == DEPTH_CHILD))	{
		// append children information to message
		/*
		SdFile childFile;
		while(childFile.openNext(&baseFile, O_READ)) {
			yield();
			sendPropResponse(true, &childFile);
			childFile.close();
		}
		*/
		File childFile = baseFile.openNextFile();
		while(childFile) {
			yield();
			sendPropResponse(true, &childFile);
			childFile.close();
			childFile = baseFile.openNextFile();
		}
	}

	baseFile.close();
	sendContent(F("</D:multistatus>"));
}



// ------------------------
//void ESPWebDAV::sendPropResponse(boolean recursing, FatFile *curFile)	{
void ESPWebDAV::sendPropResponse(boolean recursing, File *curFile)	{
// ------------------------
	char buf[255];
	//curFile->getName(buf, sizeof(buf));
	strcpy(buf, curFile->name());

// String fullResPath = "http://" + hostHeader + uri;
	String fullResPath = uri;

	if(recursing)
		if(fullResPath.endsWith("/"))
			fullResPath += String(buf);
		else
			fullResPath += "/" + String(buf);

	// get file modified time
	//dir_t dir;
	time_t lw = curFile->getLastWrite();

	// some code for sample
	//time_t t= curFile->getLastWrite() - 3600;
	//struct tm * tmstruct = localtime(&t);
	
	// convert to required format
	//tm tmStr;
	//tmStr.tm_hour = FAT_HOUR(dir.lastWriteTime);
	//tmStr.tm_min = FAT_MINUTE(dir.lastWriteTime);
	//tmStr.tm_sec = FAT_SECOND(dir.lastWriteTime);
	//tmStr.tm_year = FAT_YEAR(dir.lastWriteDate) - 1900;
	//tmStr.tm_mon = FAT_MONTH(dir.lastWriteDate) - 1;
	//tmStr.tm_mday = FAT_DAY(dir.lastWriteDate);
	//time_t t2t = mktime(&tmStr);
	//tm *gTm = gmtime(&t2t);
	tm *gTm = localtime(&lw);

	// Tue, 13 Oct 2015 17:07:35 GMT
	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT", wdays[gTm->tm_wday], gTm->tm_mday, months[gTm->tm_mon], gTm->tm_year + 1900, gTm->tm_hour, gTm->tm_min, gTm->tm_sec);
	String fileTimeStamp = String(buf);


	// send the XML information about thyself to client
	sendContent(F("<D:response><D:href>"));
	// append full file path
	sendContent(fullResPath);
	sendContent(F("</D:href><D:propstat><D:status>HTTP/1.1 200 OK</D:status><D:prop><D:getlastmodified>"));
	// append modified date
	sendContent(fileTimeStamp);
	sendContent(F("</D:getlastmodified><D:getetag>"));
	// append unique tag generated from full path
	sendContent("\"" + sha1(fullResPath + fileTimeStamp) + "\"");
	sendContent(F("</D:getetag>"));

	if(curFile->isDirectory())
		sendContent(F("<D:resourcetype><D:collection/></D:resourcetype>"));
	else	{
		sendContent(F("<D:resourcetype/><D:getcontentlength>"));
		// append the file size
		sendContent(String(curFile->size()));
		sendContent(F("</D:getcontentlength><D:getcontenttype>"));
		// append correct file mime type
		sendContent(getMimeType(fullResPath));
		sendContent(F("</D:getcontenttype>"));
	}
	sendContent(F("</D:prop></D:propstat></D:response>"));
}




// ------------------------
void ESPWebDAV::handleGet(ResourceType resource, bool isGet)	{
// ------------------------
	Serial.println("handleGet");
	DBG_PRINTLN("Processing GET");

	// does URI refer to an existing file resource
	if(resource != RESOURCE_FILE)
		return handleNotFound();

	//SdFile rFile;
	long tStart = millis();
	uint8_t buf[1460];
	//rFile.open(uri.c_str(), O_READ);
	File rFile = SD_MMC.open(uri.c_str(), FILE_READ);

	sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");
	//size_t fileSize = rFile.fileSize();
	size_t fileSize = rFile.size();
	setContentLength(fileSize);
	String contentType = getMimeType(uri);
	if(uri.endsWith(".gz") && contentType != "application/x-gzip" && contentType != "application/octet-stream")
		sendHeader("Content-Encoding", "gzip");

	send("200 OK", contentType.c_str(), "");

	if(isGet)	{
		// disable Nagle if buffer size > TCP MTU of 1460
		// client.setNoDelay(1);

		// send the file
		while(rFile.available())	{
			// SD read speed ~ 17sec for 4.5MB file
			int numRead = rFile.read(buf, sizeof(buf));
			client.write(buf, numRead);
		}
	}

	rFile.close();
	DBG_PRINT("File "); DBG_PRINT(fileSize); DBG_PRINT(" bytes sent in: "); DBG_PRINT((millis() - tStart)/1000); DBG_PRINTLN(" sec");
}




// ------------------------
void ESPWebDAV::handlePut(ResourceType resource)	{
// ------------------------
	Serial.println("handlePut");
	DBG_PRINTLN("Processing Put");

	// does URI refer to a directory
	if(resource == RESOURCE_DIR)
		return handleNotFound();

	sendHeader("Allow", "PROPFIND,OPTIONS,DELETE,COPY,MOVE,HEAD,POST,PUT,GET");

	// Check if we can create new file.
	if(resource == RESOURCE_NONE)
	{
		File nFile = SD_MMC.open(uri.c_str(), FILE_WRITE);
		if(!nFile)
		{
			return handleWriteError("Unable to create a new file", &nFile);
		}
		nFile.close();
	}

	DBG_PRINT(uri); DBG_PRINTLN(" - ready for data");
	
	// Check if we have data to read and write.
	size_t contentLen = contentLengthHeader.toInt();
	if(contentLen <= 0)
	{
		Serial.println("There is no content to write. Write task creation skipped.");
		if(resource == RESOURCE_NONE)
			send("201 Created", NULL, "");
		else
			send("200 OK", NULL, "");
		return;
	}
	
	Serial.print("Call before new task. Running on core: "); Serial.println(xPortGetCoreID());

	// Reset some.
	S_reading = xSemaphoreCreateMutex();
	S_writing = xSemaphoreCreateMutex();
	struct DataPortin pDataPortin;
	S_dataQueue = xQueueCreate( 3, sizeof(pDataPortin) );

	// Take control on reed before writing task started.
	if(!xSemaphoreTake(S_reading, (TickType_t)100))
	{
		return handleWriteError("Unable to obtain controll over read mutex.", NULL);
	}

	// Start writing task on other core.
	int core = 0;
	if(xPortGetCoreID() == core)
	{
		core = 1;
	}

	xTaskCreatePinnedToCore(
		handlePutTrigger,   /* Task function. */
		"Task2",     		/* name of task. */
		10000,       		/* Stack size of task */
		this,        		/* parameter of the task */
		1,           		/* priority of the task */
		&S_WriteTask,		/* Task handle to keep track of created task */
		0);          		/* pin task to core 1 */

	// Removing file if it exists.
	SD_MMC.remove(uri.c_str());
	
	// Set num of date that we need to read and write.
	size_t numRemaining = contentLen;
	// Count number of blocks.
	int part_number = -1;
	// Save start time.
	long tStart = millis();
	// Prepare to count time for read operation.
	long t_read = 0;
	long t_s = 0;

	// Open file to write it.
	S_WriteFile = SD_MMC.open(uri.c_str(), FILE_WRITE);
		
	// read data from stream and write to the file
	while(numRemaining > 0)
	{
		// check if writing got error.
		if(xSemaphoreTake(S_writing, 0 ))
		{
			Serial.println ("Writing error detected.");
			xSemaphoreGive(S_writing);
			return handleWriteError(S_writeErrorMesage, &S_WriteFile);
		}

		// count work that left
		size_t numToRead = (numRemaining > 1024) ? 1024 : numRemaining;
		// save num of read date
		size_t numRead = 0;
		// save time
		t_s = millis();

		// update part number
		part_number++;
		
		Serial.print ("Reading into buffer. Part: ");Serial.println (part_number);

		// read data
		numRead = readBytesWithTimeout(pDataPortin.buffer, sizeof(pDataPortin.buffer), numToRead);
		// save info
		pDataPortin.partNum = part_number;
		pDataPortin.readNum = numRead;
		// update remainig
		numRemaining -= numRead;
		t_read = t_read + (millis() - t_s);;
		// adding data to queue
		Serial.println ("Reading into buffer. Adding to queue...");
		if(xQueueSend( S_dataQueue, &pDataPortin, 1000 / portTICK_PERIOD_MS ))
		{
			Serial.println ("Reading into buffer. Finished.");
		}
		else
		{
			Serial.println ("Reading into buffer. Error. Can not add to queue.");
			break;
		}
		delay(1);

		// stop if nothing to read
		if(numRead == 0)
		{
			break;
		}
	}
	
	// mark signal end of reading
	DBG_PRINTLN("Reading end.");
	xSemaphoreGive(S_reading);
	DBG_PRINT(" == Read to buffer time: "); DBG_PRINT( t_read / 1000.0); DBG_PRINTLN(" s.");

	// waiting for write to finish
	if(xSemaphoreTake(S_writing, 1000 / portTICK_PERIOD_MS))
	{
		DBG_PRINTLN("Wrinting detected as ended.");
		xSemaphoreGive(S_writing);
	}
	else
	{
		Serial.println("Terminateing write task.");
		vTaskDelete(S_WriteTask); 
		return handleWriteError("Unable to obtain controll over read mutex.", &S_WriteFile);
	}

	// stop writing operation
	S_WriteFile.flush();
	S_WriteFile.close();

	DBG_PRINTLN("=====================================================");
	DBG_PRINT("File "); DBG_PRINT(contentLen - numRemaining); DBG_PRINT(" bytes stored in: "); DBG_PRINT((millis() - tStart)/1000.0); DBG_PRINTLN(" sec");
	DBG_PRINT("Speed: "); DBG_PRINT( ((contentLen - numRemaining) / 1024.0) / ((millis() - tStart)/1000.0) ); DBG_PRINTLN(" KB/s.");
	DBG_PRINTLN("=====================================================");

	if(resource == RESOURCE_NONE)
		send("201 Created", NULL, "");
	else
		send("200 OK", NULL, "");
}


void handlePutTrigger(void * pvParameters)	{
// ------------------------
	((ESPWebDAV *) pvParameters)->handlePutPP();
	vTaskDelete(NULL);
	return;
}

// ------------------------
void ESPWebDAV::handlePutPP()	{
// ------------------------
	Serial.print("    |    Writing running on core: "); Serial.println(xPortGetCoreID());
	// Take control on write.
	
	if(!xSemaphoreTake(S_writing,  200 / portTICK_PERIOD_MS))
	{
		// error detected
		S_writeErrorMesage = "    |    Unable to obtain controll over write mutex.";
		vTaskDelete(NULL);
		return;
	}
	
	Serial.println("    |    Writing running... ");

	// give some time for read to start;
	delay(10);

	// save time
	long t_write = 0;
	long t_s = 0;
	
	// read data from stream and write to the file
	while(true)	
	{
		struct DataPortin pDataPortin;
		
		t_s = millis();
		
		Serial.println("    |    Ask for data in queue... ");
		if( xQueueReceive( S_dataQueue, &pDataPortin, 200 / portTICK_PERIOD_MS ))
		{
			Serial.printf("    |    readNum: %d, partNum: %d \n", pDataPortin.readNum, pDataPortin.partNum);
			if(pDataPortin.partNum == -1)
			{
				// error detected
				S_writeErrorMesage = "    |    Write data failed. Empty data.";
				// release semaphores
				xSemaphoreGive(S_writing);
				vTaskDelete(NULL);
				return;
			}

			Serial.print ("    |    Writing from buffer. Part:"); Serial.println(pDataPortin.partNum);
			if (S_WriteFile.write(pDataPortin.buffer, pDataPortin.readNum))
			{
				t_write = t_write + (millis() - t_s);
				Serial.println ("    |    Writing from buffer. Finished.");
				continue;
			}
			else
			{	
				// error detected
				S_writeErrorMesage = "    |    Write data failed.";
				// release semaphores
				xSemaphoreGive(S_writing);
				vTaskDelete(NULL);
				return;
			}
		}
		/*
		else
		{
			// error detected
			S_writeErrorMesage = "    |    Write data failed. Waiting for data in queue for to long.";
			// release semaphores
			xSemaphoreGive(S_writing);
			vTaskDelete(NULL);
			return;
		}
		*/
		Serial.println("    |    Waiting for data in queue for to long.");
		Serial.println("    |    Check for writing end. ");
		// check for writing end.
		if(xSemaphoreTake(S_reading, 0))
		{
			Serial.println ("    |    Writing. Detected finish.");
			DBG_PRINT("Write to card time: "); DBG_PRINT( t_write / 1000.0); DBG_PRINTLN(" s.");
			delay(10);
			xSemaphoreGive(S_writing);
			xSemaphoreGive(S_reading);
			vTaskDelete(NULL);
			return;
		}
		Serial.println("    |    Check for writing end. End not detected. Continue.");
		Serial.println("    |    ...");
		delay(10);
	}
}




// ------------------------
//void ESPWebDAV::handleWriteError(String message, FatFile *wFile)	{
void ESPWebDAV::handleWriteError(String message, File *wFile)	{
// ------------------------
	Serial.println("handleWriteError");
	// close this file
	wFile->close();
	// delete the wrile being written
	//sd.remove(uri.c_str());
	SD_MMC.remove(uri.c_str());
	// send error
	send("500 Internal Server Error", "text/plain", message);
	DBG_PRINTLN(message);
}


// ------------------------
void ESPWebDAV::handleDirectoryCreate(ResourceType resource)	{
// ------------------------
	Serial.println("handleDirectoryCreate");
	DBG_PRINTLN("Processing MKCOL");
	
	// does URI refer to anything
	if(resource != RESOURCE_NONE)
		return handleNotFound();
	
	// create directory
	//if (!sd.mkdir(uri.c_str(), true)) {
	if (!SD_MMC.mkdir(uri.c_str())) {
		// send error
		send("500 Internal Server Error", "text/plain", "Unable to create directory");
		DBG_PRINTLN("Unable to create directory");
		return;
	}

	DBG_PRINT(uri);	DBG_PRINTLN(" directory created");
	sendHeader("Allow", "OPTIONS,MKCOL,LOCK,POST,PUT");
	send("201 Created", NULL, "");
}



// ------------------------
void ESPWebDAV::handleMove(ResourceType resource)	{
// ------------------------
	Serial.println("handleMove");
	DBG_PRINTLN("Processing MOVE");
	
	// does URI refer to anything
	if(resource == RESOURCE_NONE)
		return handleNotFound();

	if(destinationHeader.length() == 0)
		return handleNotFound();
	
	String dest = urlDecode(urlToUri(destinationHeader));
		
	DBG_PRINT("Move destination: "); DBG_PRINTLN(dest);

	// move file or directory
	//if ( !sd.rename(uri.c_str(), dest.c_str())	) {
	if ( !SD_MMC.rename(uri.c_str(), dest.c_str())	) {
		// send error
		send("500 Internal Server Error", "text/plain", "Unable to move");
		DBG_PRINTLN("Unable to move file/directory");
		return;
	}

	DBG_PRINTLN("Move successful");
	sendHeader("Allow", "OPTIONS,MKCOL,LOCK,POST,PUT");
	send("201 Created", NULL, "");
}




// ------------------------
void ESPWebDAV::handleDelete(ResourceType resource)	{
// ------------------------
	Serial.println("handleDelete");
	DBG_PRINTLN("Processing DELETE");
	
	// does URI refer to anything
	if(resource == RESOURCE_NONE)
		return handleNotFound();

	bool retVal;
	
	if(resource == RESOURCE_FILE) {
		// delete a file
		//retVal = sd.remove(uri.c_str());
		retVal = SD_MMC.remove(uri.c_str());
	}
	else {
		// delete a directory
		//retVal = sd.rmdir(uri.c_str());
		retVal = SD_MMC.rmdir(uri.c_str());
	}

	if(!retVal)	{
		// send error
		send("500 Internal Server Error", "text/plain", "Unable to delete");
		DBG_PRINTLN("Unable to delete file/directory");
		return;
	}

	DBG_PRINTLN("Delete successful");
	sendHeader("Allow", "OPTIONS,MKCOL,LOCK,POST,PUT");
	send("200 OK", NULL, "");
}

