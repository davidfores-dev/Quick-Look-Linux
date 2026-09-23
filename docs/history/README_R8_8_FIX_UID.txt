QuickLook Linux 1.0.2 R8.8 — FINAL COMPLETO

Corrección sobre R8.7:
- Corrige el aborto de repair-folderview-root causado por intentar asignar la variable reservada Bash UID.
- Usa FILE_UID / FILE_GID / FILE_MODE.
- Escribe FolderView.qml mediante archivo staged y mv, conservando propietario y permisos.
- Mantiene el visor, Dolphin, centrado dinámico R8.2, handler, hook de pacman y parche de escritorio R2.
- El instalador aborta si FolderView.qml no queda realmente modificado y verificado.
