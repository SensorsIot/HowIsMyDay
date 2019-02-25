/*
 Copyright 2018 Andreas Spiess

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <credentials.h>

/* credentiald.h file contains these lines:
#define mySSID "...."
#define   myPASSWORD "....."
#define OPENWEATHERKEY "....."
*/  

// TrigBoard definitions
#define NEOPIXELPIN 5
#define PIXELS 8
#define DONEPIN 15
#define EXT_WAKE 16

#define MAXINTENSITY 255

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);

const char* ssid = mySSID;
const char* password =  myPASSWORD;

const String endpoint = "http://api.openweathermap.org/data/2.5/forecast?q=Lausen,CH&units=metric&APPID=";
const String key = OPENWEATHERKEY;

int timeUTC[40];
float temp[40];
float rain[40];
float cloud[40];
float snow[40];

int morningNumber = 99;
int eveningNumber = 99;

struct {
  uint32_t crc32;
  byte data[508];
} rtcData;

struct pixelColorStruc {
  int red;
  int green;
  int blue;
} ;
pixelColorStruc pixelColors[PIXELS];


int morningHour = 6;
int eveningHour = 15;


String getWeatherData() {
  String payload = "";
  HTTPClient http;

  http.begin(endpoint + key); //Specify the URL
  int httpCode = http.GET();  //Make the request

  if (httpCode == 200) { //Check for the returning code

    payload = http.getString();
  }

  else {
    payload = "E";

  }
  http.end(); //Free the resources
 // Serial.println(payload);
  // only first 8 elements of array. Delete rest
  int i = 0;
  int last = payload.indexOf(",{""dt", 0);
  while (i < 8) {
    last = payload.indexOf(",{", last + 1);
    i = i + 1;
  }

  return payload.substring(0, last) + "]}";
}

void parsePaket(int number, JsonObject& liste) {

  temp[number] = liste["main"]["temp"]; // 20.31
  Serial.print(" Temp ");
  Serial.print(temp[number]);

  rain[number] = liste["rain"]["3h"]; // 0
  Serial.print(" Rain ");
  Serial.print(rain[number]);

  snow[number] = liste["snow"]["3h"]; // 0
  Serial.print(" Snow ");
  Serial.print(snow[number]);

  const char* list_dt_txt = liste["dt_txt"]; // "2018-10-17 15:00:00"
  timeUTC[number] = (list_dt_txt[11] - '0') * 10 + list_dt_txt[12] - '0';
  Serial.print(" Time ");
  Serial.println(timeUTC[number]);
}

void decodeWeather(String JSONline) {

  const size_t bufferSize = 4 * JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(4) + 8 * JSON_OBJECT_SIZE(1) + 4 * JSON_OBJECT_SIZE(2) + 5 * JSON_OBJECT_SIZE(4) + 4 * JSON_OBJECT_SIZE(7) + 4 * JSON_OBJECT_SIZE(8) + 1270;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  const char* json = JSONline.c_str();
  JsonObject& root = jsonBuffer.parseObject(json);
  JsonArray& list = root["list"];
  for (int i = 0; i < 8; i++) {
    parsePaket(i, list[i]);
  }
}

void applyRules(int dayTime, int index) {


  // Temp
  Serial.print("Temp ");
  Serial.println(temp[index]);
  if (temp[index] > 21.0) {
    pixelColors[dayTime * 4].red = MAXINTENSITY;
  } else {
    if (temp[index] > 10.0) {
      pixelColors[dayTime * 4].red = MAXINTENSITY / 2;
      pixelColors[dayTime * 4].blue = MAXINTENSITY / 3;
    } else {
      pixelColors[dayTime * 4].blue = MAXINTENSITY;
    }
  }

  // Rain
  Serial.print(" Rain ");
  Serial.println(rain[index]);
  if (rain[index] > 7.0) {
    pixelColors[dayTime * 4 + 1].red = MAXINTENSITY;
  } else {
    if (rain[index] > 2.0) {
      pixelColors[dayTime * 4 + 1].red = MAXINTENSITY / 2;
      pixelColors[dayTime * 4 + 1].blue = MAXINTENSITY / 3;
    } else {
      pixelColors[dayTime * 4 + 1].blue = MAXINTENSITY;
    }
  }
  if (rain[index] < 0.1)  pixelColors[dayTime * 4 + 1].blue = 0;


  // Snow
  Serial.print(" Snow ");
  Serial.println(snow[index]);
  if (snow[index] > 7.0) {
    pixelColors[dayTime * 4 + 2].red = MAXINTENSITY;
  } else {
    if (snow[index] > 2.0) {
      pixelColors[dayTime * 4 + 2].red = MAXINTENSITY / 2;
      pixelColors[dayTime * 4 + 2].blue = MAXINTENSITY / 2;
    } else {
      pixelColors[dayTime * 4 + 2].blue = MAXINTENSITY;
    }
  }
  if (snow[index] < 0.1)  pixelColors[dayTime * 4 + 2].blue = 0;
}

int mapSpecial(float in, float fromLow, float fromHigh, int toLow, int toHigh) {
  int out = (int)map(in, fromLow, fromHigh, toLow, toHigh);
  if (out > toHigh) out = toHigh;
  if (out < toLow) out = toLow;
  return out;
}


//-------------- SETUP ---------------------------------

void setup() {
  String weather;
  int extWake = 0;

  Serial.begin(115200);
  Serial.println();
  Serial.println();

#ifdef DONEPIN
  pinMode(DONEPIN, OUTPUT);
#endif


#ifdef EXT_WAKE
  pinMode(EXT_WAKE, INPUT);
  Serial.print("Ext_Wake ");
  extWake = digitalRead(EXT_WAKE);
  Serial.println(extWake);
#endif

  if (extWake == 0) {
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'

    float battVoltage = analogRead(A0) / 232.6;
    battVoltage = 3.4;
    Serial.print("BattVoltage ");
    Serial.println(battVoltage);

    if (battVoltage < 3.6) {
      Serial.println("BattVoltage low");
      for (int i = 0; i < 8 ; i++) {
        strip.setPixelColor(i, MAXINTENSITY, 0, 0);
        strip.show();
        delay(50);
        strip.setPixelColor(i, 0, 0, 0);
        strip.show();
        delay(100);
      }
    }


    for (int i = 0; i < PIXELS; i++) {
      pixelColors[i].green = 0;
      pixelColors[i].red = 0;
      pixelColors[i].blue = 0;
    }

    WiFi.begin(ssid, password);
    bool indicator = false;

    while (WiFi.status() != WL_CONNECTED) {
      strip.setPixelColor(7, 0, indicator * MAXINTENSITY, 0);
      strip.show();
      indicator = !indicator;
      delay(1000);
      Serial.println("Connecting to WiFi..");
    }

    Serial.println("Connected to the WiFi network");
    if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
      do {
        weather = getWeatherData();
        Serial.println(weather);

      }  while (weather[0] == 'E');
      decodeWeather(weather);

      for (int i = 0; i < 8 ; i++) {   
          if (timeUTC[i] >= morningHour && timeUTC[i] < morningHour + 3) morningNumber = i;
          if (timeUTC[i] >= eveningHour && timeUTC[i] < eveningHour + 3) eveningNumber = i;
      }

      Serial.print("morningNumber ");
      Serial.println(morningNumber);
      Serial.print("eveningNumber ");
      Serial.println(eveningNumber);

      applyRules(0, morningNumber);
      applyRules(1, eveningNumber);

      for (int i = 0; i < PIXELS; i++) {
        strip.setPixelColor(i, pixelColors[i].red, pixelColors[i].green, pixelColors[i].blue);
      }
      strip.show();
    }
    delay(5000);
    for (int i = 0; i < PIXELS; i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
    strip.show();
  } else {
    delay(1000);
  }
  Serial.println("Switching off");
  delay(100);
#ifdef DONEPIN
  digitalWrite(DONEPIN, HIGH);
#else
  ESP.deepSleep(1000000);
#endif
}

void loop() {
  yield();
}
