#define WIFI_CHECK_INTERVAL 60000
#define CHANGE_NET_INTERVAL 7500
#define WIFI_MAX_CONNECT_TIME 15000

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

    int start = millis();
    while ((millis() - start < WIFI_MAX_CONNECT_TIME) && (WiFi.status() != WL_CONNECTED)) {
        digitalWrite (BUILTIN_LED, HIGH);
        delay (250);
        digitalWrite (BUILTIN_LED, LOW);
        delay (250);
    }

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
  
    free (file_content);
    free (ssid);
    free (pass);
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
  clock_sync ();
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

    int start = millis();
    while ((millis() - start < WIFI_MAX_CONNECT_TIME) && (WiFi.status() != WL_CONNECTED)) {
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

