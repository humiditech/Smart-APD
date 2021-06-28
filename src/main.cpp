#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include "FS.h"
#include "SPIFFS.h"
#include <SPI.h>
#include <time.h>
#include "credentials.h"

String version = "v1.0";
String site_width = "1023";
WiFiClient client;
AsyncWebServer server(80);

int log_time_unit = 15;
int time_reference = 60;
int const table_size = 72;
int index_ptr, timer_cnt, log_interval, log_count, max_temp, min_temp;
String webpage, time_now, log_time, lastcall, time_str, DataFile = "datalog.txt";
bool AScale, auto_smooth, AUpdate, log_delete_approved;
float temp, humi;

const long gmtOffset_sec = 25200; //GMT +7.00 : 7 *60 *60
const long daylightOffset_sec = 0;
const char *ntpServer = "pool.ntp.org";

// typedef signed short sint16;
typedef struct
{
  int lcnt;
  String ltime;
  float temp;
  float humi;
} record_type;

record_type sensor_data[table_size + 1];

#define DHT_PIN 18
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
unsigned long current_time = 0;
const int interval = 1000;

// Functions declaration
void StartSPIFFS();
int StartWiFi(const char *ssid, const char *password);
void StartTime();
void UpdateLocalTime();
String GetTime();
String calcDateTime(int epoch);
void reset_array();
String readDHTTemp();
String readDHTHumidity();

void setup()
{
  Serial.begin(115200);
  dht.begin();
  StartSPIFFS();
  StartWiFi(ssid, password);
  StartTime();
  Serial.println(F("WiFi Connected ...."));
  Serial.println(F("Webserver started ..."));
  Serial.println("Connect to this url : http://" + WiFi.localIP().toString() + "/");

  server.begin();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html"); });

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", readDHTTemp().c_str()); });

  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", readDHTHumidity().c_str()); });

  // index_ptr = 0;  // Array pointer
  // log_count = 0;  // Data logger counter
  // max_temp = 40;  // Maximum displayed temperature
  // min_temp = -10; // Minimum displayed temperature
  // lastcall = "temp_humi";
  // log_interval = log_time_unit * 10;
  // timer_cnt = log_interval + 1;
  // log_delete_approved = false;
  // reset_array();
}

void loop()
{
  // humi = dht.readHumidity();
  // temp = dht.readTemperature();

  // time_t now = time(nullptr);
  // time_now = String(ctime(&now)).substring(0, 24);

  // if (time_now != "Thu Jan 01 00:00:00 1970" && timer_cnt >= log_interval)
  // {
  //   timer_cnt = 0;
  //   log_count += 1;
  //   sensor_data[index_ptr].lcnt = log_count;
  //   sensor_data[index_ptr].temp = temp;
  //   sensor_data[index_ptr].humi = humi;
  //   sensor_data[index_ptr].ltime = calcDateTime(time(&now)); // Timestamp

  //   File datafile = SPIFFS.open("/" + DataFile, FILE_APPEND);
  //   time_t now = time(nullptr);
  //   if (datafile == true)
  //   {                                                                                                                                                                                   // if the file is available, write to it
  //     datafile.println(((log_count < 10) ? "0" : "") + String(log_count) + char(9) + String(temp / 10, 2) + char(9) + String(humi / 10, 2) + char(9) + calcDateTime(time(&now)) + "."); // TAB delimited
  //     Serial.println(((log_count < 10) ? "0" : "") + String(log_count) + " New Record Added");
  //   }
  //   datafile.close();
  //   index_ptr += 1;
  //   if (index_ptr > table_size)
  //   {
  //     index_ptr = table_size;
  //     for (int i = 0; i < table_size; i++) // If data table full, scroll to the left and add new reading to the end
  //     {
  //       sensor_data[i].lcnt = sensor_data[i + 1].lcnt;
  //       sensor_data[i].temp = sensor_data[i + 1].temp;
  //       sensor_data[i].humi = sensor_data[i + 1].humi;
  //       sensor_data[i].ltime = calcDateTime(time(&now));
  //     }
  //   }
  //   timer_cnt += 1;
  // }
  // Serial.print(F("Humidity: "));
  // Serial.print(humi);
  // Serial.print(F(" %   Temperature : "));
  // Serial.print(temp);
  // Serial.println(F(" C"));
  // delay(500);
}

void StartSPIFFS()
{
  boolean SPIFFS_Status;
  SPIFFS_Status = SPIFFS.begin();
  if (SPIFFS_Status == false)
  { // Most likely SPIFFS has not yet been formated, so do so
    SPIFFS.begin();
    File datafile = SPIFFS.open("/" + DataFile, FILE_READ);
    if (!datafile || !datafile.isDirectory())
    {
      Serial.println("SPIFFS failed to start..."); // If ESP32 nothing more can be done, so delete and then create another file
      SPIFFS.remove("/" + DataFile);               // The file is corrupted!!
      datafile.close();
    }
  }
  else
    Serial.println("SPIFFS Started successfully...");
}

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

void StartTime()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  UpdateLocalTime();
}

void UpdateLocalTime()
{
  struct tm timeInfo;
  while (!getLocalTime(&timeInfo))
  {
    Serial.println("Failed to obtain time");
  }
  Serial.println(&timeInfo, "%a %b %d %Y   %H:%M:%S");
  char output[50];
  strftime(output, 50, "%a %d-%b-%y  (%H:%M:%S)", &timeInfo);
  time_str = output;
}

String GetTime()
{
  struct tm timeInfo;
  while (!getLocalTime(&timeInfo))
  {
    Serial.println("Failed to obtain time - trying again");
  }
  Serial.println(&timeInfo, "%a %b %d %Y %H:%M:%S");
  char output[50];
  strftime(output, 50, "%d/%m/%y %H:%M:%S", &timeInfo);
  time_str = output;
  Serial.println(time_str);
  return time_str;
}

String calcDateTime(int epoch)
{ // From UNIX time becuase google charts can use UNIX time
  int seconds, minutes, hours, dayOfWeek, current_day, current_month, current_year;
  seconds = epoch;
  minutes = seconds / 60;  // calculate minutes
  seconds -= minutes * 60; // calculate seconds
  hours = minutes / 60;    // calculate hours
  minutes -= hours * 60;
  current_day = hours / 24; // calculate days
  hours -= current_day * 24;
  current_year = 1970; // Unix time starts in 1970
  dayOfWeek = 4;       // on a Thursday
  while (1)
  {
    bool leapYear = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (current_day >= daysInYear)
    {
      dayOfWeek += leapYear ? 2 : 1;
      current_day -= daysInYear;
      if (dayOfWeek >= 7)
        dayOfWeek -= 7;
      ++current_year;
    }
    else
    {
      dayOfWeek += current_day;
      dayOfWeek %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for (current_month = 0; current_month < 12; ++current_month)
      {
        uint8_t dim = daysInMonth[current_month];
        if (current_month == 1 && leapYear)
          ++dim; // add a day to February if a leap year
        if (current_day >= dim)
          current_day -= dim;
        else
          break;
      }
      break;
    }
  }
  current_month += 1; // Months are 0..11 and returned format is dd/mm/ccyy hh:mm:ss
  current_day += 1;
  String date_time = (current_day < 10 ? "0" + String(current_day) : String(current_day)) + "/" + (current_month < 10 ? "0" + String(current_month) : String(current_month)) + "/" + String(current_year).substring(2) + " ";
  date_time += ((hours < 10) ? "0" + String(hours) : String(hours)) + ":";
  date_time += ((minutes < 10) ? "0" + String(minutes) : String(minutes)) + ":";
  date_time += ((seconds < 10) ? "0" + String(seconds) : String(seconds));
  return date_time;
}

void reset_array()
{
  for (int i = 0; i <= table_size; i++)
  {
    sensor_data[i].lcnt = 0;
    sensor_data[i].temp = 0;
    sensor_data[i].humi = 0;
    sensor_data[i].ltime = "";
  }
}

String readDHTTemp()
{
  float temp = dht.readTemperature();
  if (isnan(temp))
  {
    Serial.println("Failed to read temperature");
    return "";
  }
  else
  {
    Serial.println(temp);
    return String(temp);
  }
}

String readDHTHumidity()
{
  float humid = dht.readHumidity();
  if (isnan(humid))
  {
    Serial.println("Failed to read humidity");
    return "";
  }
  else
  {
    Serial.println(humid);
    return String(humid);
  }
}
