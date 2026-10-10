# Pacecar - 3 piloti con penalita' e limite di incidenti (Freenove ESP32-S3 CYD 2.8" FNK0104B)

- Sketch: `pacecar_delay/pacecar_delay.ino` (display ILI9341; libreria "GFX Library for Arduino", board "ESP32S3 Dev Module")
- Schema: `pacecar_delay/schema.svg`

## Funzionamento
- Tre pulsanti fisici PILOTA 1 (IO2), PILOTA 2 (IO3), PILOTA 3 (IO14), verso GND.
- Pressione accettata -> attesa X secondi (PENALITA') -> OUT (IO21) alto 400 ms -> RIPARTENZA (nessuna pausa).
- Dal touch: PENALITA' (0..10 s) con [-]/[+] in alto; un unico limite di INCIDENTI (1..10), uguale per i tre piloti, con i suoi [-]/[+] nella riga INCIDENTI.
- Il display mostra per ogni pilota `premute/limite`. Raggiunto il limite il tasto e' disabilitato (rosso) e non fa piu' nulla finche' non si tocca RESET sul display (azzera i tre conteggi).
- Si conta solo una pressione accettata: durante attesa/impulso i tasti sono ignorati e non contati. Se due tasti sono premuti insieme vale il primo (PILOTA 1, 2, 3).
- Subito dopo l'impulso (nessuna pausa) i tasti restano disabilitati e al posto dello stato compare il bottone **RIPARTENZA**: toccandolo OUT va alto 400 ms (senza ritardo X) e finito l'impulso i tre tasti tornano attivi. La ripartenza non conta come pressione. Senza touch la ripartenza viene saltata.
- Penalita' e incidenti sono modificabili solo a ciclo fermo; RESET e' sempre attivo. I contatori sono in RAM: si azzerano a ogni riavvio.

| Funzione | Pin |
|---|---|
| Piloti 1, 2, 3 | IO2, IO3, IO14 (verso GND, pull-up interno) |
| OUT | IO21 |
| Display SPI | MOSI 11, SCLK 12, MISO 13, CS 10, DC 46, BL 45, RST -1, SPI 40 MHz |
| Touch FT6336U (I2C 0x38) | SDA 16, SCL 15, RST 18, INT 17 non usato |

Impostazioni board: ESP32S3 Dev Module, Flash 16MB, Flash Mode DIO, PSRAM OPI, USB CDC On Boot Enabled.

## Schermata di avvio (4 secondi)
All'accensione lo sketch mostra per 4 secondi una schermata e poi passa all'interfaccia. Di default e' una bandiera a scacchi con la scritta SAFETY CAR su due righe, disegnata da codice. Per usare una tua immagine:
1. `pip3 install pillow`
2. `python3 tools/img2splash.py la_tua_immagine.png` (aggiungi `--fit` per non ritagliarla): crea `pacecar_delay/splash_image.h` (320x240, circa 150 KB in flash).
3. Ricompila e carica lo sketch. Per tornare alla schermata di default cancella `splash_image.h`.

## Da verificare al primo avvio
- Colori invertiti: `LCD_IPS` true/false.
- Touch specchiato/ruotato: `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` (con `TOUCH_DEBUG true` si leggono le coordinate su Serial).
- Se compare "TOUCH NON TROVATO" non si possono cambiare penalita' e incidenti (restano 3 s e limite 3).
- IO3 e' un pin di strapping: il pulsante del PILOTA 2 non deve essere premuto durante accensione/reset.

## Note di sicurezza
- Nessun Wi-Fi/BLE/OTA attivo: nessuna superficie di attacco remota.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Il limite di pressioni non e' una protezione di sicurezza: e' solo software e si azzera a ogni riavvio. Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile).
