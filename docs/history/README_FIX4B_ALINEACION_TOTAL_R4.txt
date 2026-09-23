QUICKLOOK LINUX — FIX4B ALINEACIÓN TOTAL R4
=============================================

BASE
- R3, derivada de la FIX4B exacta del usuario.
- Icono con tamaño visual correcto conservado.
- Aspecto visual de FIX4B conservado.

TAMAÑO BASE
- Imagen, vídeo, PDF, DOC/DOCX/ODT, EPUB, TXT, ZIP, etc.: 980x650.
- Audio: 760x430.

POSICIÓN
- Todas las ventanas no-audio nacen exactamente en el mismo X/Y.
- Se calcula con QApplication::primaryScreen()->availableGeometry().
- El instalador pregunta al propio binario Qt la geometría útil real.
- KWin aplica la posición como Apply Initially.
- Así Escritorio y Dolphin reciben la misma posición incluso bajo Wayland.

AUDIO
- Conserva 760x430.
- Queda centrado horizontalmente.
- Su borde SUPERIOR usa exactamente el mismo Y que las ventanas 980x650.

MOVIMIENTO / RESIZE
- Se puede mover desde la cabecera.
- Se puede redimensionar desde bordes/esquinas.
- Apply Initially NO bloquea el movimiento posterior.
- Al abrir otra preview, vuelve al rectángulo inicial.
- Imagen/GIF siguen en COVER y pegados a los bordes durante resize.

IMPORTANTE
- Se eliminan de la regla KWin las antiguas claves placement/size
  que podían interferir con la alineación.
