# Pacecar - timer di ritardo (Heltec WiFi Kit 8, ESP8266 + OLED 0.91")

- Sketch: `pacecar_delay/pacecar_delay.ino` (libreria "U8g2", board "NodeMCU 1.0 (ESP-12E)" o pacchetto Heltec)
- Schema: `pacecar_delay/schema.svg`

| Funzione | GPIO | Note |
|---|---|---|
| START | 0 | tasto PRG onboard (non premerlo durante reset/accensione) |
| +1 s | 12 | pulsante esterno verso GND (pull-up interno) |
| -1 s | 13 | pulsante esterno verso GND |
| OUT | 14 | HIGH 400 ms dopo X s; pilotare i carichi con un NPN |
| OLED | 4, 5, 16 | SDA, SCL, RST (interni) |

Ciclo: START -> attesa X s (0..10) -> OUT alto 400 ms -> pausa 5 s -> pronto.
X resta fisso a sinistra sull'OLED, lo stato a destra.

## Note di sicurezza
- Radio Wi-Fi spenta all'avvio (`WiFi.mode(WIFI_OFF)`): l'ESP8266 altrimenti si riconnette a reti salvate in flash.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile): il firmware da solo non garantisce l'OFF se si pianta.
