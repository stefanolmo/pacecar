# Pacecar - timer di ritardo con touch (Freenove ESP32-S3 CYD 2.8")

- Sketch: `pacecar_delay/pacecar_delay.ino` (display ILI9341; libreria "GFX Library for Arduino", board "ESP32S3 Dev Module")
- Schema: `pacecar_delay/schema.svg`

Ciclo: START (touch o pulsante su IO14) -> attesa X s (0..10, impostati con [-]/[+] sul touch) -> OUT alto 400 ms -> pausa 5 s -> pronto.
X e' sempre visibile fisso al centro del display.

| Funzione | Pin |
|---|---|
| Display SPI | MOSI 11, SCLK 12, MISO 13, CS 10, DC 46, BL 45 |
| Touch FT6336U (I2C 0x38) | SDA 16, SCL 15, RST 18 (reset condiviso col display), INT 17 non usato |
| START fisico (opz.) | IO14 verso GND (pull-up interno) |
| OUT | IO21 |
| Liberi | IO2, IO3 (strapping: non forzarlo al boot) |

## Da verificare al primo avvio (non confermato da documentazione ufficiale)
- Colori invertiti: `LCD_IPS` true/false.
- Touch specchiato/ruotato: `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` (con `TOUCH_DEBUG true` si leggono le coordinate su Serial).
- I pin del touch (SDA16/SCL15/RST18) sono dedotti dalle varianti Freenove note e dal fatto che IO2 e' libero. Se compare "TOUCH NON TROVATO" lo sketch funziona comunque con il pulsante su IO14.

## Note di sicurezza
- Nessun Wi-Fi/BLE/OTA attivo: nessuna superficie di attacco remota.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile): il firmware da solo non garantisce l'OFF se si pianta.
