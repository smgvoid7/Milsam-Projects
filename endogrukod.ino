#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Adafruit_Protomatter.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <esp_wifi.h>
#include "time.h"

// Erişim Noktası Ayarları
const char* ap_ssid = "ESP2";
const char* ap_password = "123456789";
bool connected = false;

const uint16_t port = 12000;
const char * sunucu = "192.168.115.207";

// DNS ve Web Server
DNSServer dnsServer;
WebServer webServer(80);
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

#if defined(ESP32)
  uint8_t rgbPins[]  = {4, 12, 13, 14, 15, 21};
  uint8_t addrPins[] = {16, 17, 25, 26, 23};
  uint8_t clockPin   = 27;
  uint8_t latchPin   = 32;
  uint8_t oePin      = 33;

#endif

Adafruit_Protomatter matrix(
  225,
  1,
  1, rgbPins,
  5, addrPins,
  clockPin, latchPin, oePin,
  true);

int16_t  textX;
int16_t  textY;
int16_t  textMin;
char     str[94];

String text = "192.168.4.1";

void setup() {
  Serial.begin(115200);
  ProtomatterStatus status = matrix.begin();

  Serial.print("Protomatter begin() status: ");
  Serial.println((int)status);
  if(status != PROTOMATTER_OK) {
    for(;;);
  }

  // Erişim Noktasını Ayarla
  WiFi.softAP(ap_ssid, ap_password);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  // DNS ve Web Server Başlat
  dnsServer.start(DNS_PORT, "*", apIP);
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/scan", HTTP_GET, handleScan);
  webServer.on("/connect", HTTP_POST, handleConnect);
  webServer.begin();

  Serial.print("Erişim Noktası IP Adresi: ");
  Serial.println(WiFi.softAPIP());

  text = WiFi.softAPIP().toString() + " Adresine baglanti bekleniyor";

  sprintf(str, "MILSAN ELEKTRONIK KART FABRIKASI", matrix.width(), matrix.height());
  matrix.setFont(&FreeSansBold18pt7b);
  matrix.setTextWrap(false);
  matrix.setTextColor(0xFFFF);
  int16_t  x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  textMin = -w;
  textX = matrix.width();
  textY = matrix.height() / 2 - (y1 + h / 2);

}

int minc = 0;

void loop() {
  // DNS ve Web Server İşleme
  dnsServer.processNextRequest();
  webServer.handleClient();

  delay(10);

  handleWrite();
}

WiFiClient client;

void handleTCP(){
  if (connected) {
    minc++;
    Serial.print(sunucu);
    Serial.print(":");
    Serial.println(port);
    if(minc == 2){
      if(client.connected()){
        char mystr[40];
        sprintf(mystr,"Millis: %lu",random(10000,99999));
        client.print((String("check/") + String(mystr)).c_str());

        int wait = 0;

        while(!client.available()){
          delay(1000);
          wait++;
          if(wait == 2){
            break;
          }
        }

        if(client.available() > 0){
          String response = client.readStringUntil('\r');
          if(response.indexOf(String(mystr)) >= 0){
            response.replace(mystr,"");
            response.replace("/","");
            if(response != "nope" && response != "clear"){
              text = response;
            }
            if(response == "clear"){
              text = "MILSAN ELEKTRONIK";
            }
          }
        }
        //client.stop();
        minc = 0;
      }
      else{
        while(true){
          if (client.connect(sunucu, port)) {
            break;
          }
          else{
            delay(500);
          }
        }
        minc = 0;
      }
    }
  }
  else{
    text = WiFi.softAPIP().toString() + " Adresine baglanti bekleniyor";
  }
}

void handleWrite(){
  matrix.fillScreen(matrix.color565(225,0,0));

  int16_t  x1, y1;
  uint16_t w, h;
  sprintf(str, text.c_str(), matrix.width(), matrix.height());
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  textMin = -w;

  matrix.setCursor(textX, textY);
  matrix.print(text.c_str());

  if((textX--) < textMin - 3) {
    textX = matrix.width();
    handleTCP();
  }
  
  matrix.show();
}

// Web Server İşlevleri
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 WiFi Manager</title></head><body>";
  html += "<h1>ESP32 WiFi Manager</h1>";
  html += "<form action='/connect' method='POST'>";
  html += "<label for='ssid'>SSID:</label><input type='text' id='ssid' name='ssid'><br>";
  html += "<label for='password'>Password:</label><input type='password' id='password' name='password'><br>";
  html += "<input type='submit' value='Connect'>";
  html += "</form></body></html>";
  webServer.send(200, "text/html", html);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  text = "Wifi agları taraniyor...";
  String networks = "";
  if (n == 0) {
    networks = "No networks found.";
  } else {
    for (int i = 0; i < n; ++i) {
      networks += WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)<br>";
    }
  }
  webServer.send(200, "text/plain", networks);
  text = "Wifi agları tarandi!";
}

void handleConnect() {
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");

  WiFi.begin(ssid.c_str(), password.c_str());
  int timeout = 10;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    String message = "Baglandi: " + ssid + "\nIP: " + WiFi.localIP().toString();
    connected = true;
    webServer.send(200, "text/plain", message);
    Serial.println(message);

    while(true){
      if (client.connect(sunucu, port)) {
        break;
      }
      else{
        delay(500);
      }
    }

    text = message;
  } else {
    webServer.send(200, "text/plain", "Bağlantı başarısız!");
    Serial.println("Bağlantı başarısız!");
    text = "Baglanti basarisiz!";
  }
}