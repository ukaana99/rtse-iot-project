#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define DHTPIN 2
#define DHTTYPE DHT11

#define NTP_OFFSET   60 * 60 * 8  // GMT +8, In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "my.pool.ntp.org"

#define WIFI_SSID "Bbi_wifi_2.4G"
#define WIFI_PASSWORD "bibi1234"

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
unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

void setup(){
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
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
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("AUTHENTICATED");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  dht.begin();
}

void loop(){
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 * 60 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Update timestamp
    timeClient.update();
    // Read humidity
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.readTemperature(true);

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    // Compute heat index in Fahrenheit (the default)
    float hif = dht.computeHeatIndex(f, h);
    // Compute heat index in Celsius (isFahreheit = false)
    float hic = dht.computeHeatIndex(t, h, false);
    
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
    if (Firebase.RTDB.setJSON(&fbdo, dataPath, &json)){
      Serial.println("PASSED: " + fbdo.dataPath());
    } else {
      Serial.println("FAILED: " + fbdo.errorReason());
    }
  }
}
