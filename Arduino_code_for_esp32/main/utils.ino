void confirmHome() {
  
  // WiFi.mode(WIFI_STA);
  WiFi.disconnect(true,true);  // Optional: makes sure scan is clean
  delay(100);         // Give WiFi hardware a moment
  WiFi.begin();
  delay(100);

  Serial.println("Confirming home zone...");
  int n = WiFi.scanNetworks();
  if (n == -1) {
    Serial.println("❌ Wi-Fi scan failed (returned -1).");
    return;
  } else if (n == 0) {
    Serial.println("No networks found.");
    return;
  }
  Serial.println("n:"+String(n));
  String bssidList = "";

  for (int i = 0; i < n; ++i) {
    String bssid = WiFi.BSSIDstr(i);
    Serial.println("ssids 1"+bssid);
    if (bssidList.indexOf(bssid) == -1) {
      if (bssidList.length() > 0) bssidList += ",";
      bssidList += bssid;
    }
  }
  size_t written = prefs.putString("known_bssids", bssidList);
  if (written > 0) {
    Serial.println("✅ Stored known BSSIDs: " + bssidList);
  } else {
    Serial.println("❌ Failed to store BSSIDs!");
  }

  // Store home GPS location
  GPSLocation location = getGPSLocation();
  Serial.println("🌎 GPS Location : ("+String(location.latitude, 6) + "," + String(location.longitude, 6)+")");
  prefs.putString("Home_location", String(location.latitude, 6) + "," + String(location.longitude, 6));
}

void shut_down(){
  Serial.println("Entering deep sleep state.....");
  esp_deep_sleep_start(); // without configuring any wake-up source
}

void restart(){
  Serial.println("Restarting...");
  esp_restart();
}