QuickLook Linux 1.0.2 R8.7 FINAL COMPLETO

Base funcional:
- R8.1: visor + integración KDE/Dolphin estable.
- R8.2: centrado dinámico por pantalla en cada apertura.
- R8.7: restaura de forma estricta el parche original R2 de Espacio en el escritorio Plasma.

Corrección crítica R8.7:
El instalador solicita sudo antes de comenzar, instala el helper root y el hook de pacman,
parchea /usr/share/plasma/plasmoids/org.kde.desktopcontainment/contents/ui/FolderView.qml,
verifica que el SHA cambió y que existe QUICKLOOK_LINUX_DESKTOP_SPACE_R2,
limpia la caché QML y reinicia plasmashell. Si falla, la instalación se detiene con ERROR.

El .run de la release es autocontenido y contiene este SOURCE completo.
