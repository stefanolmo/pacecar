/*
 * Carrera Digital 124/132 - pulsante START via seriale
 * Scheda: ESP32-S3-DevKitC-1 WROOM-1-N16R8 (board "ESP32S3 Dev Module")
 *
 * Protocollo (da carreralib di Thomas Kemmer, reverse engineering di Stephan Hess):
 *   19200 baud 8N1, frame:  '"'  payload  checksum  '$'
 *   START = pressione del "tasto 2" (START/ENTER) = payload "T2"
 *   checksum = carattere ('0' + (somma byte del payload & 0x0F))  ->  "T2" => '6'
 *   Frame completo inviato: "T26$   (5 byte)
 *
 * Cablaggio (livelli 3.3 V dell'ESP32-S3; verifica i livelli della tua CU, vedi README):
 *   GPIO17 (TX1) -> RX della Control Unit
 *   GPIO18 (RX1) <- TX della Control Unit
 *   GND comune
 *   Pulsante tra GPIO4 e GND (pull-up interno)
 *
 * Il monitor seriale USB (115200) mostra cosa viene inviato e cosa risponde la CU.
 */

constexpr uint8_t  PIN_BUTTON = 4;
constexpr int8_t   PIN_CU_TX  = 17;
constexpr int8_t   PIN_CU_RX  = 18;
constexpr uint32_t CU_BAUD    = 19200;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t MIN_GAP_MS  = 1000;   // anti doppio start

// Invia un comando alla CU con framing e checksum Carrera.
void cuSend(const char *payload) {
  uint8_t sum = 0;
  Serial1.write('"');
  for (const char *p = payload; *p; ++p) { Serial1.write(*p); sum += (uint8_t)*p; }
  Serial1.write((char)('0' + (sum & 0x0F)));
  Serial1.write('$');
  Serial1.flush();
}

// Stampa su USB le eventuali risposte della CU (fino a '$' o '#').
void cuDrain() {
  while (Serial1.available()) {
    int c = Serial1.read();
    Serial.write(c);
    if (c == '$' || c == '#') Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(CU_BAUD, SERIAL_8N1, PIN_CU_RX, PIN_CU_TX);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  Serial.println("Pronto: premi il pulsante per inviare START");
}

void loop() {
  static bool     stable = HIGH, last = HIGH;
  static uint32_t tChange = 0, tLastStart = 0;

  bool r = digitalRead(PIN_BUTTON);
  if (r != last) { last = r; tChange = millis(); }
  if ((millis() - tChange) > DEBOUNCE_MS && r != stable) {
    stable = r;
    if (stable == LOW && (millis() - tLastStart > MIN_GAP_MS || tLastStart == 0)) {
      tLastStart = millis();
      cuSend("T2");
      Serial.println("-> START inviato (\"T26$)");
    }
  }
  cuDrain();
}
