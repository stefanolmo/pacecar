# Pacecar - timer di ritardo (Heltec WiFi Kit 8, ESP8266 + OLED 0.91")

- Sketch: `pacecar_delay/pacecar_delay.ino` (libreria "U8g2", board "NodeMCU 1.0 (ESP-12E)" o pacchetto Heltec)
- Schema: `pacecar_delay/schema.svg`

| Funzione | GPIO | Note |
|---|---|---|
| START | 0 (D3) | tasto PRG onboard; un pulsante esterno su D3-GND funziona in parallelo (non premerlo durante reset/accensione) |
| +1 s | 12 (D6) | pulsante esterno verso GND (pull-up interno) |
| -1 s | 13 (D7) | pulsante esterno verso GND |
| OUT | 15 (D8) | HIGH 400 ms dopo X s; deve essere LOW al boot (pull-down sulla scheda); carichi tramite NPN |
| OLED | 4, 5, 16 | SDA, SCL, RST (interni) |

Ciclo: START -> attesa X s (0..10) -> OUT alto 400 ms -> pausa 5 s -> pronto.
X resta fisso a sinistra sull'OLED, lo stato a destra.

Pin letti dalla serigrafia della scheda (D2, D3, D8, D7, D6, SCL, D0, A0): D5/GPIO14 non e' esposto. La corrispondenza D6=12, D7=13, D8=15 segue la numerazione NodeMCU.

## Note di sicurezza
- Radio Wi-Fi spenta all'avvio (`WiFi.mode(WIFI_OFF)`): l'ESP8266 altrimenti si riconnette a reti salvate in flash.
- R2 10k (base Q1 -> GND): senza, il pin e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k affidabile fino a ~100 mA; oltre usare un MOSFET logic-level. D1 obbligatorio con carichi induttivi.
- Se un carico bloccato ON e' pericoloso serve un limite hardware (fusibile): il firmware da solo non garantisce l'OFF se si pianta.
