#include "PreviewWindow.h"
#include <QSizePolicy>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

#include <QApplication>
#include <QAudioOutput>
#include <QCryptographicHash>
#include <QCursor>
#include <QDir>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QSettings>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QHideEvent>
#include <QImageReader>
#include <QIcon>
#include <QKeyEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QMediaPlayer>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QMovie>
#include <QPdfDocument>
#include <QPdfView>
#include <QProcess>
#include <QRegularExpression>
#include <QRegion>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWheelEvent>

void ClickSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && orientation() == Qt::Horizontal) {
        const double ratio = qBound(0.0, event->position().x() / qMax(1, width()), 1.0);
        const int v = minimum() + int(ratio * (maximum() - minimum()));
        setValue(v);
        emit sliderMoved(v);
        emit sliderReleased();
    }
    QSlider::mousePressEvent(event);
}

PreviewWindow::PreviewWindow(const QString &path, QWidget *parent)
    : QMainWindow(parent), m_path(path)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAcceptDrops(true);
    // R3.7: QuickLook owns its complete frame; KWin decoration is removed only here.
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);

    QSettings settings("QuickLookLinux", "QuickLookLinux");
    m_windowOpacity = settings.value("windowOpacity", 0.92).toDouble();
    m_adaptiveWindow = settings.value("adaptiveWindow", true).toBool();
    m_skin = settings.value("skin", "generic").toString();

    if (auto *screen = QApplication::primaryScreen()) {
        const QRect a = screen->availableGeometry();
        resize(qMin(1100, int(a.width() * 0.80)),
               qMin(760, int(a.height() * 0.80)));
    } else {
        resize(1000, 700);
    }

    buildUi();
    if (statusBar()) statusBar()->hide();
    applyAppearance();

    // R3.6: prioridad real de Space/Esc antes de QTextEdit/PDF/etc.
    qApp->installEventFilter(this);
    this->installEventFilter(this);
    if (centralWidget()) centralWidget()->installEventFilter(this);
    m_image->installEventFilter(this);
    m_image->setMouseTracking(true);

    m_zoomRenderTimer = new QTimer(this);
    m_zoomRenderTimer->setSingleShot(true);
    m_zoomRenderTimer->setInterval(70);
    connect(m_zoomRenderTimer, &QTimer::timeout, this, [this]() {
        updateImageScale(true);
    });
    rebuildSiblingList();
    loadFile(path);
}

PreviewWindow::~PreviewWindow()
{
    qApp->removeEventFilter(this);
    stopMovie();
    stopMedia();

    delete m_originalPixmap;
    m_originalPixmap = nullptr;

    delete m_scaledPixmapCache;
    m_scaledPixmapCache = nullptr;

    delete m_pdfDoc;
}

void PreviewWindow::closePreview()
{
    if (m_zoomRenderTimer)
        m_zoomRenderTimer->stop();

    stopMovie();
    stopMedia();
    close();
}

void PreviewWindow::openPath(const QString &path, bool rebuildSiblings)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;

    m_path = fi.absoluteFilePath();
    m_imagePath.clear();
    m_imageZoom = 1.0;
    m_photoViewport = QSize();
    m_wheelAccumulator = 0;
    m_panOffset = QPointF();
    m_draggingImage = false;

    delete m_originalPixmap;
    m_originalPixmap = nullptr;

    delete m_scaledPixmapCache;
    m_scaledPixmapCache = nullptr;

    if (rebuildSiblings)
        this->rebuildSiblingList();

    // Every new preview starts from canonical geometry again.
    m_initialGeometryApplied = false;
    m_userChangedGeometry = false;

    loadFile(m_path);
}

void PreviewWindow::buildUi()
{
    applyAppearance();
    setWindowIcon(QIcon(QStringLiteral(":/quicklook/assets/quicklook-linux-512.png")));

    m_root = new QWidget;
    m_root->setObjectName("root");
    auto *outer = new QVBoxLayout(m_root);
    outer->setContentsMargins(4, 10, 4, 4);
    outer->setSpacing(5);

    auto *header = new QHBoxLayout;

    m_prev = new QPushButton("‹");
    m_prev->setObjectName("nav");
    m_prev->setFocusPolicy(Qt::NoFocus);
    connect(m_prev, &QPushButton::clicked, this, [this]() { goSibling(-1); });

    m_next = new QPushButton("›");
    m_next->setObjectName("nav");
    m_next->setFocusPolicy(Qt::NoFocus);
    connect(m_next, &QPushButton::clicked, this, [this]() { goSibling(1); });

    header->addWidget(m_prev);
    header->addWidget(m_next);

    auto *labels = new QVBoxLayout;
    m_title = new QLabel;
    m_title->setObjectName("title");
    m_detail = new QLabel;
    m_detail->setObjectName("detail");
    labels->addWidget(m_title);
    labels->addWidget(m_detail);
    header->addLayout(labels, 1);

    m_zoomOut = new QPushButton("−");
    m_zoomReset = new QPushButton("100%");
    m_zoomIn = new QPushButton("+");
    for (auto *b : {m_zoomOut, m_zoomReset, m_zoomIn}) b->setFocusPolicy(Qt::NoFocus);
    connect(m_zoomOut, &QPushButton::clicked, this, [this]() { zoomOut(); });
    connect(m_zoomReset, &QPushButton::clicked, this, [this]() { resetZoom(); });
    connect(m_zoomIn, &QPushButton::clicked, this, [this]() { zoomIn(); });
    header->addWidget(m_zoomOut);
    header->addWidget(m_zoomReset);
    header->addWidget(m_zoomIn);

    m_settingsButton = new QPushButton("⚙");
    m_settingsButton->setToolTip("Apariencia");
    m_settingsButton->setFocusPolicy(Qt::NoFocus);
    m_settingsButton->setFixedWidth(40);
    m_detail->setVisible(false);
    m_detail->setMaximumHeight(0);
    m_detail->setContentsMargins(0, 0, 0, 0);
    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        showSettingsDialog();
    });
    header->addWidget(m_settingsButton);

    m_open = new QPushButton("Abrir ↗");
    m_open->setFocusPolicy(Qt::NoFocus);
    connect(m_open, &QPushButton::clicked, this, [this]() {
        stopMovie();
        stopMedia();
        QProcess::startDetached("xdg-open", {m_path});
        close();
    });

    m_close = new QPushButton(QStringLiteral("×"));
    m_close->setObjectName("closeButton");
    m_close->setFocusPolicy(Qt::NoFocus);
    connect(m_close, &QPushButton::clicked, this, [this]() {
        closePreview();
    });

    header->addWidget(m_open);
    header->addWidget(m_close);
    outer->addLayout(header);

    m_stack = new QStackedWidget;
    m_stack->setObjectName("previewStack");

    // Final inner mask/bisel: always above the active content.
    // It never receives mouse events.
    m_contentOverlay = new QWidget(m_stack);
    m_contentOverlay->setObjectName("contentMaskOverlay");
    m_contentOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_contentOverlay->setAttribute(Qt::WA_NoSystemBackground, true);
    m_contentOverlay->show();

    m_image = new QLabel;
    m_image->setObjectName("image");
    m_image->setAlignment(Qt::AlignCenter);
    m_image->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_image->setMinimumSize(0, 0);
    m_image->setCursor(Qt::ArrowCursor);
    m_stack->addWidget(m_image);

    m_text = new QTextEdit;
    m_text->setObjectName("textSurface");
    m_text->setReadOnly(true);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_text->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_text->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_text->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->addWidget(m_text);

    m_video = new QVideoWidget;
    m_video->setObjectName("videoSurface");
    m_video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->addWidget(m_video);

    m_audioView = new QWidget;
    m_audioView->setObjectName("audioSurface");
    auto *audioRow = new QHBoxLayout(m_audioView);
    audioRow->setContentsMargins(10, 10, 10, 10);
    audioRow->setSpacing(18);

    m_audioCard = new QLabel;
    m_audioCard->setObjectName("audioArtwork");
    m_audioCard->setAlignment(Qt::AlignCenter);
    m_audioCard->setMinimumSize(250, 250);
    m_audioCard->setMaximumSize(270, 270);
    m_audioCard->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *audioInfo = new QWidget;
    auto *audioInfoLayout = new QVBoxLayout(audioInfo);
    audioInfoLayout->setContentsMargins(0, 0, 0, 0);
    audioInfoLayout->setSpacing(8);

    m_audioTitle = new QLabel;
    m_audioTitle->setObjectName("audioTitle");
    m_audioTitle->setAlignment(Qt::AlignCenter);
    m_audioTitle->setWordWrap(true);

    m_audioDuration = new QLabel;
    m_audioDuration->setObjectName("audioDuration");
    m_audioDuration->setAlignment(Qt::AlignCenter);

    audioInfoLayout->addStretch(1);
    audioInfoLayout->addWidget(m_audioTitle);
    audioInfoLayout->addWidget(m_audioDuration);
    audioInfoLayout->addStretch(1);

    audioRow->addWidget(m_audioCard);
    audioRow->addWidget(audioInfo, 1);

    m_stack->addWidget(m_audioView);

    m_fallback = new QLabel;
    m_fallback->setObjectName("card");
    m_fallback->setAlignment(Qt::AlignCenter);
    m_fallback->setWordWrap(true);
    m_stack->addWidget(m_fallback);

    outer->addWidget(m_stack, 1);
    buildMediaControls();
    outer->addWidget(m_mediaControls);


    setCentralWidget(m_root);
}

void PreviewWindow::buildMediaControls()
{
    m_mediaControls = new QWidget;
    auto *row = new QHBoxLayout(m_mediaControls);
    row->setContentsMargins(6, 5, 6, 5);
    row->setSpacing(9);

    m_back15 = new QPushButton("−15");
    m_back15->setToolTip("Retroceder 15 segundos");
    m_back15->setFixedWidth(48);
    m_back15->setFocusPolicy(Qt::NoFocus);

    m_playPause = new QPushButton("⏸");
    m_playPause->setFixedWidth(46);
    m_playPause->setFocusPolicy(Qt::NoFocus);

    m_forward15 = new QPushButton("+15");
    m_forward15->setToolTip("Avanzar 15 segundos");
    m_forward15->setFixedWidth(48);
    m_forward15->setFocusPolicy(Qt::NoFocus);

    m_seek = new ClickSlider(Qt::Horizontal);
    m_seek->setRange(0, 1000);
    m_seek->setFocusPolicy(Qt::NoFocus);

    m_time = new QLabel("00:00 / 00:00");
    m_time->setObjectName("detail");
    m_time->setMinimumWidth(110);

    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volume->setValue(80);
    m_volume->setMaximumWidth(92);
    m_volume->setToolTip("Volumen");
    m_volume->setFocusPolicy(Qt::NoFocus);

    row->addWidget(m_back15);
    row->addWidget(m_playPause);
    row->addWidget(m_forward15);
    row->addWidget(m_seek, 1);
    row->addWidget(m_time);
    row->addWidget(m_volume);

    m_mediaControls->setVisible(false);

    connect(m_back15, &QPushButton::clicked, this, [this]() {
        seekRelative(-15000);
    });

    connect(m_forward15, &QPushButton::clicked, this, [this]() {
        seekRelative(15000);
    });

    connect(m_playPause, &QPushButton::clicked, this, [this]() {
        if (!m_player) return;
        if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
        else m_player->play();
        updateMediaUi();
    });

    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        if (m_audio) m_audio->setVolume(value / 100.0);
    });

    connect(m_seek, &QSlider::sliderPressed, this, [this]() { m_userSeeking = true; });

    connect(m_seek, &QSlider::sliderReleased, this, [this]() {
        if (m_player && m_player->duration() > 0) {
            const qint64 pos = (m_seek->value() * m_player->duration()) / 1000;
            m_player->setPosition(pos);
        }
        m_userSeeking = false;
    });

    connect(m_seek, &QSlider::sliderMoved, this, [this](int value) {
        if (m_player && m_player->duration() > 0) {
            const qint64 pos = (value * m_player->duration()) / 1000;
            m_time->setText(formatMs(pos) + " / " + formatMs(m_player->duration()));
        }
    });
}


void PreviewWindow::applyAppearance()
{
    applySkin(m_skin);
}

void PreviewWindow::applySkin(const QString &skin)
{
    m_skin = skin;

    const int alpha = qBound(0, int(m_windowOpacity * 255.0), 255);

    // Siempre transparente en el exterior para que las esquinas del root
    // puedan verse realmente redondeadas.
    setAttribute(Qt::WA_TranslucentBackground, true);

    QString accent = "#2f9bff";
    QString accentSoft = "rgba(47,155,255,105)";
    QString root = QString("rgba(16,18,22,%1)").arg(alpha);
    QString panel = "rgba(12,14,17,242)";
    QString card = "rgba(25,29,34,235)";
    QString button = "rgba(44,49,56,230)";
    QString buttonHover = "rgba(61,67,76,240)";
    QString border = "rgba(255,255,255,30)";
    QString text = "#f4f4f4";
    QString detail = "#a5abb4";

    if (skin == "chaios") {
        accent = "#c45cff";
        accentSoft = "rgba(196,92,255,120)";
        root = QString("rgba(12,9,18,%1)").arg(alpha);
        panel = "rgba(10,8,16,244)";
        card = "rgba(20,12,28,238)";
        button = "rgba(38,17,51,235)";
        buttonHover = "rgba(64,26,84,245)";
        border = "rgba(196,92,255,105)";
        text = "#f3dcff";
        detail = "#ce8df1";
    } else if (skin == "apple") {
        accent = "#f2f2f2";
        accentSoft = "rgba(255,255,255,110)";
        // Apple actual: cristal gris/negro, no gris sólido antiguo.
        root = QString("rgba(36,38,42,%1)").arg(qMin(235, alpha));
        panel = "rgba(31,33,36,218)";
        card = "rgba(43,45,49,210)";
        button = "rgba(72,74,78,180)";
        buttonHover = "rgba(92,94,98,205)";
        border = "rgba(255,255,255,55)";
        text = "#ffffff";
        detail = "#d1d1d4";
    }

    const QString css = QString(R"(
        QMainWindow {
            background: transparent;
        }

        QWidget#root {
            background: %1;
            color: %8;
            border: 1px solid %7;
            border-radius: 18px;
        }

        QLabel#title {
            font-size: 15px;
            font-weight: 600;
            color: %8;
        }

        QLabel#detail {
            font-size: 11px;
            color: %9;
        }

        QStackedWidget#previewStack {
            background: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
        }

        QWidget#contentMaskOverlay {
            background: transparent;
            border: 1px solid %7;
            border-radius: 13px;
        }

        QLabel#image {
            background: transparent;
            border: none;
            border-radius: 0px;
            padding: 0px;
            margin: 0px;
        }

        QLabel#card {
            background: %3;
            border: 1px solid %7;
            border-radius: 14px;
            padding: 28px;
            font-size: 16px;
            color: %8;
        }

        QWidget#pdfFrame {
            background: #ffffff;
            border: none;
            margin: 0px;
            padding: 0px;
        }

        QPdfView#pdfSurface {
            background: #ffffff;
            border: none;
            border-radius: 0px;
            margin: 0px;
            padding: 0px;
        }

        QTextEdit#textSurface {
            background: %2;
            color: %8;
            border: none;
            padding: 10px;
            margin: 0px;
            border-radius: 0px;
            selection-background-color: %5;
        }

        QVideoWidget#videoSurface {
            background: #000000;
            border: none;
            border-radius: 0px;
        }

        QWidget#audioSurface {
            background: %3;
            border: 1px solid %7;
            border-radius: 13px;
        }

        QLabel#audioArtwork {
            background: rgba(10,12,15,205);
            border: 1px solid %7;
            border-radius: 11px;
            padding: 0px;
        }

        QLabel#audioTitle {
            background: transparent;
            color: %8;
            font-size: 20px;
            font-weight: 600;
            padding: 0px 12px;
        }

        QLabel#audioDuration {
            background: transparent;
            color: %9;
            font-size: 14px;
        }

        /*
         * Una sola barra de desplazamiento para TODOS los visores:
         * estrecha, elegante y pegada al borde derecho/inferior.
         */
        /* FINAL common scrollbar: grey, thin, short, flush to edge */
        /* Common scrollbar: grey, thin, short, flush to content edge */
        QScrollBar:vertical {
            background: transparent;
            width: 4px;
            margin: 14px 0px 14px 0px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background: rgba(145,145,150,190);
            min-height: 22px;
            border-radius: 2px;
        }

        QScrollBar::handle:vertical:hover {
            background: rgba(178,178,184,225);
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
            border: none;
            height: 0px;
        }

        QScrollBar:horizontal {
            background: transparent;
            height: 4px;
            margin: 0px 14px 0px 14px;
            border: none;
        }

        QScrollBar::handle:horizontal {
            background: rgba(145,145,150,190);
            min-width: 22px;
            border-radius: 2px;
        }

        QScrollBar::handle:horizontal:hover {
            background: rgba(178,178,184,225);
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
            border: none;
            width: 0px;
        }

        QPushButton {
            background: %4;
            color: %8;
            border: 1px solid %7;
            border-radius: 10px;
            padding: 6px 10px;
        }

        QPushButton:hover {
            background: %6;
            border-color: %5;
        }

        QPushButton#closeButton {
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            border-radius: 15px;
            padding: 0px;
            font-size: 20px;
            font-weight: 600;
        }

        QPushButton#nav {
            font-size: 16px;
            font-weight: 600;
            min-width: 30px;
            max-width: 30px;
            min-height: 30px;
            max-height: 30px;
            border-radius: 8px;
            padding: 0px;
        }

        QSlider::groove:horizontal {
            height: 4px;
            background: rgba(255,255,255,48);
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            width: 12px;
            margin: -5px 0;
            border-radius: 6px;
            background: %5;
        }

        QSlider::sub-page:horizontal {
            background: %5;
            border-radius: 2px;
        }

        QDialog {
            background: %1;
            color: %8;
        }

        QComboBox, QSpinBox {
            background: %4;
            color: %8;
            border: 1px solid %7;
            border-radius: 9px;
            padding: 6px 10px;
        }

        QCheckBox {
            color: %8;
        }
    )")
        .arg(root)         // %1
        .arg(panel)        // %2
        .arg(card)         // %3
        .arg(button)       // %4
        .arg(accent)       // %5
        .arg(buttonHover)  // %6
        .arg(border)       // %7
        .arg(text)         // %8
        .arg(detail);      // %9

    setStyleSheet(css);
}

void PreviewWindow::showSettingsDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("QuickLook · Settings");
    dlg.setModal(true);
    dlg.setMinimumWidth(390);

    auto *layout = new QFormLayout(&dlg);

    auto *skin = new QComboBox;
    skin->addItem("QuickLook Linux (Genérica)", "generic");
    skin->addItem("ChaïOS Dark", "chaios");
    skin->addItem("Apple Quick Look", "apple");

    const int skinIndex = qMax(0, skin->findData(m_skin));
    skin->setCurrentIndex(skinIndex);

    auto *opacity = new QSpinBox;
    opacity->setRange(65, 100);
    opacity->setSuffix(" %");
    opacity->setValue(int(m_windowOpacity * 100.0));

    auto *adaptive = new QCheckBox("Ajustar ventana al contenido");
    adaptive->setChecked(m_adaptiveWindow);

    layout->addRow("Skin:", skin);
    layout->addRow("Opacidad:", opacity);
    layout->addRow("", adaptive);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
    );
    layout->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Preview instantánea mientras eliges skin.
    connect(skin, &QComboBox::currentIndexChanged, this, [this, skin]() {
        applySkin(skin->currentData().toString());
    });

    if (dlg.exec() != QDialog::Accepted) {
        QSettings previous("QuickLookLinux", "QuickLookLinux");
        applySkin(previous.value("skin", "generic").toString());
        return;
    }

    m_skin = skin->currentData().toString();
    m_windowOpacity = opacity->value() / 100.0;
    m_adaptiveWindow = adaptive->isChecked();

    QSettings settings("QuickLookLinux", "QuickLookLinux");
    settings.setValue("skin", m_skin);
    settings.setValue("windowOpacity", m_windowOpacity);
    settings.setValue("adaptiveWindow", m_adaptiveWindow);

    applyAppearance();
    resizeForCurrentContent();
}


QSize PreviewWindow::calculatePhotoViewport(const QSize &intrinsic) const
{
    if (!intrinsic.isValid())
        return QSize(640, 480);

    QScreen *screen = windowHandle() && windowHandle()->screen()
        ? windowHandle()->screen()
        : QApplication::primaryScreen();

    if (!screen)
        return intrinsic;

    const QRect area = screen->availableGeometry();

    // Dejamos margen real solo para cabecera propia y márgenes del visor.
    // La foto manda sobre la proporción de la ventana.
    const int reserveW = 20;
    const int reserveH = 62;

    const QSize maximum(
        qMax(320, int(area.width() * 0.92) - reserveW),
        qMax(240, int(area.height() * 0.92) - reserveH)
    );

    QSize viewport = intrinsic;

    // No ampliamos fotos pequeñas al abrir.
    // Solo reducimos cuando no caben en pantalla.
    if (viewport.width() > maximum.width() ||
        viewport.height() > maximum.height()) {
        viewport.scale(maximum, Qt::KeepAspectRatio);
    }

    return viewport;
}

void PreviewWindow::lockPhotoWindowToViewport(const QSize &viewport)
{
    if (!viewport.isValid())
        return;

    m_photoViewport = viewport;

    // La interfaz permanece EXACTAMENTE igual.
    // Lo único que fijamos es el área destinada a la foto.
    m_stack->setMinimumSize(viewport);
    m_stack->setMaximumSize(viewport);
    m_stack->resize(viewport);

    if (m_image) {
        m_image->setMinimumSize(viewport);
        m_image->setMaximumSize(viewport);
    }

    if (m_root && m_root->layout())
        m_root->layout()->activate();

    // Qt calcula la ventana a partir de:
    // cabecera actual + foto exacta + márgenes actuales.
    adjustSize();

    QScreen *screen = windowHandle() && windowHandle()->screen()
        ? windowHandle()->screen()
        : QApplication::primaryScreen();

    if (screen) {
        const QRect area = screen->availableGeometry();
        const QRect frame = frameGeometry();
        move(area.center().x() - frame.width() / 2,
             area.center().y() - frame.height() / 2);
    }
}

void PreviewWindow::resizeForCurrentContent(const QSize &intrinsic)
{
    Q_UNUSED(intrinsic);

    // El tipo de contenido NO decide ni tamaño ni posición inicial.
    if (!m_initialGeometryApplied) {
        applyCanonicalInitialGeometry();
        m_initialGeometryApplied = true;
    }

    if (m_root && m_root->layout())
        m_root->layout()->activate();

    if (m_kind == Kind::Image || m_kind == Kind::Gif)
        m_photoViewport = m_stack->contentsRect().size();

    updateContentMask();

    QTimer::singleShot(0, this, [this]() {
        updateContentMask();

        if ((m_kind == Kind::Image || m_kind == Kind::Gif) && m_stack)
            m_photoViewport = m_stack->contentsRect().size();

        if (m_kind == Kind::Image && m_originalPixmap)
            renderImageViewport(true);

        if (m_kind == Kind::Gif && m_movie && m_photoViewport.isValid()) {
            QSize gifSize = m_movie->frameRect().size();
            if (gifSize.isValid()) {
                gifSize.scale(m_photoViewport, Qt::KeepAspectRatioByExpanding);
                m_movie->setScaledSize(gifSize);
            }
        }
    });
}

void PreviewWindow::applyCanonicalInitialGeometry()
{
    QScreen *screen = windowHandle() && windowHandle()->screen()
        ? windowHandle()->screen()
        : QApplication::primaryScreen();

    if (!screen)
        return;

    const QRect area = screen->availableGeometry();

    // User-approved reference size.
    const int canonicalW = qMin(820, qMax(420, area.width() - 80));
    const int canonicalH = qMin(540, qMax(320, area.height() - 100));

    const int canonicalX =
        area.left() + (area.width() - canonicalW) / 2;
    const int canonicalY =
        area.top() + (area.height() - canonicalH) / 2;

    setMinimumSize(420, 300);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    if (m_kind == Kind::Audio) {
        // Audio keeps its approved own size.
        const int audioW = qMin(760, qMax(420, area.width() - 80));
        const int audioH = qMin(430, qMax(300, area.height() - 100));

        // Same TOP line as every other QuickLook window.
        // Horizontally centred so the smaller audio window looks intentional.
        const int audioX =
            area.left() + (area.width() - audioW) / 2;

        setGeometry(audioX, canonicalY, audioW, audioH);
        return;
    }

    // Every non-audio content type gets EXACTLY the same initial rectangle.
    setGeometry(canonicalX, canonicalY, canonicalW, canonicalH);
}


void PreviewWindow::recenterCanonicalWindow()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
        return;

    const QRect area = screen->availableGeometry();
    const QSize ws = size();

    move(area.left() + (area.width() - ws.width()) / 2,
         area.top() + (area.height() - ws.height()) / 2);
}

void PreviewWindow::updateContentMask()
{
    if (!m_stack || !m_contentOverlay)
        return;

    // Audio stays exactly as it was.
    if (m_kind == Kind::Audio) {
        m_stack->clearMask();
        m_contentOverlay->hide();
        return;
    }

    const QRect r = m_stack->rect();
    if (r.width() <= 2 || r.height() <= 2)
        return;

    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(r), 13.0, 13.0);
    const QRegion region(rounded.toFillPolygon().toPolygon());
    m_stack->setMask(region);

    // Video gets the same mask directly because QVideoWidget may use a
    // native paint surface and can otherwise overdraw sibling overlays.
    if (m_video) {
        if (m_kind == Kind::Video) {
            QPainterPath videoRounded;
            videoRounded.addRoundedRect(QRectF(m_video->rect()), 13.0, 13.0);
            m_video->setMask(QRegion(videoRounded.toFillPolygon().toPolygon()));
        } else {
            m_video->clearMask();
        }
    }

    m_contentOverlay->setGeometry(r);
    m_contentOverlay->show();
    m_contentOverlay->raise();
}

bool PreviewWindow::imageCanPan() const
{
    return m_scaledPixmapCache &&
           !m_scaledPixmapCache->isNull() &&
           m_photoViewport.isValid() &&
           (m_scaledPixmapCache->width() > m_photoViewport.width() ||
            m_scaledPixmapCache->height() > m_photoViewport.height());
}

void PreviewWindow::seekRelative(qint64 deltaMs)
{
    if (!m_player)
        return;

    const qint64 duration = m_player->duration();
    qint64 target = m_player->position() + deltaMs;

    if (duration > 0)
        target = qBound<qint64>(0, target, duration);
    else
        target = qMax<qint64>(0, target);

    m_player->setPosition(target);
}

void PreviewWindow::setHeader(const QString &path, const QString &detail)
{
    QFileInfo fi(path);
    m_title->setText(fi.fileName());

    QString d = detail;
    if (d.isEmpty()) {
        const qint64 n = fi.size();
        if (n < 1024) d = QString::number(n) + " B";
        else if (n < 1024*1024) d = QString::number(n/1024.0, 'f', 1) + " KB";
        else d = QString::number(n/(1024.0*1024.0), 'f', 1) + " MB";
    }

    m_detail->setText(d);
    setWindowTitle(fi.fileName() + " — QuickLook");
}

QString PreviewWindow::formatMs(qint64 ms) const
{
    if (ms < 0) ms = 0;
    qint64 s = ms / 1000;
    qint64 h = s / 3600;
    qint64 m = (s % 3600) / 60;
    qint64 sec = s % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

void PreviewWindow::updateMediaUi()
{
    if (!m_player) return;
    m_playPause->setText(m_player->playbackState() == QMediaPlayer::PlayingState ? "⏸" : "▶");
    if (!m_userSeeking && m_player->duration() > 0) {
        m_seek->setValue(int((m_player->position() * 1000) / m_player->duration()));
    }
    m_time->setText(formatMs(m_player->position()) + " / " + formatMs(m_player->duration()));
}

void PreviewWindow::stopMovie()
{
    if (m_movie) {
        m_movie->stop();
        m_image->setMovie(nullptr);
        delete m_movie;
        m_movie = nullptr;
    }
}

void PreviewWindow::stopMedia()
{
    m_mediaControls->setVisible(false);
    if (m_player) {
        m_player->stop();
        delete m_player;
        m_player = nullptr;
    }
    if (m_audio) {
        delete m_audio;
        m_audio = nullptr;
    }
}

bool PreviewWindow::isPreviewableCandidate(const QString &path) const
{
    QFileInfo fi(path);
    if (!fi.isFile()) return false;
    const QString ext = fi.suffix().toLower();

    static const QStringList exts = {
        "jpg","jpeg","png","webp","gif","bmp","svg","tif","tiff","avif",
        "pdf","epub",
        "txt","md","json","xml","yaml","yml","ini","cfg","conf","log","csv",
        "py","js","ts","css","html","htm","c","cpp","h","hpp","sh",
        "mp3","wav","flac","ogg","m4a","aac","opus",
        "mp4","mkv","mov","webm","avi","mpg","mpeg","m4v","ts","m2ts","mts","wmv",
        "doc","docx","odt","rtf","xls","xlsx","ods","ppt","pptx","odp",
        "zip","7z","rar","tar","gz","bz2","xz",
        "ttf","otf"
    };
    return exts.contains(ext);
}

void PreviewWindow::rebuildSiblingList()
{
    QFileInfo fi(m_path);
    QDir dir = fi.dir();
    QStringList list;
    const QFileInfoList infos = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot,
                                                  QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &item : infos)
        if (isPreviewableCandidate(item.absoluteFilePath()))
            list << item.absoluteFilePath();

    m_siblings = list;
    m_siblingIndex = m_siblings.indexOf(fi.absoluteFilePath());
}

void PreviewWindow::goSibling(int delta)
{
    if (m_siblings.isEmpty() || m_siblingIndex < 0) return;

    int idx = m_siblingIndex + delta;
    if (idx < 0) idx = m_siblings.size() - 1;
    if (idx >= m_siblings.size()) idx = 0;

    m_siblingIndex = idx;
    openPath(m_siblings.at(idx), false);
}

QString PreviewWindow::officeCachePath(const QString &path) const
{
    QFileInfo fi(path);
    QByteArray key = fi.absoluteFilePath().toUtf8();
    key += '|';
    key += QByteArray::number(fi.size());
    key += '|';
    key += QByteArray::number(fi.lastModified().toMSecsSinceEpoch());

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex()
    );

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/office";
    QDir().mkpath(dir);
    return dir + "/" + hash + ".pdf";
}

void PreviewWindow::loadFile(const QString &path)
{
    stopMovie();
    stopMedia();

    // Liberar cualquier viewport fijo de una foto anterior.
    m_stack->setMinimumSize(0, 0);
    m_stack->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    m_image->setMinimumSize(0, 0);
    m_image->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    setHeader(path);

    // Default: non-media previews keep their zoom controls visible.
    m_zoomOut->setVisible(true);
    m_zoomReset->setVisible(true);
    m_zoomIn->setVisible(true);

    m_zoomOut->setEnabled(false);
    m_zoomReset->setEnabled(false);
    m_zoomIn->setEnabled(false);

    QFileInfo fi(path);

    if (fi.isDir()) {
        QDir d(path);
        const QFileInfoList entries = d.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
        );
        QString text;
        qint64 files = 0, dirs = 0;
        text += "CARPETA\n────────────────────────────────────────\n";
        const int limit = qMin(120, entries.size());
        for (int i = 0; i < limit; ++i) {
            const QFileInfo &e = entries.at(i);
            if (e.isDir()) {
                ++dirs;
                text += "📁  " + e.fileName() + "/\n";
            } else {
                ++files;
                text += "     " + e.fileName() + "\n";
            }
        }
        if (entries.size() > limit)
            text += QString("\n… y %1 elementos más").arg(entries.size() - limit);

        m_text->setPlainText(text);
        m_stack->setCurrentWidget(m_text);
        m_kind = Kind::Text;
        setHeader(path, QString("Carpeta · %1 elementos visibles").arg(entries.size()));
        resizeForCurrentContent();
        return;
    }

    const QString ext = fi.suffix().toLower();

    static const QStringList office = {
        "doc","docx","odt","rtf","xls","xlsx","ods","csv","ppt","pptx","odp"
    };
    static const QStringList archives = {
        "zip","7z","rar","tar","gz","bz2","xz"
    };
    static const QStringList fonts = {"ttf","otf"};

    if (ext == "epub" && showEpub(path)) return;
    if (office.contains(ext) && showOffice(path)) return;
    if (archives.contains(ext) && showArchive(path)) return;
    if (fonts.contains(ext) && showFont(path)) return;

    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent).name();

    if (mime == "application/pdf") showPdf(path);
    else if (mime.startsWith("image/")) showImage(path);
    else if (mime.startsWith("audio/")) showMedia(path, false);
    else if (mime.startsWith("video/")) showMedia(path, true);
    else if (mime.startsWith("text/") ||
             QStringList({"md","json","xml","yaml","yml","ini","cfg","conf","log",
                          "py","js","ts","css","html","htm","c","cpp","h","hpp","sh"}).contains(ext))
        showText(path);
    else
        showFallback(path);
}

void PreviewWindow::showImage(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "gif") {
        m_movie = new QMovie(path);
        if (m_movie->isValid()) {
            m_image->setMovie(m_movie);
            m_movie->setCacheMode(QMovie::CacheAll);
            m_movie->start();
            m_stack->setCurrentWidget(m_image);
            m_kind = Kind::Gif;
            setHeader(path, "GIF animado");
            resizeForCurrentContent();
            m_photoViewport = m_stack->contentsRect().size();
            QSize gifSize = m_movie->frameRect().size();
            if (gifSize.isValid() && m_photoViewport.isValid()) {
                gifSize.scale(m_photoViewport, Qt::KeepAspectRatioByExpanding);
                m_movie->setScaledSize(gifSize);
            }
            updateContentMask();
            return;
        }
        delete m_movie;
        m_movie = nullptr;
    }

    QImageReader r(path);
    r.setAutoTransform(true);

    if (!r.canRead()) {
        showFallback(path, "Qt no puede decodificar esta imagen.");
        return;
    }

    // R3.2 FIX2:
    // decodificamos la foto UNA sola vez y conservamos el pixmap original.
    const QImage decoded = r.read();
    if (decoded.isNull()) {
        showFallback(path, "No se pudo decodificar esta imagen.");
        return;
    }

    delete m_originalPixmap;
    m_originalPixmap = new QPixmap(QPixmap::fromImage(decoded));

    m_imagePath = path;
    m_kind = Kind::Image;
    m_imageZoom = 1.0;
    m_panOffset = QPointF();
    m_draggingImage = false;

    delete m_scaledPixmapCache;
    m_scaledPixmapCache = nullptr;

    m_zoomOut->setEnabled(true);
    m_zoomReset->setEnabled(true);
    m_zoomIn->setEnabled(true);

    m_stack->setCurrentWidget(m_image);

    const QSize original = m_originalPixmap->size();
    if (original.isValid()) {
        setHeader(path, QString("%1 × %2 px · %3")
                       .arg(original.width())
                       .arg(original.height())
                       .arg(QString::fromLatin1(r.format())));
        resizeForCurrentContent();
    } else {
        resizeForCurrentContent();
    }

    m_photoViewport = m_stack->contentsRect().size();
    updateImageScale(true);
}

void PreviewWindow::clampPanOffset()
{
    if (!m_scaledPixmapCache || m_scaledPixmapCache->isNull() || !m_photoViewport.isValid()) {
        m_panOffset = QPointF();
        return;
    }

    const double maxX = qMax(0, (m_scaledPixmapCache->width()  - m_photoViewport.width())  / 2);
    const double maxY = qMax(0, (m_scaledPixmapCache->height() - m_photoViewport.height()) / 2);

    m_panOffset.setX(qBound(-maxX, m_panOffset.x(), maxX));
    m_panOffset.setY(qBound(-maxY, m_panOffset.y(), maxY));
}

void PreviewWindow::renderImageViewport(bool highQuality)
{
    if (!m_originalPixmap || m_originalPixmap->isNull())
        return;

    QSize viewport = m_photoViewport;
    if (!viewport.isValid())
        viewport = m_stack->contentsRect().size();
    if (!viewport.isValid())
        return;

    QSize fitted = m_originalPixmap->size();
    fitted.scale(viewport, Qt::KeepAspectRatioByExpanding);

    const QSize scaledSize(
        qMax(1, qRound(fitted.width()  * m_imageZoom)),
        qMax(1, qRound(fitted.height() * m_imageZoom))
    );

    if (!m_scaledPixmapCache ||
        m_scaledPixmapCache->size() != scaledSize) {
        delete m_scaledPixmapCache;
        m_scaledPixmapCache = new QPixmap(
            m_originalPixmap->scaled(
                scaledSize,
                Qt::KeepAspectRatio,
                highQuality ? Qt::SmoothTransformation : Qt::FastTransformation
            )
        );
    }

    clampPanOffset();

    // Construimos exactamente el viewport visible y lo recortamos con
    // esquinas INTERNAS redondeadas. Nada acaba en punta.
    QPixmap canvas(viewport);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(viewport)), 13.0, 13.0);
    painter.setClipPath(rounded);

    const QPointF center(
        (viewport.width()  - m_scaledPixmapCache->width())  / 2.0 + m_panOffset.x(),
        (viewport.height() - m_scaledPixmapCache->height()) / 2.0 + m_panOffset.y()
    );

    painter.drawPixmap(center, *m_scaledPixmapCache);
    painter.end();

    m_image->setPixmap(canvas);
    m_zoomReset->setText(QString::number(qRound(m_imageZoom * 100.0)) + "%");

    m_image->setCursor(imageCanPan() ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void PreviewWindow::updateImageScale(bool highQuality)
{
    // Solo esta función reconstruye el cache cuando cambia el zoom.
    delete m_scaledPixmapCache;
    m_scaledPixmapCache = nullptr;
    renderImageViewport(highQuality);
}

void PreviewWindow::scheduleImageRender()
{
    if (m_kind != Kind::Image || !m_originalPixmap)
        return;

    m_zoomReset->setText(QString::number(qRound(m_imageZoom * 100.0)) + "%");

    if (m_zoomRenderTimer)
        m_zoomRenderTimer->start();
}

void PreviewWindow::showText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        showFallback(path, "No se pudo leer el archivo.");
        return;
    }

    QByteArray b = f.read(4 * 1024 * 1024);
    QString s = QString::fromUtf8(b);

    if (f.size() > b.size()) s += "\n\n[Vista previa limitada a 4 MB]";

    m_text->setPlainText(s);
    m_kind = Kind::Text;
    m_stack->setCurrentWidget(m_text);
    resizeForCurrentContent();
}

void PreviewWindow::showPdf(const QString &path)
{
    if (m_pdfFrame) {
        m_stack->removeWidget(m_pdfFrame);
        delete m_pdfFrame;
        m_pdfFrame = nullptr;
        m_pdfView = nullptr;
    }

    delete m_pdfDoc;
    m_pdfDoc = new QPdfDocument(this);

    if (m_pdfDoc->load(path) != QPdfDocument::Error::None) {
        showFallback(path, "No se pudo renderizar el PDF.");
        return;
    }

    // Real inner frame:
    // left/right/bottom/top content gap identical = 4 px.
    // The outer QuickLook header remains the only thicker region.
    m_pdfFrame = new QWidget;
    m_pdfFrame->setObjectName("pdfFrame");
    auto *pdfLayout = new QVBoxLayout(m_pdfFrame);
    pdfLayout->setContentsMargins(0, 0, 0, 0);
    pdfLayout->setSpacing(0);

    m_pdfView = new QPdfView;
    m_pdfView->setObjectName("pdfSurface");
    m_pdfView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pdfView->setDocument(m_pdfDoc);

    // Only the white document is allowed to be visible.
    // Remove QPdfView's own grey outer canvas/margins.
    m_pdfView->setDocumentMargins(QMargins(0, 0, 0, 0));
    m_pdfView->setPageSpacing(0);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);

    if (m_pdfView->viewport()) {
        m_pdfView->viewport()->setAutoFillBackground(true);
        m_pdfView->viewport()->setStyleSheet(
            QStringLiteral("background: #ffffff; border: 0px;")
        );
    }

    m_pdfView->setStyleSheet(
        QStringLiteral("QPdfView { background: #ffffff; border: 0px; }")
    );

    m_pdfView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_pdfView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    pdfLayout->addWidget(m_pdfView);

    m_stack->addWidget(m_pdfFrame);
    m_stack->setCurrentWidget(m_pdfFrame);

    m_kind = Kind::Pdf;
    m_zoomOut->setEnabled(true);
    m_zoomReset->setEnabled(true);
    m_zoomIn->setEnabled(true);
    m_zoomReset->setText("Ajustar");

    setHeader(m_path, QString("PDF · %1 páginas").arg(m_pdfDoc->pageCount()));
    resizeForCurrentContent();
}

bool PreviewWindow::showOffice(const QString &path)
{
    const QString soffice = QStandardPaths::findExecutable("soffice");
    if (soffice.isEmpty()) {
        showFallback(path, "Falta LibreOffice para generar la vista previa.");
        return true;
    }

    const QString cached = officeCachePath(path);
    if (QFileInfo::exists(cached)) {
        showPdf(cached);
        setHeader(path, "Documento Office · caché");
        return true;
    }

    const QString cacheDir = QFileInfo(cached).absolutePath();

    m_fallback->setText("Generando vista previa del documento…");
    m_stack->setCurrentWidget(m_fallback);
    QApplication::processEvents();

    QProcess p;
    p.start(soffice, {
        "--headless", "--nologo", "--nodefault", "--nofirststartwizard",
        "--convert-to", "pdf", "--outdir", cacheDir, path
    });

    if (!p.waitForFinished(8000)) {
        p.kill();
        p.waitForFinished(800);
        showFallback(path, "La vista previa tardó demasiado. QuickLook la canceló para no bloquearse.");
        return true;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        showFallback(path, "LibreOffice no pudo convertir este documento.");
        return true;
    }

    const QString produced = cacheDir + "/" + QFileInfo(path).completeBaseName() + ".pdf";
    if (!QFileInfo::exists(produced)) {
        showFallback(path, "La conversión terminó pero no produjo un PDF.");
        return true;
    }

    QFile::remove(cached);
    if (!QFile::rename(produced, cached)) {
        QFile::copy(produced, cached);
        QFile::remove(produced);
    }

    showPdf(cached);
    setHeader(path, "Documento Office · vista previa cacheada");
    return true;
}

bool PreviewWindow::showArchive(const QString &path)
{
    const QString seven = QStandardPaths::findExecutable("7z");
    if (seven.isEmpty()) {
        showFallback(path, "Falta 7-Zip para listar este archivo comprimido.");
        return true;
    }

    QProcess p;
    p.start(seven, {"l", "-ba", path});
    if (!p.waitForFinished(5000)) {
        p.kill();
        p.waitForFinished(500);
        showFallback(path, "El archivo tarda demasiado en responder. Vista previa cancelada.");
        return true;
    }
    if (p.exitCode() != 0) {
        showFallback(path, "No se pudo leer el contenido del archivo comprimido.");
        return true;
    }

    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    if (out.trimmed().isEmpty()) out = "[Archivo comprimido vacío o sin listado legible]";

    m_text->setPlainText("CONTENIDO DEL ARCHIVO\n────────────────────────────────────────\n" + out);
    m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_stack->setCurrentWidget(m_text);
    m_kind = Kind::Archive;
    setHeader(path, "Archivo comprimido · contenido sin extraer");
    resizeForCurrentContent();
    return true;
}


bool PreviewWindow::showEpub(const QString &path)
{
    const QString seven = QStandardPaths::findExecutable("7z");
    if (seven.isEmpty()) {
        showFallback(path, "Falta 7-Zip para leer el EPUB.");
        return true;
    }

    // EPUB es un contenedor ZIP. Extraemos SOLO a caché temporal de usuario,
    // nunca junto al libro original.
    QByteArray key = QFileInfo(path).absoluteFilePath().toUtf8();
    key += '|';
    key += QByteArray::number(QFileInfo(path).size());
    key += '|';
    key += QByteArray::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex()
    );
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                        + "/epub/" + hash;
    QDir().mkpath(dir);

    // Si aún no está cacheado, extraemos el EPUB.
    if (!QFileInfo::exists(dir + "/META-INF/container.xml")) {
        QProcess p;
        p.start(seven, {"x", "-y", "-o" + dir, path});
        if (!p.waitForFinished(6000)) {
            p.kill();
            p.waitForFinished(500);
            showFallback(path, "El EPUB tarda demasiado en responder. Vista previa cancelada.");
            return true;
        }
        if (p.exitCode() != 0) {
            showFallback(path, "No se pudo abrir el contenido del EPUB.");
            return true;
        }
    }

    // Intentamos encontrar primero la portada.
    QDir root(dir);
    QStringList imageFilters = {
        "*cover*.jpg","*cover*.jpeg","*cover*.png","*cover*.webp",
        "*Cover*.jpg","*Cover*.jpeg","*Cover*.png","*Cover*.webp"
    };

    QString cover;
    const QFileInfoList all = root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    Q_UNUSED(all);

    // Búsqueda recursiva sencilla usando find del sistema.
    const QString findExe = QStandardPaths::findExecutable("find");
    if (!findExe.isEmpty()) {
        QProcess fp;
        fp.start(findExe, {dir, "-type", "f"});
        if (fp.waitForFinished(2500)) {
            const QStringList files =
                QString::fromLocal8Bit(fp.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
            for (const QString &f : files) {
                const QString low = QFileInfo(f).fileName().toLower();
                if (low.contains("cover") &&
                    (low.endsWith(".jpg") || low.endsWith(".jpeg") ||
                     low.endsWith(".png") || low.endsWith(".webp"))) {
                    cover = f;
                    break;
                }
            }

            // Si no hay portada nominal, buscamos XHTML/HTML para mostrar texto.
            QString html;
            for (const QString &f : files) {
                const QString low = f.toLower();
                if (low.endsWith(".xhtml") || low.endsWith(".html") || low.endsWith(".htm")) {
                    html = f;
                    break;
                }
            }

            if (!cover.isEmpty()) {
                m_imagePath = cover;
                m_kind = Kind::Image;
                m_zoomOut->setEnabled(true);
                m_zoomReset->setEnabled(true);
                m_zoomIn->setEnabled(true);
                updateImageScale();
                m_stack->setCurrentWidget(m_image);
                setHeader(path, "EPUB · portada · ←/→ cambia de archivo");
                resizeForCurrentContent();
                m_photoViewport = m_stack->contentsRect().size();
                updateImageScale(true);
                return true;
            }

            if (!html.isEmpty()) {
                QFile f(html);
                if (f.open(QIODevice::ReadOnly)) {
                    QString text = QString::fromUtf8(f.read(2 * 1024 * 1024));
                    // Conversión ligera HTML -> texto visible.
                    text.replace(QRegularExpression("<script[\\\\s\\\\S]*?</script>",
                                                    QRegularExpression::CaseInsensitiveOption), "");
                    text.replace(QRegularExpression("<style[\\\\s\\\\S]*?</style>",
                                                    QRegularExpression::CaseInsensitiveOption), "");
                    text.replace(QRegularExpression("<[^>]+>"), " ");
                    text.replace("&nbsp;", " ");
                    text.replace("&amp;", "&");
                    text.replace("&lt;", "<");
                    text.replace("&gt;", ">");
                    text.replace(QRegularExpression("\\\\s+"), " ");

                    m_text->setPlainText(text.trimmed());
                    m_stack->setCurrentWidget(m_text);
                    m_kind = Kind::Text;
                    setHeader(path, "EPUB · contenido · ENTER abre el lector");
                    resizeForCurrentContent();
                    return true;
                }
            }
        }
    }

    showFallback(path, "EPUB válido, pero no encontré portada ni contenido XHTML legible.");
    return true;
}

bool PreviewWindow::showFont(const QString &path)
{
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0) {
        showFallback(path, "No se pudo cargar esta fuente.");
        return true;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) {
        showFallback(path, "Fuente sin familia detectable.");
        return true;
    }

    const QString family = families.first();
    m_fallback->setFont(QFont(family, 28));
    m_fallback->setText(
        family + "\n\n"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
        "abcdefghijklmnopqrstuvwxyz\n"
        "0123456789\n\n"
        "El veloz murciélago hindú comía feliz cardillo y kiwi."
    );
    m_stack->setCurrentWidget(m_fallback);
    m_kind = Kind::Font;
    setHeader(path, "Fuente · " + family);
    resizeForCurrentContent();
    return true;
}

void PreviewWindow::showMedia(const QString &path, bool video)
{
    Q_UNUSED(video);

    // Audio and Video never show -, 100%, + in the header.
    m_zoomOut->setVisible(false);
    m_zoomReset->setVisible(false);
    m_zoomIn->setVisible(false);

    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_audio->setVolume(m_volume->value() / 100.0);
    m_player->setAudioOutput(m_audio);

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64) { updateMediaUi(); });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64) { updateMediaUi(); });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState) {
        updateMediaUi();
    });

    if (video) {
        m_player->setVideoOutput(m_video);
        m_stack->setCurrentWidget(m_video);
        m_kind = Kind::Video;
        setHeader(path, "Vídeo");
        QTimer::singleShot(0, this, [this]() { updateContentMask(); });
    } else {
        // AUDIO FINAL:
        // portada izquierda; título y duración REAL centrados a la derecha.
        // No waveform inventada.
        QPixmap cover;
        QString title = QFileInfo(path).completeBaseName();
        qint64 probedDurationMs = 0;

        const QString ffprobe =
            QStandardPaths::findExecutable(QStringLiteral("ffprobe"));

        if (!ffprobe.isEmpty()) {
            QProcess probe;
            probe.start(ffprobe, {
                QStringLiteral("-v"), QStringLiteral("error"),
                QStringLiteral("-show_entries"),
                QStringLiteral("format=duration:format_tags=title,artist"),
                QStringLiteral("-of"),
                QStringLiteral("default=noprint_wrappers=1"),
                path
            });

            if (probe.waitForFinished(900)) {
                const QStringList lines =
                    QString::fromUtf8(probe.readAllStandardOutput())
                        .split('\n', Qt::SkipEmptyParts);

                QString tagTitle;
                QString tagArtist;

                for (const QString &line : lines) {
                    if (line.startsWith(QStringLiteral("duration="))) {
                        bool ok = false;
                        const double sec =
                            line.mid(QStringLiteral("duration=").size()).toDouble(&ok);
                        if (ok)
                            probedDurationMs = qint64(sec * 1000.0);
                    } else if (line.startsWith(QStringLiteral("TAG:title="))) {
                        tagTitle =
                            line.mid(QStringLiteral("TAG:title=").size()).trimmed();
                    } else if (line.startsWith(QStringLiteral("TAG:artist="))) {
                        tagArtist =
                            line.mid(QStringLiteral("TAG:artist=").size()).trimmed();
                    }
                }

                if (!tagTitle.isEmpty()) {
                    title = tagArtist.isEmpty()
                        ? tagTitle
                        : tagArtist + QStringLiteral(" - ") + tagTitle;
                }
            } else {
                probe.kill();
                probe.waitForFinished(200);
            }
        }

        const QString ffmpeg =
            QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));

        if (!ffmpeg.isEmpty()) {
            const QString coverPath = QDir::tempPath() +
                QStringLiteral("/quicklook-audio-cover-%1.jpg")
                    .arg(QCoreApplication::applicationPid());

            QFile::remove(coverPath);

            QProcess extract;
            extract.start(ffmpeg, {
                QStringLiteral("-hide_banner"),
                QStringLiteral("-loglevel"), QStringLiteral("error"),
                QStringLiteral("-y"),
                QStringLiteral("-i"), path,
                QStringLiteral("-an"),
                QStringLiteral("-map"), QStringLiteral("0:v:0?"),
                QStringLiteral("-frames:v"), QStringLiteral("1"),
                coverPath
            });

            if (extract.waitForFinished(1200) &&
                extract.exitStatus() == QProcess::NormalExit &&
                extract.exitCode() == 0 &&
                QFileInfo::exists(coverPath)) {
                cover.load(coverPath);
            }

            if (extract.state() != QProcess::NotRunning) {
                extract.kill();
                extract.waitForFinished(200);
            }
            QFile::remove(coverPath);
        }

        if (!cover.isNull()) {
            m_audioCard->setPixmap(
                cover.scaled(
                    QSize(260, 260),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                )
            );
            m_audioCard->setText(QString());
        } else {
            m_audioCard->setPixmap(QPixmap());
            m_audioCard->setText(QStringLiteral("♪"));
            QFont noteFont = m_audioCard->font();
            noteFont.setPointSize(88);
            noteFont.setWeight(QFont::Light);
            m_audioCard->setFont(noteFont);
        }

        // Si no hay tags, limpiamos prefijos tipo [0] y sufijos [remix...]
        // para que el título no quede como un nombre de fichero bruto.
        if (title == QFileInfo(path).completeBaseName()) {
            title.remove(QRegularExpression(QStringLiteral("^\\[[^\\]]+\\]\\s*")));
            title.remove(QRegularExpression(QStringLiteral("\\s*\\[[^\\]]+\\]\\s*$")));
            title = title.trimmed();
        }

        m_audioTitle->setText(title);

        const qint64 d = probedDurationMs > 0
            ? probedDurationMs
            : (m_player ? m_player->duration() : 0);

        m_audioDuration->setText(
            d > 0
                ? QStringLiteral("Duración: ") + formatMs(d)
                : QStringLiteral("Duración: —")
        );

        m_stack->setCurrentWidget(m_audioView);
        m_kind = Kind::Audio;
        setHeader(path, "Audio");
    }

    m_mediaControls->setVisible(true);
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
    updateMediaUi();
    resizeForCurrentContent();
}

void PreviewWindow::showFallback(const QString &path, const QString &reason)
{
    QMimeDatabase db;
    QFileInfo fi(path);

    QString text = fi.fileName() + "\n\n" +
                   db.mimeTypeForFile(path, QMimeDatabase::MatchContent).name();

    if (!reason.isEmpty()) text += "\n\n" + reason;
    text += "\n\nENTER = abrir con la aplicación predeterminada";

    m_fallback->setText(text);
    m_fallback->setFont(QApplication::font());
    m_kind = Kind::Fallback;
    m_stack->setCurrentWidget(m_fallback);
    resizeForCurrentContent();
}

void PreviewWindow::zoomIn()
{
    if (m_kind == Kind::Image) {
        const int current = qRound(m_imageZoom * 100.0);
        const int next = qMin(250, current + 10);
        if (next == current)
            return;

        m_imageZoom = next / 100.0;
        scheduleImageRender();
    } else if (m_kind == Kind::Pdf && m_pdfView) {
        if (m_pdfView->zoomMode() != QPdfView::ZoomMode::Custom)
            m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(qMin(5.0, m_pdfView->zoomFactor() * 1.15));
        m_zoomReset->setText(QString::number(int(m_pdfView->zoomFactor() * 100)) + "%");
    }
}

void PreviewWindow::zoomOut()
{
    if (m_kind == Kind::Image) {
        const int current = qRound(m_imageZoom * 100.0);
        const int next = qMax(100, current - 10);
        if (next == current)
            return;

        m_imageZoom = next / 100.0;
        if (m_imageZoom <= 1.0)
            m_panOffset = QPointF();

        scheduleImageRender();
    } else if (m_kind == Kind::Pdf && m_pdfView) {
        if (m_pdfView->zoomMode() != QPdfView::ZoomMode::Custom)
            m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(qMax(0.20, m_pdfView->zoomFactor() / 1.15));
        m_zoomReset->setText(QString::number(int(m_pdfView->zoomFactor() * 100)) + "%");
    }
}

void PreviewWindow::resetZoom()
{
    if (m_kind == Kind::Image) {
        m_imageZoom = 1.0;
        m_panOffset = QPointF();

        if (m_zoomRenderTimer)
            m_zoomRenderTimer->stop();

        updateImageScale(true);
    } else if (m_kind == Kind::Pdf && m_pdfView) {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        m_zoomReset->setText("Ajustar");
    }
}

void PreviewWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space) {
        closePreview();
        return;
    }

    if (event->key() == Qt::Key_Left) { goSibling(-1); return; }
    if (event->key() == Qt::Key_Right) { goSibling(1); return; }

    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) { zoomIn(); return; }
    if (event->key() == Qt::Key_Minus) { zoomOut(); return; }
    if (event->key() == Qt::Key_0) { resetZoom(); return; }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        stopMovie();
        stopMedia();
        QProcess::startDetached("xdg-open", {m_path});
        close();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void PreviewWindow::wheelEvent(QWheelEvent *event)
{
    if (m_kind == Kind::Image) {
        const int dy = event->angleDelta().y();

        if (dy != 0) {
            m_wheelAccumulator += dy;

            // Una muesca estándar = 120 unidades Qt.
            // No importa cuántos eventos fraccionados entregue Wayland/ratón:
            // SOLO avanzamos un nivel por muesca completa.
            while (m_wheelAccumulator >= 120) {
                zoomIn();
                m_wheelAccumulator -= 120;
            }

            while (m_wheelAccumulator <= -120) {
                zoomOut();
                m_wheelAccumulator += 120;
            }
        }

        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn();
        else if (event->angleDelta().y() < 0) zoomOut();
        event->accept();
        return;
    }

    QMainWindow::wheelEvent(event);
}




void PreviewWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void PreviewWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    const QString local = urls.first().toLocalFile();
    if (!local.isEmpty() && QFileInfo::exists(local)) {
        openPath(local, true);
        event->acceptProposedAction();
    }
}

bool PreviewWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!isVisible())
        return QMainWindow::eventFilter(watched, event);

    // Frameless native window interaction.
    // This works on KDE Wayland and does not depend on which child widget
    // is below the pointer (image, PDF, text, video, etc.).
    QWidget *eventWidget = qobject_cast<QWidget *>(watched);

    if (eventWidget &&
        eventWidget->window() == this &&
        event->type() == QEvent::MouseButtonPress) {

        auto *me = static_cast<QMouseEvent *>(event);

        if (me->button() == Qt::LeftButton && windowHandle()) {
            const QPoint local =
                mapFromGlobal(me->globalPosition().toPoint());

            // Invisible resize grip around every edge/corner.
            const int grip = 9;
            Qt::Edges edges;

            if (local.x() <= grip)
                edges |= Qt::LeftEdge;
            else if (local.x() >= width() - grip - 1)
                edges |= Qt::RightEdge;

            if (local.y() <= grip)
                edges |= Qt::TopEdge;
            else if (local.y() >= height() - grip - 1)
                edges |= Qt::BottomEdge;

            if (edges != Qt::Edges()) {
                m_userChangedGeometry = true;
                windowHandle()->startSystemResize(edges);
                return true;
            }

            // Move from the header, but never steal clicks from buttons/sliders.
            bool interactive = false;
            QObject *node = watched;

            while (node && node != this) {
                if (qobject_cast<QPushButton *>(node) ||
                    qobject_cast<QSlider *>(node)) {
                    interactive = true;
                    break;
                }
                node = node->parent();
            }

            const int headerBottom =
                m_stack ? m_stack->geometry().top() : 64;

            if (!interactive && local.y() < headerBottom) {
                m_userChangedGeometry = true;
                windowHandle()->startSystemMove();
                return true;
            }
        }
    }


    // PRIORIDAD GLOBAL: Space/Esc nunca llega a QTextEdit/PDF/etc.
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);

        if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Escape) {
            closePreview();
            return true;
        }

        if (ke->key() == Qt::Key_Left &&
            !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            goSibling(-1);
            return true;
        }

        if (ke->key() == Qt::Key_Right &&
            !(ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            goSibling(1);
            return true;
        }
    }

    // FOTO AMPLIADA: click izquierdo + arrastrar, como tocar y mover.
    if (watched == m_image && m_kind == Kind::Image && imageCanPan()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_draggingImage = true;
                m_dragStartPos = me->position();
                m_dragStartOffset = m_panOffset;
                m_image->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_draggingImage) {
            auto *me = static_cast<QMouseEvent *>(event);
            const QPointF delta = me->position() - m_dragStartPos;

            m_panOffset = m_dragStartOffset + delta;
            clampPanOffset();

            // No reescalamos: solo recortamos el cache ya existente.
            renderImageViewport(false);
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && m_draggingImage) {
                m_draggingImage = false;
                m_image->setCursor(Qt::OpenHandCursor);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}


void PreviewWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_userChangedGeometry)
        return;

    // The KWin Apply-Initially rule is the authoritative Wayland placement.
    // These calls keep X11 / same-process navigation consistent too.
    applyCanonicalInitialGeometry();

    for (const int delay : {0, 60, 180, 350}) {
        QTimer::singleShot(delay, this, [this]() {
            if (!m_userChangedGeometry)
                applyCanonicalInitialGeometry();
        });
    }
}

void PreviewWindow::hideEvent(QHideEvent *event)
{
    // Regla QuickLook: nada multimedia puede seguir sonando/reproduciéndose
    // cuando la ventana deja de estar visible.
    stopMovie();
    stopMedia();
    QMainWindow::hideEvent(event);
}

void PreviewWindow::closeEvent(QCloseEvent *event)
{
    if (m_zoomRenderTimer)
        m_zoomRenderTimer->stop();

    stopMovie();
    stopMedia();
    QMainWindow::closeEvent(event);
}

void PreviewWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if ((m_kind == Kind::Image || m_kind == Kind::Gif) && m_stack) {
        const QSize vp = m_stack->contentsRect().size();

        if (vp.isValid() && vp != m_photoViewport) {
            m_photoViewport = vp;

            // Images always re-cover the whole preview area.
            if (m_kind == Kind::Image && m_originalPixmap)
                renderImageViewport(false);

            if (m_kind == Kind::Gif && m_movie) {
                QSize gifSize = m_movie->frameRect().size();

                if (gifSize.isValid()) {
                    gifSize.scale(vp, Qt::KeepAspectRatioByExpanding);
                    m_movie->setScaledSize(gifSize);
                }
            }
        }
    }

    // Keep the same FIX4B visual mask/border attached to the resized stack.
    updateContentMask();
}
