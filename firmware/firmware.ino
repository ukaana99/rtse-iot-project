////////////////////////////////////////
// PREPROCESSORS
////////////////////////////////////////
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <CoopTask.h>
#include <CoopSemaphore.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

////////////////////////////////////////
// DEFINITIONS
////////////////////////////////////////

#if defined(ARDUINO_attiny)
#define LED_BUILTIN 1
#endif

#define DHTPIN 2
#define DHTTYPE DHT11

#define NTP_OFFSET   60 * 60 * 8  // GMT +8, In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "my.pool.ntp.org"

#define WIFI_SSID "lag"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyCWMZcTnr_paEB3YLSK3H59M8qiHh1EDfc"
#define DATABASE_URL "rtse-project-default-rtdb.asia-southeast1.firebasedatabase.app"

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

// Initialize NTPClient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Define Firebase data object
FirebaseAuth auth;
FirebaseData fbdo;
FirebaseConfig config;

char buffer[65];
bool signupOK = false;
unsigned long sendDataPrevMillis = millis();
unsigned long readSensorPrevMillis = millis();
float h = 0, t = 0, f = 0, hif = 0, hic = 0;

CoopSemaphore taskSema(1, 1);
int taskToken = 1;

////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////
// Task 1: Read and process sensor data
void loop1() {
    for (;;) // explicitly run forever without returning
    {
        taskSema.wait();
        Serial.println(F("TASK 1: Wait for Semaphore!"));
        if (1 != taskToken)
        {
          Serial.println(F("TASK 1: Wrong turn, signal Semaphore!"));
          taskSema.post();
          yield();
          continue;
        }
        
        delay(250);
        // Read humidity
        h = dht.readHumidity();
        // Read temperature as Celsius (the default)
        t = dht.readTemperature();
        // Read temperature as Fahrenheit (isFahrenheit = true)
        f = dht.readTemperature(true);
    
        // Check if any reads failed and exit early (to try again).
        if (isnan(h) || isnan(t) || isnan(f)) {
          Serial.println(F("Failed to read from DHT sensor!"));
          return;
        }
    
        // Compute heat index in Fahrenheit (the default)
        hif = dht.computeHeatIndex(f, h);
        // Compute heat index in Celsius (isFahreheit = false)
        hic = dht.computeHeatIndex(t, h, false);
        delay(1000 * 1);

        taskToken = 2;
        taskSema.post();
        Serial.println(F("TASK 1: Finished processing data, signal Semaphore"));
    }
}

// Task 2: Send data to Firebase
void loop2() {
    for (;;) // explicitly run forever without returning
    {
        taskSema.wait();
        Serial.println(F("TASK 2: Wait for Semaphore!"));
        if (2 != taskToken)
        {
            taskSema.post();
            Serial.println(F("TASK 2: Wrong turn, signal Semaphore"));
            yield();
            continue;
        }
        
        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
        if (Firebase.ready() && signupOK){
          // Update timestamp
          timeClient.update();
      
          // Write sensor data on the database path dht11/{timestamp}
          String timestamp = ultoa(timeClient.getEpochTime(), buffer, 10);
          String sensorPath = "dht11/";
          String dataPath = sensorPath + timestamp;
          FirebaseJson json;
          json.add("humidity", h);
          json.add("temperature_c", t);
          json.add("temperature_f", f);
          json.add("heat_index_c", hic);
          json.add("heat_index_f", hif);
          if (Firebase.RTDB.setJSON(&fbdo, dataPath, &json)) {
//            Serial.println("PASSED: " + fbdo.dataPath());
          } else {
//            Serial.println("FAILED: " + fbdo.errorReason());
          }        
        }
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000 * 1);
        
        taskToken = 1;
        taskSema.post();
        Serial.println(F("TASK 2: Finished sending data, signal Semaphore"));
    }
}

BasicCoopTask task1("t1", loop1);
BasicCoopTask task2("t2", loop2);

////////////////////////////////////////
// SETUP
////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Authenticate to Firebase
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("AUTHENTICATED");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  dht.begin();

  
  // Add "loop1" and "loop2" to CoopTask scheduling.
  // "loop" is always started by default, and is not under the control of CoopTask. 
  task1.scheduleTask();
  task2.scheduleTask();
}

////////////////////////////////////////
// RUN
////////////////////////////////////////
void loop() {
  // loops forever by default
  runCoopTasks();
}
