#include <Arduino.h>
#include <EEPROM.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- PIN ESP32 ---
#define COIN_ACCEPTOR_PIN 2
#define S0 4
#define S1 5
#define S2 18
#define S3 19
#define signal 33
#define OE 17
#define buttonPin 15
#define SERVO_PIN 14
#define BUZZER_PIN 12

Servo myServo;
const unsigned long waktu[] = {1000, 1000};
const int sudut[] = {0, 60};
unsigned long lastMillisServo = 0;
int stateServo = 0;

// EEPROM
#define EEPROM_SIZE 64

// WiFi
const char* ssid = "Joseeeeehhhhh";
const char* password = "naninuneno";
const char* serverName = "https://script.google.com/macros/s/AKfycbwro0-_xc4moL0juVBN1uFQK4W7JSnrs9My1e3g_Ukdcmyp_Zs82KJvb7uJCrDbjfWL/exec";

// Coin
volatile int impulsCount = 0;
unsigned long lastImpulsTime = 0;
const unsigned long COIN_PROCESS_DELAY = 200;
float total_amount = 0.0;

// Cooldown deteksi uang kertas
unsigned long lastDetectTime = 0;
const unsigned long cooldownWaktu = 2000; 

// Toleransi Warna
const int toleransiR = 10;
const int toleransiG = 10;
const int toleransiB = 10;

struct ColorRange {
  int nominal;
  int r_min, r_max, g_min, g_max, b_min, b_max;
};

// Data RGB Uang Kertas
ColorRange ranges[] = {
  {100000, 233 - toleransiR, 233 + toleransiR, 316 - toleransiG, 316 + toleransiG, 297 - toleransiB, 297 + toleransiB},
  //50bersih2022
  {50000, 282 - toleransiR, 282 + toleransiR, 255 - toleransiG, 255 + toleransiG, 213 - toleransiB, 213 + toleransiB},
  //50butek2016
  {50000, 301 - toleransiR, 301 + toleransiR, 272 - toleransiG, 272 + toleransiG, 231 - toleransiB, 231 + toleransiB},
   //20bersih
  {20000, 245 - toleransiR, 245 + toleransiR, 229 - toleransiG, 229 + toleransiG, 228 - toleransiB, 228 + toleransiB},
    //20butek
  {20000, 278 - toleransiR, 278 + toleransiR, 274 - toleransiG, 274 + toleransiG, 284 - toleransiB, 284 + toleransiB},
  //10edisi2022
  {10000, 232 - toleransiR, 232 + toleransiR, 265 - toleransiG, 265 + toleransiG, 225 - toleransiB, 225 + toleransiB},
  {10000, 245 - toleransiR, 245 + toleransiR, 275 - toleransiG, 275 + toleransiG, 233 - toleransiB, 233 + toleransiB},
  //5000bersih2022
  {5000, 174 - toleransiR, 174 + toleransiR, 221 - toleransiG, 221 + toleransiG, 212 - toleransiB, 212 + toleransiB},
  //5000edisi2022butek
  {5000, 209 - toleransiR, 209 + toleransiR, 262 - toleransiG, 262 + toleransiG, 258 - toleransiB, 258 + toleransiB},
  //2000edisi2022
  {2000, 242 - toleransiR, 242 + toleransiR, 248 - toleransiG, 248 + toleransiG, 231 - toleransiB, 231 + toleransiB},
  //2000edisi2012
  {2000, 219 - toleransiR, 219 + toleransiR, 257 - toleransiG, 257 + toleransiG, 245 - toleransiB, 245 + toleransiB},
    // 1000butek2022
  {1000, 185 - toleransiR, 185 + toleransiR, 214 - toleransiG, 214 + toleransiG, 216 - toleransiB, 216 + toleransiB},
  // 1000butek2016
  {1000, 237 - toleransiR, 237 + toleransiR, 295 - toleransiG, 295 + toleransiG, 327 - toleransiB, 327 + toleransiB},

};

unsigned long red, green, blue;
bool modeKalibrasi = false;

void IRAM_ATTR incomingImpuls() {
  impulsCount++;
  lastImpulsTime = millis();
}

void bacaSensor() {
  digitalWrite(S2, LOW);
  digitalWrite(S3, LOW);
  red = pulseIn(signal, HIGH, 100000);

  digitalWrite(S2, HIGH);
  digitalWrite(S3, HIGH);
  green = pulseIn(signal, HIGH, 100000);

  digitalWrite(S2, LOW);
  digitalWrite(S3, HIGH);
  blue = pulseIn(signal, HIGH, 100000);
}

int deteksiNominal(int r, int g, int b) {
  for (auto &rg : ranges) {
    if (r >= rg.r_min && r <= rg.r_max &&
        g >= rg.g_min && g <= rg.g_max &&
        b >= rg.b_min && b <= rg.b_max) {
      return rg.nominal;
    }
  }
  return 0;
}

void kalibrasiWarna() {
  const int jumlahSampel = 5;
  long totalR = 0, totalG = 0, totalB = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Kalibrasi...");
  Serial.println("Masukkan uang kertas di depan sensor...");

  for (int i = 0; i < jumlahSampel; i++) {
    bacaSensor();
    totalR += red;
    totalG += green;
    totalB += blue;
    delay(100);
  }

  int rataR = totalR / jumlahSampel;
  int rataG = totalG / jumlahSampel;
  int rataB = totalB / jumlahSampel;

  Serial.println("=== HASIL KALIBRASI ===");
  Serial.print("Rata-rata R: "); Serial.println(rataR);
  Serial.print("Rata-rata G: "); Serial.println(rataG);
  Serial.print("Rata-rata B: "); Serial.println(rataB);

  Serial.println();
  Serial.print("{NOMINAL, ");
  Serial.print(rataR); Serial.print(" - toleransiR, ");
  Serial.print(rataR); Serial.print(" + toleransiR, ");
  Serial.print(rataG); Serial.print(" - toleransiG, ");
  Serial.print(rataG); Serial.print(" + toleransiG, ");
  Serial.print(rataB); Serial.print(" - toleransiB, ");
  Serial.print(rataB); Serial.println(" + toleransiB},");
  Serial.println("========================");
}

void masukModeDeteksi() {
  if (millis() - lastDetectTime < cooldownWaktu) {
    Serial.println("⏳ Cooldown, tunggu sebentar...");
    return;
  }

  bacaSensor();
  int nilai = deteksiNominal(red, green, blue);

  if (nilai > 0) {
    lastDetectTime = millis();

    total_amount += nilai;
    EEPROM.put(0, total_amount);
    EEPROM.commit();

    myServo.attach(SERVO_PIN);
    myServo.write(0);
    unsigned long startServoTime = millis();
    while (millis() - startServoTime < 3000) { 
      myServo.write(sudut[stateServo]);
      if (millis() - lastMillisServo >= waktu[stateServo]) {
        lastMillisServo = millis();
        stateServo = (stateServo + 1) % 2; 
      }
    }
    myServo.detach();


    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Uang: Rp ");
    lcd.print(nilai);
    lcd.setCursor(0, 1);
    lcd.print("Saldo: Rp ");
    lcd.print((int)total_amount);

    kirimKeSpreadsheet(nilai, total_amount);
    Serial.print("✅ Uang kertas terdeteksi: Rp ");
    Serial.println(nilai);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Uang tidak");
    lcd.setCursor(0, 1);
    lcd.print("dikenali!");
    Serial.println("❌ Tidak dikenali");

    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  }
}

void kirimKeSpreadsheet(int nominal, float total) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(serverName) + "?nominal=" + nominal + "&total=" + total;
    http.begin(url);
    http.useHTTP10(true);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("📤 Data terkirim ke Spreadsheet!");
    } else {
      Serial.print("❌ Gagal kirim. Kode: ");
      Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("⚠️ WiFi tidak terhubung!");
  }
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Mendeteksi...");
  lcd.setCursor(0, 1);
  lcd.print("Saldo: Rp 0");


  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, total_amount);
  if (isnan(total_amount) || total_amount < 0 || total_amount > 10000000) {
    total_amount = 0.0;
    EEPROM.put(0, total_amount);
    EEPROM.commit();
  }

  WiFi.begin(ssid, password);
  Serial.print("🔌 Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi Tersambung!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Tersambung");
  delay(500);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(COIN_ACCEPTOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_ACCEPTOR_PIN), incomingImpuls, FALLING);
  pinMode(buttonPin, INPUT_PULLUP);

  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);
  pinMode(signal, INPUT);
  pinMode(OE, OUTPUT);
  digitalWrite(S0, HIGH);
  digitalWrite(S1, LOW);
  digitalWrite(OE, LOW);

  lcd.setCursor(0, 1);
  lcd.print("Saldo: Rp ");
  lcd.print((int)total_amount);
  Serial.println("=== Sistem Siap ===");
}

void loop() {
  // Tombol → kalibrasi atau deteksi normal
  if (digitalRead(buttonPin) == LOW) {
    unsigned long startPress = millis();
    while (digitalRead(buttonPin) == LOW) {
      delay(10);
    }
    unsigned long pressDuration = millis() - startPress;

    if (pressDuration > 1500) {
      Serial.println("=== MODE KALIBRASI ===");
      modeKalibrasi = true;
      kalibrasiWarna();
      modeKalibrasi = false;
    } else {
      Serial.println("=== MODE DETEKSI NORMAL ===");
      masukModeDeteksi();
    }
  }

  // Deteksi koin
  if (impulsCount > 0 && (millis() - lastImpulsTime > COIN_PROCESS_DELAY)) {
    int nominalKoin = 0;
    if (impulsCount == 1) nominalKoin = 100;
    else if (impulsCount == 2) nominalKoin = 200;
    else if (impulsCount == 3) nominalKoin = 500;
    else if (impulsCount == 4) nominalKoin = 1000;

    if (nominalKoin > 0) {
      total_amount += nominalKoin;
      EEPROM.put(0, total_amount);
      EEPROM.commit();

      Serial.print("✅ Koin terdeteksi: Rp ");
      Serial.println(nominalKoin);
      Serial.print("💰 Saldo total: Rp ");
      Serial.println((int)total_amount);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Koin: Rp ");
      lcd.print(nominalKoin);
      lcd.setCursor(0, 1);
      lcd.print("Saldo: Rp ");
      lcd.print((int)total_amount);

      kirimKeSpreadsheet(nominalKoin, total_amount);
    }
    impulsCount = 0;
  }
}
