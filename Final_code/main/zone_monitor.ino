//********************************************************* zone monitor ***********************
bool isKnownBSSID(const String& bssidToCheck, const std::vector<String>& knownList) {
  for (const auto& bssid : knownList) {
    if (bssid == bssidToCheck) return true;
  }
  return false;
}

std::vector<String> getKnownBSSIDList() {
  String list = prefs.getString("known_bssids", "");
  std::vector<String> bssids;
  int start = 0;
  while (start < list.length()) {
    int commaIndex = list.indexOf(',', start);
    if (commaIndex == -1) commaIndex = list.length();
    bssids.push_back(list.substring(start, commaIndex));
    start = commaIndex + 1;
  }
  return bssids;
}

void addBSSID(const String& newBssid, const std::vector<String>& knownBSSIDs) {
  String list = prefs.getString("known_bssids", "");
  if (knownBSSIDs.size() < 6 && !isKnownBSSID(newBssid,knownBSSIDs)) {
    list += (list.length() > 0 ? "," : "") + newBssid;
    prefs.putString("known_bssids", list);
    Serial.println("🧠 Learned and added: " + newBssid);
  }
}

void monitorZone() {
  Serial.printf("monitoring zone... WiFi status: %d\n", WiFi.status());

// commented for send render msgs through wifi for testing 
   
  // If already connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ SAFE: Already connected to WiFi.");
    wifiConnected = true; // for LED status
    return;
  }

  // If not connected, try to reconnect to saved WiFi
  Serial.println("WiFi not connected. Attempting to connect to saved WiFi...");
  connectToSavedWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ SAFE: Reconnected to saved WiFi.");
    wifiConnected = true; // for LED status
    return;
  }


  // If still not connected, proceed with scanning
  wifiConnected = false; // for LED status

  WiFi.disconnect(true);
  delay(100);

  Serial.println("WiFi mode before scanning: " + String(WiFi.getMode()));  // Should be 1 (WIFI_STA)
  Serial.println("Scanning WiFi...");

  int n = WiFi.scanNetworks();

  if (n < 0) {
    debugPrint("❌ Wi-Fi scan failed (returned -1).");
    return;
  } else if (n == 0) {
    Serial.println("No networks found.");
    return;
  }

  auto knownBSSIDs = getKnownBSSIDList();  // fetched once

  String last_connected_bssid = prefs.getString("bssid", "-1");
  int knownCount = 0;
  String newBSSID = "";
  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    if (bssid == last_connected_bssid) {
      connectToSavedWiFi();          // comment this in the testing through home wifi
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ SAFE: Connected to last known BSSID.");
        wifiConnected = true;
        return;
      }
    } else if (isKnownBSSID(bssid, knownBSSIDs)) {
      knownCount++;
    } else {
      newBSSID = bssid;
    }
  }

  if (knownCount == 0) {
    debugPrint("🚨 OUT OF ZONE: No known BSSIDs.");
    potential_zone_left();
  } else {
    Serial.printf("✅ SAFE: %d known BSSIDs found.\n", knownCount);
    if (knownCount >= 2 && newBSSID.length() > 0) {
      addBSSID(newBSSID, knownBSSIDs);
    }
  }
   if (fall_detected) {  
    if (WiFi.status() != WL_CONNECTED) {
      connectToSavedWiFi();
    }
    send_message_Render("notify","fall_detected");
    Serial.println("Falling msg sent.");      
    }

  Serial.println();
}
