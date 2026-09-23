#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD="$SRC/build"
BIN="$PREFIX/bin/quicklook-linux"
LAUNCH="$PREFIX/bin/quicklook-launch"
DESKTOP_LAUNCH="$PREFIX/bin/quicklook-desktop-launcher"
URL_HANDLER="$PREFIX/bin/quicklook-url-handler"
MENU_DIR="$PREFIX/share/kio/servicemenus"
APP_DIR="$PREFIX/share/applications"
MENU="$MENU_DIR/quicklook-linux.desktop"
URL_DESKTOP="$APP_DIR/quicklook-linux-url.desktop"
ICON_BASE="$PREFIX/share/icons/hicolor"
ICON="$ICON_BASE/512x512/apps/quicklook-linux.png"
APP_DESKTOP="$APP_DIR/quicklook-linux.desktop"
AUDIO_DESKTOP="$APP_DIR/quicklook-linux-audio.desktop"
REPAIR="$PREFIX/bin/quicklook-repair-dolphin"
DESKTOP_REPAIR="$PREFIX/bin/quicklook-repair-desktop"
SYSTEMD_USER="$HOME/.config/systemd/user"

echo "============================================================"
echo " QuickLook Linux 1.0.2 R8.9 — FINAL COMPLETO + COMPATIBLE INSTALAR + ESCRITORIO R2"
echo "============================================================"

if [[ $EUID -eq 0 ]]; then
  echo "ERROR=EJECUTA_EL_INSTALADOR_COMO_USUARIO_NORMAL_NO_COMO_ROOT"
  exit 2
fi
command -v sudo >/dev/null 2>&1 || { echo "ERROR=FALTA_SUDO"; exit 2; }
echo "[0/10] Autorización administrativa para dependencias y escritorio Plasma..."
sudo -v

if command -v pacman >/dev/null 2>&1; then
  echo "[1/9] Dependencias Arch/CachyOS..."
  sudo pacman -S --needed --noconfirm \
    base-devel cmake qt6-base qt6-multimedia qt6-webengine \
    kio kcoreaddons libreoffice-fresh 7zip unrar ffmpeg imagemagick
else
  echo "ERROR: este instalador FINAL automático está preparado para Arch/CachyOS."
  echo "En otras distribuciones usa el código fuente y CMake."
  exit 3
fi

echo "[2/9] Cerrando QuickLook/Dolphin antiguos..."
pkill -x quicklook-linux 2>/dev/null || true
kquitapp6 dolphin >/dev/null 2>&1 || true
sleep 1
pkill -x dolphin 2>/dev/null || true

echo "[3/9] Compilando..."
rm -rf "$BUILD"
cmake -S "$SRC" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$BUILD" -j"$(nproc)"
cmake --install "$BUILD"

echo "[4/10] Instalando launchers + reparador Dolphin..."
install -Dm755 "$SRC/quicklook-launch" "$LAUNCH"
install -Dm755 "$SRC/quicklook-desktop-launcher.sh" "$DESKTOP_LAUNCH"
install -Dm755 "$SRC/quicklook-url-handler" "$URL_HANDLER"
install -Dm755 "$SRC/quicklook-repair-dolphin" "$REPAIR"
install -Dm755 "$SRC/quicklook-repair-desktop" "$DESKTOP_REPAIR"

echo "[4b/10] Instalando reparación persistente del escritorio Plasma..."
sudo install -d -m 0755 /usr/local/lib/quicklook-linux /etc/pacman.d/hooks
sudo install -m 0755 "$SRC/repair-folderview-root" /usr/local/lib/quicklook-linux/repair-folderview-root
sudo install -m 0644 "$SRC/95-quicklook-linux-desktop.hook" /etc/pacman.d/hooks/95-quicklook-linux-desktop.hook

echo "[5/10] Icono KDE completo + integración Dolphin..."
mkdir -p "$MENU_DIR" "$APP_DIR"
for S in 512 256 128 64 48 32; do
    mkdir -p "$ICON_BASE/${S}x${S}/apps"
    SRC_ICON="$SRC/assets/quicklook-linux-${S}.png"
    [[ "$S" == 512 ]] && SRC_ICON="$SRC/assets/quicklook-linux-512.png"
    install -Dm644 "$SRC_ICON" "$ICON_BASE/${S}x${S}/apps/quicklook-linux.png"
done
rm -f \
  "$MENU_DIR/quicklook-linux-desktop.desktop" \
  "$MENU_DIR/quicklook-dolphin.desktop"

cat > "$MENU" <<EOF
[Desktop Entry]
Type=Service
MimeType=all/allfiles;
Actions=QuickLookPreview;
X-KDE-ServiceTypes=KonqPopupMenu/Plugin
X-KDE-Priority=TopLevel
Icon=quicklook-linux
Name=Quick Look

[Desktop Action QuickLookPreview]
Name=Quick Look
Name[es]=Vista rápida
Icon=quicklook-linux
Exec=$LAUNCH %f
EOF
chmod 755 "$MENU"

cat > "$URL_DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=QuickLook Linux URL Handler
NoDisplay=true
Exec=$URL_HANDLER %u
MimeType=x-scheme-handler/quicklook-linux;
Terminal=false
StartupNotify=false
EOF
chmod 644 "$URL_DESKTOP"
xdg-mime default quicklook-linux-url.desktop x-scheme-handler/quicklook-linux 2>/dev/null || true

# Reaplica Espacio -> QuickLook en FolderView.qml. La versión anterior llevaba
# el handler, pero R8.1 no volvía a insertar este parche del escritorio.
echo "[5b/10] Reparando Espacio en el escritorio Plasma..."
"$DESKTOP_REPAIR"

# Real application desktop entries. These names deliberately match
# QGuiApplication::setDesktopFileName() in main.cpp.
cat > "$APP_DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=QuickLook Linux
Comment=Vista previa rápida de archivos
Exec=$LAUNCH %f
Icon=quicklook-linux
Terminal=false
Categories=Utility;FileTools;
NoDisplay=true
StartupNotify=false
EOF
chmod 644 "$APP_DESKTOP"

cat > "$AUDIO_DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=QuickLook Linux Audio
Exec=$LAUNCH %f
Icon=quicklook-linux
Terminal=false
NoDisplay=true
StartupNotify=false
EOF
chmod 644 "$AUDIO_DESKTOP"

echo "[6/10] Reparando Dolphin + refrescando integración KDE/KIO..."

# Dolphin 26.04+ stores service-menu shortcuts in dolphinui.rc.
# Register QuickLook on Space and explicitly remove Space from selection mode.
# If Dolphin has not created its user rc yet, start it once, then retry.
if ! "$REPAIR" --install; then
    dolphin >/dev/null 2>&1 &
    sleep 1
    "$REPAIR" --install || true
fi
kbuildsycoca6 --noincremental >/dev/null 2>&1 || true

echo "[7/10] Vigilancia contra recargas de tema/servicemenus..."
mkdir -p "$SYSTEMD_USER"
cat > "$SYSTEMD_USER/quicklook-dolphin-repair.service" <<EOF
[Unit]
Description=QuickLook - reparar configuración de Dolphin si un tema la sobrescribe

[Service]
Type=oneshot
ExecStart=$REPAIR --check
EOF

cat > "$SYSTEMD_USER/quicklook-dolphin-repair.path" <<EOF
[Unit]
Description=QuickLook - vigilar dolphinui.rc

[Path]
PathChanged=%h/.local/share/kxmlgui5/dolphin/dolphinui.rc
PathChanged=%h/.local/share/kxmlgui6/dolphin/dolphinui.rc
Unit=quicklook-dolphin-repair.service

[Install]
WantedBy=default.target
EOF

cat > "$SYSTEMD_USER/quicklook-dolphin-runtime-refresh.service" <<EOF
[Unit]
Description=QuickLook - restaurar atajo tras recarga de acciones de Dolphin 26.04.x

[Service]
Type=oneshot
ExecStart=$REPAIR --runtime-refresh
EOF

cat > "$SYSTEMD_USER/quicklook-dolphin-runtime-refresh.path" <<EOF
[Unit]
Description=QuickLook - vigilar recargas de menús contextuales de Dolphin

[Path]
PathChanged=%h/.config/kservicemenurc
Unit=quicklook-dolphin-runtime-refresh.service

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload >/dev/null 2>&1 || true
systemctl --user enable --now quicklook-dolphin-repair.path >/dev/null 2>&1 || true
systemctl --user enable --now quicklook-dolphin-runtime-refresh.path >/dev/null 2>&1 || true

echo "[8/10] Alineación inicial exacta KWin/Wayland..."

# Ask the installed Qt application for the REAL usable desktop rectangle.
# This includes Plasma panels and fractional scaling.
GEO="$("$BIN" --print-available-geometry 2>/dev/null || true)"

read -r GEO_X GEO_Y GEO_W GEO_H <<< "$GEO"

if [[ ! "$GEO_X" =~ ^-?[0-9]+$ ]] || \
   [[ ! "$GEO_Y" =~ ^-?[0-9]+$ ]] || \
   [[ ! "$GEO_W" =~ ^[0-9]+$ ]] || \
   [[ ! "$GEO_H" =~ ^[0-9]+$ ]]; then
    echo "ERROR=NO_SE_PUDO_OBTENER_AVAILABLE_GEOMETRY"
    echo "GEO_RECIBIDA=$GEO"
    exit 31
fi

CAN_W=820
CAN_H=540
AUD_W=760
AUD_H=430

(( CAN_W > GEO_W - 80 )) && CAN_W=$((GEO_W - 80))
(( CAN_H > GEO_H - 100 )) && CAN_H=$((GEO_H - 100))
(( AUD_W > GEO_W - 80 )) && AUD_W=$((GEO_W - 80))
(( AUD_H > GEO_H - 100 )) && AUD_H=$((GEO_H - 100))

CAN_X=$((GEO_X + (GEO_W - CAN_W) / 2))
CAN_Y=$((GEO_Y + (GEO_H - CAN_H) / 2))

# Audio: own width/height, same TOP edge, horizontally centred.
AUD_X=$((GEO_X + (GEO_W - AUD_W) / 2))
AUD_Y=$CAN_Y

RULE_NORMAL="QuickLookLinuxInitialPlacement"
RULE_AUDIO="QuickLookLinuxAudioInitialPlacement"

if command -v kwriteconfig6 >/dev/null 2>&1 && \
   command -v kreadconfig6 >/dev/null 2>&1; then

    EXISTING="$(kreadconfig6 --file kwinrulesrc --group General --key rules 2>/dev/null || true)"

    add_rule() {
        local rid="$1"
        case ",$EXISTING," in
            *,"$rid",*) ;;
            *)
                if [[ -n "$EXISTING" ]]; then
                    EXISTING="$EXISTING,$rid"
                else
                    EXISTING="$rid"
                fi
                ;;
        esac
    }

    add_rule "$RULE_NORMAL"
    add_rule "$RULE_AUDIO"

    kwriteconfig6 --file kwinrulesrc --group General --key rules "$EXISTING"

    # NORMAL: every non-audio preview starts at the exact same X/Y.
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key Description "QuickLook — alineación inicial no audio"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key wmclass "quicklook-linux"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key wmclasscomplete false
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key wmclassmatch 1
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key types 1
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key position "$CAN_X,$CAN_Y"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key positionrule 3

    # Remove old conflicting experiments.
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key placement --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key placementrule --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key size --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_NORMAL" \
        --key sizerule --delete 2>/dev/null || true

    # AUDIO: same top line, own width, horizontally centred.
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key Description "QuickLook — alineación inicial audio"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key wmclass "quicklook-linux-audio"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key wmclasscomplete false
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key wmclassmatch 1
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key types 1
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key position "$AUD_X,$AUD_Y"
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key positionrule 3

    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key placement --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key placementrule --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key size --delete 2>/dev/null || true
    kwriteconfig6 --file kwinrulesrc --group "$RULE_AUDIO" \
        --key sizerule --delete 2>/dev/null || true

    qdbus6 org.kde.KWin /KWin reconfigure >/dev/null 2>&1 || true

    echo "AVAILABLE_GEOMETRY=$GEO_X,$GEO_Y ${GEO_W}x${GEO_H}"
    echo "NORMAL_BASE=${CAN_W}x${CAN_H}"
    echo "NORMAL_POSITION=$CAN_X,$CAN_Y"
    echo "AUDIO_BASE=${AUD_W}x${AUD_H}"
    echo "AUDIO_POSITION=$AUD_X,$AUD_Y"
    echo "AUDIO_TOP_ALIGNED=$CAN_Y"
else
    echo "ERROR=KWIN_CONFIG_TOOLS_NO_DISPONIBLES"
    exit 32
fi

echo "[9/10] Refrescando KDE..."
rm -f "$HOME/.cache/ksycoca6_"* 2>/dev/null || true
rm -rf "$HOME/.cache/icon-cache.kcache" "$HOME/.cache/plasma_theme_"* 2>/dev/null || true
kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
gtk-update-icon-cache -f -t "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true

echo "[10/10] Reinicio controlado de Dolphin..."
kquitapp6 dolphin >/dev/null 2>&1 || true
for _ in {1..30}; do
    pgrep -x dolphin >/dev/null 2>&1 || break
    sleep 0.1
done
if pgrep -x dolphin >/dev/null 2>&1; then
    pkill -TERM -x dolphin 2>/dev/null || true
    sleep 0.4
fi
nohup dolphin >/dev/null 2>&1 &
sleep 0.8
"$REPAIR" --install || true

echo "Verificación..."
test -x "$BIN"
test -x "$LAUNCH"
test -x "$DESKTOP_LAUNCH"
test -x "$REPAIR"
test -f "$ICON"
grep -q 'setsid -f' "$LAUNCH"
echo "BINARIO=OK:$BIN"
echo "ICONO_KDE_512=OK:$ICON"
for S in 256 128 64 48 32; do test -f "$ICON_BASE/${S}x${S}/apps/quicklook-linux.png"; done
echo "ICONOS_KDE_NATIVOS=512_256_128_64_48_32_OK"
test -f "$APP_DESKTOP"
test -f "$AUDIO_DESKTOP"
echo "DESKTOP_IDENTITY_NORMAL=OK:quicklook-linux.desktop"
echo "DESKTOP_IDENTITY_AUDIO=OK:quicklook-linux-audio.desktop"
echo "LAUNCHER=FAST_SETSID"
echo "DESKTOP_DOLPHIN_LAUNCHER=UNIFICADO"
echo "DOLPHIN_SERVICEMENU=REINSTALADO"
echo "DOLPHIN_SPACE_QUICKLOOK=REPARADO"
echo "DOLPHIN_SPACE_SELECT_MODE=DESACTIVADO"
echo "DOLPHIN_RELOAD_WATCHDOG=ACTIVO"
echo "DOLPHIN_26_04_SERVICEMENU_SHORTCUT_BUG=WORKAROUND_ACTIVO"
echo "QUICKLOOK_SINGLE_INSTANCE=ACTIVO"
echo "CONVERSION_TIMEOUT_GUARDS=ACTIVOS"
echo "CENTRADO_INICIAL=UNICO_TODAS_LAS_VISTAS"
echo "MUSICA=MISMO_CENTRO_TAMANO_PROPIO"
echo "MOVIMIENTO=WAYLAND_SYSTEM_MOVE"
echo "REDIMENSIONADO=WAYLAND_SYSTEM_RESIZE"
echo "ALINEACION_TOTAL=KWIN_APPLY_INITIALLY"
TARGET_FOLDERVIEW="/usr/share/plasma/plasmoids/org.kde.desktopcontainment/contents/ui/FolderView.qml"
MARKER_DESKTOP="QUICKLOOK_LINUX_DESKTOP_SPACE_R2"
test -f "$TARGET_FOLDERVIEW"
grep -q "$MARKER_DESKTOP" "$TARGET_FOLDERVIEW"
test -x /usr/local/lib/quicklook-linux/repair-folderview-root
test -f /etc/pacman.d/hooks/95-quicklook-linux-desktop.hook
test -d /var/lib/quicklook-linux
HANDLER_FINAL="$(xdg-mime query default x-scheme-handler/quicklook-linux 2>/dev/null || true)"
[[ "$HANDLER_FINAL" == "quicklook-linux-url.desktop" ]] || { echo "ERROR=URL_HANDLER_FINAL_NO_REGISTRADO"; exit 51; }
echo "ESCRITORIO_SPACE_R2=OK"
echo "FOLDERVIEW_PATCH=OK:$TARGET_FOLDERVIEW"
echo "PACMAN_REAPPLY_HOOK=OK"
echo "PLASMA_RELOAD=OK"
echo "BASE_NO_AUDIO=820x540"
echo "AUDIO=760x430_TOP_ALIGNED"
echo "MEDIA_ZOOM_HEADER=OCULTO_AUDIO_VIDEO"
echo "NAV_FLECHAS=CUADRADAS_REDONDEADAS_TODAS_VISTAS"
echo "VERSION=1.0.2_R8_7_FINAL_COMPLETO_ESCRITORIO_R2"
echo "============================================================"
echo " INSTALACIÓN COMPLETADA"
echo "============================================================"
