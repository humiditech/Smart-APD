#include <Arduino.h>
#include <WiFi.h>
#include <ESP32WebServer.h>
#include <DHT.h>
#include "FS.h"
#include "SPIFFS.h"
#include <SPI.h>
#include <time.h>
#include "credentials.h"

String version = "v1.0";
String site_width = "1023";
WiFiClient client;

int log_time_unit = 15;
int time_reference = 60;
int const table_size = 72;
int index_ptr, timer_ptr, log_interval, log_count, max_temp, min_temp;
String webpage, time_now, log_time, lastcall, time_str, DataFile = "datalog.txt";
bool AScale, auto_smooth, AUpdate, log_delete_approved;
float temp, humi;

typedef signed short sint16;
typedef struct
{
  int lcnt;
  String ltime;
  sint16 temp;
  sint16 humi;
} record_type;

record_type sensor_data[table_size + 1];

#define DHT_PIN 18
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
unsigned long current_time = 0;
const int interval = 1000;

void setup()
{
  Serial.begin(115200);
  dht.begin();
}

void loop()
{
  if ((millis() - current_time) > interval)
  {
    humi = dht.readHumidity();
    temp = dht.readTemperature();
    current_time = millis();
    if (isnan(humi) || isnan(temp))
    {
      Serial.println(F("Failed to read from DHT Sensor"));
      return;
    }
    Serial.print(F("Humidity: "));
    Serial.print(humi);
    Serial.print(F("%  Temperature: "));
    Serial.print(temp);
    Serial.println(F("°C "));
  }
}