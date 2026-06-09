#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <DHT.h>
#include <ESP32Servo.h>
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "AudioFileSourcePROGMEM.h"

#include "logo_bitmap.h"

// ===== SES DOSYALARI =====
#include "merhaba.h"
#include "ne_yapmami_istersin.h"
#include "sallaniyoruz.h"
#include "karanlik.h"
#include "hamle.h"
#include "sira.h"
#include "kazandim.h"
#include "kazandin.h"
#include "berabere.h"

// ===== TFT PİNLERİ =====
#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
#define TFT_MOSI 23
#define TFT_SCLK 18

// ===== SENSÖR VE MOTOR PİNLERİ =====
#define TOUCH_PIN    12
#define DHTPIN       14
#define DHTTYPE      DHT11
#define LDR_PIN      34
#define TILT_PIN     27
#define SAG_KOL_PIN  13
#define SOL_KOL_PIN  15

// ===== I2S (MAX98357A) PİNLERİ =====
#define I2S_BCLK  26
#define I2S_LRC   25
#define I2S_DOUT  22

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);
Servo sagKol;
Servo solKol;
AsyncWebServer espSunucu(8080);  // Python'dan ses komutu alacak port

// ===== SES NESNELERİ =====
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourcePROGMEM *kaynak = nullptr;
AudioOutputI2S *cikis = nullptr;
bool sesCaliniyor = false;
bool merhabaCalindiMi = false;
bool acilisSesiBekleniyor = false;

// ===== AĞ VE SUNUCU AYARLARI =====
const char* ssid     = "Kendi İnternetiniz";
const char* password = "Kendi İnternetinizin Şifresi";
String weatherUrl = "http://api.open-meteo.com/v1/forecast?latitude=39.92&longitude=32.85&current_weather=true";
String sunucuIP   = "http://ip adresiniz: ve atadığınız port/hamle-al";

// ===== DURUM DEĞİŞKENLERİ =====
int ekranDurumu = 1;
int oncekiDurum = -1;
bool sonTouchDurumu = LOW;
int isik = 1000, tiltDurumu = 0;
float odaSicakligi = 0.0, odaNemi = 0.0;
String internetSicaklik = "--";
unsigned long sonHavaGuncelleme = 0, sonGozKirma = 0;
unsigned long kronoBaslangic = 0, pomodoroBaslangic = 0;
unsigned long sonSensorOkuma = 0;
bool gozlerAcik = true, kollarKalkikMi = false;
const int karanlikSiniri = 1500;
int sonDakika = -1, sonSaniye = -1, sonPomoSaniye = -1;
const char* gunler[] = {"Pazar","Pazartesi","Sali","Carsamba","Persembe","Cuma","Cumartesi"};

// ===== SES FONKSİYONLARI =====
void sesCal(const unsigned char* veri, int boyut) {
  if (mp3)    { mp3->stop();    delete mp3;    mp3    = nullptr; }
  if (kaynak) {                 delete kaynak; kaynak = nullptr; }
  kaynak = new AudioFileSourcePROGMEM(veri, boyut);
  mp3    = new AudioGeneratorMP3();
  mp3->begin(kaynak, cikis);
  sesCaliniyor = true;
}

void sesDurumunuGuncelle() {
  if (sesCaliniyor && mp3) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        sesCaliniyor = false;
        if (acilisSesiBekleniyor) {
          acilisSesiBekleniyor = false;
          merhabaCalindiMi = true;
          sesCal(ne_yapmami_istersin_mp3, sizeof(ne_yapmami_istersin_mp3));
        }
      }
    } else {
      sesCaliniyor = false;
    }
  }
}

// ===== ESP32 SES SUNUCUSU (Python'dan komut alır) =====
void espSesSunucusuBaslat() {
  espSunucu.on("/ses", HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      String body = "";
      for (size_t i = 0; i < len; i++) body += (char)data[i];

      int dIdx = body.indexOf("\"dosya\":\"");
      if (dIdx >= 0) {
        int bas = dIdx + 9;
        int son = body.indexOf("\"", bas);
        String dosya = body.substring(bas, son);

        if      (dosya == "sira")      sesCal(sira_mp3,      sizeof(sira_mp3));
        else if (dosya == "kazandin")  sesCal(kazandin_mp3,  sizeof(kazandin_mp3));
        else if (dosya == "kazandim")  sesCal(kazandim_mp3,  sizeof(kazandim_mp3));
        else if (dosya == "berabere")  sesCal(berabere_mp3,  sizeof(berabere_mp3));
        else if (dosya == "hamle")     sesCal(hamle_mp3,     sizeof(hamle_mp3));
      }
      req->send(200, "text/plain", "ok");
    }
  );
  espSunucu.begin();
  Serial.println("Ses sunucusu 8080 portunda basladi.");
}

// ===== HAVA DURUMU =====
void havaDurumunuCek() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    if (http.GET() > 0) {
      String payload = http.getString();
      int cIndex = payload.indexOf("\"current_weather\":");
      if (cIndex > 0) {
        int tIndex = payload.indexOf("\"temperature\":", cIndex);
        if (tIndex > 0) {
          int vIndex = payload.indexOf(",", tIndex);
          internetSicaklik = payload.substring(tIndex + 14, vIndex);
        }
      }
    }
    http.end();
  }
}

// ===== YÜZ FONKSİYONLARI =====
void cizGozler() {
  tft.fillCircle(40,  60, 20, ST77XX_WHITE);
  tft.fillCircle(120, 60, 20, ST77XX_WHITE);
}
void cizKorkmusYuz() {
  tft.fillCircle(40,  35, 18, ST77XX_WHITE); tft.fillCircle(40,  35, 5, ST77XX_BLACK);
  tft.fillCircle(120, 35, 18, ST77XX_WHITE); tft.fillCircle(120, 35, 5, ST77XX_BLACK);
  tft.fillCircle(80,  80, 14, ST77XX_WHITE); tft.fillCircle(80,  80,10, ST77XX_BLACK);
}
void cizSallananYuz() {
  tft.fillCircle(40,  35, 22, ST77XX_WHITE); tft.fillCircle(40,  35, 8, ST77XX_BLACK);
  tft.fillCircle(120, 25, 12, ST77XX_WHITE); tft.fillCircle(120, 25, 4, ST77XX_BLACK);
  tft.drawLine(60, 75, 100, 85, ST77XX_WHITE);
  tft.drawLine(60, 76, 100, 86, ST77XX_WHITE);
}
void cizBekleyenYuz() {
  tft.fillCircle(40,  35, 20, ST77XX_WHITE);
  tft.fillCircle(120, 35, 20, ST77XX_WHITE);
}

// ===== XOX TAHTA FONKSİYONLARI =====
void xoxTahtaCiz(int tahta[9], int secili) {
  tft.drawFastVLine(53,  30, 90, ST77XX_WHITE);
  tft.drawFastVLine(106, 30, 90, ST77XX_WHITE);
  tft.drawFastHLine(13, 60, 134, ST77XX_WHITE);
  tft.drawFastHLine(13, 90, 134, ST77XX_WHITE);

  for (int i = 0; i < 9; i++) {
    int sutun = i % 3;
    int satir = i / 3;
    int cx = 13 + sutun * 53 + 26;
    int cy = 30 + satir * 30 + 15;

    if (i == secili && tahta[i] == 0) {
      tft.fillRect(13 + sutun*53 + 2, 30 + satir*30 + 2, 48, 26, ST77XX_YELLOW);
    }
    if (tahta[i] == 1) {
      tft.drawLine(cx-10, cy-8, cx+10, cy+8, ST77XX_CYAN);
      tft.drawLine(cx+10, cy-8, cx-10, cy+8, ST77XX_CYAN);
    } else if (tahta[i] == 2) {
      tft.drawCircle(cx, cy, 11, ST77XX_RED);
    }
  }
}

void xoxHamleGonder(int hamle, int tahta[9], String& xoxDurum,
                    bool& oyunBitti, int& oyuncuSkor, int& robotSkor) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(sunucuIP);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  String json = "{\"robot_mesaji\":\"" + String(hamle) + "\"}";
  int kod = http.POST(json);

  if (kod <= 0) {
    tft.fillRect(0, 115, 160, 13, ST77XX_BLACK);
    tft.setTextSize(1); tft.setTextColor(ST77XX_RED);
    tft.setCursor(5, 118); tft.print("Sunucuya ulasilamadi!");
    http.end();
    return;
  }

  String cevap = http.getString();
  http.end();

  // durum
  String durum = "";
  int dIdx = cevap.indexOf("\"durum\":\"");
  if (dIdx >= 0) {
    int bas = dIdx + 9;
    durum = cevap.substring(bas, cevap.indexOf("\"", bas));
  }

  // robot_hamlesi
  int robotKare = -1;
  int rIdx = cevap.indexOf("\"robot_hamlesi\":");
  if (rIdx >= 0) {
    int bas = rIdx + 16;
    int son = cevap.indexOf(",", bas);
    if (son < 0) son = cevap.indexOf("}", bas);
    robotKare = cevap.substring(bas, son).toInt();
  }

  // Tahtayı güncelle
  tahta[hamle] = 1;
  if (robotKare >= 0 && robotKare <= 8) tahta[robotKare] = 2;

  // Duruma göre tepki
  // (Ses Python tarafından ESP32'ye /ses endpoint'i üzerinden zaten gönderiliyor)
  if (durum == "devam") {
    xoxDurum = "oyuncu_oynuyor";
  } else if (durum == "oyuncu_kazandi") {
    sagKol.write(90); solKol.write(90);
    delay(1500);
    sagKol.write(0);  solKol.write(0);
    oyuncuSkor++;
    oyunBitti = true;
    xoxDurum = "bitti";
  } else if (durum == "robot_kazandi") {
    robotSkor++;
    oyunBitti = true;
    xoxDurum = "bitti";
  } else if (durum == "beraberlik") {
    oyunBitti = true;
    xoxDurum = "bitti";
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);
  pinMode(TILT_PIN,  INPUT_PULLUP);
  dht.begin();

  sagKol.attach(SAG_KOL_PIN); sagKol.write(0);
  solKol.attach(SOL_KOL_PIN); solKol.write(0);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  tft.drawRGBBitmap(40, 5, logo, 90, 90);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(43, 105);
  tft.print("AnkaBot");

  cikis = new AudioOutputI2S();
  cikis->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  cikis->SetGain(0.5);

  WiFi.begin(ssid, password);
  int deneme = 0;
  while (WiFi.status() != WL_CONNECTED && deneme < 20) {
    delay(500); deneme++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    tft.setTextSize(1); tft.setCursor(5, 110);
    tft.print(WiFi.localIP());
  }

  configTime(10800, 0, "pool.ntp.org");
  havaDurumunuCek();
  espSesSunucusuBaslat();  // Ses komut sunucusunu başlat

  acilisSesiBekleniyor = true;
  sesCal(merhaba_mp3, sizeof(merhaba_mp3));
}

// ===== LOOP =====
void loop() {
  sesDurumunuGuncelle();

  bool guncelTouch = digitalRead(TOUCH_PIN);

  // Sayfa geçişi
  if (guncelTouch == HIGH && sonTouchDurumu == LOW) {
    ekranDurumu++;
    if (ekranDurumu > 9) ekranDurumu = 1;
    tft.fillScreen(ST77XX_BLACK);
    oncekiDurum = -1; sonDakika = -1; sonSaniye = -1; sonPomoSaniye = -1;
    if (ekranDurumu == 1) { gozlerAcik = true; sonGozKirma = millis(); }
    if (ekranDurumu == 7) kronoBaslangic  = millis();
    if (ekranDurumu == 8) pomodoroBaslangic = millis();
    delay(300);
  }
  sonTouchDurumu = guncelTouch;

  // Sensörler
  if (millis() - sonSensorOkuma > 1000) {
    isik        = analogRead(LDR_PIN);
    tiltDurumu  = digitalRead(TILT_PIN);
    odaSicakligi = dht.readTemperature();
    odaNemi      = dht.readHumidity();
    sonSensorOkuma = millis();
  }

  // Servo / sarsıntı
  if (tiltDurumu == 1 && !kollarKalkikMi) {
    sagKol.write(90); solKol.write(90);
    kollarKalkikMi = true;
    if (merhabaCalindiMi) sesCal(sallaniyoruz_mp3, sizeof(sallaniyoruz_mp3));
  } else if (tiltDurumu == 0 && kollarKalkikMi) {
    sagKol.write(0); solKol.write(0);
    kollarKalkikMi = false;
  }

  switch (ekranDurumu) {

    case 1: {
      if (millis() - sonGozKirma > (gozlerAcik ? 4000 : 300)) {
        gozlerAcik = !gozlerAcik;
        sonGozKirma = millis();
        tft.fillScreen(ST77XX_BLACK);
        if (gozlerAcik) cizGozler();
        else {
          tft.fillRect(20,  60, 40, 4, ST77XX_WHITE);
          tft.fillRect(100, 60, 40, 4, ST77XX_WHITE);
        }
      }
      if (oncekiDurum != 1 && gozlerAcik) {
        tft.fillScreen(ST77XX_BLACK); cizGozler(); oncekiDurum = 1;
      }
      break;
    }

    case 2: {
      struct tm ti;
      if (getLocalTime(&ti)) {
        if (ti.tm_min != sonDakika || oncekiDurum != 2) {
          tft.fillScreen(ST77XX_BLACK);
          tft.setTextSize(4); tft.setTextColor(ST77XX_CYAN);
          tft.setCursor(20, 30); tft.printf("%02d:%02d", ti.tm_hour, ti.tm_min);
          String gunIsmi = gunler[ti.tm_wday];
          int hb = (160 - (gunIsmi.length() * 12)) / 2;
          tft.setTextSize(2); tft.setTextColor(ST77XX_YELLOW);
          tft.setCursor(hb, 80); tft.print(gunIsmi);
          sonDakika = ti.tm_min; oncekiDurum = 2;
        }
      }
      break;
    }

    case 3: {
      if (millis() - sonHavaGuncelleme > 300000) { havaDurumunuCek(); sonHavaGuncelleme = millis(); }
      if (oncekiDurum != 3) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(2); tft.setTextColor(ST77XX_ORANGE);
        tft.setCursor(40, 30); tft.print("DISARI");
        tft.setTextSize(3); tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(30, 70); tft.print(internetSicaklik); tft.print(" C");
        oncekiDurum = 3;
      }
      break;
    }

    case 4: {
      static float gosterilenIsi = -100.0;
      static float gosterilenNem = -100.0;
      if (oncekiDurum != 4) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(2); tft.setTextColor(ST77XX_CYAN);
        tft.setCursor(30, 20); tft.print("ODA ICI");
        oncekiDurum = 4; gosterilenIsi = -100.0;
      }
      if (!isnan(odaSicakligi) && !isnan(odaNemi)) {
        if (odaSicakligi != gosterilenIsi || odaNemi != gosterilenNem) {
          tft.fillRect(0, 60, 160, 60, ST77XX_BLACK);
          tft.setTextColor(ST77XX_WHITE); tft.setTextSize(2);
          tft.setCursor(10, 60); tft.print("Isi: "); tft.print(odaSicakligi, 1); tft.print("C");
          tft.setCursor(10, 90); tft.print("Nem: %"); tft.print((int)odaNemi);
          gosterilenIsi = odaSicakligi; gosterilenNem = odaNemi;
        }
      }
      break;
    }

    case 5: {
      static bool karanlikUyari = false;
      if (oncekiDurum != 5) {
        tft.fillScreen(ST77XX_BLACK);
        oncekiDurum = 5;
        karanlikUyari = !(isik > karanlikSiniri);
      }
      if (isik > karanlikSiniri && !karanlikUyari) {
        tft.fillScreen(ST77XX_BLACK);
        cizKorkmusYuz();
        tft.setTextSize(1); tft.setTextColor(ST77XX_RED);
        tft.setCursor(45, 110); tft.print("Korkuyorum!");
        if (merhabaCalindiMi) sesCal(karanlik_mp3, sizeof(karanlik_mp3));
        karanlikUyari = true;
      } else if (isik <= karanlikSiniri && karanlikUyari) {
        tft.fillScreen(ST77XX_BLACK);
        cizBekleyenYuz();
        tft.setTextSize(1); tft.setTextColor(ST77XX_YELLOW);
        tft.setCursor(50, 110); tft.print("Aydinlik");
        karanlikUyari = false;
      }
      break;
    }

    case 6: {
      static bool sarsintiUyari = false;
      if (oncekiDurum != 6) {
        tft.fillScreen(ST77XX_BLACK);
        oncekiDurum = 6;
        sarsintiUyari = !(tiltDurumu == 1);
      }
      if (tiltDurumu == 1 && !sarsintiUyari) {
        tft.fillScreen(ST77XX_BLACK);
        cizSallananYuz();
        tft.setTextSize(1); tft.setTextColor(ST77XX_RED);
        tft.setCursor(35, 110); tft.print("Sallaniyoruz!");
        sarsintiUyari = true;
      } else if (tiltDurumu != 1 && sarsintiUyari) {
        tft.fillScreen(ST77XX_BLACK);
        cizBekleyenYuz();
        tft.setTextSize(1); tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(65, 110); tft.print("Sabit");
        sarsintiUyari = false;
      }
      break;
    }

    case 7: {
      if (oncekiDurum != 7) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(2); tft.setTextColor(ST77XX_CYAN);
        tft.setCursor(25, 20); tft.print("Kronometre");
        oncekiDurum = 7;
      }
      int gecerliSaniye = (millis() - kronoBaslangic) / 1000;
      if (gecerliSaniye != sonSaniye) {
        tft.fillRect(0, 65, 160, 40, ST77XX_BLACK);
        tft.setTextSize(4); tft.setTextColor(ST77XX_GREEN);
        String s = String(gecerliSaniye) + "s";
        tft.setCursor((160 - (s.length() * 24)) / 2, 70);
        tft.print(s);
        sonSaniye = gecerliSaniye;
      }
      break;
    }

    case 8: {
      if (oncekiDurum != 8) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(2); tft.setTextColor(ST77XX_MAGENTA);
        tft.setCursor(30, 20); tft.print("POMODORO");
        oncekiDurum = 8;
      }
      long kalanZaman = (25 * 60) - ((millis() - pomodoroBaslangic) / 1000);
      if (kalanZaman < 0) kalanZaman = 0;
      if (kalanZaman != sonPomoSaniye) {
        tft.fillRect(0, 60, 160, 40, ST77XX_BLACK);
        tft.setTextSize(4); tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(20, 60);
        tft.printf("%02d:%02d", (int)(kalanZaman/60), (int)(kalanZaman%60));
        sonPomoSaniye = kalanZaman;
      }
      break;
    }

    case 9: {
      static int   xoxTahta[9]   = {0,0,0,0,0,0,0,0,0};
      static int   xoxSecili     = 0;
      static bool  xoxBitti      = false;
      static int   xoxOyuncuSkor = 0;
      static int   xoxRobotSkor  = 0;
      static String xoxDurumStr  = "oyuncu_oynuyor";
      static unsigned long xoxButonBasma = 0;
      static bool  xoxButonBekle = false;

      if (oncekiDurum != 9) {
        for (int i = 0; i < 9; i++) xoxTahta[i] = 0;
        xoxSecili   = 0;
        xoxBitti    = false;
        xoxDurumStr = "oyuncu_oynuyor";
        oncekiDurum = 9;

        HTTPClient http;
        http.begin(sunucuIP);
        http.addHeader("Content-Type", "application/json");
        http.POST("{\"robot_mesaji\":\"sifirla\"}");
        http.end();

        tft.fillScreen(ST77XX_BLACK);
        tft.setTextSize(1); tft.setTextColor(ST77XX_MAGENTA);
        tft.setCursor(35, 5); tft.print("XOX - Sen X, Ben O");
        tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(50, 18);
        tft.print("S:"); tft.print(xoxOyuncuSkor);
        tft.print(" R:"); tft.print(xoxRobotSkor);
        xoxTahtaCiz(xoxTahta, xoxSecili);

        sesCal(hamle_mp3, sizeof(hamle_mp3));
      }

      if (xoxBitti) {
        if (guncelTouch == HIGH && sonTouchDurumu == LOW) {
          for (int i = 0; i < 9; i++) xoxTahta[i] = 0;
          xoxSecili   = 0;
          xoxBitti    = false;
          xoxDurumStr = "oyuncu_oynuyor";

          HTTPClient http;
          http.begin(sunucuIP);
          http.addHeader("Content-Type", "application/json");
          http.POST("{\"robot_mesaji\":\"sifirla\"}");
          http.end();

          tft.fillScreen(ST77XX_BLACK);
          tft.setTextSize(1); tft.setTextColor(ST77XX_MAGENTA);
          tft.setCursor(35, 5); tft.print("XOX - Sen X, Ben O");
          tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
          tft.setCursor(50, 18);
          tft.print("S:"); tft.print(xoxOyuncuSkor);
          tft.print(" R:"); tft.print(xoxRobotSkor);
          xoxTahtaCiz(xoxTahta, xoxSecili);
        }
        break;
      }

      if (guncelTouch == HIGH && sonTouchDurumu == LOW) {
        xoxButonBasma = millis();
        xoxButonBekle = true;
      }

      if (guncelTouch == LOW && sonTouchDurumu == HIGH && xoxButonBekle) {
        xoxButonBekle = false;
        long sure = millis() - xoxButonBasma;

        if (sure < 800) {
          // Kısa basış: sonraki boş kare
          int eskiSecili = xoxSecili;
          do {
            xoxSecili = (xoxSecili + 1) % 9;
          } while (xoxTahta[xoxSecili] != 0 && xoxSecili != eskiSecili);

          tft.fillScreen(ST77XX_BLACK);
          tft.setTextSize(1); tft.setTextColor(ST77XX_MAGENTA);
          tft.setCursor(35, 5); tft.print("XOX - Sen X, Ben O");
          tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
          tft.setCursor(50, 18);
          tft.print("S:"); tft.print(xoxOyuncuSkor);
          tft.print(" R:"); tft.print(xoxRobotSkor);
          xoxTahtaCiz(xoxTahta, xoxSecili);

        } else {
          // Uzun basış: hamle yap
          if (xoxTahta[xoxSecili] == 0) {
            tft.fillRect(0, 115, 160, 13, ST77XX_BLACK);
            tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(5, 118); tft.print("Dusunuyorum...");

            xoxHamleGonder(xoxSecili, xoxTahta, xoxDurumStr,
                           xoxBitti, xoxOyuncuSkor, xoxRobotSkor);

            tft.fillScreen(ST77XX_BLACK);
            tft.setTextSize(1); tft.setTextColor(ST77XX_MAGENTA);
            tft.setCursor(35, 5); tft.print("XOX - Sen X, Ben O");
            tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
            tft.setCursor(50, 18);
            tft.print("S:"); tft.print(xoxOyuncuSkor);
            tft.print(" R:"); tft.print(xoxRobotSkor);
            xoxTahtaCiz(xoxTahta, xoxBitti ? -1 : xoxSecili);

            if (xoxBitti) {
              tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
              tft.setCursor(5, 118); tft.print("Dokun=yeni oyun");
            }
          }
        }
      }
      break;
    }

  } // switch sonu
}
