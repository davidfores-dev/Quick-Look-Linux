QUICKLOOK LINUX 1.0.1 — VISUAL FIX3
====================================

REHECHO DESDE 1.0.0 FINAL.

BORDES / BISELES
- izquierdo: 4 px
- derecho: 4 px
- inferior: 4 px
- superior: más grueso porque contiene cabecera/botones
- vídeo/audio mantienen franja inferior de controles con aire interno
- no hay máscara global ni recorte global del stack

SCROLLBAR COMÚN
- 6 px
- misma barra para TXT/ZIP/EPUB/PDF/Office
- pegada al borde derecho/inferior
- sin flechas
- pequeño margen solo arriba/abajo del thumb/track visual

DOCUMENTOS / WORD / ODT / PDF
- backend original 1.0.0 FINAL
- ocupa todo el interior
- sin panel flotante ni desplazamiento artificial
- scrollbars comunes y finas

ZIP / EPUB / TEXTO
- backend original 1.0.0 FINAL
- mismo bisel 4 px
- mismo scrollbar común

AUDIO
- NO waveform inventada
- portada a la izquierda
- título real centrado a la derecha
- debajo: Duración real
- controles inferiores con margen suficiente respecto a las esquinas

ICONO
- exacto aprobado por el usuario
- solo se elimina el fondo negro EXTERIOR
- el cuadrado negro redondeado del icono se conserva
- PNG transparente embebido e instalado en KDE

NO TOCA:
Dolphin, Space, reproducción, zoom, navegación, formatos ni
arquitectura funcional estable de 1.0.0 FINAL.
