QuickLook Linux 1.0.2 R8.2 — DESKTOP + CENTER REPAIR
====================================================

BASE
- Parte byte-a-byte del paquete R8.1 FINAL FIX2 antes de aplicar estos cambios.
- Conserva visor, formatos, aspecto, Dolphin R8.1, watchdogs e integración KDE.

CORRECCIÓN 1 — ESPACIO EN EL ESCRITORIO
- Restaura el parche probado QUICKLOOK_LINUX_DESKTOP_SPACE_R2 en FolderView.qml.
- Ruta: icono seleccionado -> quicklook-linux:URL -> quicklook-url-handler -> quicklook-launch.
- No crea un atajo global y no cambia Space dentro de Gwenview.
- Instala un hook de pacman para intentar reaplicar el parche tras actualizar plasma-desktop.
- Si una futura versión de Plasma cambia el manejador incompatible, el hook no fuerza un parche a ciegas.

CORRECCIÓN 2 — CENTRADO
- Mantiene las reglas KWin necesarias en Wayland.
- Ya no depende de las coordenadas calculadas únicamente el día de instalación.
- quicklook-launch recalcula X/Y en cada apertura usando la geometría útil actual.
- Con varias pantallas toma primero la pantalla donde está el cursor; si no existe, usa la principal.

COMANDOS ÚTILES
- Reparar solo escritorio: ~/.local/bin/quicklook-repair-desktop
- Reinstalar todo: ./install-final.sh
