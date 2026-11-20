// ============================================================
// RFID Anak Pulang Sekolah — ESP32 + RC522
// Kode dari YouTube Short "RFID Anak Pulang Sekolah"
// Repo: https://github.com/oerban77/rfid-anak-pulang-sekolah
//
// Cara kerja: anak menempelkan kartu RFID ke modul RC522.
// Kalau UID kartu ada di daftar, kunci elektrik terbuka 5 detik,
// LED hijau menyala, dan HP orang tua dapat notifikasi Telegram
// "Alif sudah sampai rumah." Kalau kartu tidak dikenal, pintu
// tetap terkunci dan buzzer berbunyi 3 kali.
//
// Wiring (SPI, lihat schematic.png):
//   RC522 SDA  -> GPIO5    RC522 SCK  -> GPIO18
//   RC522 MOSI -> GPIO23   RC522 MISO -> GPIO19
//   RC522 RST  -> GPIO22   RC522 VCC  -> 3V3 (BUKAN 5V!), GND -> GND
//   Relay IN   -> GPIO26 (kunci elektrik 12V lewat kontak relay)
//   Buzzer (+) -> GPIO25 (aktif HIGH), (-) GND
//   LED hijau  -> GPIO27 (lewat resistor 220 ohm ke GND)
//   LED merah  -> GPIO32 (lewat resistor 220 ohm ke GND)
//
// Daftarkan kartu: upload sketch ini, buka Serial Monitor
// (115200), tempel kartu, salin UID yang tercetak, lalu isi
// ke daftar KARTU_DIIZINKAN di bawah dan upload ulang.
// ============================================================

#include <SPI.h>
#include <MFRC522.h>

#define USE_TELEGRAM 1   // 1 = kirim notifikasi Telegram, 0 = Serial saja

// ============ ISI BAGIAN INI (kalau USE_TELEGRAM = 1) ============
const char* WIFI_SSID     = "NAMA_WIFI_KAMU";
const char* WIFI_PASSWORD = "PASSWORD_WIFI_KAMU";
const String BOT_TOKEN    = "ISI_TOKEN_BOT_DARI_BOTFATHER";
const String CHAT_ID      = "ISI_CHAT_ID_KAMU";
// Cara dapat BOT_TOKEN: chat @BotFather di Telegram -> /newbot
// Cara dapat CHAT_ID: chat @userinfobot di Telegram, catat "Id" kamu.
// ================================================================

// ============ DAFTAR KARTU YANG DIIZINKAN ============
// Ganti UID contoh di bawah dengan UID kartu asli (lihat Serial Monitor).
// Format UID: heksadesimal huruf besar, dipisah spasi, misal "A1 B2 C3 D4".
struct Kartu { const char* uid; const char* nama; };
const Kartu KARTU_DIIZINKAN[] = {
  { "AA BB CC DD", "Alif" },
  { "11 22 33 44", "Khalil" },
};
const int JUMLAH_KARTU = sizeof(KARTU_DIIZINKAN) / sizeof(KARTU_DIIZINKAN[0]);
// =====================================================

#if USE_TELEGRAM
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);
#endif

#define PIN_SS_RFID    5    // SDA RC522
#define PIN_RST_RFID   22
#define PIN_RELAY_KUNCI 26
#define PIN_BUZZER     25
#define PIN_LED_HIJAU  27
#define PIN_LED_MERAH  32

const unsigned long BUKA_MS = 5000;    // kunci terbuka 5 detik
const unsigned long ABAIKAN_MS = 10000; // abaikan kartu yang sama 10 detik

MFRC522 rfid(PIN_SS_RFID, PIN_RST_RFID);
bool telegramAktif = false;
bool kunciTerbuka = false;
unsigned long kunciDibukaPada = 0;
String uidTerakhir = "";
unsigned long uidTerakhirPada = 0;

String bacaUid() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

const char* cariNama(const String& uid) {
  for (int i = 0; i < JUMLAH_KARTU; i++) {
    if (uid == KARTU_DIIZINKAN[i].uid) return KARTU_DIIZINKAN[i].nama;
  }
  return nullptr;
}

void kirimPesan(const String& teks) {
  Serial.println(teks);
#if USE_TELEGRAM
  if (telegramAktif) {
    bot.sendMessage(CHAT_ID, teks, "");
  }
#endif
}

void bukaKunci(const char* nama) {
  digitalWrite(PIN_RELAY_KUNCI, HIGH);
  digitalWrite(PIN_LED_HIJAU, HIGH);
  digitalWrite(PIN_LED_MERAH, LOW);
  kunciTerbuka = true;
  kunciDibukaPada = millis();
  kirimPesan(String(nama) + " sudah sampai rumah. Pintu terbuka.");
}

void tutupKunci() {
  digitalWrite(PIN_RELAY_KUNCI, LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);
  kunciTerbuka = false;
  Serial.println("Pintu terkunci lagi.");
}

void tolakKartu(const String& uid) {
  digitalWrite(PIN_LED_MERAH, HIGH);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(150);
  }
  digitalWrite(PIN_LED_MERAH, LOW);
  Serial.println("Kartu tidak dikenal: " + uid + " (daftarkan UID ini di KARTU_DIIZINKAN)");
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(PIN_RELAY_KUNCI, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  digitalWrite(PIN_RELAY_KUNCI, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);
  digitalWrite(PIN_LED_MERAH, LOW);

#if USE_TELEGRAM
  if (BOT_TOKEN.startsWith("ISI_") || CHAT_ID.startsWith("ISI_")) {
    Serial.println("PERINGATAN: BOT_TOKEN / CHAT_ID belum diisi, notifikasi Telegram nonaktif.");
  } else {
    Serial.print("Menghubungkan WiFi ");
    Serial.print(WIFI_SSID);
    Serial.print(" ... ");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long mulai = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - mulai < 20000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" terhubung!");
      telegramClient.setInsecure();  // tanpa verifikasi sertifikat (sesuai contoh UniversalTelegramBot)
      telegramAktif = true;
    } else {
      Serial.println(" GAGAL. Notifikasi Telegram nonaktif, kunci tetap jalan.");
    }
  }
#endif

  Serial.println("RFID siap. Tempel kartu ke modul RC522.");
  rfid.PCD_DumpVersionToSerial();  // cetak versi firmware RC522 untuk debug
}

void loop() {
  // Tutup kunci lagi setelah 5 detik terbuka.
  if (kunciTerbuka && millis() - kunciDibukaPada >= BUKA_MS) {
    tutupKunci();
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uid = bacaUid();
  rfid.PICC_HaltA();  // hentikan kartu supaya tidak terbaca berulang

  // Abaikan kartu yang sama selama 10 detik (anti spam notifikasi).
  if (uid == uidTerakhir && millis() - uidTerakhirPada < ABAIKAN_MS) return;
  uidTerakhir = uid;
  uidTerakhirPada = millis();

  Serial.print("Kartu ditempel, UID: ");
  Serial.println(uid);

  const char* nama = cariNama(uid);
  if (nama != nullptr) {
    bukaKunci(nama);
  } else {
    tolakKartu(uid);
  }
}
