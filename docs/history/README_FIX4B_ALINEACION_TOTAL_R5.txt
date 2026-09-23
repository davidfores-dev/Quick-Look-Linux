QUICKLOOK LINUX — FIX4B ALINEACIÓN TOTAL R5
=============================================

BASE EXACTA
- Parte de QuickLook FIX4B ALINEACIÓN TOTAL R4.
- La lógica de alineación R4 se conserva.
- El aspecto visual FIX4B se conserva.
- El icono de tamaño correcto se conserva.

ÚNICO CAMBIO DE TAMAÑO
- Todas las vistas NO-AUDIO:
    tamaño inicial = 820 x 540
  (aproximadamente el tamaño mostrado en la captura de referencia).

- AUDIO:
    tamaño inicial = 760 x 430
  Sin cambios.

ALINEACIÓN
- Todas las vistas NO-AUDIO usan exactamente el mismo X/Y.
- Se mantienen centradas usando la geometría útil real de Qt/KWin.
- Audio queda centrado horizontalmente y su borde superior coincide
  exactamente con el Y superior de las demás ventanas.

INTERACCIÓN (SIN CAMBIOS RESPECTO A R4)
- Se pueden mover.
- Se pueden redimensionar desde bordes/esquinas.
- El contenido se adapta al tamaño.
- Imagen/GIF mantienen COVER y permanecen pegados a los bordes.
- Al abrir una nueva preview se recuperan tamaño y posición iniciales.
