/*
 * Pacecar - timer di ritardo per LILYGO T-Display-S3 (ESP32-S3, ST7789 1.9" 170x320)
 *
 * Funzionamento:
 *   - tasto START (GPIO21) premuto -> dopo X secondi, OUT (GPIO16) = HIGH per 400 ms
 *   - X (0..10 s) si imposta con i due tasti onboard: KEY2 (GPIO14) = +1 s, BOOT (GPIO0) = -1 s
 *   - X e' sempre visibile fisso sul display
 *   - dopo l'impulso: pausa di 5 s in cui START e' ignorato, poi si riparte
 *   - durante ritardo/impulso/pausa i tasti +/- sono ignorati (X non cambia a ciclo in corso)
 *
 * Librerie: TFT_eSPI (fork LilyGO: https://github.com/Xinyuan-LilyGO/T-Display-S3)
 *   In TFT_eSPI/User_Setup_Select.h abilitare:  #include <User_Setups/Setup206_LilyGo_T_Display_S3.h>
 * Board: "ESP32S3 Dev Module", USB CDC On Boot: Enabled, Flash 16MB, PSRAM OPI (come da LilyGO)
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

// ---- Pin ----
constexpr uint8_t PIN_POWER_ON = 15;  // abilita alimentazione display (necessario a batteria)
constexpr uint8_t PIN_LCD_BL   = 38;  // backlight
constexpr uint8_t PIN_PLUS     = 14;  // tasto onboard KEY2  (+1 s)
constexpr uint8_t PIN_MINUS    = 0;   // tasto onboard BOOT  (-1 s)
constexpr uint8_t PIN_START    = 21;  // tasto esterno START (verso GND, pull-up interno)
constexpr uint8_t PIN_OUT      = 16;  // uscita impulso (3.3 V, max ~10 mA: usare un transistor per carichi)

// ---- Temporizzazioni ----
constexpr uint32_t PULSE_MS    = 400;
constexpr uint32_t COOLDOWN_MS = 5000;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint8_t  DELAY_MAX_S = 10;

TFT_eSPI tft;

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
    case IDLE:     return "PRONTO";
    case WAITING:  return "ATTESA...";
    case PULSING:  return "IMPULSO";
    default:       return "PAUSA";
  }
}

void drawStatus() {
  tft.fillRect(0, 130, 320, 40, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(state == PULSING ? TFT_RED : (state == IDLE ? TFT_GREEN : TFT_YELLOW), TFT_BLACK);
  tft.drawString(stateLabel(state), 160, 150, 4);
}

void drawDelay() {
  tft.fillRect(0, 22, 320, 106, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(delayS), 160, 78, 8);   // font 8: cifre grandi
}

void setState(State s) {
  state = s;
  stateStart = millis();
  digitalWrite(PIN_OUT, s == PULSING ? HIGH : LOW);
  drawStatus();
}

void setup() {
  pinMode(PIN_POWER_ON, OUTPUT); digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_LCD_BL, OUTPUT);   digitalWrite(PIN_LCD_BL, HIGH);
  pinMode(PIN_OUT, OUTPUT);      digitalWrite(PIN_OUT, LOW);
  btnPlus.begin(); btnMinus.begin(); btnStart.begin();

  tft.init();
  tft.setRotation(1);            // landscape 320x170
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("RITARDO (s)", 160, 2, 2);
  drawDelay();
  drawStatus();
}

void loop() {
  bool plus  = btnPlus.pressed();
  bool minus = btnMinus.pressed();
  bool start = btnStart.pressed();
  uint32_t elapsed = millis() - stateStart;

  switch (state) {
    case IDLE:
      if (plus  && delayS < DELAY_MAX_S) { delayS++; drawDelay(); }
      if (minus && delayS > 0)           { delayS--; drawDelay(); }
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
}
