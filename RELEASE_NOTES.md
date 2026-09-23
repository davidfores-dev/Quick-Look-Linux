# QuickLook Linux 1.0.2 R8.9 — Release Notes

R8.9 es el repack final validado de la rama 1.0.2.

Cambios relevantes acumulados en esta release:

- Contenedor `.run` compatible con la app Instalar, restaurando el formato autocontenido utilizado por R8.1.
- Conserva el centrado dinámico introducido en R8.2.
- Restaura la captura de Espacio en el escritorio Plasma mediante `QUICKLOOK_LINUX_DESKTOP_SPACE_R2`.
- Añade verificación estricta del parche de `FolderView.qml` y backup previo.
- Corrige el fallo de Bash causado por intentar asignar la variable reservada `UID`; R8.9 usa `FILE_UID`, `FILE_GID` y `FILE_MODE`.
- Mantiene la integración existente de Dolphin, el handler `quicklook-linux:` y el launcher de QuickLook.
- Mantiene un hook de pacman para reaplicar la integración del escritorio tras actualizaciones de `plasma-desktop`.

El código funcional incluido en este repack procede de la R8.9 que fue instalada y validada antes de preparar el paquete para GitHub.
