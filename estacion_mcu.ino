#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_BMP085.h>
#include <DHTesp.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <math.h>

#define READ_INTERVAL 300e6

#define PIN_DHT RX
#define PIN_LDR A0
#define PIN_SCK D5
#define PIN_MISO D6
#define PIN_MOSI D7
#define PIN_SDCS D3
#define PIN_ERROR_LED TX
#define PIN_SLEEP D0

#define TIMEZONE -3

#define WIFI_SSID "wifi_wemos"
#define WIFI_PASSWORD "wifi_wemos"

#define WIFI_ALT_SSID "unlu"
#define WIFI_ALT_PASSWORD ""

#define SEND_URL "http://serv.cidetic.unlu.edu.ar/estacionunlu/tiempo_real/cargar_datos.php"
#define TIMESTAMP_URL "http://serv.cidetic.unlu.edu.ar/estacionunlu/tiempo_real/cargar_datos.php"

DHTesp dht;
RTC_DS1307 rtc;
Adafruit_BMP085 bmp;
float global_temp;
float global_hum;
bool connecting = false;

void setup_pinmodes () {
  pinMode (BUILTIN_LED, OUTPUT);
  pinMode (PIN_ERROR_LED, OUTPUT);
  pinMode (PIN_DHT, INPUT);
  pinMode (PIN_LDR, INPUT);
  pinMode (PIN_SCK, OUTPUT);
  pinMode (PIN_MISO, INPUT);
  pinMode (PIN_MOSI, OUTPUT);
  pinMode (PIN_SDCS, OUTPUT);
}

void setup() {
  setup_pinmodes ();
	global_temp = -100.0;
	global_hum = -100.0;
	Serial.begin (9600);

	Wire.begin (D2, D1);
	bmp.begin ();
	rtc.begin ();
	digitalWrite (BUILTIN_LED, LOW);
	dht.setup (PIN_DHT, DHTesp::DHT22);
  digitalWrite (PIN_ERROR_LED, LOW);

  connect_wifi ();
  sensors_main ();
  ESP.deepSleep (READ_INTERVAL);
}

void loop() {
}


