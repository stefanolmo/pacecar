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
