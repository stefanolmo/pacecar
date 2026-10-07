# Pacecar - 3 tasti con ritardo e limite di pressioni (Freenove ESP32-S3 CYD 2.8" FNK0104B)

- Sketch: `pacecar_delay/pacecar_delay.ino` (display ILI9341; libreria "GFX Library for Arduino", board "ESP32S3 Dev Module")
- Schema: `pacecar_delay/schema.svg`

## Funzionamento
- Tre pulsanti fisici T1 (IO2), T2 (IO3), T3 (IO14), verso GND.
- Pressione accettata -> attesa X secondi -> OUT (IO21) alto 400 ms -> pausa 5 s -> pronto.
- Dal touch: ritardo X (0..10 s) con [-]/[+] in alto; un unico limite di pressioni (1..10), uguale per T1, T2 e T3, con i suoi [-]/[+] nella riga LIMITE.
- Il display mostra per ogni tasto `premute/limite`. Raggiunto il limite il tasto e' disabilitato (rosso) e non fa piu' nulla finche' non si tocca RESET sul display (azzera i tre conteggi).
- Si conta solo una pressione accettata: durante attesa/impulso/pausa i tasti sono ignorati e non contati. Se due tasti sono premuti insieme vale il primo (T1, T2, T3).
- Dopo il ciclo completo (ritardo, impulso, pausa di 5 s) i tasti restano disabilitati e al posto dello stato compare il bottone **RIPARTENZA**: toccandolo OUT va alto 400 ms (senza ritardo X) e finito l'impulso i tre tasti tornano attivi. La ripartenza non conta come pressione. Senza touch la ripartenza viene saltata.
- Ritardo e limite sono modificabili solo a ciclo fermo; RESET e' sempre attivo. I contatori sono in RAM: si azzerano a ogni riavvio.

| Funzione | Pin |
|---|---|
| Tasti T1, T2, T3 | IO2, IO3, IO14 (verso GND, pull-up interno) |
| OUT | IO21 |
| Display SPI | MOSI 11, SCLK 12, MISO 13, CS 10, DC 46, BL 45, RST -1, SPI 40 MHz |
| Touch FT6336U (I2C 0x38) | SDA 16, SCL 15, RST 18, INT 17 non usato |

Impostazioni board: ESP32S3 Dev Module, Flash 16MB, Flash Mode DIO, PSRAM OPI, USB CDC On Boot Enabled.

## Da verificare al primo avvio
- Colori invertiti: `LCD_IPS` true/false.
- Touch specchiato/ruotato: `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` (con `TOUCH_DEBUG true` si leggono le coordinate su Serial).
- Se compare "TOUCH NON TROVATO" non si possono cambiare ritardo e limite (restano 3 s e limite 3).
- IO3 e' un pin di strapping: T2 non deve essere premuto durante accensione/reset.

## Note di sicurezza
- Nessun Wi-Fi/BLE/OTA attivo: nessuna superficie di attacco remota.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Il limite di pressioni non e' una protezione di sicurezza: e' solo software e si azzera a ogni riavvio. Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile).
