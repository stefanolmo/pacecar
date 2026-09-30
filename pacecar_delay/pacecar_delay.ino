/*
 * Pacecar - timer di ritardo per Waveshare ESP32-C6-LCD-1.47 (ESP32-C6, ST7789 172x320)
 *
 * Funzionamento:
 *   - tasto START (GPIO3) premuto -> dopo X secondi, OUT (GPIO0) = HIGH per 400 ms
 *   - X (0..10 s) si imposta con due tasti esterni: PLUS (GPIO1) = +1 s, MINUS (GPIO2) = -1 s
 *   - X e' sempre visibile fisso sul display
 *   - dopo l'impulso: pausa di 5 s in cui START e' ignorato, poi si riparte
 *   - durante ritardo/impulso/pausa i tasti +/- sono ignorati
 *
 * Libreria: "GFX Library for Arduino" (moononournation/Arduino_GFX) - installabile da Library Manager
 * Board: "ESP32C6 Dev Module", USB CDC On Boot: Enabled, Flash 4MB
 *
 * Pin display (fissi sulla scheda): MOSI 6, SCLK 7, CS 14, DC 15, RST 21, BL 22.
 * Pin evitati: 4/5 (SD card), 8 (LED RGB), 9 (BOOT), 12/13 (USB).
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ---- Display ----
constexpr int8_t LCD_MOSI = 6, LCD_SCLK = 7, LCD_CS = 14, LCD_DC = 15, LCD_RST = 21, LCD_BL = 22;

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0 /*rotation*/, true /*IPS*/, 172, 320,
                                      34, 0, 34, 0 /*offset: pannello 172 px in ST7789 240*/);

// ---- Pin utente (header della scheda) ----
constexpr uint8_t PIN_PLUS  = 1;   // +1 s  (verso GND)
constexpr uint8_t PIN_MINUS = 2;   // -1 s  (verso GND)
constexpr uint8_t PIN_START = 3;   // START (verso GND)
constexpr uint8_t PIN_OUT   = 0;   // uscita impulso (3.3 V, max ~10 mA: usare un transistor per carichi)

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

// Stampa testo centrato orizzontalmente a y (angolo alto del testo)
void drawCentered(const char* txt, int y, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  gfx->setTextColor(color);
  gfx->setCursor((gfx->width() - w) / 2 - x1, y);
  gfx->print(txt);
}

void drawStatus() {
  gfx->fillRect(0, 128, gfx->width(), 44, BLACK);
  drawCentered(stateLabel(state), 138, 4,
               state == PULSING ? RED : (state == IDLE ? GREEN : YELLOW));
}

void drawDelay() {
  gfx->fillRect(0, 24, gfx->width(), 102, BLACK);
  char buf[4]; snprintf(buf, sizeof(buf), "%u", delayS);
  drawCentered(buf, 34, 12, WHITE);   // 72x96 px per cifra
}

void setState(State s) {
  state = s;
  stateStart = millis();
  digitalWrite(PIN_OUT, s == PULSING ? HIGH : LOW);
  drawStatus();
}

void setup() {
  pinMode(PIN_OUT, OUTPUT); digitalWrite(PIN_OUT, LOW);
  pinMode(LCD_BL, OUTPUT);  digitalWrite(LCD_BL, HIGH);
  btnPlus.begin(); btnMinus.begin(); btnStart.begin();

  gfx->begin();
  gfx->setRotation(1);               // landscape 320x172
  gfx->fillScreen(BLACK);
  drawCentered("RITARDO (s)", 4, 2, CYAN);
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
