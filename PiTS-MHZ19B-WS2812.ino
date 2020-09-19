/****************************************************************************
 *  PiTS-MHZ19B-WS2812 Modul                                                *
 *  ========================                                                *
 *  Dieser Sketch für den ESP8266 dient als remote Sensor für PiTS-It! zur  *
 *  Erfassung der CO2-Konzentration mit MHZ19B-Sensor und Darstellung als   *
 *  CO2-Ampel über WS2812B LED. Es werden folgende Libraries benötigt:      *
 *   WiFi (Bestandteil der Arduino IDE),                                    *
 *   NTP (https://github.com/chrismelba/NTP),                               *
 *   Time (https://github.com/PaulStoffregen/Time),                         *
 *   Timezone (https://github.com/JChristensen/Timezone),                   *
 *   Adafruit_NeoPixel (https://github.com/adafruit/Adafruit_NeoPixel),     *
 *                                                                          *
 *  Die Übertragung des Messwerte erfolgt per HTTP-Get Request an das       *
 *  Webserver Modul von PiTS-It!                                            *
 *                                                                          *
 *  Homepage: http://pits.TGD-Consulting.de                                 *
 *                                                                          *
 *  Version 0.1.0                                                           *
 *  Datum 19.09.2020                                                        *
 *                                                                          *
 *  (C) 2020 TGD-Consulting , Author: Dirk Weyand                           *
 ****************************************************************************/

/*************************
 *** Globale Parameter ***
 *************************/

#define WLAN_SSID               "SSID des WLAN"          // change to your WiFi SSID 
#define WLAN_PASSPHRASE         "DAS GEHEIME PASSWORT"   // change to your passphrase
#define RETRIES 10                                       // maximale Anzahl der Verbindungsversuche mit WLAN-AP
#define NTP_SERVER              "192.168.0.1"            // set your local NTP-Server here, or eg. "ptbtime2.ptb.de"
#define PITS_HOST               "192.168.0.25"           // PiTS-It! Webserver
#define PITS_PORT               8080                     // Port des Webservers
#define ZAEHLER_ID              "123456789"              // eindeutige ID des Sensors
#define TOKEN                   "000000453c67f0"         // Verbindungstoken (Seriennummer des RPi)
#define PST 0            // GMT/UTC - Anpassung an lokale Sommer/Winterzeit erfolgt über Timezone Library
//#define SERDEBUG 1       // Debug-Infos über Serielle Schnittstelle senden, auskommentiert = Debugging OFF  
#define PIN 2            // WS2812B wird an GPIO 2 des ESP-01 angeschlossen
#define NUMPIXELS 1      // Anzahl der am PIN angeschlossenen WS2812B (eine LED ausreichend für einfache CO2-Ampel)
#define MINUTEN 5        // Abtastrate, Anzahl Minuten bis zur nächsten Datenübermittlung

// include requiered library header
#include <ESP8266WiFi.h> // WiFi functionality
#include <WiFiUdp.h>     // udp for network time
#include <TimeLib.h>
#include <Timezone.h>    // Anpassung an lokale Zeitzone
#include <ntp.h>
#include <Adafruit_NeoPixel.h>

// function pre declaration 2 avoid errors
bool startWiFi(void);
time_t getNTPtime(void);

uint8_t count;  // Zähler für WiFi-Connect Versuche

Adafruit_NeoPixel leds(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800); // Adafruit_NeoPixel Library setup
NTP NTPclient;

//Central European Time (Berlin, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};    //Central European Summer Time = UTC + 2 hours
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};       //Central European Standard Time = UTC + 1 hours
Timezone CE(CEST, CET);

TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev

ADC_MODE(ADC_VCC);          // Aktiviert Spannungsabfrage

void setup() {
#ifdef SERDEBUG
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println("PROG INFORMATION =========================================================");
  Serial.println("PROG >> INFO >> PiTS-ESP8266 with MH-Z19B and WS2812");
  Serial.println("PROG >> ID   >> " ZAEHLER_ID );
  Serial.println("PROG >> DATE >> " __DATE__ );
  Serial.println("PROG >> TIME >> " __TIME__ );
  Serial.println("PROG >> GCC  >> " __VERSION__ );
  Serial.println(String("PROG >> IDE  >> ") + IDEString() );
  Serial.println("CHIP INFORMATION =========================================================");
  Serial.printf("CHIP >> CORE  >> ID: %08X\r\n", ESP.getChipId());
  Serial.println(String("CHIP >> CORE  >> Free Heap: ") + ESP.getFreeHeap() / 1024 + " kB");
  Serial.println("CHIP >> CORE  >> Speed: 80 MHz");
  Serial.println(String("CHIP >> CORE  >> Voltage: ") + ESP.getVcc() / 1000.0f + " V");
  Serial.printf("CHIP >> FLASH >> ID : %08X\r\n", ESP.getFlashChipId());
  Serial.println(String("CHIP >> FLASH >> Size: ") + ESP.getFlashChipRealSize() / 1024 + " kB");
  Serial.println(String("CHIP >> FLASH >> Speed: ") + ESP.getFlashChipSpeed() / 1000000 + " MHz");
  Serial.println("RUNTIME INFORMATION========================================================");
  Serial.print("PITS >> SENSOR >> ID ");
  Serial.println(ZAEHLER_ID);
#endif

  leds.begin();
  leds.setBrightness(250); //die Helligkeit setzen 0 dunkel -> 255 ganz hell
  leds.show();

  // mit WLAN-AP verbinden
  count = 0;
  while (count <= RETRIES && !startWiFi()) {
    delay(1500);
    count++;
    leds.setBrightness(250 - (count * 15)); // dunkler dimmen 
    leds.show();
  }

  if(count <= RETRIES){
#ifdef SERDEBUG
    Serial.println("WiFi connected");
    Serial.print("WIFI >> IP address: ");
    Serial.println(WiFi.localIP());
#endif

    NTPclient.begin(NTP_SERVER, PST);
    setSyncProvider(getNTPtime);
    setSyncInterval(SECS_PER_HOUR);  // jede Stunde aktualisieren

    delay(1000);  // nach dem Start 1 Sekunde Zeit, für NTP-Synchronisation
  }

#ifdef SERDEBUG
  Serial.flush();
  Serial.end();
#endif

  Serial.begin(9600);      // richtige Geschwindigkeit der seriellen Schnittstelle für MH-Z19B setzen

  leds.setBrightness(255); //die Helligkeit ganz hell
  leds.show();
  delay(500);  // warte 500ms
  leds.setPixelColor(1, leds.Color(255, 0, 0)); // Farbe Rot setzen
  leds.show(); //Anzeigen
  delay(1000); // warte 1s
  leds.setPixelColor(1, leds.Color(255, 255, 204)); // Farbe Gelb setzen
  leds.show(); //Anzeigen
  delay(1000); // warte 1s
  leds.setPixelColor(1, leds.Color(0, 255, 0)); // Farbe Grün setzen
  leds.show(); //Anzeigen
  delay(1000); // warte 1s
  leds.clear();            // alle LEDs ausschalten
}

void loop() {
  uint32_t ID4 = leds.Color(255, 0, 0); //RGB Farbe Rot
  uint32_t ID1 = leds.Color(0, 255, 0); //RGB Farbe Grün
  uint32_t ID2 = leds.Color(255, 255, 204); //RGB Farbe Gelb
  int co2;
  Intervall = MINUTEN * 60 * 1000;

  // MH-Z19B Sensor auslesen
  co2 = co2ppm();

  // CO2-Ampel
  if(co2 > 1900){   // ID4 sehr niedrig => rot blinken
    leds.setPixelColor(1, ID4); // Farbe Rot setzen
  } else {
    if(co2 > 1400){ // ID4 niedrig => rot
      leds.setPixelColor(1, ID4); // Farbe Rot setzen
    } else {
      if(co2 >= 1000){ // ID3 mäßig => gelb
        leds.setPixelColor(1, ID3); // Farbe Gelb setzen
      } else {
        if(co2 >= 800){ // ID2 mittel => hellgrün 
          leds.setPixelColor(1, ID1); // Farbe Grün setzen
        } else {        // ID1 gut => grün
          leds.setPixelColor(1, ID1); // Farbe Grün setzen
        }
      }
    }
  }
  leds.show(); //Anzeigen

  if(count <= RETRIES){                       // nur Messdaten senden, wenn erfolgreich WiFi
    // Werte des MH-Z19B Sensors ausgelesen => Signalisierung an PITS-Server
    time_t t = CE.toLocal(now(), &tcr);      // Store the current local time in time variable t
//    time_t t = now();                      // Store the current time in time variable t
    String DateTimeString = String(day(t), DEC) + "-" + String(month(t), DEC) + "-" + String(year(t), DEC);
    DateTimeString = DateTimeString + "/" + String(hour(t), DEC) + ":" + String(minute(t), DEC) + ":" + String(second(t), DEC);
  
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    if (!client.connect(PITS_HOST, PITS_PORT)) {
#ifdef SERDEBUG
      Serial.println("PITS >> HTTP   >> connection failed");
#endif
      return;
    }
  
    // We now create a URI for the request
    String url = "/cgi-bin/import.html?id=";
    url += ZAEHLER_ID;
    url += "&token=";
    url += TOKEN;
    url += "&data=";
    url += co2;
    url += "&run=";
    url += uptime();
    if (timeStatus() != timeNotSet) { // Falls Zeit synchron zum NTP-Server, Zeitpunkt übermitteln
      url += "&time=";
      url += DateTimeString;        // im REBOL Time-Format
    }
  
#ifdef SERDEBUG
    Serial.print("PITS >> HTTP   >> Requesting URL: ");
    Serial.println(url);
#endif

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + PITS_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");
  }

  delay(Intervall); // Abstand zwischen den Messungen
}

int co2ppm() {
  static byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  static byte response[9] = {0};

  Serial.write(cmd, 9);
  
  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (Serial.available() > 0 && (unsigned char)Serial.peek() != 0xFF) {
    Serial.read();     // liest ein Byte von der seriellen Scnittstelle zum resyncen
  }

  Serial.readBytes(response, 9);

  unsigned int responseHigh = (unsigned int) response[2];
  unsigned int responseLow = (unsigned int) response[3];

  return (256 * responseHigh) + responseLow;
}

String uptime() {
  //long days = 0;
  long hours = 0;
  long mins = 0;
  long secs = 0;
  secs = millis () / 1000;     // convert current milliseconds from ESP to seconds
  mins = secs / 60;            // convert seconds to minutes
  hours = mins / 60;           // convert minutes to hours
  //days = hours / 24;           // convert hours to days
  secs = secs - (mins * 60);
  mins = mins - (hours * 60);
  //hours = hours - (days * 24);
  String rc = "";
  rc += String(hours);
  rc += ":";
  rc += String(mins);
  rc += ":";
  rc += String(secs); 
  return rc;
}

String IDEString() {
  uint16_t IDE = ARDUINO;
  String tmp = "";
  tmp += String(IDE/10000);
  IDE %= 10000;
  tmp += ".";
  tmp += String(IDE/100);
  IDE %= 100;
  tmp += ".";
  tmp += String(IDE);
  return tmp;
}

#define NTP_RETRIES 3 // Anzahl Versuche, die Uhrzeit vom NTP zu bekommen

time_t getNTPtime(void) {
  time_t retVal = 0;

  for ( int i = 0; i < NTP_RETRIES && retVal == 0; i++ )
  {
    retVal = NTPclient.getNtpTime();
  }
  return ( retVal );
}

bool startWiFi(void) {
  uint8_t i;

#ifdef SERDEBUG
  Serial.print("WIFI >> Attempting to connect to ");
  Serial.print(WLAN_SSID);
  Serial.print(" using password ");
  Serial.println(WLAN_PASSPHRASE);
#endif

  WiFi.persistent(false); // Reduces flash access, memory wearing
  WiFi.mode(WIFI_STA);    // Explicitly set the ESP8266 to be a WiFi-client

  if (WiFi.begin(WLAN_SSID, WLAN_PASSPHRASE) != WL_CONNECTED) {
#ifdef SERDEBUG
    Serial.print("WIFI >> ");
#endif
    for (i = 0; i < 10; i++) {
      if (WiFi.status() == WL_CONNECTED) return true;
      delay(600);
#ifdef SERDEBUG
      Serial.print(".");
#endif
    }
  }

#ifdef SERDEBUG
  Serial.print("Failed to connect to: ");
  Serial.println(WLAN_SSID);

  Serial.print("WIFI >> using pass phrase: ");
  Serial.println(WLAN_PASSPHRASE);
#endif

  return false;
}