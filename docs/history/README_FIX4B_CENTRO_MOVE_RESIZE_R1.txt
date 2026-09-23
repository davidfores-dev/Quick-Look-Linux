QUICKLOOK LINUX — FIX4B CENTRO + MOVE + RESIZE R1
===================================================

BASE
- Se parte EXCLUSIVAMENTE de la FIX4B adjunta por el usuario.
- No se incorpora el código visual de FIX4C/FIX4D posteriores.

CENTRADO
- Escritorio y Dolphin usan el mismo launcher de FIX4B.
- Conserva el recentrado canónico de FIX4B al mostrar la ventana.
- KWin/Wayland puede terminar de mapear la ventana unos ms después;
  se conservan los recentrados 0/60/180 ms.
- Si el usuario empieza a mover o redimensionar, esos recentrados
  dejan inmediatamente de tocar la ventana.

TAMAÑO
- Se elimina setFixedSize().
- 980x650 (limitado por pantalla) es únicamente el tamaño INICIAL.
- Audio conserva su tamaño inicial original FIX4B.
- La ventana puede hacerse más pequeña o más grande.
- Al cerrar QuickLook, la siguiente preview es un proceso nuevo:
  vuelve a tamaño y posición iniciales.

MOVIMIENTO
- Arrastrar la cabecera usa QWindow::startSystemMove().
- No se roban clics de botones/sliders.

REDIMENSIONADO
- Bordes y esquinas tienen un grip invisible de 9 px.
- Usa QWindow::startSystemResize(), nativo de KDE Wayland.
- Imagen/GIF se recalculan durante el resize.
- La imagen mantiene modo COVER y permanece pegada a los bordes.
- PDF/TXT/ZIP/EPUB/Vídeo siguen el tamaño del layout de Qt.
- La máscara/bisel visual es exactamente la de FIX4B y se actualiza
  con el nuevo tamaño.

ICONO
- Se sustituyen los iconos antiguos de FIX4B por los assets de la última versión.
- Mantiene el mismo dibujo aprobado.
- Fondo exterior transparente.
- Escala visual corregida para cuadrar con iconos cuadrados estilo Apple.
