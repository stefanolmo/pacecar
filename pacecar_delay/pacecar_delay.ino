/*
 * Pacecar - timer di ritardo per Heltec WiFi Kit 8 (ESP8266 + OLED SSD1306 0.91" 128x32)
 *
 * Funzionamento:
 *   - tasto START (PRG onboard, GPIO0) -> dopo X secondi, OUT (GPIO14) = HIGH per 400 ms
 *   - X (0..10 s) si imposta con due pulsanti esterni: PLUS (GPIO12) = +1 s, MINUS (GPIO13) = -1 s
 *   - X e' sempre visibile fisso a sinistra sull'OLED; a destra lo stato
 *   - dopo l'impulso: pausa di 5 s in cui ogni comando e' ignorato, poi si riparte
 *   - durante ritardo/impulso/pausa i tasti +/- sono ignorati
 *
 * Libreria: "U8g2" (olikraus), da Library Manager.
 * Board: "NodeMCU 1.0 (ESP-12E Module)" oppure il pacchetto Heltec "WiFi Kit 8"; Flash 4MB.
 *
 * Pin OLED (fissi sulla scheda): SDA 4, SCL 5, RST 16.
 * Pin evitati: 15 (deve essere LOW al boot), 2 (LED + strapping), 9/10 (flash), 1/3 (seriale/USB).
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>

// ---- OLED ----
constexpr uint8_t OLED_RST = 16, OLED_SCL = 5, OLED_SDA = 4;
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// ---- Pin utente ----
constexpr uint8_t PIN_START = 0;    // tasto PRG onboard (verso GND). Non tenerlo premuto durante reset/accensione!
constexpr uint8_t PIN_PLUS  = 12;   // +1 s (verso GND, pull-up interno)
constexpr uint8_t PIN_MINUS = 13;   // -1 s (verso GND, pull-up interno)
constexpr uint8_t PIN_OUT   = 14;   // uscita impulso (3.3 V, max ~10 mA: usare un transistor per carichi)

// ---- Temporizzazioni ----
constexpr uint32_t PULSE_MS    = 400;
constexpr uint32_t COOLDOWN_MS = 5000;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint8_t  DELAY_MAX_S = 10;

// Tasto attivo basso con debounce; pressed() vale true una sola volta per pressione.
struct Button {
  uint8_t  pin;
  bool     stable = HIGH, last = HIGH;
  uint32_t t = 0;
  explicit Button(uint8_t p) : pin(p) {}
  void begin() { pinMode(pin, INPUT_PULLUP); stable = last = digitalRead(pin); }
  bool pressed() {
    bool r = digitalRead(pin);
    if (r != last) { last = r; t = millis(); }
    if (r != stable && millis() - t >= DEBOUNCE_MS) {
      stable = r;
      if (stable == LOW) return true;
    }
    return false;
  }
};

Button btnPlus(PIN_PLUS), btnMinus(PIN_MINUS), btnStart(PIN_START);

enum State { IDLE, WAITING, PULSING, COOLDOWN };
State    state = IDLE;
uint32_t stateStart = 0;
uint8_t  delayS = 3;

const char* stateLabel(State s) {
  switch (s) {
    case IDLE:    return "PRONTO";
    case WAITING: return "ATTESA...";
    case PULSING: return "IMPULSO";
    default:      return "PAUSA";
  }
}

// Ridisegna l'intero display (128x32: buffer completo, ~20 ms)
void draw() {
  char buf[4];
  snprintf(buf, sizeof(buf), "%u", delayS);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso28_tn);               // cifre alte 28 px
  u8g2.drawStr((46 - u8g2.getStrWidth(buf)) / 2, 31, buf);
  u8g2.drawVLine(48, 2, 28);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(54, 11, "RITARDO (s)");
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.drawStr(54, 29, stateLabel(state));
  u8g2.sendBuffer();
}

void setState(State s) {
  state = s;
  stateStart = millis();
  digitalWrite(PIN_OUT, s == PULSING ? HIGH : LOW);
  draw();
}

void setup() {
  pinMode(PIN_OUT, OUTPUT); digitalWrite(PIN_OUT, LOW);
  btnPlus.begin(); btnMinus.begin(); btnStart.begin();

  WiFi.persistent(false);       // l'ESP8266 si riconnette da solo a credenziali salvate: radio spenta
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  u8g2.begin();
  u8g2.setBusClock(400000);
  draw();
}

void loop() {
  bool plus  = btnPlus.pressed();
  bool minus = btnMinus.pressed();
  bool start = btnStart.pressed();
  uint32_t elapsed = millis() - stateStart;

  switch (state) {
    case IDLE:
      if (plus  && delayS < DELAY_MAX_S) { delayS++; draw(); }
      if (minus && delayS > 0)           { delayS--; draw(); }
      if (start) setState(WAITING);
      break;
    case WAITING:
      if (elapsed >= (uint32_t)delayS * 1000UL) setState(PULSING);
      break;
    case PULSING:
      if (elapsed >= PULSE_MS) setState(COOLDOWN);
      break;
    case COOLDOWN:
      if (elapsed >= COOLDOWN_MS) setState(IDLE);
      break;
  }
  yield();
}
