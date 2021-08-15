#include <Arduino.h>
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include <SPI.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include "credentials.h"
#include "LiquidCrystal_I2C.h"
#include "Wire.h"

String version = "v1.0";
String site_width = "1023";
WiFiClient client;
LiquidCrystal_I2C lcd(0x27,20,4);
char buf[20];

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// DHT 22
#define DHT_PIN 23
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

// Flow Sensor
#define FLOW_PIN 16

// Relay
#define KIPAS 25
#define UV 26
#define PELTIER 27

// Variables
float temp, humid;
long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long lcdprevmillis = 0;
const int interval = 1000;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMiliLitres;
unsigned int totalMiliLitres;
String uvstatus, fanstatus, thermocoolerstatus;
String flowThres = "";
String humidThres = "";
String tempThres = "";
String jsonString;
String message = "";
const char *param1 = "tempThres";
const char *param2 = "humidThres";
const char *param3 = "flowThres";
uint8_t cnt = 0;

// Functions declaration
void StartSPIFFS();
bool initWiFiAP(const char *ssid, const char *password);
int initWiFiSTA(const char *ssid, const char *password);
void IRAM_ATTR pulseCounter();
void readDHTSensors();
void readFlowSensors();
String processor(const String &var);
void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length);
void notFound(AsyncWebServerRequest *request);
void update_webpage();
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void lcdDisplaySensor(uint8_t sel);

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
  dht.begin();

  pulseCount = 0;
  flowRate = 0.0;
  flowMiliLitres = 0;
  totalMiliLitres = 0;
  previousMillis = 0;
  Wire.setClock(10000);
  lcd.begin();
  lcd.backlight();

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // StartSPIFFS();
  // initWiFiSTA(ssid, password);
  initWiFiAP(ssid, password);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String inputParam, inputMessage;
              if (request->hasParam(param1))
              {
                inputParam = param1;
                inputMessage = request->getParam(param1)->value();
                tempThres = request->getParam(param1)->value();
                writeFile(SPIFFS, "/tempThres.txt", tempThres.c_str());
              }
              else if (request->hasParam(param2))
              {
                inputParam = param2;
                inputMessage = request->getParam(param2)->value();
                humidThres = request->getParam(param2)->value();
                writeFile(SPIFFS, "/humidThres.txt", humidThres.c_str());
              }
              else if (request->hasParam(param3))
              {
                inputParam = param3;
                inputMessage = request->getParam(param3)->value();
                flowThres = request->getParam(param3)->value();
                writeFile(SPIFFS, "/flowThres.txt", flowThres.c_str());
              }
              Serial.println(inputMessage);
              request->send(200, "text/text", inputMessage);
            });
  server.onNotFound(notFound);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  lcd.setCursor(8,1);sprintf(buf,"SMART");lcd.print(buf);
  lcd.setCursor(9,2);sprintf(buf,"APD");lcd.print(buf);
  delay(1000);
  lcd.clear();
}

void loop()
{
  // server.handleClient();
  tempThres = readFile(SPIFFS, "/tempThres.txt");
  humidThres = readFile(SPIFFS, "/humidThres.txt");
  flowThres = readFile(SPIFFS, "/flowThres.txt");
  webSocket.loop();
  if ((millis() - previousMillis) > interval)
  {
    readDHTSensors();
    readFlowSensors();
    update_webpage();
    previousMillis = millis();
    flowMiliLitres = (flowRate / 60) * 1000;
    totalMiliLitres += flowMiliLitres;

    Serial.println(tempThres);
    Serial.println(humidThres);
    Serial.println(flowThres);

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
  }

  if ((int(flowRate) < flowThres.toInt()) || (temp > tempThres.toInt()) || (humid < humidThres.toInt()))
  {
    digitalWrite(KIPAS, HIGH);
    digitalWrite(UV, HIGH);
    digitalWrite(PELTIER, LOW);
    uvstatus = "ON";
    fanstatus = "ON";
    thermocoolerstatus = "ON";
  }
  else
  {
    digitalWrite(KIPAS, LOW);
    digitalWrite(UV, LOW);
    digitalWrite(PELTIER, HIGH);
    uvstatus = "OFF";
    fanstatus = "OFF";
    thermocoolerstatus = "OFF";
  }

  if((millis() - lcdprevmillis) >= 3500){
    lcd.clear();
    lcdprevmillis = millis();
    lcdDisplaySensor(cnt);
    cnt++;
    if(cnt > 1)cnt=0;
  }
}

void lcdDisplaySensor(uint8_t sel)
{
  char msg[20];

  if(sel == 0){
  char degree = (char)223;
  message = "SUHU: " + String(temp) + String(degree) +"C";
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,0);sprintf(buf,"%s",msg);lcd.print(buf);

  message = "KELEMBAPAN: " + String(temp) + "%";
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,1);sprintf(buf,"%s",msg);lcd.print(buf);

  message = "FLOW: " + String(flowRate) + "L/m";
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,2);sprintf(buf,"%s",msg);lcd.print(buf);
  }

  else if(sel == 1){
  message = "KIPAS: " + fanstatus;
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,0);sprintf(buf,"%s",msg);lcd.print(buf);

  message = "LAMPU UV: " + uvstatus;
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,1);sprintf(buf,"%s",msg);lcd.print(buf);

  message = "PELTIER: " + thermocoolerstatus;
  message.toCharArray(msg,message.length()+1);
  lcd.setCursor(0,2);sprintf(buf,"%s",msg);lcd.print(buf);
  }

}

bool initWiFiAP(const char *ssid, const char *password)
{
  WiFi.softAP(ssid, password);
  delay(2000);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
}
int intiWiFiSTA(const char *ssid, const char *password)
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
  humid = dht.readHumidity();
  temp = dht.readTemperature();

  if (isnan(humid) || isnan(temp))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  Serial.print(F("Humidity: "));
  Serial.print(humid);
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

void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.print(F("WS Type: "));
    Serial.print(type);
    Serial.println(F(" DISCONNECTED"));
    break;
  case WStype_CONNECTED:
    Serial.print(F("WS Type: "));
    Serial.print(type);
    Serial.println(F(" CONNECTED"));
    break;
  case WStype_TEXT:
    break;
  }
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void update_webpage()
{
  StaticJsonDocument<255> doc;
  JsonObject object = doc.to<JsonObject>();
  object["temp"] = temp;
  object["hum"] = humid;
  object["flow"] = int(flowRate);
  object["uvstats"] = uvstatus;
  object["fanstats"] = fanstatus;
  object["tcstats"] = thermocoolerstatus;

  serializeJson(doc, jsonString);
  Serial.println(jsonString);
  webSocket.broadcastTXT(jsonString);
  jsonString = "";
}

String readFile(fs::FS &fs, const char *path)
{
  // Serial.printf("Readinf file: %s\r\n", path);
  File file = fs.open(path, "r");

  if (!file || file.isDirectory())
  {
    Serial.println(F("- empty file or failed to open file"));
    return String();
  }
  // Serial.println(F("- read from file:"));
  String fileContent;
  while (file.available())
  {
    fileContent += String((char)file.read());
  }
  // Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file : %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file)
  {
    Serial.println(F("- failed to open file for writing"));
    return;
  }
  if (file.print(message))
  {
    Serial.println(F("- file written"));
  }
  else
  {
    Serial.println(F("- write failed"));
  }
}

String processor(const String &var)
{
  if (var == "tempThres")
  {
    return readFile(SPIFFS, "/tempThres.txt");
  }
  else if (var == "humidThres")
  {
    return readFile(SPIFFS, "/humidThres.txt");
  }
  else if (var == "flowThres")
  {
    return readFile(SPIFFS, "/flowThres.txt");
  }
  return String();
}