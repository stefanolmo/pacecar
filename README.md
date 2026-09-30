# Pacecar - timer di ritardo (LILYGO T-Display-S3)

- Sketch: `pacecar_delay/pacecar_delay.ino`
- Schema: `pacecar_delay/schema.svg`

| Funzione | GPIO | Note |
|---|---|---|
| +1 s | 14 | tasto onboard KEY2 |
| -1 s | 0 | tasto onboard BOOT |
| START | 21 | pulsante esterno verso GND (pull-up interno) |
| OUT | 16 | HIGH 400 ms dopo X s; pilotare i carichi con un NPN |

Ciclo: START -> attesa X s (0..10) -> OUT alto 400 ms -> pausa 5 s -> pronto.
Tutti i GPIO usati sono liberi sulla T-Display-S3 (il display usa 5-9, 15, 38-42, 45-48).
