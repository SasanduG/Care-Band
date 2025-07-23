
void detect_falls(void *pvParameters){
  while (1){
  if (millis() - last_sample_time >= sample_interval) {
    last_sample_time = millis();
    readRawAccel();
    acc_x = (acc_x_raw - acc_x_offset) / 16384.0;
    acc_y = (acc_y_raw - acc_y_offset) / 16384.0;
    acc_z = (acc_z_raw - acc_z_offset) / 16384.0;
    float magnitude = sqrt(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);
    //Serial.print("Magnitude: ");
    //Serial.println(magnitude);
    if (!fall_detected && magnitude > impact_threshold && (millis() - last_fall_time > fall_timeout)) {
      Serial.println("Impact detected! Checking for low activity...");
      bool isLowMovement = checkPostImpactStillness();
      if (isLowMovement) {
        debugPrint("✅ Fall Confirmed!");
        last_fall_time = millis();
        fall_detected = true;
        fallDetected = true;           // led status
        

        if (xSemaphoreTake(wifiMutex, portMAX_DELAY)) {
          connectToSavedWiFi();
          send_message_Render("notify","fall_detected");
          Serial.println("Falling msg sent.");
          xSemaphoreGive(wifiMutex);
        }


        // Critical section - add error handling
        if (xPortGetFreeHeapSize() > 16*1024) {  // Check available memory
          send_message_Render("notify","fall_detected");
        } else {
          Serial.println("Error: Low memory. Couldn't send notification for fall detection");
        }
        // digitalWrite(LED_PIN, HIGH);
      } else {
        debugPrint("❌ Movement after impact — not a fall.");
      }
    }
  }
}
} 
// ------------------ Functions ------------------

void readRawAccel() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  if (Wire.available() >= 6) {
    acc_x_raw = Wire.read() << 8 | Wire.read();
    acc_y_raw = Wire.read() << 8 | Wire.read();
    acc_z_raw = Wire.read() << 8 | Wire.read();
  }
}

void calibrateAccelerometer() {
  long ax_sum = 0, ay_sum = 0, az_sum = 0;
  const int samples = 1000;

  for (int i = 0; i < samples; i++) {
    readRawAccel();
    ax_sum += acc_x_raw;
    ay_sum += acc_y_raw;
    az_sum += acc_z_raw;
    delay(2);
  }

  acc_x_offset = ax_sum / float(samples);
  acc_y_offset = ay_sum / float(samples);
  acc_z_offset = az_sum / float(samples);
}

bool checkPostImpactStillness() {
  int lowActivityCount = 0;
  const int sampleCount = 20;

  for (int i = 0; i < sampleCount; i++) {
    readRawAccel();
    float ax = (acc_x_raw - acc_x_offset) / 16384.0;
    float ay = (acc_y_raw - acc_y_offset) / 16384.0;
    float az = (acc_z_raw - acc_z_offset) / 16384.0;

    float mag = sqrt(ax * ax + ay * ay + az * az);
    if (mag < low_activity_threshold) {
      lowActivityCount++;
    }

    delay(50); // 20 x 50ms = 1 second observation
  }

  // 70% of samples must be "still" to confirm fall
  return (lowActivityCount >= 14);
}
