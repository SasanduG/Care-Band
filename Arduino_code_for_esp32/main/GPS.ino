// ********************************* GPS functions ***********
// This function returns the location when found for initial setup
GPSLocation getGPSLocation() {
  unsigned long startTime = millis();
  Serial.println("Start finding GPS location...");

  bool gpsResponding = false;

  while (millis() - startTime < GPS_TIMEOUT) {
    bool anyData = false;

    // Read and parse all available data
    while (gpsSerial.available() > 0) {
      anyData = true;
      gpsResponding = true;

      char c = gpsSerial.read();
      gps.encode(c);

      if (gps.location.isUpdated()) {
        GPSLocation loc;
        loc.latitude = gps.location.lat();
        loc.longitude = gps.location.lng();
        Serial.printf("✅ GPS fix: Lat=%.6f, Lon=%.6f\n", loc.latitude, loc.longitude);
        return loc;
      }
    }

    if (!anyData && !gpsResponding) {
      Serial.println("⚠️ GPS not responding (no data). Check wiring or power.");
    } else if (!anyData) {
      Serial.println("📡 Waiting for GPS fix... No data in this cycle.");
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);  // Let WiFi/GSM tasks breathe
  }

  Serial.println("⏱️ GPS timeout—no fix acquired.");
  return {};  // Return empty if fix not found
}



// Function to get the saved GPS location using prefs
GPSLocation getHomeLocation() {
  GPSLocation loc = {0.0, 0.0};

  String locationStr = prefs.getString("Home_location", "");
  int commaIndex = locationStr.indexOf(',');
  if (commaIndex != -1) {
    String latStr = locationStr.substring(0, commaIndex);
    String lonStr = locationStr.substring(commaIndex + 1);
    loc.latitude = latStr.toDouble();
    loc.longitude = lonStr.toDouble();
  } else {
    Serial.println("Invalid saved location format");
  }

  return loc;
}