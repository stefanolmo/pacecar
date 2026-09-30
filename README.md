# Pacecar - timer di ritardo (Waveshare ESP32-C6-LCD-1.47)

- Sketch: `pacecar_delay/pacecar_delay.ino` (libreria "GFX Library for Arduino", board "ESP32C6 Dev Module")
- Schema: `pacecar_delay/schema.svg`

| Funzione | GPIO | Note |
|---|---|---|
| +1 s | 1 | pulsante esterno verso GND (pull-up interno) |
| -1 s | 2 | pulsante esterno verso GND |
| START | 3 | pulsante esterno verso GND |
| OUT | 0 | HIGH 400 ms dopo X s; pilotare i carichi con un NPN |

Ciclo: START -> attesa X s (0..10) -> OUT alto 400 ms -> pausa 5 s -> pronto.
Il display usa GPIO 6, 7, 14, 15, 21, 22. Evitati: 4/5 (SD), 8 (LED RGB), 9 (BOOT), 12/13 (USB).

## Note di sicurezza (analisi statica + revisione hardware)

- Software: nessun Wi-Fi/BLE/OTA/seriale attivo -> nessuna superficie di attacco remota; nessun buffer non limitato; macchina a stati non bloccante.
- Pull-down R2 (10k, base Q1 -> GND): senza, GPIO0 e' flottante durante reset/flash e puo' attivare il carico.
- NPN + R1 1k: affidabile fino a ~100 mA. Oltre usare un MOSFET logic-level (gate 100 ohm + pull-down 10k).
- D1 (1N4007) obbligatorio con carichi induttivi.
- Cavi lunghi ai pulsanti: 100 ohm in serie + 100 nF verso GND per ESD/rimbalzi; i GPIO non sono 5 V tolerant.
- Se un carico bloccato ON e' pericoloso, aggiungere un fusibile/limite hardware: il firmware da solo non garantisce l'OFF se si pianta.
