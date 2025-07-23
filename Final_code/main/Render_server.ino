/***********************HTTP Error codees******************
Response:{"error":"[Errno 5] Input/output error"} - It is works, msg has sent to the server
Error on HTTP request: -11   - Connections is fine, server might be turned off
Error on HTTP request: -1     - Connection Problem, check the code
*/
//********************************** Render server ***********

// APN for SIM card
const char* apn = "mobitel3g";  // e.g., "dialogbb" or "mobitel3g"

void send_message_Render(String command, String message) {
  // Make HTTPS or HTTP POST request
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    Serial.println("Sending over WiFi...");
    Serial.println("Message: " + message);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Free heap before HTTPS request: %u bytes\n", ESP.getFreeHeap());

    // Initialize HTTP (NO separate WiFiClient!)
    http.begin(serverURL);

    http.addHeader("Content-Type", "application/json");

    // Create JSON string
    String jsonPayload = 
      String("{\"x-api-key\":\"") + x_apiKey +
      "\", \"my-api-key\":\"" + my_apiKey +
      "\", \"command\":\"" + command +
      "\", \"message\":\"" + message + "\"}";

    // Send POST request
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("Response:");
      Serial.println(payload);
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }

    http.end(); // Close connection

  } else {
    wifiConnected = false;  // for led status

    Serial.println("WiFi not connected. Using GSM...");
    //xSemaphoreGive(wifiMutex);

    useGSMToSend(command, message);
    

    
  }

  Serial.println("Sending process is end.");
}

// GSM***********************************************************
/*
void sendCommand(String cmd) {
  Serial.println("Sending: " + cmd);
  sim900.println(cmd);
  waitResponse();
}

void waitResponse() {
  long t = millis();
  while (millis() - t < 5000) {
    while (sim900.available()) {
      String s = sim900.readStringUntil('\n');
      Serial.println(s);
    }
  }
}*/

void sendCommand(String cmd) {
  Serial.println("Sending: " + cmd);
  sim900.println(cmd);
}


bool waitForResponse(const String& expected, uint32_t timeoutMs = 5000) {
  uint32_t start = millis();
  String buffer;
  while (millis() - start < timeoutMs) {
    while (sim900.available()) {
      char c = sim900.read();
      buffer += c;
      Serial.write(c);  // Echo everything
      if (buffer.indexOf(expected) != -1) {
        return true;  // Found it
      }
    }
    vTaskDelay(1);  // Yield to scheduler
  }
  return false;  // Timed out
}


// Main function
void useGSMToSend(String command, String message) {
  Serial.println("🔄 Starting GSM HTTP POST...");

  // Parse server IP and endpoint
  String url(serverURL);
  String serverIP;
  String endpoint = "/";

  if (url.startsWith("http://")) {
    url = url.substring(7);
  }

  int slashIndex = url.indexOf('/');
  if (slashIndex >= 0) {
    serverIP = url.substring(0, slashIndex);
    endpoint = url.substring(slashIndex);
  } else {
    serverIP = url;
  }

  Serial.println("🌐 Server IP: " + serverIP);
  Serial.println("🌐 Endpoint: " + endpoint);

  // Build JSON payload
  String jsonPayload = 
      String("{\"x-api-key\":\"") + x_apiKey +
      "\", \"my-api-key\":\"" + my_apiKey +
      "\", \"command\":\"" + command +
      "\", \"message\":\"" + message + "\"}";

  Serial.println("📦 JSON: " + jsonPayload);

  // 1️⃣ Start GPRS
  sendCommand("AT");
  waitForResponse("OK");

  sendCommand("AT+CPIN?");
  waitForResponse("READY");

  sendCommand("AT+CREG?");
  waitForResponse("+CREG: 0,1", 10000);

  sendCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
  waitForResponse("OK");

  sendCommand("AT+SAPBR=3,1,\"APN\",\"" + String(apn) + "\"");
  waitForResponse("OK");

  sendCommand("AT+SAPBR=1,1");
  waitForResponse("OK", 10000);

  sendCommand("AT+SAPBR=2,1");
  waitForResponse("+SAPBR: 1,1", 5000);

  // 2️⃣ Initialize HTTP
  sendCommand("AT+HTTPINIT");
  waitForResponse("OK");

  sendCommand("AT+HTTPPARA=\"CID\",1");
  waitForResponse("OK");

  sendCommand("AT+HTTPPARA=\"URL\",\"http://" + serverIP + endpoint + "\"");
  waitForResponse("OK");

  sendCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  waitForResponse("OK");

  // 3️⃣ Prepare HTTP data
  String dataLenCmd = "AT+HTTPDATA=" + String(jsonPayload.length()) + ",10000";
  sendCommand(dataLenCmd);

  if (waitForResponse("DOWNLOAD", 5000)) {
    sim900.print(jsonPayload);
    Serial.println("📤 JSON sent, waiting for OK...");

    if (waitForResponse("OK", 10000)) {
      Serial.println("✅ Data accepted by modem.");
    } else {
      Serial.println("❌ No OK after data send.");
      return;
    }
  } else {
    Serial.println("❌ Did not get DOWNLOAD prompt.");
    return;
  }

  // 4️⃣ Start POST
  sendCommand("AT+HTTPACTION=1");
  if (waitForResponse("+HTTPACTION:", 10000)) {
    Serial.println("✅ HTTP POST action completed.");
  } else {
    Serial.println("❌ HTTPACTION response timeout.");
  }

  // 5️⃣ Read response
  sendCommand("AT+HTTPREAD");
  waitForResponse("OK", 5000);

  // 6️⃣ Clean up
  sendCommand("AT+HTTPTERM");
  waitForResponse("OK");

  sendCommand("AT+SAPBR=0,1");
  waitForResponse("OK");

  Serial.println("✅ HTTP POST completed.");
}