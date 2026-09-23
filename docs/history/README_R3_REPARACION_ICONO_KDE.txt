QUICKLOOK LINUX R6 — R3 REPARACIÓN CADENA DE ICONO KDE

No se cambia la interfaz ni el motor de preview de R6.

Causa corregida:
- R6 publicaba solo quicklook-linux.png a 512x512.
- Spotlight FIX3E publica tamaños nativos 512/256/128/64/48/32.
- Plasma/KWin podía reescalar el 512 de QuickLook para el panel, produciendo artefactos.
- QuickLook tampoco publicaba una entrada de aplicación quicklook-linux.desktop equivalente a su DesktopFileName.

Corrección:
- Se generan con LANCZOS, exactamente igual que los assets pequeños de Spotlight FIX3E,
  quicklook-linux-256/128/64/48/32.png desde el 512 aprobado.
- Se instalan todos los tamaños en hicolor.
- Se crean quicklook-linux.desktop y quicklook-linux-audio.desktop con Icon=quicklook-linux.
- main.cpp fija identidad estable y QApplication::setWindowIcon global.
- Se refrescan ksycoca e icon-cache.

R6 funcional permanece intacta.
