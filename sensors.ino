  void sensors_main () {
    float lum_perc, temp_bmp, presion;
    
    read_sensors (&global_temp, &temp_bmp, &presion, &global_hum, &lum_perc);
    char *filename = create_current_day_file ();
    store_sd (filename, global_temp, temp_bmp, presion, global_hum, lum_perc);
    free (filename);

    if (WiFi.status() == WL_CONNECTED) {
      send_to_server (global_temp, temp_bmp, presion, global_hum, lum_perc);
    } else {
      store_sd ("ENV_PEND.CSV", global_temp, temp_bmp, presion, global_hum, lum_perc);
    }

    notify_error (&global_temp, &temp_bmp, &presion, &global_hum, &lum_perc);
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

