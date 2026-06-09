# Masa-st-Asistan-Robot-AnkaBot
BÖZ214 Fiziksel Programlama Final Ödevi


AnkaBot Masaüstü Asistan Robot 🤖AnkaBot, 
insan-robot etkileşimini makro endüstriyel vizyondan çalışma masamıza taşıyan, Nesnelerin İnterneti (IoT) tabanlı, çoklu mod (multimodal) etkileşimli kişisel bir asistan robotudur.  
Sistem, uç cihaz (edge device) olarak görev yapan bir ESP32 mikrodenetleyicisi ile yerel ağ üzerinden haberleşen Python (Flask + Pygame) tabanlı bir masaüstü sunucusundan oluşmaktadır. Sürekli ortam dinlemesi yapan ticari asistanların aksine AnkaBot; mikrofonsuz ve kamerasız yapısıyla donanımsal olarak mutlak mahremiyet (Privacy by Design) sağlar.  


🌟 Temel Özellikler
Zaman Yönetimi: Öğrenciler ve ofis çalışanları için dijital saat, gerçek zamanlı kronometre ve Pomodoro sayacı.  
Görsel ve Fiziksel Etkileşim: 1.8" TFT ekran üzerinden duruma göre değişen yüz animasyonları ("bekleyen", "korkmuş", "sallanan") ve sayfa geçişlerinde çift servo motor ile verilen anlık fiziksel selamlaşma tepkileri.  
Çevresel Farkındalık:
LDR Sensörü: Ortam ışığını algılar karanlıkta robotun yüz ifadesi korkmuş duruma geçer.  
DHT11 Sensörü: Odanın anlık sıcaklık ve nem verilerini ekrana yansıtır.  
Eğim (Tilt) Sensörü: Masadaki sarsıntıları veya devrilmeleri algılayarak motorlar ve ekran animasyonu aracılığıyla anında tepki verir.  
Ağ Üzerinden XOX (Tic-Tac-Toe) Oyunu: Minimax yapay zekâ algoritması ile güçlendirilmiş, ESP32 dokunmatik sensörü veya bilgisayardaki Pygame arayüzü üzerinden eşzamanlı oynanabilen interaktif oyun. Ağ gecikmelerine karşı asenkron HTTP zaman aşımı protokolleri ile korunmaktadır.  


🛠️ Donanım Listesi (BOM)Proje, düşük maliyetli (~960 ₺) ve erişilebilir bileşenlerle tasarlanmıştır:  
Mikrodenetleyici: 1x ESP32 DevKit  
Ekran: 1x 1.8" SPI 128x160 TFT Ekran  
Motorlar: 2x SG90 9g Micro Servo Motor (Sağ ve Sol Kol)  
Sensörler: LDR (Işık), DHT22 (Sıcaklık/Nem), TTP223 (Dokunmatik), Eğim/Tilt Sensörü  
Diğer: Jumper kablolar, dirençler, breadboard ve özel tasarım 3D Baskı Gövde (PLA/PETG)  


💻 Yazılım Mimarisi ve GereksinimlerProje, merkezi olmayan (Edge-to-Server) bir mimari ile çalışır. ESP32 sensör okumaları ve motor kontrollerinden sorumluyken, ağır oyun zekâsı (Minimax algoritması) Python sunucusunda işlenir.  
1. ESP32 Tarafı (C++)Platform: Arduino IDEGerekli Kütüphaneler: Adafruit_GFX, Adafruit_ST7735, WiFi, HTTPClient, DHT, ESP32ServoKurulum: AnkaBot__1_.ino dosyasını Arduino IDE ile açın. ssid ve password değişkenlerine kendi Wi-Fi bilgilerinizi girin. sunucuIP değişkenini Python sunucusunun çalışacağı bilgisayarın yerel IP adresi (örn: [http://192.168.1.](http://192.168.1.)x:5000/hamle-al) olarak güncelleyin ve ESP32'ye yükleyin.
2. 2. Bilgisayar Tarafı (Python)Platform: Python 3.xGerekli Kütüphaneler: flask, pygame, requestsKurulum:Gerekli paketleri terminal üzerinden yükleyin:pip install flask pygame requests    2. `ankabot_beyin.py` dosyasındaki `ESP32_IP` değişkenini, AnkaBot'un ekranında yazan IP adresi ile güncelleyin.
    3. Dosyayı çalıştırın:
       ```bash
python ankabot_beyin.py
4. Pygame penceresi açıldığında sistem iletişime hazır hale gelecektir.


🕹️ Kullanım Senaryosu
Sisteme güç verildiğinde AnkaBot Wi-Fi ağına bağlanır ve gözleri açık şekilde bekleme moduna geçer.Robotun üst kısmında bulunan Dokunmatik Sensöre (TTP223) her basıldığında menüler arası (Saat -> Hava Durumu -> Oda İçi Sıcaklık -> Sensörler -> Kronometre -> Pomodoro -> XOX) geçiş yapılır.  
Sayfa her değiştiğinde robot kollarını bir saniye boyunca havaya kaldırarak geçişi onaylar.  
XOX (9. Sayfa) ekranına gelindiğinde, dokunmatik sensöre kısa basışlar ile kareler arası gezinilebilir, uzun basış ile hamle yapılabilir. Alternatif olarak eşzamanlı çalışan Pygame masaüstü arayüzünden fare ile de oyun kontrol edilebilir.  

Geliştirici: Meliha AteşAnkara Üniversitesi / Bilgisayar ve Öğretim Teknolojileri Eğitimi - Fiziksel Programlama Final Projesi
