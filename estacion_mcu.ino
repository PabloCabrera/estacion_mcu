#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Scheduler.h>

#include <Adafruit_BMP085.h>
#include <DHTesp.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <math.h>

#define READ_INTERVAL 30000
#define WIFI_CHECK_INTERVAL 60000
#define CHANGE_NET_INTERVAL 15000

#define PIN_DHT D0
#define PIN_LDR A0
#define PIN_SCK D5
#define PIN_MISO D6
#define PIN_MOSI D7
#define PIN_SDCS D3
#define PIN_ERROR_LED D8

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

class WifiTask: public Task {
	private:
		boolean connecting = false;

	public:
	void setup () {
	}

	void loop () {
		// Serial.println ("WifiTask-> loop()");
		if (WiFi.status() != WL_CONNECTED) {
			connect_wifi ();
		}
		delay (WIFI_CHECK_INTERVAL);
	}

  void connect_wifi () {
    if (SD.begin (PIN_SDCS)) {
      Serial.println ("SD init OK");
    } else {
      Serial.println ("SD init FAILED");
    }
    if (SD.exists ("wifi.txt")) {
      connect_wifi_sd ();
    } else {
      connect_wifi_old ();
    }
    //connect_wifi_old();
  }

  void connect_wifi_sd () {
      Serial.println ("LEYENDO CONFIGURACION WIFI DESDE SD");
      File sd_file = SD.open ("WIFI.TXT", FILE_READ);
      char *file_content = get_file_content (sd_file);
      sd_file.close ();
      Serial.println ("LEIDO ARCHIVO WIFI.TXT");
      char *ssid = get_text_field ("ssid=", file_content);
      char *pass = get_text_field ("password=", file_content);
      
      Serial.println ("DATOS:");
      Serial.println (ssid);
      Serial.println (pass);

      Serial.print ("\r\nConectando a red "); Serial.println (ssid);
      WiFi.begin (ssid, pass);
      WiFi.reconnect ();
  
      while (WiFi.status() != WL_CONNECTED) {
          digitalWrite (BUILTIN_LED, HIGH);
          delay (250);
          digitalWrite (BUILTIN_LED, LOW);
          delay (250);
      }

      Serial.print("Connected, IP address: ");
      Serial.println(WiFi.localIP());
      clock_sync ();
    
      free (file_content);
      free (ssid);
      free (pass);
  }

  char *get_text_field (char *field, char *body) {
    char *field_value = NULL;
    char *field_start = strstr (body, field);
    if (field_start != NULL) {
      char *field_end = strstr (field_start, "\r"); // DOS line end
      if (field_end == NULL) {
        field_end = strstr (field_start, "\n"); // Unix line end
      }
      if (field_end == NULL) {
        field_end = field_start + strlen (field_start); // No line end
      }
      int len = field_end - field_start - strlen (field);
      field_value = (char*) malloc (len+1);
      strncpy (field_value,  (field_start + strlen(field)), len);
      field_value[len] = '\0';
    }
    return field_value;
  }


  char *get_file_content (File sd_file) {
    char file_content [1024];
    int content_length = 0;
    boolean stp = false;
    
    while (!stp) {
      int readed = sd_file.read ();
      if (readed != -1 && content_length < 1023) {
        file_content [content_length] = readed;
        content_length++;
      } else {
        file_content[content_length] = '\0';
        stp = true;
      }
    }
    return strdup (file_content);
  }
  
	void connect_wifi_old () {
		boolean prev_use_alt = false;
		boolean use_alt = false;
		if (!connecting) {
			connecting = true;

			Serial.print ("\r\nBuscando red "); Serial.println (WIFI_SSID);
			WiFi.begin (WIFI_SSID, WIFI_PASSWORD);
			WiFi.reconnect ();
	
			while (WiFi.status() != WL_CONNECTED) {
				use_alt = (((int) (millis() / CHANGE_NET_INTERVAL)) % 2) != 0;
				if (use_alt != prev_use_alt) {
					if (use_alt) {
						Serial.print ("\r\nBuscando red "); Serial.println (WIFI_ALT_SSID);
						WiFi.begin (WIFI_ALT_SSID, WIFI_ALT_PASSWORD);
						WiFi.reconnect ();
					} else {
						Serial.print ("\r\nBuscando red "); Serial.println (WIFI_SSID);
						WiFi.begin (WIFI_SSID, WIFI_PASSWORD);
						WiFi.reconnect ();
					}
				}
				prev_use_alt = use_alt;

				digitalWrite (BUILTIN_LED, HIGH);
				delay (250);
				digitalWrite (BUILTIN_LED, LOW);
				delay (250);
			}
			Serial.println();

			Serial.print("Connected, IP address: ");
			Serial.println(WiFi.localIP());
			clock_sync ();
			connecting = false;
		}
	}

	void clock_sync () {
		uint32_t timestamp = get_remote_timestamp ();
		if (timestamp > 0) {
			DateTime *now = new DateTime ((uint32_t) timestamp);
			rtc.adjust (*now);
			Serial.println ("HORA ESTABLECIDA");
		} else {
			Serial.println ("NO SE PUEDE CONSULTAR LA HORA DEL SERVIDOR");
		}
	}

	uint32_t get_remote_timestamp () {
		unsigned long start_millis = millis ();
		HTTPClient http;
		http.begin (TIMESTAMP_URL);
		Serial.print ("Consultado hora a servidor: ");
		int response_code = http.GET ();
		uint32_t timestamp;
		if (response_code == 200) {
			String response = http.getString ();
			timestamp = response.toInt ();
			unsigned long end_millis = millis ();
			unsigned long adjust =  (((end_millis - start_millis)/2 + 500) / 1000);
			adjust += (TIMEZONE) * 3600;
			timestamp += (uint32_t) adjust;
			Serial.println (timestamp);
		} else {
			timestamp = 0;
		}
		http.end ();
		return timestamp;
	}

} wifi_task;

class SensorsTask: public Task {
	protected:
	void setup () {
	}

	void loop () {
		float lum_perc, temp_bmp, presion;
		// Serial.println ("SensorsTask-> loop()");
	
		read_sensors (&global_temp, &temp_bmp, &presion, &global_hum, &lum_perc);
		notify_error (&global_temp, &temp_bmp, &presion, &global_hum, &lum_perc);
		char *filename = create_current_day_file ();
		store_sd (filename, global_temp, temp_bmp, presion, global_hum, lum_perc);
		free (filename);

		if (WiFi.status() == WL_CONNECTED) {
			send_to_server (global_temp, temp_bmp, presion, global_hum, lum_perc);
		} else {
			store_sd ("ENV_PEND.CSV", global_temp, temp_bmp, presion, global_hum, lum_perc);
		}

		delay (READ_INTERVAL);
	}
	

	void read_sensors (float *temp, float *temp_bmp, float *presion, float *hum, float *lum_perc) {
		digitalWrite (BUILTIN_LED, HIGH);
		float input;

		input = dht.getTemperature ();
		if (!isnan (input)) {
			*temp = input;
		}

		input = dht.getHumidity ();
		if (!isnan (input)) {
			*hum = input;
		}

		int lum = analogRead (PIN_LDR);
		*lum_perc = ((float) lum)/10.23;
		*temp_bmp = bmp.readTemperature();
		*presion = 0.01*bmp.readPressure();

		*hum = (isnan (input))? (*hum): input;
		digitalWrite (BUILTIN_LED, LOW);
	}

	void notify_error (float *temp, float *temp_bmp, float *presion, float *hum, float *lum_perc) {
		if ((*temp == -100) || (*hum == -100)) {
			digitalWrite (PIN_ERROR_LED, HIGH);
		} else {
			digitalWrite (PIN_ERROR_LED, LOW);
		}
	}

	void store_sd (char *filename, float temp_dht, float temp_bmp, float presion, float hum, float lum_perc) {
		char write_data[2048];
		char *time_string = get_time_string ();
		
		SD.begin (PIN_SDCS);

		sprintf (write_data, "%s,%d.%d,%d.%d,%d.%d,%d.%d,%d.%d",
			time_string,
			((int) temp_dht),((int)(temp_dht*100.0))%100,
			((int) temp_bmp), ((int)(temp_bmp*100.0))%100,
			((int) presion), ((int)(presion*100.0))%100,
			((int) hum), ((int)(hum*100.0))%100,
			((int) lum_perc), ((int)(lum_perc*100.0))%100
		);
		File sd_file = SD.open (filename, FILE_WRITE);
		byte written = sd_file.println (write_data);
		sd_file.close ();
		if (written == 0) {
			for (int i=0; i < 4;i++) {
				digitalWrite (PIN_ERROR_LED, !digitalRead (PIN_ERROR_LED));
				delay (100);
			}
		}
		free (time_string);
	}

	void send_to_server (float temp_dht, float temp_bmp, float presion, float hum, float lum_perc) {
		digitalWrite (BUILTIN_LED, HIGH);
		char post_data[2048];
		HTTPClient http;
		http.begin (SEND_URL);
		http.addHeader ("Content-Type", "application/x-www-form-urlencoded");
		sprintf (post_data, "temperatura_dht=%d.%d&temperatura_bmp=%d.%d&presion=%d.%d&humedad=%d.%d&luminosidad=%d.%d",
			((int)temp_dht),((int)(temp_dht*100.0))%100,
			((int) temp_bmp), ((int)(temp_bmp*100.0))%100,
			((int)presion), ((int)(presion*100.0))%100,
			((int)hum), ((int)(hum*100.0))%100,
			((int)lum_perc), ((int)(lum_perc*100.0))%100
		);
		Serial.print ("Enviando a servidor: ");
		Serial.println (post_data);
		int response_code = http.POST ((unsigned char*) post_data, strlen (post_data));
		http.end ();
		if (response_code != 200) {
			store_sd ("ENV_PEND.CSV", temp_dht, temp_bmp, presion, hum, lum_perc);
		}
		digitalWrite (BUILTIN_LED, LOW);
	}


	char *get_time_string () {
		char *time_string = (char*) malloc (20);
		DateTime now = rtc.now ();
		sprintf(time_string, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
		return time_string;
	}

	char *create_current_day_file () {
		char *filepath = (char*) malloc (15);
		DateTime now = rtc.now ();
		sprintf (filepath, "%04d%02d%02d.CSV", now.year(), now.month(), now.day());
		return filepath;
	}

} sensors_task;


void setup() {
	pinMode (BUILTIN_LED, OUTPUT);
	pinMode (PIN_ERROR_LED, OUTPUT);
	pinMode (PIN_DHT, INPUT);
	pinMode (PIN_LDR, INPUT);
	pinMode (PIN_SCK, OUTPUT);
	pinMode (PIN_MISO, INPUT);
	pinMode (PIN_MOSI, OUTPUT);
	pinMode (PIN_SDCS, OUTPUT);

	global_temp = -100.0;
	global_hum = -100.0;
	Serial.begin (9600);

	Wire.begin (D2, D1);
	bmp.begin ();
	rtc.begin ();
	digitalWrite (PIN_ERROR_LED, LOW);
	digitalWrite (BUILTIN_LED, LOW);
	dht.setup (PIN_DHT, DHTesp::DHT22);

	Scheduler.start (&wifi_task);
	Scheduler.start (&sensors_task);
	Scheduler.begin ();
}

void loop() {
}


