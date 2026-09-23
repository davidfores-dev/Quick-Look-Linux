# QuickLook Linux

**Version 1.0.2 R8.9 — Final / Compatible con Instalar**

QuickLook Linux lleva una vista previa rápida estilo Quick Look a KDE Plasma en Arch Linux / CachyOS. Permite seleccionar un archivo y pulsar **Espacio** para abrir una vista previa rápida, tanto desde Dolphin como desde el escritorio Plasma.

## R8.9 final

Esta es la release final validada de la rama R8.9. Mantiene el contenedor `.run` compatible con la app **Instalar**, el centrado dinámico de la ventana y la integración con el escritorio Plasma mediante el parche R2 verificado.

### Funciones principales

- Vista rápida con **Espacio** en Dolphin y en el escritorio Plasma.
- Imágenes JPG, PNG, WebP, GIF y otros formatos compatibles.
- PDF y EPUB.
- TXT, código y otros documentos de texto.
- Documentos Office mediante conversión para vista previa.
- Audio y vídeo con controles de reproducción.
- Comprimidos y vista previa de su contenido.
- Zoom y arrastre de imágenes ampliadas.
- Carátulas embebidas para audio cuando están disponibles.
- Ventana sin marco, esquinas redondeadas y centrado dinámico.
- Integración KDE/Dolphin y handler propio de QuickLook.
- Reparación persistente de la integración del escritorio Plasma mediante hook de pacman.

## Instalación recomendada

La release probada está en `release/`:

```bash
chmod +x release/QuickLookLinux_R8_9_FINAL_COMPATIBLE_INSTALAR.run
./release/QuickLookLinux_R8_9_FINAL_COMPATIBLE_INSTALAR.run
```

También puede abrirse con la app **Instalar** del sistema para la que se mantuvo la compatibilidad del contenedor `.run`.

El instalador solicita `sudo` cuando necesita instalar dependencias o modificar la integración de Plasma.

## Compilar desde el código fuente

Dependencias principales de compilación:

- CMake 3.20 o superior
- compilador C++17
- Qt 6: Widgets, Multimedia, MultimediaWidgets, Pdf, PdfWidgets y Network

Compilación manual:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Para instalar el binario compilado en el prefijo configurado por el proyecto puede usarse CMake, aunque para obtener toda la integración de Dolphin/Plasma se recomienda `install-final.sh` o el `.run` de `release/`.

## Estructura

- `src/` — código C++ de la aplicación.
- `assets/` — iconos y recursos gráficos.
- `install-final.sh` — instalador completo de la aplicación e integraciones.
- `quicklook-launch` — launcher principal.
- `quicklook-repair-dolphin` — reparación/integración con Dolphin.
- `quicklook-repair-desktop` — reparación de integración con el escritorio Plasma.
- `repair-folderview-root` — parche verificado de `FolderView.qml` con privilegios administrativos.
- `95-quicklook-linux-desktop.hook` — reaplica la integración después de actualizaciones de Plasma.
- `docs/history/` — notas de las revisiones anteriores conservadas del proyecto.
- `release/` — `.run` final probado y archivo fuente original de R8.9.

## Integridad

Los hashes SHA-256 de los artefactos de release están en `release/SHA256SUMS.txt` y los del repack completo en `SHA256SUMS.txt`.

## Nota sobre licencia

Este repack conserva el estado del proyecto R8.9 y **no añade una licencia nueva**. Si el repositorio va a aceptar reutilización o contribuciones de terceros, conviene añadir explícitamente la licencia que el autor quiera aplicar.
