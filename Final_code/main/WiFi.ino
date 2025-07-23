// ********************************************************** wifi and bluetooth ****************
String getConnectedBSSID() {
  if (WiFi.status() == WL_CONNECTED) {
    uint8_t* bssid = WiFi.BSSID();
    char bssidStr[18];
    sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    return String(bssidStr);
  }
  return "";
}

// Function to connect to WiFi
void connect_wifi(const char* ssid, const char* password) {
  if (ssid == "" || password == "") {
    return;
  }

  // Save credentials immediately
  String bssid = ""; // Unknown until connected
  saveWiFiCredentials(ssid, bssid, password);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    wifiConnected = true; // led status
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // Now we can get the BSSID properly
    bssid = getConnectedBSSID();
    Serial.println("BSSID:" + bssid);

    // Update saved credentials with the real BSSID
    saveWiFiCredentials(ssid, bssid, password);
  } else {
    Serial.println("\nFailed to connect.");
    wifiConnected = false; // led status
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.begin();
    delay(100);
  }
}

void saveWiFiCredentials(String ssid, String bssid, String password) {
  prefs.putString("ssid", ssid);
  prefs.putString("bssid",bssid);
  prefs.putString("password", password);
  Serial.println("Saved.");
}

void connectToSavedWiFi() {
  String savedSSID = prefs.getString("ssid", "");
  String savedPassword = prefs.getString("password", "");
  Serial.println(savedSSID);
  Serial.println(savedPassword);
  if (savedSSID != "" && savedPassword != "") {
    connect_wifi(savedSSID.c_str(), savedPassword.c_str());
  } else {
    Serial.println("No saved credentials.");
  }
}


