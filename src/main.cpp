#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "DHT.h"
#include "FS.h"
#include "SPIFFS.h"
#include <SPI.h>
#include <time.h>
#include "credentials.h"

String version = "v1.0";
String site_width = "1023";
WiFiClient client;
AsyncWebServer server(80);
AsyncEventSource events("/events");

// DHT 22
#define DHT_PIN 34
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
// unsigned long current_time = 0;
// const int interval = 1000;

// Flow Sensor
#define FLOW_PIN 16

// Relay
#define KIPAS 25
#define UV 26
#define PELTIER 27

// Variables
float temp, humi;
long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMiliLitres;
unsigned int totalMiliLitres;

// Functions declaration
void StartSPIFFS();
int StartWiFi(const char *ssid, const char *password);
void IRAM_ATTR pulseCounter();
void readDHTSensors();
void readFlowSensors();
String processor(const String &var);

void setup()
{
  Serial.begin(115200);
  pinMode(KIPAS, OUTPUT);
  pinMode(UV, OUTPUT);
  pinMode(PELTIER, OUTPUT);
  pinMode(DHT_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  digitalWrite(KIPAS, HIGH);
  digitalWrite(UV, HIGH);
  digitalWrite(PELTIER, HIGH);

  pulseCount = 0;
  flowRate = 0.0;
  flowMiliLitres = 0;
  totalMiliLitres = 0;
  previousMillis = 0;

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  dht.begin();
  // StartSPIFFS();
  StartWiFi(ssid, password);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });
  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client)
                   {
                     if (client->lastId())
                     {
                       Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
                     }
                     client->send("Hello!", NULL, millis(), 10000);
                   });
  server.addHandler(&events);
  server.begin();
}

void loop()
{
  if ((millis() - previousMillis) > interval)
  {
    readDHTSensors();
    readFlowSensors();

    previousMillis = millis();
    flowMiliLitres = (flowRate / 60) * 1000;
    totalMiliLitres += flowMiliLitres;

    Serial.print(F("Flow rate: "));
    Serial.print(int(flowRate)); // Print the integer part of the variable
    Serial.print(F(" L/min"));
    Serial.print("\t"); // Print tab space

    // Print the cumulative total of litres flowed since starting
    Serial.print(F("Output Air Quantity: "));
    Serial.print(totalMiliLitres);
    Serial.print(F(" mL / "));
    Serial.print(totalMiliLitres / 1000);
    Serial.println(F(" L"));

    events.send("ping", NULL, millis());
    events.send(String(temp).c_str(), "temperature", millis());
    events.send(String(humi).c_str(), "humidity", millis());
    events.send(String(flowRate).c_str(), "flow_rate", millis());
  }
}

// void StartSPIFFS()
// {
//   boolean SPIFFS_Status;
//   SPIFFS_Status = SPIFFS.begin();
//   if (SPIFFS_Status == false)
//   { // Most likely SPIFFS has not yet been formated, so do so
//     SPIFFS.begin();
//     File datafile = SPIFFS.open("/" + DataFile, FILE_READ);
//     if (!datafile || !datafile.isDirectory())
//     {
//       Serial.println("SPIFFS failed to start..."); // If ESP32 nothing more can be done, so delete and then create another file
//       SPIFFS.remove("/" + DataFile);               // The file is corrupted!!
//       datafile.close();
//     }
//   }
//   else
//     Serial.println("SPIFFS Started successfully...");
// }

int StartWiFi(const char *ssid, const char *password)
{
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to : "));
  Serial.println(String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (connAttempts > 20)
    {
      Serial.println("\nFailed to connect to a Wi-Fi network.");
      return -5;
    }
    connAttempts++;
  }
  Serial.print(F("Wi-Fi Connected at : "));
  Serial.println(WiFi.localIP());
  return 1;
}

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void readDHTSensors()
{
  humi = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(humi) || isnan(temp))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(humi);
  Serial.print(F(" %   Temperature : "));
  Serial.print(temp);
  Serial.println(F(" °C"));
}

void readFlowSensors()
{
  pulse1Sec = pulseCount;
  pulseCount = 0;
  flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
}

String processor(const String &var)
{
  readDHTSensors();
  readFlowSensors();
  if (var == "TEMPERATURE")
  {
    return String(temp);
  }
  else if (var == "HUMIDITY")
  {
    return String(humi);
  }
  else if (var == "FLOWRATE")
  {
    return String(flowRate);
  }
}
