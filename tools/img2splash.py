#!/usr/bin/env python3
"""Converte un'immagine (PNG/JPG/...) nella schermata di avvio dello sketch Pacecar.

Uso:
    python3 tools/img2splash.py la_mia_immagine.png            # ritaglia al centro per riempire 320x240
    python3 tools/img2splash.py la_mia_immagine.png --fit      # adatta senza ritagliare (bande nere)

Scrive pacecar_delay/splash_image.h (RGB565, 320x240 = 153.600 byte in flash).
Richiede Pillow:  pip3 install pillow
Se il file splash_image.h non esiste, lo sketch mostra una schermata di avvio disegnata da codice.
"""
import argparse
import os
import sys

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.exit("Serve Pillow: pip3 install pillow")

W, H = 320, 240   # schermo in orizzontale

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument("immagine", help="file immagine di partenza")
ap.add_argument("--fit", action="store_true", help="adatta l'immagine intera, con bande nere, invece di ritagliarla")
ap.add_argument("-o", "--out", default=os.path.join(os.path.dirname(__file__), "..", "pacecar_delay", "splash_image.h"),
                help="file di uscita (default: pacecar_delay/splash_image.h)")
args = ap.parse_args()

img = Image.open(args.immagine)
img = ImageOps.exif_transpose(img).convert("RGB")      # rispetta la rotazione delle foto da telefono
if args.fit:
    img = ImageOps.contain(img, (W, H), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    canvas.paste(img, ((W - img.width) // 2, (H - img.height) // 2))
    img = canvas
else:
    img = ImageOps.fit(img, (W, H), Image.LANCZOS, centering=(0.5, 0.5))

raw = img.tobytes()                                     # R,G,B,R,G,B,...
vals = [((raw[i] & 0xF8) << 8) | ((raw[i + 1] & 0xFC) << 3) | (raw[i + 2] >> 3) for i in range(0, len(raw), 3)]

lines = []
for i in range(0, len(vals), 12):
    lines.append("  " + ", ".join("0x%04X" % v for v in vals[i:i + 12]) + ",")

with open(args.out, "w") as f:
    f.write("// Generato da tools/img2splash.py - non modificare a mano\n")
    f.write("// Immagine %dx%d, RGB565, in flash\n#pragma once\n#include <stdint.h>\n\n" % (W, H))
    f.write("const uint16_t SPLASH_IMG[%d * %d] PROGMEM = {\n" % (W, H))
    f.write("\n".join(lines))
    f.write("\n};\n")

print("Scritto %s (%d byte di immagine)" % (os.path.normpath(args.out), len(vals) * 2))
