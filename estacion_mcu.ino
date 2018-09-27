#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_BMP085.h>
#include <DHTesp.h>
#include <Wire.h>
#include <math.h>

#define PIN_DHT D3
#define PIN_LDR A0
#define SEND_URL "http://serv.cidetic.unlu.edu.ar/estacionunlu/tiempo_real/cargar_datos.php"

DHTesp dht;
Adafruit_BMP085 bmp;
float global_temp;
float global_hum;

void connect_wifi () {
  WiFi.begin ("unlu", "");
  
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay (500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}


void read_sensors (float *temp, float *temp_bmp, float *presion, float *hum, float *lum_perc) {
  digitalWrite (BUILTIN_LED, LOW);
  float input;

  input = dht.getHumidity ();
  *hum = (isnan (input))? (*hum): input;
  input = dht.getTemperature ();
  *temp = (isnan (input))? (*temp): input;
  int lum = analogRead (PIN_LDR);
  *lum_perc = ((float) lum)/10.23;
  *temp_bmp = bmp.readTemperature();
  *presion = 0.01*bmp.readPressure();
  
   digitalWrite (BUILTIN_LED, HIGH);
}

void send_to_server (float temp_dht, float temp_bmp, float presion, float hum, float lum_perc) {
  char post_data[2048];
  HTTPClient http;
  http.begin (SEND_URL);
  http.addHeader ("Content-Type", "application/x-www-form-urlencoded");
  va_list ap;
  sprintf (post_data, "temperatura_dht=%d.%d&temperatura_bmp=%d.%d&presion=%d.%d&humedad=%d.%d&luminosidad=%d.%d",
    ((int)temp_dht),((int)(temp_dht*100.0))%100,
    ((int) temp_bmp), ((int)(temp_bmp*100.0))%100,
    ((int)presion), ((int)(presion*100.0))%100,
    ((int)hum), ((int)(hum*100.0))%100,
    ((int)lum_perc), ((int)(lum_perc*100.0))%100
  );
  Serial.print ("Enviando a servidor: ");
  Serial.println (post_data);
  http.POST ((unsigned char*) post_data, strlen (post_data));
  http.end ();
}


void setup() {
  pinMode (BUILTIN_LED, OUTPUT);
  pinMode (PIN_LDR, INPUT);
  Serial.begin (9600);
  digitalWrite (BUILTIN_LED, HIGH);
  dht.setup (PIN_DHT, DHTesp::DHT22);
  Wire.begin (D2, D1);
  bmp.begin ();
  connect_wifi ();
  global_temp = -100.0;
  global_hum = -100.0;
}

void loop() {
  float lum_perc, temp_bmp, presion;
  read_sensors (&global_temp, &temp_bmp, &presion, &global_hum, &lum_perc);
  send_to_server (global_temp, temp_bmp, presion, global_hum, lum_perc);
  delay (5000);
}

