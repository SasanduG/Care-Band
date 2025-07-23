#include "BluetoothSerial.h"
#include <WiFi.h>
#include <ArduinoJson.h>  // Needed to parse JSON-formatted Bluetooth commands
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// MPU6050 libraries and variables
#include <Wire.h>

const int MPU_ADDR = 0x68;  // MPU6050 I2C address
// const int LED_PIN = 15;     // GPIO pin connected to the LED
int16_t acc_x_raw, acc_y_raw, acc_z_raw;
// Calibrated values
float acc_x, acc_y, acc_z;
// Calibration offsets
float acc_x_offset = 0.0;
float acc_y_offset = 0.0;
float acc_z_offset = 0.0;
// Fall detection thresholds 0            iin
float impact_threshold = 1.5;        // g-force for impact
float low_activity_threshold = 1.3;  // g-force for post-impact stillness
unsigned long last_fall_time = 0;
unsigned long fall_timeout = 1000;   // 3 seconds between fall events

bool fall_detected = false; // Flag to keep LED on after fall
unsigned long last_sample_time = 0;
const unsigned long sample_interval = 20; // milliseconds (50Hz)
// MPU6050 libraries and variables

// Store task handle globally when creating task:
TaskHandle_t zoneMonitorHandle = NULL;


//############### wifi bluetooth
BluetoothSerial SerialBT;
Preferences prefs;

SemaphoreHandle_t wifiMutex;
volatile bool pauseMonitor = false;
volatile bool isUser_atHome = true;

// ###############  GPS
#define RXD2 17
#define TXD2 16
#define GPS_BAUD 9600
#define GPS_TIMEOUT 5000

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);


struct GPSLocation {
  double latitude;
  double longitude;
};

// ENABLE BLUETOOTH
const int buttonPin = 0;  // Change if using another GPIO
bool btInitialized = false;

// GSM serial
HardwareSerial sim900(1); // Use UART1 (pins 26, 27)
#define GSM_RX 27  // Change according to your wiring
#define GSM_TX 26

int indicating_led = 14;

volatile bool wifiConnected = false;
volatile bool bluetoothEnabled = false;
volatile bool fallDetected = false;
volatile bool leftHome = false;

void LED_indicator(void *pvParameters) {
  pinMode(indicating_led, OUTPUT);

  while (true) {
    if (fallDetected) {
      // ⚠️ Fall detected - triple blink
      for (int i = 0; i < 3; i++) {
        digitalWrite(indicating_led, HIGH);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        digitalWrite(indicating_led, LOW);
        vTaskDelay(200 / portTICK_PERIOD_MS);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS); // pause before repeating
    }
    else if (bluetoothEnabled) {
      // 📶 Bluetooth enabled - fast blink
      digitalWrite(indicating_led, HIGH);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(indicating_led, LOW);
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    else if (wifiConnected) {
      // ✅ WiFi connected - solid ON
      digitalWrite(indicating_led, HIGH);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    else if (!wifiConnected) {
      // ❌ WiFi not connected - slow blink
      digitalWrite(indicating_led, HIGH);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      digitalWrite(indicating_led, LOW);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if (leftHome) {
      // 🏠 Absolute left home - double blink
      for (int i = 0; i < 2; i++) {
        digitalWrite(indicating_led, HIGH);
        vTaskDelay(300 / portTICK_PERIOD_MS);
        digitalWrite(indicating_led, LOW);
        vTaskDelay(300 / portTICK_PERIOD_MS);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS); // pause before repeating
    }
    else {
      // Default: LED OFF
      digitalWrite(indicating_led, LOW);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}



// ********************************* setup ************
void setup() {
  Serial.begin(115200);
  pinMode(indicating_led, OUTPUT);
  digitalWrite(indicating_led, LOW);

  pinMode(buttonPin, INPUT_PULLUP);  // Button connected to GND
  prefs.begin("wifi_prefs", false);
  wifiMutex = xSemaphoreCreateMutex();

  xTaskCreate(LED_indicator, "LED_indicator", 1024, NULL, 1, NULL);

  xTaskCreate(Zone_monitor, "ZoneMonitor", 4096, NULL, 1, &zoneMonitorHandle);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  // SerialBT.begin("ESP32_BT_Device"); // Bluetooth device name
  // Serial.println("Bluetooth started!");
  WiFi.setAutoReconnect(false);

  Serial.println("Initializing SIM800L...");
  sim900.begin(57600, SERIAL_8N1, 27, 26);
  delay(2000);


  Wire.begin(32,33);
  Wire.setClock(400000);  // Fast I2C speed
  // pinMode(LED_PIN, OUTPUT);
  // digitalWrite(LED_PIN, LOW);  // LED off initially
  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);  // Set sleep = 0
  Wire.endTransmission(true);
  // Calibrate offsets
  calibrateAccelerometer();
  Serial.println("Fall Detection Initialized.");

  xTaskCreate(detect_falls,"detect_falls", 6144, NULL, 1 , NULL);
  size_t freeStack = uxTaskGetStackHighWaterMark(NULL);  // NULL gets the stack usage of the current task
  Serial.print("freeStack: ");
  Serial.println(freeStack);
}

void Zone_monitor(void *pvParameters) {
  while (1) {
    if (pauseMonitor) {
      // If paused, skip this cycle (but not in the middle of one)
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }/*
     if (fall_detected) {
      if (xSemaphoreTake(wifiMutex, portMAX_DELAY)) {
          connectToSavedWiFi();
          send_message_Render("notify","fall_detected");
          Serial.println("Falling msg sent.");
          xSemaphoreGive(wifiMutex);
          fall_detected = false;

        }
      // If paused, skip this cycle (but not in the middle of one)
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      continue;
    }
*/
    if (xSemaphoreTake(wifiMutex, portMAX_DELAY)) {

      
       // Re-check pause **after** taking mutex, in case Bluetooth handler set it in between
      if (pauseMonitor) {
        xSemaphoreGive(wifiMutex);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        continue;
      }


      Serial.println("Zone monitoring...");
      if (isUser_atHome){
        monitorZone();
      } else {
        trackUser();
      }
      xSemaphoreGive(wifiMutex);
    }

    if (isUser_atHome){
      vTaskDelay(3000 / portTICK_PERIOD_MS);  // monitorZone() function delay
    } else {
      vTaskDelay(1000*20 / portTICK_PERIOD_MS);  // trackUser() function delay
    }
    Serial.println("done........");
  }
}


void loop() {
  // Check for button press to enable Bluetooth
  if (!btInitialized && digitalRead(buttonPin) == LOW) {
    // Serial.println("Button pressed! Shutting down all other activity...");

    // // Suspend Zone_monitor task only
    // if (zoneMonitorHandle != NULL) {
    //   vTaskSuspend(zoneMonitorHandle);
    // }

    // if (WiFi.isConnected()) {
    //   WiFi.disconnect(true);
    //   WiFi.mode(WIFI_OFF);
    // }

    // gpsSerial.end();
    // gsm.end();

    SerialBT.begin("ESP32_BT_Device");
    btInitialized = true;
    bluetoothEnabled = true;
    Serial.println("Bluetooth initialized");
  }

  // Only access SerialBT if it's initialized
  if (btInitialized && SerialBT.available()) {
    String incoming = SerialBT.readStringUntil('\n'); // Read till newline for safer JSON parsing
    Serial.println("Received: " + incoming);
    SerialBT.println("Echo: " + incoming);
    handle_bluetooth_command(incoming);
  }

  // You can add other non-Bluetooth logic here too
  delay(10);  // Short delay to avoid CPU hogging
}

