QUICKLOOK LINUX — FIX4B CENTRO + MOVE + RESIZE R3
===================================================

BASE
- R2, que a su vez parte de la FIX4B exacta aportada por el usuario.
- Conserva el icono de tamaño visual correcto de la última versión.

GEOMETRÍA INICIAL
- Imagen, vídeo, PDF, DOCX/ODT, EPUB, TXT, ZIP y demás vistas no-audio:
  MISMO tamaño inicial y MISMAS coordenadas.
- Ningún cargador de contenido decide la geometría.
- Se usa una única función: applyCanonicalInitialGeometry().

MÚSICA
- Única excepción de tamaño.
- Conserva su tamaño pequeño aprobado.
- Comparte el MISMO CENTRO geométrico que todas las demás ventanas.

WAYLAND/KWIN
- La geometría canónica se reaplica a 0/60/180 ms tras mostrar,
  para neutralizar diferencias de mapeo entre Escritorio y Dolphin.
- En cuanto el usuario mueve o redimensiona, la app deja de recentrar.

INTERACCIÓN
- Se puede mover desde la cabecera.
- Se puede redimensionar desde bordes/esquinas.
- Al cerrar y abrir una nueva preview, vuelve a tamaño/posición inicial.
- Imágenes siguen en COVER pegadas a los bordes durante resize.
