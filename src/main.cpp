#include <Arduino.h>
#include <WiFi.h>
#include "DHT.h"
#include "FS.h"
#include "SPIFFS.h"
#include <SPI.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <WebServer.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <WiFiClientSecure.h>
#include "credentials.h"

String version = "v1.0";
String site_width = "1023";
WiFiClient client;
// WebServer server(80);
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
// AsyncWebSocket ws("/ws");

// DHT 22
#define DHT_PIN 34
#define DHT_TYPE DHT11

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
const int interval = 1000;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMiliLitres;
unsigned int totalMiliLitres;
String uvstatus, fanstatus, thermocoolerstatus;
String flowThres = "10";
String humidThres = "80";
String tempThres = "30";
String jsonString;
const char *param1 = "tempThres";
const char *param2 = "humidThres";
const char *param3 = "flowThres";

const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML>
<html>

<head>
    <title>Smart APD </title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css'
        integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'>
    <link rel='icon' href='data:,'>
    <script src='https://code.highcharts.com/highcharts.js'></script>
    <style>
        html {
            font-family: Arial, sans-serif;
            display: inline-block;
            text-align: center;
        }

        p {
            font-size: 1.2rem;
        }

        body {
            min-width: 310px;
            max-width: 800px;
            height: 400px;
            margin: 0 auto;
        }

        .topnav {
            overflow: hidden;
            background-color: #50B8B4;
            color: white;
            font-size: 1rem;
        }

        .content {
            padding: 20px;
        }

        .card {
            background-color: white;
            box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
            border-radius: 5px;
        }

        .cards {
            max-width: 800px;
            margin: 10px auto;
            display: grid;
            grid-gap: 2rem;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
        }

        .reading {
            font-size: 1.4rem;
        }

        @media screen and (max-width : 320px) {
            .cards {
                max-width: 100px;
                margin: 10px auto;
                grid-template-columns: repeat(3, 1fr);
            }
        }
    </style>
</head>

<body>
    <div class='topnav'>
        <h1>Smart APD Webserver</h1>
    </div>
    <div class='content'>
        <div class='cards'>
            <div class='card'>
                <p><i class='fas fa-thermometer-half' style='color:#059e8a;'></i> TEMPERATURE</p>
                <p><span class='reading'><span id='temp'>%TEMPERATURE%</span> &deg;C</span></p>
            </div>
            <div class='card'>
                <p><i class='fas fa-tint' style='color:#00add6;'></i> HUMIDITY</p>
                <p><span class='reading'><span id='hum'>%HUMIDITY%</span> &percnt;</span></p>
            </div>
            <div class='card'>
                <p><i class='fas fa-angle-double-down' style='color:#e1e437;'></i> FLOW RATE</p>
                <p><span class='reading'><span id='flow'>%FLOWRATE%</span> L/min</span></p>
            </div>
        </div>

        <div class='cards'>
            <div class='card'>
                <p>UV STATUS</p>
                <p class='reading'><span id='uvstats'>%UVSTATUS%</span></p>
            </div>
            <div class='card'>
                <p>FAN STATUS</p>
                <p class='reading'><span id='fanstats'>%FANSTATUS%</span></p>
            </div>
            <div class='card'>
                <p>COOLER STATUS</p>
                <p class='reading'><span id='tcstats'>%THERMOCOOLERSTATUS%</span></p>
            </div>
        </div>

        <form action='/get'>
            Temperature Threshold : <input type='text' name='tempThres'>
            <input type='submit' value='Submit'>
        </form><br>
        <form action='/get'>
            Humidity Threshold : <input type='text' name='humidThres'>
            <input type='submit' value='Submit'>
        </form><br>
        <form action='/get'>
            Flow Threshold : <input type='text' name='flowThres'>
            <input type='submit' value='Submit'>
        </form><br>
    </div>

    <script>
        var gateway = `ws://${window.location.hostname}:81/`;
        var websocket;
        window.addEventListener('load', onLoad);

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = function (event) {
                processCommand(event);
            };
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function processCommand(event) {
            var obj = JSON.parse(event.data);
            document.getElementById('temp').innerHTML = obj.temp;
            document.getElementById('hum').innerHTML = obj.hum;
            document.getElementById('flow').innerHTML = obj.flow;
            document.getElementById('uvstats').innerHTML = obj.uvstats;
            document.getElementById('fanstats').innerHTML = obj.fanstats;
            document.getElementById('tcstats').innerHTML = obj.tcstats;

            console.log(obj.temp);
            console.log(obj.hum);
            console.log(obj.flow);
            console.log(obj.uvstats);
            console.log(obj.fanstats);
            console.log(obj.tcstats);
        }

        function onLoad(event) {
            initWebSocket();
        }
    </script>
</body>

</html>
)rawliteral";
// Functions declaration
void StartSPIFFS();
int StartWiFi(const char *ssid, const char *password);
void IRAM_ATTR pulseCounter();
void readDHTSensors();
void readFlowSensors();
// String processor(const String &var);
// void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
// void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
// void initWebSocket();
void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length);
void notFound(AsyncWebServerRequest *request);
void update_webpage();

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
  // server.on("/", []()
  //           { server.send(200, "text\html", web); });
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String inputParam, inputMessage;
              if (request->hasParam(param1))
              {
                inputParam = param1;
                inputMessage = request->getParam(param1)->value();
                tempThres = request->getParam(param1)->value();
              }
              else if (request->hasParam(param2))
              {
                inputParam = param2;
                inputMessage = request->getParam(param2)->value();
                humidThres = request->getParam(param2)->value();
              }
              else if (request->hasParam(param3))
              {
                inputParam = param3;
                inputMessage = request->getParam(param3)->value();
                flowThres = request->getParam(param3)->value();
              }
              request->send(200, "text/html", "HTTP GET request sent to your ESP on input field (" + inputParam + ") with value: " + inputMessage + "<br><a href=\"/\">Return to Home Page</a>");
            });
  server.onNotFound(notFound);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop()
{
  // server.handleClient();
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
    digitalWrite(KIPAS, LOW);
    digitalWrite(UV, LOW);
    digitalWrite(PELTIER, LOW);
    uvstatus = "ON";
    fanstatus = "ON";
    thermocoolerstatus = "ON";
  }
  else
  {
    digitalWrite(KIPAS, HIGH);
    digitalWrite(UV, HIGH);
    digitalWrite(PELTIER, HIGH);
    uvstatus = "OFF";
    fanstatus = "OFF";
    thermocoolerstatus = "OFF";
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
