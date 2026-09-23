QUICKLOOK LINUX 1.0.2 — VISUAL FIX4B
=====================================

Base: FIX4A.

CAMBIOS EXCLUSIVOS:
1. PDF / DOCX / ODT:
   - QPdfView sin márgenes externos.
   - separación de páginas = 0.
   - FitToWidth.
   - viewport y frame forzados a blanco.
   - el gris del lienzo del visor no debe volver a filtrarse.

2. VIDEO:
   - conserva el overlay/bisel común.
   - además recibe una máscara redondeada directa sobre QVideoWidget.

3. POSICIÓN:
   - Dolphin y Escritorio usan el mismo launcher.
   - tras mapear la ventana, se recentra a 0/60/180 ms para neutralizar
     diferencias de colocación de KWin.

AUDIO: NO MODIFICADO.
IMAGEN/TEXTO: se conserva la geometría canónica FIX4A.
