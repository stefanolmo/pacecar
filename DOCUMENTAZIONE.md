# Pacecar - documentazione dell'ultima versione

**Versione:** 3 tasti con ritardo, limite di pressioni comune e conteggi sul display.
**Scheda:** Freenove ESP32-S3 CYD 2.8" **FNK0104B** (240x320 IPS, driver ILI9341, touch capacitivo FT6336U).
**File:** `pacecar_delay/pacecar_delay.ino` (sketch), `pacecar_delay/schema.svg` (schema elettrico), `mockup/layout.png` (proposta grafica, non ancora implementata).

---

## 1. Cosa fa

Tre pulsanti fisici (T1, T2, T3) avviano lo stesso ciclo su un'unica uscita:

1. pressione accettata di un tasto;
2. attesa di **X secondi** (0-10, impostabili dal touch);
3. uscita **IO21 a livello alto per 400 ms**;
4. **pausa di 5 s** in cui i tasti sono ignorati;
5. si torna in attesa del prossimo tasto.

Ogni tasto ha un **conteggio** delle pressioni accettate. Esiste un **limite unico (1-10)**, uguale per tutti i tasti: quando un tasto raggiunge il limite viene **disabilitato** (non fa piu' nulla) finche' non si tocca **RESET** sul display, che azzera i tre conteggi e li riabilita.

### Regole precise
- Conta solo una pressione **accettata**: a ciclo in corso (attesa, impulso, pausa) i tasti sono ignorati e **non vengono contati**.
- Se due tasti sono premuti insieme vale il primo (T1, poi T2, poi T3).
- Ritardo e limite si modificano **solo a ciclo fermo**; RESET e' sempre attivo e non interrompe il ciclo.
- Se si abbassa il limite sotto un conteggio, quel tasto risulta subito disabilitato; rialzandolo si riabilita.
- I contatori sono in RAM: si **azzerano a ogni riavvio o spegnimento**. Valori iniziali: ritardo 3 s, limite 3.

## 2. Interfaccia (versione attuale)

Schermo in orizzontale 320x240:

| Zona | Contenuto |
|---|---|
| In alto | `RITARDO s`: [-] valore [+] |
| Sotto | `LIMITE`: [-] valore [+] (comune ai tre tasti) |
| Tre colonne | T1/IO2, T2/IO3, T3/IO14 con `premute/limite`; in rosso se disabilitato |
| In basso a sinistra | tasto RESET |
| In basso a destra | stato: PRONTO, ATTESA..., IMPULSO, PAUSA |
| Riga finale | `premute / limite`, oppure `TOUCH NON TROVATO` in rosso |

I tasti [-] [+] diventano grigi quando il ciclo e' in corso. Se il touch non viene trovato non si possono cambiare ritardo e limite (restano 3 s e limite 3), ma i tre tasti fisici funzionano.

La nuova grafica proposta (tema scuro, card, due schermate, barra di avanzamento) e' solo nel mockup `mockup/layout.png`: **non e' ancora nel codice**.

## 3. Collegamenti

| Funzione | Pin | Note |
|---|---|---|
| T1 | IO2 | pulsante verso GND, pull-up interno |
| T2 | IO3 | pulsante verso GND; **pin di strapping**: non premere durante accensione/reset |
| T3 | IO14 | pulsante verso GND, pull-up interno |
| OUT | IO21 | 3,3 V, max ~10 mA: i carichi vanno pilotati con un transistor |
| Display SPI | MOSI 11, SCLK 12, MISO 13, CS 10, DC 46, BL 45, RST -1, SPI 40 MHz | da `common.ini` del porting PlatformIO del tutorial FNK0104B |
| Touch FT6336U (I2C 0x38) | SDA 16, SCL 15, RST 18, INT 17 (non usato) | idem |

I quattro pin IO2, IO3, IO14, IO21 sono gli unici liberi sul connettore della scheda (da serigrafia indicata dall'utente). Sono tutti usati.

### Stadio di uscita (vedi `schema.svg`)
`IO21 -> R1 1 kΩ -> base NPN (2N2222 / BC337)`, `R2 10 kΩ` dalla base a GND (pull-down), carico tra `+V` e collettore, emettitore a GND, **diodo 1N4007 in antiparallelo** al carico se e' induttivo (relè, solenoide). GND del carico e della scheda in comune.

- Limite dell'NPN con R1 da 1 kΩ: circa **100 mA**. Oltre usare un MOSFET logic-level (gate 100 Ω + pull-down 10 kΩ).
- R2 serve a tenere spento il carico durante reset e caricamento, quando il pin e' flottante.

## 4. Installazione e caricamento (Arduino IDE)

1. Installa il pacchetto **"esp32" by Espressif Systems** (Gestore schede).
2. Installa la libreria **"GFX Library for Arduino"** (autore moononournation) dal Library Manager. Non va confusa con "Adafruit GFX".
3. Impostazioni in Strumenti:

| Opzione | Valore |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 16MB |
| Flash Mode | DIO (indicato come obbligatorio nel porting del tutorial) |
| PSRAM | OPI PSRAM (modulo N16R8) |
| Upload Speed | 921600 (se da' errori: 460800 o 115200) |

4. La cartella dello sketch deve chiamarsi `pacecar_delay`, come il file `.ino`.
5. Su Linux, se il caricamento da' `Permission denied` su `/dev/ttyACM0`: `sudo usermod -aG dialout $USER`, poi logout/login. In alternativa temporanea: `sudo chmod a+rw /dev/ttyACM0`. Se la porta e' occupata, verifica `ModemManager`.
6. Se l'upload non parte: tieni premuto BOOT, premi e rilascia RST, poi rilascia BOOT.

## 5. Da verificare al primo avvio

Non ho potuto provare lo sketch sulla scheda. Le fonti per display e touch sono un porting di terzi del tutorial ufficiale; non ho potuto aprire la documentazione Freenove.

| Sintomo | Costante nello sketch |
|---|---|
| Colori invertiti | `LCD_IPS` true/false |
| Tocco specchiato o ruotato | `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` (con `TOUCH_DEBUG` a true le coordinate escono sulla seriale) |
| Display nero o immagine corrotta | verifica Flash Mode DIO, PSRAM OPI, velocita' SPI (40 MHz) |
| `TOUCH NON TROVATO` | verifica SDA 16 / SCL 15 / RST 18 |

## 6. Verifiche effettuate

- Controllo di sintassi senza avvisi (con le librerie sostituite da stub).
- Simulazione su PC della logica con tempo, tasti e tocchi simulati: ritardo di 3 s e impulso di 400 ms; il tasto al limite non fa piu' nulla; conteggi indipendenti; pressioni durante il ciclo non contate; RESET riabilita tutto; limite 1-10 e ritardo 0-10 rispettati; modifiche bloccate durante il ciclo.
- **Non verificati:** display e touch reali, orientamento del touch, colori, temporizzazione misurata sull'uscita fisica.

## 7. Sicurezza e limiti

- Nessun Wi-Fi, BLE, OTA o seriale di comando attivi: nessuna superficie di attacco remota.
- Il limite di pressioni **non e' una protezione di sicurezza**: e' solo software e si azzera al riavvio. Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile, timer esterno).
- I pin sono a 3,3 V e non tolleranti ai 5 V.

## 8. Storia delle modifiche

1. Prima versione per LilyGO T-Display-S3.
2. Porting a Waveshare ESP32-C6-LCD-1.47, poi a Freenove ESP32-S3 CYD 2.8" con touch.
3. Prova su Heltec WiFi Kit 8 (abbandonata, scheda originale ripresa).
4. Pin Freenove FNK0104B allineati al tutorial (TFT_RST -1, SPI 40 MHz, reset del solo touch su IO18).
5. Correzione errori di compilazione: `enum State` prima delle funzioni, colori RGB565 definiti nello sketch.
6. Tre tasti fisici con limite per tasto e conteggi sul display.
7. **Limite di pressioni unico per tutti i tasti (versione attuale).**
8. Mockup della nuova interfaccia (proposto, non implementato).

## 9. Prossimi passi possibili

- Nuova interfaccia (font FreeSans, tema scuro, schermata principale + impostazioni, barra di avanzamento), con canvas in memoria per evitare lo sfarfallio.
- Pressione prolungata sui [-] [+] per cambiare valore velocemente.
- Salvataggio di ritardo, limite e conteggi in memoria permanente.
- Calibrazione del touch guidata dal display.
