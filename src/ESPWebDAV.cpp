// WebDAV server using ESP32 and SD_MMC card filesystem
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
void ESPWebDAV::init(int serverPort) {
// ------------------------

	DBG_PRINTLN(" a0 ---- ");
	// start the wifi server
	server = new WiFiServer(serverPort);
	server->begin();

	/*
	// Get time
	WiFi.waitForConnectResult();
  	configTime(0, 0, "pool.ntp.org");// https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  	setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
  	tzset();
  	while (time(NULL) < 1E6) delay(1000);

	DBG_PRINTLN(" a ---- ");
  	time_t now = time(&now);
	DBG_PRINTLN(ctime(&now));
	*/

	DBG_PRINTLN(" b ---- ");
	
	// initialize the SD card
	//return sd.begin(chipSelectPin, spiSettings);
	//return SD_MMC.begin();
}

// ------------------------
bool ESPWebDAV::takeSD() {
// ------------------------
	// initialize the SD card
	return SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_HIGHSPEED);
}

// ------------------------
void ESPWebDAV::releaseSD() {
// ------------------------

	return SD_MMC.end();
}


// ------------------------
void ESPWebDAV::handleNotFound() {
// ------------------------
	DBG_PRINTLN("handleNotFound");
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
	DBG_PRINTLN("handleReject");
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
	DBG_PRINTLN("handleRequest");
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
	DBG_PRINTLN("handleOptions");
	DBG_PRINTLN("Processing OPTION");
	sendHeader("Allow", "PROPFIND,GET,DELETE,PUT,COPY,MOVE");
	send("200 OK", NULL, "");
}



// ------------------------
void ESPWebDAV::handleLock(ResourceType resource)	{
// ------------------------
	DBG_PRINTLN("handleLock");
	
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
	DBG_PRINTLN("handleUnlock");
	DBG_PRINTLN("Processing UNLOCK");
	sendHeader("Allow", "PROPPATCH,PROPFIND,OPTIONS,DELETE,UNLOCK,COPY,LOCK,MOVE,HEAD,POST,PUT,GET");
	sendHeader("Lock-Token", "urn:uuid:26e57cb3-834d-191a-00de-000042bdecf9");
	send("204 No Content", NULL, "");
	
}



// ------------------------
void ESPWebDAV::handlePropPatch(ResourceType resource)	{
// ------------------------
	DBG_PRINTLN("handlePropPatch");
	DBG_PRINTLN("PROPPATCH forwarding to PROPFIND");
	handleProp(resource);
}



// ------------------------
void ESPWebDAV::handleProp(ResourceType resource)	{
// ------------------------
	DBG_PRINTLN("handleProp");
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
	{
		if(fullResPath.endsWith("/"))
		{
			fullResPath += String(buf);
		}
		else
		{
			fullResPath += "/" + String(buf);
		}
	}
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
	DBG_PRINTLN("handleGet");
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
	DBG_PRINTLN("handlePut");
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
		DBG_PRINTLN("There is no content to write. Write task creation skipped.");
		if(resource == RESOURCE_NONE)
			send("201 Created", NULL, "");
		else
			send("200 OK", NULL, "");
		return;
	}
	
	DBG_PRINT("Call before new task. Running on core: "); DBG_PRINTLN(xPortGetCoreID());

	// Reset some.
	//S_reading = xSemaphoreCreateMutex();
	//S_writing = xSemaphoreCreateMutex();
	S_writingHasError = false;
	S_readingHasError = false;
	bool readingIsDone = false;
	bool writingIsDone = false;
	struct DataPortin pDataPortin;
	S_dataQueue = xQueueCreate( RW_Q_SIZE, sizeof(pDataPortin) );

	// Save start time.
	long tStart = millis();
	// Open file to write it.
	S_WriteFile = SD_MMC.open(uri.c_str(), FILE_WRITE);

	DBG_PRINT("Free memory: ");DBG_PRINT(ESP.getFreeHeap()/1024.0);DBG_PRINTLN(" KB.");

	xTaskCreatePinnedToCore(
		StartReadTask,		/* Task function. */
		"ReadTask",     	/* name of task. */
		10000,       		/* Stack size of task */
		this,        		/* parameter of the task */
		0,           		/* priority of the task */
		&S_ReadTask,		/* Task handle to keep track of created task */
		0);          		/* pin task to core 1 */

	xTaskCreatePinnedToCore(
		StartWriteTask,		/* Task function. */
		"WriteTask",     	/* name of task. */
		10000,       		/* Stack size of task */
		this,        		/* parameter of the task */
		1,           		/* priority of the task */
		&S_WriteTask,		/* Task handle to keep track of created task */
		1);          		/* pin task to core 1 */


	
	// read data from stream and write to the file
	while(true)
	{
		delay(100);
		// check if reading got error / done.
		if( !readingIsDone && !S_reading )
		{
			if(S_readingHasError)
			{
				DBG_PRINTLN ("Reading error detected.");
				// stop writing task
				S_writing = false;
				vQueueDelete(S_dataQueue);
				delay(300);
				return handleWriteError(S_readingError, &S_WriteFile);
			}
			DBG_PRINTLN ("Reading detected as done.");
			readingIsDone = true;
		}

		// check if writing got error / done.
		if( !writingIsDone && !S_writing )
		{
			if(S_writingHasError)
			{
				DBG_PRINTLN ("Writing error detected.");
				// stop writing task
				S_reading = false;
				S_writing = false;
				vQueueDelete(S_dataQueue);
				delay(300);
				return handleWriteError(S_writingError, &S_WriteFile);
			}
			DBG_PRINTLN ("Writing detected as done.");
			writingIsDone = true;
		}

		if(readingIsDone && writingIsDone)
		{
			break;
		}
	}

	// stop writing operation
	S_WriteFile.flush();
	S_WriteFile.close();
	vQueueDelete(S_dataQueue);

	DBG_PRINTLN("=====================================================");
	DBG_PRINT("File "); DBG_PRINT(contentLen / 1024.0); DBG_PRINT(" KB stored in: "); DBG_PRINT((millis() - tStart)/1000.0); DBG_PRINTLN(" sec");
	DBG_PRINT("Speed: "); DBG_PRINT( ((contentLen) / 1024.0) / ((millis() - tStart)/1000.0) ); DBG_PRINTLN(" KB/s.");
	DBG_PRINTLN("=====================================================");
	DBG_PRINT("Free memory: ");DBG_PRINT(ESP.getFreeHeap()/1024.0);DBG_PRINTLN(" KB.");

	if(resource == RESOURCE_NONE)
		send("201 Created", NULL, "");
	else
		send("200 OK", NULL, "");
}

// ------------------------
void ESPWebDAV::ReadTask()	{
// ------------------------
	DBG_PRINT("Read task: Running on core: "); DBG_PRINTLN(xPortGetCoreID());

	// Reset some.
	struct DataPortin pDataPortin;
	size_t contentLen = contentLengthHeader.toInt();

	S_reading = true;

	// Set num of date that we need to read and write.
	size_t numRemaining = contentLen;
	// Count number of blocks.
	int part_number = -1;
	// Prepare to count time for read operation.
	long t_read = 0;
	long t_s = 0;
	//uint8_t buffer[1024];
	//for (size_t i = 0; i < 1024; i++)	buffer[i] = (uint8_t)i;
	
	// read data from stream and write to the file
	while(numRemaining > 0)
	{
		// save time
		t_s = micros();

		if(!S_reading)	return;

		//DBG_PRINT("Free memory: ");DBG_PRINT(ESP.getFreeHeap()/1024.0);DBG_PRINTLN(" KB.");

		// count work that left
		size_t numToRead = (numRemaining > RW_BUF_SIZE) ? RW_BUF_SIZE : numRemaining;
		// save num of read date
		size_t numRead = 0;

		// update part number
		part_number++;
		
		//DBG_PRINTf ("Reading into buffer. Part: %d\n", part_number);


		// read data
		numRead = readBytesWithTimeout(pDataPortin.buffer, sizeof(pDataPortin.buffer), numToRead);

		if(!S_reading)	return;

		// save info
		pDataPortin.partNum = part_number;
		pDataPortin.readNum = numRead;
		// update remainig
		numRemaining -= numRead;
		t_read = t_read + (micros() - t_s);
		//DBG_PRINTf ("R t : %d\n", micros() - t_s);

		// adding data to queue
		//DBG_PRINTLN ("Reading into buffer. Adding to queue...");
		if(xQueueSend( S_dataQueue, &pDataPortin, 1000 / portTICK_PERIOD_MS ))
		{
			//DBG_PRINTLN ("Reading into buffer. Finished.");
			//DBG_PRINTf ("Reading. Done. Part: %d \n", part_number);
			//DBG_PRINTf ("R : %d\n", part_number);
		}
		else
		{
			// error detected
			S_readingHasError = true;
			S_readingError = "Reading into buffer. Error. Can not add to queue.";
			DBG_PRINTLN ("Reading into buffer. Error. Can not add to queue.");
			S_reading = false;
			return;
		}

		// stop if nothing to read
		if(numRead == 0)
		{
			break;
		}
	}

	// mark signal end of reading
	DBG_PRINT("Read to buffer time: "); DBG_PRINT( t_read / 1000.0 / 1000.0); DBG_PRINTLN(" s.");
	S_reading = false;
}


// ------------------------
void StartWriteTask(void * pvParameters){
// ------------------------
	((ESPWebDAV *) pvParameters)->WriteTask();
	vTaskDelete(NULL);
	return;
}

// ------------------------
void StartReadTask(void * pvParameters)	{
// ------------------------
	((ESPWebDAV *) pvParameters)->ReadTask();
	vTaskDelete(NULL);
	return;
}

// ------------------------
void ESPWebDAV::WriteTask()	{
// ------------------------
	S_writing = true;
	DBG_PRINT("Write task: Running on core: "); DBG_PRINTLN(xPortGetCoreID());

	// save time
	long t_write = 0;
	long t_write2 = 0;
	long t_s = 0;
	long t_s2 = 0;
	
	struct DataPortin pDataPortin;
	// read data from stream and write to the file
	while(true)	
	{
		t_s = micros();
		//delay(200);
		if(!S_writing)	return;
		
		//DBG_PRINTLN("Writing. Ask for data in queue... ");
		if( xQueueReceive( S_dataQueue, &pDataPortin, 400 / portTICK_PERIOD_MS ))
		{
			//DBG_PRINTf("    |    readNum: %d, partNum: %d \n", pDataPortin.readNum, pDataPortin.partNum);
			if(pDataPortin.partNum == -1)
			{
				// error detected
				DBG_PRINTLN("Writing. Write data failed. Empty data.");
				S_writingHasError = true;
				S_writingError = "Write data failed. Empty data.";
				// release semaphores
				S_writing = false;
				return;
			}

			t_s2 = micros();
			if(!S_writing)	return;

			//DBG_PRINT ("Writing from buffer. Part:"); DBG_PRINTLN(pDataPortin.partNum);
			if (S_WriteFile.write(pDataPortin.buffer, pDataPortin.readNum))
			{
				t_write = t_write + (micros() - t_s);
				//DBG_PRINTf("W t: %d\n", (micros() - t_s));
				t_write2 = t_write2 + (micros() - t_s2);
				//DBG_PRINTLN ("    |    Writing from buffer. Finished.");
				//DBG_PRINTf ("    |    Writing from buffer. Finished.");
				//printf("    |    Writing from buffer. Finished. Part: %d \n", pDataPortin.partNum);
				//DBG_PRINTf("W : %d\n", pDataPortin.partNum);
				continue;
			}
			else
			{	
				// error detected
				DBG_PRINTLN("Writing. Write data failed.");
				S_writingHasError = true;
				S_writingError = "Write data failed.";
				// release semaphores
				S_writing = false;
				return;
			}
		}

		// check for writing end.
		if(!S_reading)
		{
			if(S_readingHasError)
			{
				DBG_PRINTLN ("Writing. Detected reading error. Terminating.");
				S_writing = false;
				return;
			}

			//DBG_PRINTf(" Writing. Queue count: %d.", uxQueueMessagesWaiting(S_dataQueue));
			if(uxQueueMessagesWaiting(S_dataQueue) > 0)
			{
				//DBG_PRINTLN ("Writing. Detected reading finish. Continue writing.");
				continue;
			}
			
			DBG_PRINTLN ("Writing. Finish.");
			DBG_PRINT("Write to card total time: "); DBG_PRINT( t_write / 1000.0 / 1000.0); DBG_PRINTLN(" s.");
			DBG_PRINT("Write to card real time: "); DBG_PRINT( t_write2 / 1000.0 / 1000.0); DBG_PRINTLN(" s.");
			S_writing = false;
			return;
		}
	}
}




// ------------------------
//void ESPWebDAV::handleWriteError(String message, FatFile *wFile)	{
void ESPWebDAV::handleWriteError(String message, File *wFile)	{
// ------------------------
	DBG_PRINTLN("handleWriteError");
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
	DBG_PRINTLN("handleDirectoryCreate");
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
	DBG_PRINTLN("handleMove");
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
	DBG_PRINTLN("handleDelete");
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

