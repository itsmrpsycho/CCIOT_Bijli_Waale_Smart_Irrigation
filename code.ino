#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <string.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "ThingSpeak.h"
#include "HTTPClient.h"

#define mois_PIN 36
#define relay_PIN 26
#define flow_PIN 27
#define dht_PIN 33

DHT dht(dht_PIN, DHT22);

// Parameters
bool GivingWater = false;
long waitTime = 20000; // in milli seconds (20 sec)
long WaterQuanML = 96000; // quantity of water to dispense in mL
long loopDelay = (15*60*1000);

// Thingspeak
unsigned long myChannelNumber = 2034527;
const char* myWriteAPIKey = "X97SUCBEW0VDUAEY";
int httpCode;
// Flow sensor
int interval = 1000;
long currentMillis = 0;
long previousMillis = 0;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
long flowMilliLitres;
long totalMilliLitres;
float calibrationFactor = 4.5;
// DHT
float temp;
float humid;
long amount;
// WIFI
const char* ssid = "esw-m19@iiith";
const char* password = "e5W-eMai@3!20hOct";
WiFiClient client;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // For wifi connection
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(1000);
  ThingSpeak.begin(client);

  pinMode(dht_PIN, INPUT); // dht
  pinMode(relay_PIN, OUTPUT); // relay
  pinMode(flow_PIN, INPUT_PULLUP); // flowflow_PIN
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
  attachInterrupt(digitalPinToInterrupt(flow_PIN), pulseCounter, FALLING);
  delay(1000);
  dht.begin();
}

void loop() {

  int value = analogRead(mois_PIN);  // read the analog value from flow_PIN
  int ll = 750; // lower limit
  int ul = 2800; // upper limit
  int range = ul - ll;
  value = value - ll;
  float final = ((float)value / range) * 100;
  final = 100 - final;

  if (GivingWater == 0) {
    temp = dht.readTemperature();  // Gets the values of the temperature
    humid = dht.readHumidity();
    Serial.print("Moisture value: ");
    Serial.println(final);
    ThingSpeak.setField(6, final);
    Serial.print("temp value: ");
    Serial.println(temp);
    ThingSpeak.setField(1, temp);
    Serial.print("humidity value: ");
    Serial.println(humid);
    ThingSpeak.setField(2, humid);
    httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (httpCode != 200) {
      Serial.println("Problem writing to channel. HTTP error code " + String(httpCode));
    }
  }

  if (final <= 40 || GivingWater == 1) {
    digitalWrite(relay_PIN, HIGH);
    currentMillis = millis();

    while (currentMillis - previousMillis > interval && totalMilliLitres < 3000) {
      GivingWater = 1;
      pulse1Sec = pulseCount;
      pulseCount = 0;
      flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
      previousMillis = millis();

      flowMilliLitres = (flowRate / 60) * 1000;
      totalMilliLitres += flowMilliLitres;

      // Print the flow rate for this second in litres / minute
      // Serial.print("Flow rate: ");
      // Serial.print(int(flowRate));  // Print the integer part of the variable
      // Serial.print("L/min");
      // Serial.print("\t");  // Print tab space

      // // Print the cumulative total of litres flowed since starting
      // Serial.print("Output Liquid Quantity: ");
      // Serial.print(totalMilliLitres);
      // Serial.print("mL / ");
      // Serial.print(totalMilliLitres / 1000);
      // Serial.println("L");

      if (totalMilliLitres >= 1000) {
        Serial.print("Completed");
        GivingWater = 0;
        totalMilliLitres = 0;
        digitalWrite(relay_PIN, LOW);
        ThingSpeak.setField(3, 1);  // 1 is value of spike
        break;
      }
      else if (millis()-currentMillis >= waitTime) {
        Serial.print("No flow");
        GivingWater = 0;
        totalMilliLitres = 0;
        digitalWrite(relay_PIN, LOW);
        ThingSpeak.setField(3, -1);  // -1 is value of spike (not completed)
        break;
      }
    }
  }

  if (GivingWater == 0) {
    delay(loopDelay);
  }
}