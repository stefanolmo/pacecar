/*
 * Pacecar - 3 piloti con penalita' e limite di incidenti, per Freenove ESP32-S3 CYD 2.8" FNK0104B
 * (240x320 IPS ILI9341, touch capacitivo FT6336U)
 *
 * Funzionamento:
 *   - 3 pulsanti fisici PILOTA 1 (IO2), PILOTA 2 (IO3), PILOTA 3 (IO14), verso GND.
 *   - Pressione accettata -> dopo X secondi (PENALITA'), OUT (IO21) = HIGH per 400 ms.
 *   - PENALITA' (0..10 s) si imposta dal touch con [-] [+]; sempre visibile fisso in alto.
 *   - Un unico limite di INCIDENTI (1..10), uguale per tutti e tre i piloti, si imposta dal touch con [-] [+].
 *     Il display mostra per ogni pilota "premute/limite". Raggiunto il limite quel tasto e' disabilitato
 *     (non fa piu' nulla, scritte in rosso) fino a quando non si tocca RESET sul display.
 *   - Conta solo una pressione accettata: a ciclo in corso (attesa/impulso) i tasti sono ignorati
 *     e non vengono contati. Se due tasti sono premuti insieme vale il primo (PILOTA 1, poi 2, poi 3).
 *   - Subito dopo l'impulso (nessuna pausa) i tasti restano disabilitati e al posto dello
 *     stato compare il bottone "RIPARTENZA" sul display: toccandolo OUT va alto per 400 ms (senza ritardo X);
 *     finito quell'impulso i tre tasti tornano attivi. La ripartenza non incrementa i conteggi.
 *     Se il touch non e' disponibile la ripartenza viene saltata (altrimenti il sistema resterebbe bloccato).
 *   - Durante il ciclo, ritardo e limite non sono modificabili ([-] [+] grigi). RESET e' sempre attivo.
 *   - I contatori sono in RAM: si azzerano a ogni riavvio.
 *
 * Libreria: "GFX Library for Arduino" (moononournation). Il touch FT6336U e' letto via Wire, senza libreria.
 * Board: "ESP32S3 Dev Module", USB CDC On Boot: Enabled, Flash 16MB, Flash Mode DIO, PSRAM OPI (N16R8).
 * Pin display/touch da common.ini del porting PlatformIO del tutorial Freenove FNK0104B
 * (TFT_RST=-1, SPI 40 MHz, ILI9341 con inversione, ordine colori BGR).
 *
 * Pin liberi sul connettore: IO2, IO3, IO14, IO21 (tutti usati). IO3 e' un pin di strapping:
 * il pulsante del PILOTA 2 non deve essere premuto durante accensione/reset.
 *
 * DA VERIFICARE al primo avvio:
 *   - colori invertiti       -> LCD_IPS true/false
 *   - orientamento del touch -> TOUCH_SWAP_XY / TOUCH_FLIP_X / TOUCH_FLIP_Y
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// Colori RGB565 definiti qui: i nomi BLACK/WHITE/... dipendono dalla versione di Arduino_GFX
constexpr uint16_t C_BLACK = 0x0000, C_WHITE = 0xFFFF, C_RED = 0xF800, C_GREEN = 0x07E0, C_BLUE = 0x001F;
constexpr uint16_t C_CYAN = 0x07FF, C_ORANGE = 0xFD20, C_DARKGREY = 0x4A49, C_LIGHTGREY = 0xC618;

// Tipi definiti prima di ogni funzione: il prototipo automatico dell'IDE Arduino ne ha bisogno
enum State { IDLE, WAITING, PULSING, RESTART_WAIT, RESTART_PULSE };

struct Rect { int16_t x, y, w, h; };

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
    if (r != stable && millis() - t >= 30) {            // debounce 30 ms
      stable = r;
      if (stable == LOW) return true;
    }
    return false;
  }
};

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
constexpr uint8_t NUM_KEYS = 3;
constexpr uint8_t KEY_PINS[NUM_KEYS] = {2, 3, 14};   // PILOTA 1=IO2, 2=IO3, 3=IO14 (verso GND, pull-up interno)
constexpr uint8_t PIN_OUT = 21;                      // IO21: uscita impulso (3.3 V, max ~10 mA: usare un transistor)

// ---- Temporizzazioni e limiti ----
constexpr uint32_t PULSE_MS      = 400;
constexpr uint32_t TOUCH_POLL_MS = 25;
constexpr uint8_t  DELAY_MAX_S   = 10;
constexpr uint8_t  LIMIT_MIN     = 1;
constexpr uint8_t  LIMIT_MAX     = 10;

constexpr int16_t SCREEN_W = 320, SCREEN_H = 240;    // landscape (rotation 1)

// ---- Layout (320x240) ----
constexpr Rect LBL_DELAY   = {0, 4, 110, 40};
constexpr Rect BTN_D_MINUS = {112, 4, 48, 40};
constexpr Rect NUM_AREA    = {162, 4, 60, 40};
constexpr Rect BTN_D_PLUS  = {226, 4, 48, 40};
// Zone sensibili dei tasti del ritardo: piu' grandi di quelli disegnati e fino al bordo superiore, perche' vicino
// al bordo il touch capacitivo e' meno preciso (le coordinate possono uscire dall'area disegnata). Non c'e' nulla
// di sensibile sopra o accanto a loro.
constexpr Rect HIT_D_MINUS = {100, 0, 68, 62};
constexpr Rect HIT_D_PLUS  = {218, 0, 68, 62};
constexpr int16_t COL_X[NUM_KEYS] = {6, 110, 214};
constexpr Rect TITLE[NUM_KEYS]     = {{6, 52, 100, 18}, {110, 52, 100, 18}, {214, 52, 100, 18}};
constexpr Rect COUNT[NUM_KEYS]     = {{6, 72, 100, 32}, {110, 72, 100, 32}, {214, 72, 100, 32}};
constexpr Rect LBL_LIMIT   = {0, 112, 110, 40};
constexpr Rect BTN_L_MINUS = {112, 112, 48, 40};
constexpr Rect LIM_AREA    = {162, 112, 60, 40};
constexpr Rect BTN_L_PLUS  = {226, 112, 48, 40};
constexpr Rect BTN_RESET   = {6, 164, 92, 46};
constexpr Rect STATUS_AREA = {106, 164, 208, 46};
constexpr Rect HINT_AREA   = {0, 216, 320, 22};

// ---- Stato ----
Button keys[NUM_KEYS] = { Button(KEY_PINS[0]), Button(KEY_PINS[1]), Button(KEY_PINS[2]) };
State    state = IDLE;
uint32_t stateStart = 0;
uint8_t  delayS = 3;
uint8_t  pressCount[NUM_KEYS] = {0, 0, 0};
uint8_t  limitN = 3;                       // limite comune ai tre piloti
bool     touchOk = false;

bool inside(const Rect &r, int16_t x, int16_t y) {
  constexpr int16_t slop = 3;   // margine per dita grosse, piccolo per non sconfinare nei vicini
  return x >= r.x - slop && x < r.x + r.w + slop && y >= r.y - slop && y < r.y + r.h + slop;
}

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
  if (TOUCH_DEBUG) Serial.printf("touch -> x=%d y=%d\n", x, y);
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
  gfx->fillRoundRect(r.x, r.y, r.w, r.h, 8, fill);
  gfx->drawRoundRect(r.x, r.y, r.w, r.h, 8, C_WHITE);
  drawCenteredIn(r, label, size, text);
}

void drawLimit() {
  gfx->fillRect(LIM_AREA.x, LIM_AREA.y, LIM_AREA.w, LIM_AREA.h, C_BLACK);
  char buf[4]; snprintf(buf, sizeof(buf), "%u", limitN);
  drawCenteredIn(LIM_AREA, buf, 4, C_WHITE);
}

void drawDelay() {
  gfx->fillRect(NUM_AREA.x, NUM_AREA.y, NUM_AREA.w, NUM_AREA.h, C_BLACK);
  char buf[4]; snprintf(buf, sizeof(buf), "%u", delayS);
  drawCenteredIn(NUM_AREA, buf, 4, C_WHITE);             // 24x32 px per cifra
}

// Nome pilota e "premute/limite"; rosso se disabilitato (limite raggiunto)
void drawKey(uint8_t i) {
  bool disabled = pressCount[i] >= limitN;
  gfx->fillRect(COL_X[i], 52, 100, 52, C_BLACK);
  char buf[16];
  snprintf(buf, sizeof(buf), "PILOTA %u", (unsigned)(i + 1));
  drawCenteredIn(TITLE[i], buf, 2, disabled ? C_RED : C_CYAN);
  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)pressCount[i], (unsigned)limitN);
  drawCenteredIn(COUNT[i], buf, 3, disabled ? C_RED : C_WHITE);   // "10/10" = 90 px
}

// Tasti [-] [+] di ritardo e limite: attivi solo a ciclo fermo
void drawAdjustButtons() {
  bool en = (state == IDLE);
  uint16_t fill = en ? C_BLUE : C_DARKGREY, text = en ? C_WHITE : C_LIGHTGREY;
  drawButton(BTN_D_MINUS, "-", 4, fill, text);
  drawButton(BTN_D_PLUS,  "+", 4, fill, text);
  drawButton(BTN_L_MINUS, "-", 4, fill, text);
  drawButton(BTN_L_PLUS,  "+", 4, fill, text);
}

void drawStatus() {
  switch (state) {
    case IDLE:    drawButton(STATUS_AREA, "RACE",      3, C_DARKGREY, C_WHITE);  break;
    case WAITING: drawButton(STATUS_AREA, "Safety Car!", 2, C_ORANGE,  C_BLACK);  break;
    case PULSING:       drawButton(STATUS_AREA, "STOP!",      3, C_RED,      C_WHITE);  break;   // impulso dopo la penalita'
    case RESTART_PULSE: drawButton(STATUS_AREA, "START!",     3, C_GREEN,    C_BLACK);  break;   // impulso dopo RIPARTENZA
    case RESTART_WAIT:  drawButton(STATUS_AREA, "RIPARTENZA", 3, C_GREEN,    C_BLACK);  break;   // bottone attivo
  }
}

void resetCounters() {
  for (uint8_t i = 0; i < NUM_KEYS; i++) { pressCount[i] = 0; drawKey(i); }
}

void setState(State s) {
  state = s;
  stateStart = millis();
  digitalWrite(PIN_OUT, (s == PULSING || s == RESTART_PULSE) ? HIGH : LOW);
  drawStatus();
  drawAdjustButtons();
}

void setup() {
  pinMode(PIN_OUT, OUTPUT); digitalWrite(PIN_OUT, LOW);
  pinMode(LCD_BL, OUTPUT);  digitalWrite(LCD_BL, HIGH);
  for (Button &k : keys) k.begin();
  if (TOUCH_DEBUG) Serial.begin(115200);

  gfx->begin(40000000);                                  // 40 MHz: a 80 MHz il display da' immagini corrotte
  gfx->setRotation(1);                                   // landscape 320x240
  gfx->fillScreen(C_BLACK);
  drawCenteredIn(LBL_DELAY, "PENALITA'", 2, C_CYAN);
  drawCenteredIn(LBL_LIMIT, "INCIDENTI", 2, C_CYAN);

  touchOk = initTouch();
  if (!touchOk) drawCenteredIn(HINT_AREA, "TOUCH NON TROVATO", 2, C_RED);

  drawDelay();
  drawLimit();
  for (uint8_t i = 0; i < NUM_KEYS; i++) drawKey(i);
  drawButton(BTN_RESET, "RESET", 2, C_ORANGE, C_BLACK);
  setState(IDLE);                                        // disegna stato e tasti [-] [+]
}

void loop() {
  static bool     wasDown = false;
  static uint32_t lastPoll = 0;
  bool tapDMinus = false, tapDPlus = false, tapReset = false, tapRestart = false;
  bool tapLMinus = false, tapLPlus = false;
  bool keyPressed[NUM_KEYS];

  for (uint8_t i = 0; i < NUM_KEYS; i++) keyPressed[i] = keys[i].pressed();   // sempre letti: la pressione fuori ciclo si scarta

  if (touchOk && millis() - lastPoll >= TOUCH_POLL_MS) {
    lastPoll = millis();
    int16_t x = 0, y = 0;
    bool down = readTouch(x, y);
    if (down && !wasDown) {                              // solo sul fronte di pressione
      tapDMinus = inside(HIT_D_MINUS, x, y);
      tapDPlus  = inside(HIT_D_PLUS,  x, y);
      tapReset  = inside(BTN_RESET,   x, y);
      tapRestart = inside(STATUS_AREA, x, y);           // vale solo in RESTART_WAIT
      tapLMinus = inside(BTN_L_MINUS, x, y);
      tapLPlus  = inside(BTN_L_PLUS,  x, y);
    }
    wasDown = down;
  }

  if (tapReset) resetCounters();                         // sempre attivo, non tocca il ciclo in corso

  uint32_t elapsed = millis() - stateStart;
  switch (state) {
    case IDLE:
      if (tapDPlus  && delayS < DELAY_MAX_S) { delayS++; drawDelay(); }
      if (tapDMinus && delayS > 0)           { delayS--; drawDelay(); }
      if (tapLPlus  && limitN < LIMIT_MAX) limitN++;
      if (tapLMinus && limitN > LIMIT_MIN) limitN--;
      if (tapLPlus || tapLMinus) {                       // il limite e' comune: ridisegno limite e i tre tasti
        drawLimit();
        for (uint8_t i = 0; i < NUM_KEYS; i++) drawKey(i);
      }
      for (uint8_t i = 0; i < NUM_KEYS; i++) {
        if (keyPressed[i] && pressCount[i] < limitN) {   // tasto abilitato: pressione accettata
          pressCount[i]++;
          drawKey(i);
          setState(WAITING);
          break;                                         // se piu' tasti insieme, vale il primo
        }
      }
      break;
    case WAITING:
      if (elapsed >= (uint32_t)delayS * 1000UL) setState(PULSING);
      break;
    case PULSING:
      if (elapsed >= PULSE_MS) setState(touchOk ? RESTART_WAIT : IDLE);   // senza touch niente ripartenza
      break;
    case RESTART_WAIT:                                   // tasti ignorati finche' non si tocca RIPARTENZA
      if (tapRestart) setState(RESTART_PULSE);
      break;
    case RESTART_PULSE:
      if (elapsed >= PULSE_MS) setState(IDLE);           // finito l'impulso i tasti tornano attivi
      break;
  }
}
