QUICKLOOK LINUX 1.0.2 — VISUAL FIX4A
=====================================

BASE: FIX4 (última versión que sí llegó a instalarse).

REGLAS CERRADAS
- AUDIO: no se modifica.
- TODAS las demás vistas usan exactamente la geometría exterior de VIDEO.
- Texto e imagen: mismo tamaño y misma posición.
- Abrir desde Escritorio o Dolphin: misma posición canónica (centrada en pantalla primaria).
- La ventana restringe el contenido; el contenido nunca dicta el tamaño de la ventana.
- Imagen: modo COVER. Llena todo el hueco hasta las esquinas manteniendo proporción.
- Si COVER recorta por proporción, la imagen puede arrastrarse incluso al 100%.
- PDF/DOCX/ODT/TXT/ZIP/EPUB/VIDEO/IMAGEN: máscara real en m_stack + overlay/bisel encima.
- El backend nunca puede pintar por encima de las esquinas.
- Scrollbar común: gris, 4 px, corta y pegada al borde.
- PDF/Office ya no añade margen interno flotante.

No se toca Dolphin, Space, audio ni su layout.
