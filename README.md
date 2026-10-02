# Pacecar - timer di ritardo con touch (Freenove ESP32-S3 CYD 2.8")

- Sketch: `pacecar_delay/pacecar_delay.ino` (libreria "GFX Library for Arduino", board "ESP32S3 Dev Module")
- Schema: `pacecar_delay/schema.svg`

Ciclo: START (touch o BOOT) -> attesa X s (0..10, impostati con [-]/[+] sul touch) -> OUT alto 400 ms -> pausa 5 s -> pronto.
X e' sempre visibile fisso al centro del display.

| Funzione | Pin |
|---|---|
| Display SPI | MOSI 11, SCLK 12, MISO 13, CS 10, DC 46, BL 45 |
| Touch FT6336U (I2C 0x38) | rilevato in automatico: SDA2/SCL1 oppure SDA16/SCL15 (RST18) |
| START fisico (opz.) | GPIO0 (BOOT) |
| OUT | GPIO21 - da verificare libero sul connettore di espansione |

## Da verificare al primo avvio (non confermato da documentazione ufficiale)
- Driver display ILI9341 (default) o ST7789: `DISPLAY_ILI9341`. Colori invertiti: `LCD_IPS`.
- Touch che risponde specchiato/ruotato: `TOUCH_SWAP_XY`, `TOUCH_FLIP_X`, `TOUCH_FLIP_Y` (con `TOUCH_DEBUG true` si leggono le coordinate su Serial).
- Se compare "TOUCH NON TROVATO" lo sketch funziona comunque con il solo tasto BOOT.

## Note di sicurezza
- Nessun Wi-Fi/BLE/OTA attivo: nessuna superficie di attacco remota.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile): il firmware da solo non garantisce l'OFF se si pianta.
