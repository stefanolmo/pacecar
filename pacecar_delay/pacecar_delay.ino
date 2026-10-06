/*
 * Pacecar - timer di ritardo con touch per Freenove ESP32-S3 CYD 2.8" FNK0104B (240x320 IPS ILI9341, touch capacitivo FT6336U)
 *
 * Funzionamento:
 *   - tasto START sul touch (oppure pulsante esterno su IO14) -> dopo X secondi, OUT = HIGH per 400 ms
 *   - X (0..10 s) si imposta con i tasti touch [-] e [+]; il valore e' sempre visibile fisso al centro
 *   - dopo l'impulso: pausa di 5 s in cui ogni comando e' ignorato, poi si riparte
 *   - durante ritardo/impulso/pausa i tasti [-] [+] sono disattivati (grigi)
 *
 * Librerie: "GFX Library for Arduino" (moononournation). Il touch FT6336U e' letto via Wire, senza libreria.
 * Board: "ESP32S3 Dev Module", USB CDC On Boot: Enabled, Flash 16MB, Flash Mode DIO, PSRAM OPI (N16R8).
 * Pin display/touch da common.ini del porting PlatformIO del tutorial Freenove FNK0104B
 * (TFT_RST=-1, SPI 40 MHz, ILI9341 con inversione, ordine colori BGR).
 *
 * Pin liberi sul connettore della scheda: IO2, IO3, IO14, IO21 (usati: IO21 = OUT, IO14 = START esterno opzionale).
 *
 * DA VERIFICARE al primo avvio (non confermato da documentazione ufficiale):
 *   - colori invertiti                            -> LCD_IPS true/false
 *   - orientamento del touch                      -> TOUCH_SWAP_XY / TOUCH_FLIP_X / TOUCH_FLIP_Y
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// Colori RGB565 definiti qui: i nomi C_BLACK/C_WHITE/... dipendono dalla versione di Arduino_GFX
constexpr uint16_t C_BLACK = 0x0000, C_WHITE = 0xFFFF, C_RED = 0xF800, C_GREEN = 0x07E0, C_BLUE = 0x001F;
constexpr uint16_t C_CYAN = 0x07FF, C_YELLOW = 0xFFE0, C_ORANGE = 0xFD20, C_DARKGREY = 0x4A49, C_LIGHTGREY = 0xC618;

// Definito prima di ogni funzione: il prototipo automatico dell'IDE Arduino ne ha bisogno
enum State { IDLE, WAITING, PULSING, COOLDOWN };

// ---- Display ILI9341 (SPI) ----
constexpr bool   LCD_IPS = true;     // se i colori sono invertiti, mettere false
constexpr int8_t LCD_MOSI = 11, LCD_SCLK = 12, LCD_MISO = 13, LCD_CS = 10, LCD_DC = 46, LCD_BL = 45;
constexpr int8_t LCD_RST = GFX_NOT_DEFINED;   // il reset del display non e' pilotato da GPIO (TFT_RST=-1)

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, LCD_RST, 0, LCD_IPS);

// ---- Touch FT6336U (I2C 0x38) ----
constexpr uint8_t FT_ADDR = 0x38;
constexpr int8_t TOUCH_SDA = 16, TOUCH_SCL = 15, TOUCH_RST = 18;   // TOUCH_INT = 17 (non usato)
constexpr bool TOUCH_SWAP_XY = true;    // pannello nativo 240x320 portrait -> landscape 320x240
constexpr bool TOUCH_FLIP_X  = false;
constexpr bool TOUCH_FLIP_Y  = true;
constexpr bool TOUCH_DEBUG   = false;   // true: stampa coordinate su Serial per calibrare

// ---- Pin utente ----
constexpr uint8_t PIN_START_HW = 14;    // IO14: pulsante START esterno opzionale (verso GND, pull-up interno)
constexpr uint8_t PIN_OUT      = 21;    // IO21: uscita impulso (3.3 V, max ~10 mA: usare un transistor)
// IO2 e IO3 restano liberi (IO3 e' un pin di strapping: non collegarci nulla che lo forzi al boot)

// ---- Temporizzazioni ----
constexpr uint32_t PULSE_MS    = 400;
constexpr uint32_t COOLDOWN_MS = 5000;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t TOUCH_POLL_MS = 25;
constexpr uint8_t  DELAY_MAX_S = 10;

constexpr int16_t SCREEN_W = 320, SCREEN_H = 240;   // landscape (rotation 1)

struct Rect { int16_t x, y, w, h; };
constexpr Rect BTN_MINUS = {10, 50, 80, 80};
constexpr Rect BTN_PLUS  = {230, 50, 80, 80};
constexpr Rect BTN_START = {40, 150, 240, 70};
constexpr Rect NUM_AREA  = {95, 36, 130, 108};

bool inside(const Rect &r, int16_t x, int16_t y) {
  constexpr int16_t slop = 6;   // margine per dita grosse
  return x >= r.x - slop && x < r.x + r.w + slop && y >= r.y - slop && y < r.y + r.h + slop;
}

// Pulsante fisico attivo basso con debounce; pressed() vale true una sola volta per pressione.
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
Button btnStartHw(PIN_START_HW);

State    state = IDLE;
uint32_t stateStart = 0;
uint8_t  delayS = 3;
bool     touchOk = false;

// ---------------- Touch ----------------
bool probeTouch(int8_t sda, int8_t scl) {
  Wire.end();
  Wire.begin(sda, scl, 400000);
  Wire.beginTransmission(FT_ADDR);
  return Wire.endTransmission() == 0;
}

bool initTouch() {
  pinMode(TOUCH_RST, OUTPUT);                             // reset hardware del solo touch
  digitalWrite(TOUCH_RST, LOW);  delay(10);
  digitalWrite(TOUCH_RST, HIGH); delay(120);
  if (probeTouch(TOUCH_SDA, TOUCH_SCL)) return true;
  Wire.end();
  return false;
}

// true se il dito e' appoggiato; x,y in coordinate landscape 320x240
bool readTouch(int16_t &x, int16_t &y) {
  Wire.beginTransmission(FT_ADDR);
  Wire.write(0x02);                                      // TD_STATUS, poi P1_XH..P1_YL
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)FT_ADDR, 5) != 5) return false;
  uint8_t b[5];
  for (uint8_t &v : b) v = Wire.read();
  if ((b[0] & 0x0F) == 0) return false;
  int16_t rx = ((b[1] & 0x0F) << 8) | b[2];
  int16_t ry = ((b[3] & 0x0F) << 8) | b[4];
  if (TOUCH_DEBUG) Serial.printf("touch raw %d,%d\n", rx, ry);
  x = TOUCH_SWAP_XY ? ry : rx;
  y = TOUCH_SWAP_XY ? rx : ry;
  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);
  if (TOUCH_FLIP_X) x = SCREEN_W - 1 - x;
  if (TOUCH_FLIP_Y) y = SCREEN_H - 1 - y;
  return true;
}

// ---------------- UI ----------------
void drawCenteredIn(const Rect &r, const char *txt, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  gfx->setTextColor(color);
  gfx->setCursor(r.x + (r.w - (int16_t)w) / 2 - x1, r.y + (r.h - (int16_t)h) / 2 - y1);
  gfx->print(txt);
}

void drawButton(const Rect &r, const char *label, uint8_t size, uint16_t fill, uint16_t text) {
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 10, fill);
  gfx->drawRoundRect(r.x, r.y, r.w, r.h, 10, C_WHITE);
  drawCenteredIn(r, label, size, text);
}

void drawAdjustButtons() {
  bool en = (state == IDLE);
  drawButton(BTN_MINUS, "-", 6, en ? C_BLUE : C_DARKGREY, en ? C_WHITE : C_LIGHTGREY);
  drawButton(BTN_PLUS,  "+", 6, en ? C_BLUE : C_DARKGREY, en ? C_WHITE : C_LIGHTGREY);
}

void drawStartButton() {
  switch (state) {
    case IDLE:    drawButton(BTN_START, "START",     4, C_GREEN,    C_BLACK); break;
    case WAITING: drawButton(BTN_START, "ATTESA...", 4, C_ORANGE,   C_BLACK); break;
    case PULSING: drawButton(BTN_START, "IMPULSO",   4, C_RED,      C_WHITE); break;
    default:      drawButton(BTN_START, "PAUSA",     4, C_DARKGREY, C_WHITE); break;
  }
}

void drawDelay() {
  gfx->fillRect(NUM_AREA.x, NUM_AREA.y, NUM_AREA.w, NUM_AREA.h, C_BLACK);
  char buf[4]; snprintf(buf, sizeof(buf), "%u", delayS);
  drawCenteredIn(NUM_AREA, buf, 10, C_WHITE);              // 60x80 px per cifra
}

void setState(State s) {
  state = s;
  stateStart = millis();
  digitalWrite(PIN_OUT, s == PULSING ? HIGH : LOW);
  drawStartButton();
  drawAdjustButtons();
}

void setup() {
  pinMode(PIN_OUT, OUTPUT); digitalWrite(PIN_OUT, LOW);
  pinMode(LCD_BL, OUTPUT);  digitalWrite(LCD_BL, HIGH);
  btnStartHw.begin();
  if (TOUCH_DEBUG) Serial.begin(115200);

  gfx->begin(40000000);                                  // 40 MHz: a 80 MHz il display da' immagini corrotte
  gfx->setRotation(1);                                   // landscape 320x240
  gfx->fillScreen(C_BLACK);
  Rect title = {0, 4, SCREEN_W, 24};
  drawCenteredIn(title, "RITARDO (s)", 2, C_CYAN);

  touchOk = initTouch();
  if (!touchOk) {
    Rect msg = {0, 220, SCREEN_W, 20};
    drawCenteredIn(msg, "TOUCH NON TROVATO - usa IO14", 2, C_RED);
  }
  drawDelay();
  drawStartButton();
  drawAdjustButtons();
}

void loop() {
  static bool     wasDown = false;
  static uint32_t lastPoll = 0;
  bool tapMinus = false, tapPlus = false, tapStart = false;

  if (touchOk && millis() - lastPoll >= TOUCH_POLL_MS) {
    lastPoll = millis();
    int16_t x = 0, y = 0;
    bool down = readTouch(x, y);
    if (down && !wasDown) {                              // solo sul fronte di pressione
      tapMinus = inside(BTN_MINUS, x, y);
      tapPlus  = inside(BTN_PLUS,  x, y);
      tapStart = inside(BTN_START, x, y);
    }
    wasDown = down;
  }
  if (btnStartHw.pressed()) tapStart = true;

  uint32_t elapsed = millis() - stateStart;
  switch (state) {
    case IDLE:
      if (tapPlus  && delayS < DELAY_MAX_S) { delayS++; drawDelay(); }
      if (tapMinus && delayS > 0)           { delayS--; drawDelay(); }
      if (tapStart) setState(WAITING);
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
