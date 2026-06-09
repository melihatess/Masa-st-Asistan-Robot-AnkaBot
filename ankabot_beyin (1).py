import pygame
import sys
import requests
import threading
import time
from flask import Flask, request, jsonify

# ===== AYARLAR =====
ESP32_IP = "http://192.168.1.3:8080"

# ===== FLASK SUNUCUSU =====
app = Flask(__name__)

tahta = [0] * 9
oyun_bitti = False
oyuncu_skoru = 0
robot_skoru = 0
son_mesaj = "Hamle yapmak icin bir kare sec!"
son_durum = "bekliyor"

def esp32_ses_cal(dosya):
    try:
        # json= yerine data= kullanarak boşluksuz string gönderiyoruz
        veri = f'{{"dosya":"{dosya}"}}'
        requests.post(f"{ESP32_IP}/ses", data=veri, headers={'Content-Type': 'application/json'}, timeout=2)
    except Exception as e:
        print("Ses gönderme hatası:", e)

def kazanan_bul(t):
    yollar = [[0,1,2],[3,4,5],[6,7,8],[0,3,6],[1,4,7],[2,5,8],[0,4,8],[2,4,6]]
    for a, b, c in yollar:
        if t[a] != 0 and t[a] == t[b] == t[c]:
            return t[a]
    return -1 if 0 not in t else 0

def minimax(t, robot_oynuyor):
    s = kazanan_bul(t)
    if s == 2:  return 10
    if s == 1:  return -10
    if s == -1: return 0
    skorlar = []
    for i in range(9):
        if t[i] == 0:
            t[i] = 2 if robot_oynuyor else 1
            skorlar.append(minimax(t, not robot_oynuyor))
            t[i] = 0
    return max(skorlar) if robot_oynuyor else min(skorlar)

def en_iyi_hamle(t):
    en_iyi, kare = -999, -1
    for i in range(9):
        if t[i] == 0:
            t[i] = 2
            s = minimax(t, False)
            t[i] = 0
            if s > en_iyi:
                en_iyi, kare = s, i
    return kare

def oyunu_sifirla():
    global tahta, oyun_bitti, son_mesaj, son_durum
    tahta = [0] * 9
    oyun_bitti = False
    son_mesaj = "Yeni oyun! Sen X'sin, ben O'yum."
    son_durum = "bekliyor"

@app.route('/hamle-al', methods=['POST'])
def hamle_al():
    global tahta, oyun_bitti, oyuncu_skoru, robot_skoru, son_mesaj, son_durum
    veri = request.get_json()
    mesaj = veri.get("robot_mesaji", "")

    if "sifirla" in mesaj.lower():
        oyunu_sifirla()
        return jsonify({"durum": "sifirla", "tahta": tahta, "mesaj": son_mesaj})

    if oyun_bitti:
        return jsonify({"durum": "bitti", "tahta": tahta, "mesaj": "Oyun bitti. Sifirla!"})

    try:
        hamle = int(mesaj.strip())
        assert 0 <= hamle <= 8
    except:
        return jsonify({"durum": "hata", "mesaj": "Gecersiz hamle!"})

    if tahta[hamle] != 0:
        return jsonify({"durum": "dolu", "tahta": tahta, "mesaj": f"{hamle}. kare dolu!"})

    tahta[hamle] = 1
    s = kazanan_bul(tahta)

    if s == 1:
        oyuncu_skoru += 1; oyun_bitti = True; son_durum = "oyuncu_kazandi"
        son_mesaj = f"Tebrikler kazandin! Sen:{oyuncu_skoru} Ben:{robot_skoru}"
        threading.Thread(target=esp32_ses_cal, args=("kazandin",), daemon=True).start()
        return jsonify({"durum": son_durum, "tahta": tahta, "mesaj": son_mesaj,
                        "oyuncu_skoru": oyuncu_skoru, "robot_skoru": robot_skoru})
    if s == -1:
        oyun_bitti = True; son_durum = "beraberlik"
        son_mesaj = f"Berabere! Sen:{oyuncu_skoru} Ben:{robot_skoru}"
        threading.Thread(target=esp32_ses_cal, args=("berabere",), daemon=True).start()
        return jsonify({"durum": son_durum, "tahta": tahta, "mesaj": son_mesaj,
                        "oyuncu_skoru": oyuncu_skoru, "robot_skoru": robot_skoru})

    robot_kare = en_iyi_hamle(tahta)
    tahta[robot_kare] = 2
    s = kazanan_bul(tahta)

    if s == 2:
        robot_skoru += 1; oyun_bitti = True; son_durum = "robot_kazandi"
        son_mesaj = f"Kazandim! Sen:{oyuncu_skoru} Ben:{robot_skoru}"
        threading.Thread(target=esp32_ses_cal, args=("kazandim",), daemon=True).start()
        return jsonify({"durum": son_durum, "tahta": tahta, "robot_hamlesi": robot_kare,
                        "mesaj": son_mesaj, "oyuncu_skoru": oyuncu_skoru, "robot_skoru": robot_skoru})
    if s == -1:
        oyun_bitti = True; son_durum = "beraberlik"
        son_mesaj = f"Berabere! Sen:{oyuncu_skoru} Ben:{robot_skoru}"
        threading.Thread(target=esp32_ses_cal, args=("berabere",), daemon=True).start()
        return jsonify({"durum": son_durum, "tahta": tahta, "robot_hamlesi": robot_kare,
                        "mesaj": son_mesaj, "oyuncu_skoru": oyuncu_skoru, "robot_skoru": robot_skoru})

    son_durum = "devam"; son_mesaj = f"Ben {robot_kare}. kareye oynadim."
    threading.Thread(target=esp32_ses_cal, args=("sira",), daemon=True).start()
    return jsonify({"durum": "devam", "tahta": tahta, "robot_hamlesi": robot_kare, "mesaj": son_mesaj})

def flask_baslat():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# ===== PYGAME =====
ARKA=(15,15,30); IZGARA=(80,80,120); X_RENK=(0,200,255); O_RENK=(255,60,60)
YAZI=(220,220,220); BASLIK=(180,100,255); KAZANAN_RENK=(0,255,120)
PANEL=(25,25,50); SECILI_RNK=(255,220,0)
PENCERE_W,PENCERE_H=520,580
TAHTA_X,TAHTA_Y=60,90; KARE_W,KARE_H=120,120

def kare_rect(i):
    s,r=i%3,i//3
    return pygame.Rect(TAHTA_X+s*KARE_W, TAHTA_Y+r*KARE_H, KARE_W, KARE_H)

def kazanan_cizgi_bul(t):
    yollar=[[0,1,2],[3,4,5],[6,7,8],[0,3,6],[1,4,7],[2,5,8],[0,4,8],[2,4,6]]
    for yol in yollar:
        a,b,c=yol
        if t[a]!=0 and t[a]==t[b]==t[c]: return yol
    return None

def hamle_gonder_http(h):
    try:
        r=requests.post("http://127.0.0.1:5000/hamle-al",json={"robot_mesaji":str(h)},timeout=5)
        return r.json()
    except Exception as e:
        return {"durum":"hata","mesaj":str(e)}

def sifirla_http():
    try:
        r=requests.post("http://127.0.0.1:5000/hamle-al",json={"robot_mesaji":"sifirla"},timeout=5)
        return r.json()
    except:
        return {}

def main():
    threading.Thread(target=flask_baslat, daemon=True).start()
    time.sleep(1.0)

    pygame.init()
    ekran=pygame.display.set_mode((PENCERE_W,PENCERE_H))
    pygame.display.set_caption("AnkaBot XOX")
    saat=pygame.time.Clock()

    f_baslik=pygame.font.SysFont("Arial",28,bold=True)
    f_mesaj=pygame.font.SysFont("Arial",18)
    f_skor=pygame.font.SysFont("Arial",22,bold=True)
    f_buton=pygame.font.SysFont("Arial",17,bold=True)
    f_kucuk=pygame.font.SysFont("Arial",14)

    yerel_tahta=[0]*9; yerel_bitti=False
    yerel_mesaj="Hamle yapmak icin bir kareye tikla!"
    oyuncu_s=0; robot_s=0; robot_bekle=False
    kazanan_yol=None; son_durum_l="bekliyor"

    def guncelle(cevap):
        nonlocal yerel_tahta,yerel_bitti,yerel_mesaj,oyuncu_s,robot_s,kazanan_yol,son_durum_l
        if not cevap: return
        if "tahta" in cevap: yerel_tahta=cevap["tahta"]
        yerel_mesaj=cevap.get("mesaj",yerel_mesaj)
        son_durum_l=cevap.get("durum",son_durum_l)
        oyuncu_s=cevap.get("oyuncu_skoru",oyuncu_s)
        robot_s=cevap.get("robot_skoru",robot_s)
        if son_durum_l in("oyuncu_kazandi","robot_kazandi"):
            yerel_bitti=True; kazanan_yol=kazanan_cizgi_bul(yerel_tahta)
        elif son_durum_l=="beraberlik":
            yerel_bitti=True; kazanan_yol=None
        elif son_durum_l=="sifirla":
            yerel_bitti=False; kazanan_yol=None

    calisiyor=True
    while calisiyor:
        ekran.fill(ARKA)

        byz=f_baslik.render("AnkaBot  XOX",True,BASLIK)
        ekran.blit(byz,(PENCERE_W//2-byz.get_width()//2,18))

        syz=f_skor.render(f"Sen (X): {oyuncu_s}    AnkaBot (O): {robot_s}",True,YAZI)
        ekran.blit(syz,(PENCERE_W//2-syz.get_width()//2,55))

        for i in range(1,3):
            pygame.draw.line(ekran,IZGARA,(TAHTA_X+i*KARE_W,TAHTA_Y),(TAHTA_X+i*KARE_W,TAHTA_Y+3*KARE_H),3)
            pygame.draw.line(ekran,IZGARA,(TAHTA_X,TAHTA_Y+i*KARE_H),(TAHTA_X+3*KARE_W,TAHTA_Y+i*KARE_H),3)

        mx,my=pygame.mouse.get_pos()
        for i in range(9):
            r=kare_rect(i)
            if r.collidepoint(mx,my) and yerel_tahta[i]==0 and not yerel_bitti:
                pygame.draw.rect(ekran,(40,40,70),r)
            if yerel_tahta[i]==1:
                cx,cy=r.centerx,r.centery
                pygame.draw.line(ekran,X_RENK,(cx-35,cy-35),(cx+35,cy+35),7)
                pygame.draw.line(ekran,X_RENK,(cx+35,cy-35),(cx-35,cy+35),7)
            elif yerel_tahta[i]==2:
                pygame.draw.circle(ekran,O_RENK,r.center,38,7)

        if kazanan_yol:
            r0=kare_rect(kazanan_yol[0]); r2=kare_rect(kazanan_yol[2])
            pygame.draw.line(ekran,KAZANAN_RENK,r0.center,r2.center,6)

        panel=pygame.Rect(30,TAHTA_Y+3*KARE_H+15,PENCERE_W-60,46)
        pygame.draw.rect(ekran,PANEL,panel,border_radius=10)
        pygame.draw.rect(ekran,IZGARA,panel,1,border_radius=10)
        renk_m=YAZI
        if son_durum_l=="oyuncu_kazandi": renk_m=KAZANAN_RENK
        elif son_durum_l=="robot_kazandi": renk_m=O_RENK
        elif son_durum_l=="beraberlik": renk_m=SECILI_RNK
        myz=f_mesaj.render(yerel_mesaj,True,renk_m)
        ekran.blit(myz,(panel.centerx-myz.get_width()//2,panel.centery-myz.get_height()//2))

        btn=pygame.Rect(PENCERE_W//2-80,TAHTA_Y+3*KARE_H+75,160,40)
        pygame.draw.rect(ekran,(60,60,100),btn,border_radius=8)
        pygame.draw.rect(ekran,IZGARA,btn,1,border_radius=8)
        byz2=f_buton.render("Yeni Oyun  [R]",True,YAZI)
        ekran.blit(byz2,(btn.centerx-byz2.get_width()//2,btn.centery-byz2.get_height()//2))

        eyz=f_kucuk.render(f"AnkaBot ses: {ESP32_IP}",True,(70,70,110))
        ekran.blit(eyz,(PENCERE_W//2-eyz.get_width()//2,TAHTA_Y+3*KARE_H+128))

        if robot_bekle:
            byz3=f_mesaj.render("AnkaBot dusunuyor...",True,O_RENK)
            ekran.blit(byz3,(PENCERE_W//2-byz3.get_width()//2,TAHTA_Y+3*KARE_H+150))

        pygame.display.flip()

        for event in pygame.event.get():
            if event.type==pygame.QUIT: calisiyor=False
            elif event.type==pygame.KEYDOWN:
                if event.key==pygame.K_r:
                    guncelle(sifirla_http())
            elif event.type==pygame.MOUSEBUTTONDOWN and event.button==1:
                if btn.collidepoint(event.pos):
                    guncelle(sifirla_http()); continue
                if not yerel_bitti:
                    for i in range(9):
                        if kare_rect(i).collidepoint(event.pos) and yerel_tahta[i]==0:
                            robot_bekle=True; pygame.display.flip()
                            guncelle(hamle_gonder_http(i))
                            robot_bekle=False; break

        saat.tick(60)

    pygame.quit(); sys.exit()

if __name__=='__main__':
    main()
