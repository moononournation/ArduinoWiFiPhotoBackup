// fill your own values
#define AP_SSID "WIFISD"
#define AP_PASS "YOURPASSWORD" // factory default is 12345678
#define WEB_USER "admin"
#define WEB_PASS "YOURPASSWORD" // factory default is admin
// fill your DC folder name // TODO: auto search all folders under /DCIM
#define WIFISD_ROOT_FOLDER "/DCIM/100MSDCF" // Sony

// WIFISD web interface constant
#define LIST_URL "http://192.168.11.254/cgi-bin/tslist?PATH=/www/sd"
#define TN_URL "http://192.168.11.254/cgi-bin/thumbNail?fn=/www/sd"
#define EXIF_URL "http://192.168.11.254/cgi-bin/tscmd?CMD=GET_EXIF&PIC=/www/sd"
#define IMG_URL "http://192.168.11.254/sd"

// local settings
#define TN_FOLDER "/tn"
#define EXIF_FOLDER "/exif"
#define FILENAME_SIZE 14 // '/' + stardard 8.3 filename + zero tail
#define NEW_PHOTO_BATCH_SIZE 3 // download how many new photos in one loop
#define NORMAL_LCD_BRIGHTNESS 120 // 0 - 255
#define SLEEP_LCD_BRIGHTNESS 20 // 0 - 255
#define SLEEP_INTERVAL 5 // seconds

#include <M5Stack.h>
#include <HTTPClient.h>

WiFiMulti wifiMulti;
char new_photo_list[NEW_PHOTO_BATCH_SIZE][FILENAME_SIZE];

void drawPreview(String p) {
  String exif_p = EXIF_FOLDER + p;
  String tn_p = TN_FOLDER + p;

  M5.Lcd.fillRoundRect(0, 40, 320, 180, 6, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_BLACK);
  // bold filename
  M5.Lcd.drawString(p.c_str(), 0, 50, 4);
  M5.Lcd.drawString(p.c_str(), 1, 50, 4);

  M5.Lcd.setTextColor(TFT_DARKGREY);
  M5.Lcd.drawString("EXIF Details:", 193, 45, 1);
  M5.Lcd.drawString("EXIF Details:", 194, 45, 1);
  M5.Lcd.fillRoundRect(190, 55, 125, 160, 4, 0b1111011111011110);
  uint16_t color = 0b1111000000000111;
  int x = 195;
  int y = 60;
  M5.Lcd.setTextColor(color);
  File f = SD.open(exif_p, FILE_READ);
  char c;
  M5.Lcd.setCursor(x, y);
  while (f.available()) {
    c = f.read();
    if (c == ':') { // split line between label and value
      M5.Lcd.print(c);
      c = f.read();
      if (c == ' ') {
        y += 9;
        M5.Lcd.setCursor(x + 3, y);
        M5.Lcd.setTextColor(TFT_BLACK);
      } else {
        M5.Lcd.print(c);
      }
    } else if (c == '\n') {
      y += 10;
      M5.Lcd.setCursor(x, y);
      color = (color >> 2) | (color << 14); // rotate color
      M5.Lcd.setTextColor(color);
    } else {
      M5.Lcd.print(c);
    }
  }
  f.close();

  M5.Lcd.fillRoundRect(5, 80, 180, 135, 4, TFT_BLACK);
  M5.Lcd.drawJpgFile(SD, tn_p.c_str(), 15, 90);
}

void drawFileCount() {
  M5.Lcd.fillRoundRect(205, 225, 115, 15, 6, TFT_MAGENTA);
  M5.Lcd.setTextColor(TFT_BLACK);
  M5.Lcd.setCursor(215, 229);
  M5.Lcd.print("File count: ");
  int fileCount = 0;
  File root = SD.open("/");
  File entry = root.openNextFile();
  while (entry)
  {
    if (!entry.isDirectory()) {
      // Serial.println(entry.name());
      fileCount++;
    }
    entry.close();
    entry = root.openNextFile();
  }
  entry.close();
  root.close();

  M5.Lcd.print(fileCount);
}

void downloadPhoto(String p) {
  String url;
  HTTPClient http;
  int httpCode;
  int dataSize;

  // download EXIF data
  String exif_p = EXIF_FOLDER + p;
  url = EXIF_URL WIFISD_ROOT_FOLDER + p;
  Serial.printf("[HTTP] %s\n", url.c_str());
  http.begin(url.c_str());
  http.setAuthorization(WEB_USER, WEB_PASS);
  httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String exif_str = http.getString();
      http.end();
      // Serial.println(exif_str);

      int len = exif_str.length();
      int i = 0;
      char c;
      File f = SD.open(exif_p, FILE_WRITE);
      while ((i < len) && (exif_str[i] != '\n')) {
        i++; // skip first line
      }
      i++;
      while (i < len) {
        if (
          (exif_str[i] == '<')
          && (exif_str[i + 1] == 'b')
          && (exif_str[i + 2] == 'r')
          && (exif_str[i + 3] == '>')
        ) {
          i += 3; // skip characters
        } else if (
          (exif_str[i] == 'S')
          && (exif_str[i + 1] == 'u')
          && (exif_str[i + 2] == 'c')
          && (exif_str[i + 3] == 'c')
          && (exif_str[i + 4] == 'e')
          && (exif_str[i + 5] == 's')
          && (exif_str[i + 6] == 's')
          && (exif_str[i + 7] == ':')
        ) {
          break; // skip last row
        } else {
          f.write(exif_str[i]);
          Serial.print(exif_str[i]);
        }
        i++;
      }
      f.close();
    } else {
      http.end();
    }
  } else {
    http.end();
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  // download thumbnail
  String tn_p = TN_FOLDER + p;
  url = TN_URL WIFISD_ROOT_FOLDER + p;
  Serial.printf("[HTTP] %s\n", url.c_str());
  http.begin(url.c_str());
  http.setAuthorization(WEB_USER, WEB_PASS);
  httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient s = http.getStream();
      int avail;
      dataSize = 0;
      File f = SD.open(tn_p, FILE_WRITE);
      while (avail = s.available()) {
        uint8_t buf[avail];
        int r = s.read(buf, avail);
        f.write(buf, r);

        dataSize += avail;
      }
      f.close();
      Serial.println(dataSize);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  drawPreview(p);

  // download original file
  M5.Lcd.fillRoundRect(0, 225, 200, 15, 6, TFT_GREENYELLOW);
  M5.Lcd.setTextColor(TFT_BLACK, TFT_GREENYELLOW);
  url = IMG_URL WIFISD_ROOT_FOLDER + p;
  Serial.printf("[HTTP] %s\n", url.c_str());
  http.begin(url.c_str());
  http.setAuthorization(WEB_USER, WEB_PASS);
  httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient s = http.getStream();
      int avail;
      dataSize = 0;
      File f = SD.open(p, FILE_WRITE);
      while (avail = s.available()) {
        uint8_t buf[avail];
        int r = s.read(buf, avail);
        f.write(buf, r);

        dataSize += avail;
        M5.Lcd.setCursor(10, 229);
        M5.Lcd.printf("Downloading original: %d", dataSize);
      }
      f.close();
      Serial.println(dataSize);
      // M5.Lcd.drawJpgFile(SD, p);
      // M5.Lcd.drawJpgFile(SD, p, 5, 50, 310, 165, 0, 0, JPEG_DIV_8);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void readPath(String p) {
  HTTPClient http;
  String url = LIST_URL + p;

  Serial.printf("[HTTP] %s\n", url.c_str());
  http.begin(url.c_str());
  http.setAuthorization(WEB_USER, WEB_PASS);

  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      String list_str = http.getString();
      http.end();
      // Serial.println(list_str);

      int len = list_str.length();
      int i = 0;
      int new_count = 0;
      while ((i < len) && (new_count < NEW_PHOTO_BATCH_SIZE)) {
        if (
          (list_str[i] == 'F')
          && (list_str[i + 1] == 'i')
          && (list_str[i + 2] == 'l')
          && (list_str[i + 3] == 'e')
          && (list_str[i + 4] == 'N')
          && (list_str[i + 5] == 'a')
          && (list_str[i + 6] == 'm')
          && (list_str[i + 7] == 'e')
        ) {
          i += 9;
          while ((i < len) && (list_str[i] != '=')) {
            i++;
          }
          i++;
          new_photo_list[new_count][0] = '/';
          int j = 1;
          while ((i < len) && (list_str[i] != '&')) {
            new_photo_list[new_count][j] = list_str[i];
            i++;
            j++;
          }
          new_photo_list[new_count][j] = 0;
          Serial.println(new_photo_list[new_count]);
          if (SD.exists(new_photo_list[new_count])) {
            Serial.println("File exists.");
          } else {
            new_count++;
          }
        }
        i++;
      }
      Serial.println(new_count);
      if (new_count > 0) {
        M5.Lcd.setBrightness(NORMAL_LCD_BRIGHTNESS);
        for (i = 0; i < new_count; i++) {
          downloadPhoto(new_photo_list[i]);
        }
      }
    } else {
      http.end();
    }
  } else {
    http.end();
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  drawFileCount();
}

void setup() {
  M5.begin();
  M5.Lcd.setBrightness(NORMAL_LCD_BRIGHTNESS);
  M5.Lcd.drawBitmap(0, 0, 320, 240, (uint16_t *)gImage_logoM5);
  M5.Lcd.fillRect(0, 0, 320, 40, TFT_BLACK);
  M5.Lcd.fillRect(0, 40, 6, 180, TFT_BLACK);
  M5.Lcd.fillRect(314, 40, 6, 180, TFT_BLACK);
  M5.Lcd.fillRect(0, 220, 320, 20, TFT_BLACK);
  M5.Lcd.fillRoundRect(0, 40, 12, 180, 6, TFT_WHITE);
  M5.Lcd.fillRoundRect(308, 40, 12, 180, 6, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_BLACK);
  M5.Lcd.setTextWrap(true);

  SD.mkdir(TN_FOLDER);
  SD.mkdir(EXIF_FOLDER);
  /* clear files for debug
    SD.remove("/DSC08999.JPG");
    SD.remove("/DSC09000.JPG");
    SD.remove("/DSC09001.JPG");
    SD.remove("/DSC09002.JPG");
    SD.remove("/DSC09003.JPG");
    SD.remove("/DSC09004.JPG");
    SD.remove("/DSC09005.JPG");
    //*/

  M5.Lcd.fillRoundRect(0, 0, 320, 35, 6, TFT_CYAN);
  // Bold header
  M5.Lcd.drawCentreString("WiFi   Photo   Backup", 159, 8, 4);
  M5.Lcd.drawCentreString("WiFi   Photo   Backup", 160, 8, 4);
  M5.Lcd.drawCentreString("WiFi   Photo   Backup", 161, 8, 4);
  M5.Lcd.fillRoundRect(0, 225, 200, 15, 6, TFT_YELLOW);
  M5.Lcd.setCursor(10, 229);
  M5.Lcd.printf("Connecting to " AP_SSID "...");

  drawFileCount();

  wifiMulti.addAP(AP_SSID, AP_PASS);
}

void loop() {
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED)) {
    Serial.println("Find new photo");
    M5.Lcd.fillRoundRect(0, 225, 200, 15, 6, TFT_YELLOW);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setCursor(10, 229);
    M5.Lcd.printf("Finding new photo...", SLEEP_INTERVAL);

    readPath(WIFISD_ROOT_FOLDER);

    Serial.println("Sleep");
    M5.Lcd.fillRoundRect(0, 225, 200, 15, 6, TFT_LIGHTGREY);
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.setCursor(10, 229);
    M5.Lcd.printf("Sleep %d seconds...", SLEEP_INTERVAL);
    M5.Lcd.setBrightness(SLEEP_LCD_BRIGHTNESS);
    // esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL * 1000000); // convert to microseconds
    // esp_light_sleep_start();
    delay(SLEEP_INTERVAL * 1000); // convert to milliseconds
  } else {
    delay(1000); // delay for check WiFi
  }
}
