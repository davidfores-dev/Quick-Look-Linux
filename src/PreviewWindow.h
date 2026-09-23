#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QSlider>

class QLabel;
class QTextEdit;
class QStackedWidget;
class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QPdfDocument;
class QPdfView;
class QTemporaryDir;
class QKeyEvent;
class QResizeEvent;
class QWheelEvent;
class QHideEvent;
class QCloseEvent;
class QShowEvent;
class QPushButton;
class QWidget;
class QMovie;
class QCheckBox;
class QDialog;
class QSettings;
class QTimer;
class QPixmap;
class QMouseEvent;
class QEvent;
class QDropEvent;
class QComboBox;
class QPointF;
class QDragEnterEvent;

class ClickSlider : public QSlider
{
public:
    using QSlider::QSlider;
protected:
    void mousePressEvent(QMouseEvent *event) override;
};

class PreviewWindow : public QMainWindow
{
public:
    explicit PreviewWindow(const QString &path, QWidget *parent = nullptr);
    ~PreviewWindow() override;

    void openPath(const QString &path, bool rebuildSiblings);
    void closePreview();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Kind { Image, Gif, Text, Pdf, Audio, Video, Archive, Font, Fallback };

    void buildUi();
    void buildMediaControls();
    void applyAppearance();
    void applySkin(const QString &skin);
    void showSettingsDialog();
    void resizeForCurrentContent(const QSize &intrinsic = QSize());

    void loadFile(const QString &path);
    void showImage(const QString &path);
    void showText(const QString &path);
    void showPdf(const QString &path);
    void showMedia(const QString &path, bool video);
    bool showOffice(const QString &path);
    bool showArchive(const QString &path);
    bool showFont(const QString &path);
    bool showEpub(const QString &path);
    void showFallback(const QString &path, const QString &reason = {});

    void setHeader(const QString &path, const QString &detail = {});
    void updateImageScale(bool highQuality = true);
    void scheduleImageRender();
    void renderImageViewport(bool highQuality = true);
    void clampPanOffset();
    QSize calculatePhotoViewport(const QSize &intrinsic) const;
    void lockPhotoWindowToViewport(const QSize &viewport);
    void stopMedia();
    void stopMovie();

    void rebuildSiblingList();
    void goSibling(int delta);
    bool isPreviewableCandidate(const QString &path) const;

    QString formatMs(qint64 ms) const;
    void updateMediaUi();
    void seekRelative(qint64 deltaMs);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void updateContentMask();
    void recenterCanonicalWindow();
    void applyCanonicalInitialGeometry();
    bool imageCanPan() const;

    QString officeCachePath(const QString &path) const;

    QString m_path;
    QString m_imagePath;
    Kind m_kind = Kind::Fallback;

    QStringList m_siblings;
    int m_siblingIndex = -1;

    QWidget *m_root = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_detail = nullptr;
    QLabel *m_image = nullptr;
    QTextEdit *m_text = nullptr;
    QWidget *m_audioView = nullptr;
    QLabel *m_audioCard = nullptr;
    QLabel *m_audioTitle = nullptr;
    QLabel *m_audioDuration = nullptr;
    QLabel *m_fallback = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_contentOverlay = nullptr;
    QVideoWidget *m_video = nullptr;

    QWidget *m_mediaControls = nullptr;
    QPushButton *m_back15 = nullptr;
    QPushButton *m_playPause = nullptr;
    QPushButton *m_forward15 = nullptr;
    ClickSlider *m_seek = nullptr;
    QLabel *m_time = nullptr;
    QSlider *m_volume = nullptr;

    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_open = nullptr;
    QPushButton *m_close = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QPushButton *m_zoomOut = nullptr;
    QPushButton *m_zoomReset = nullptr;
    QPushButton *m_zoomIn = nullptr;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QPdfDocument *m_pdfDoc = nullptr;
    QWidget *m_pdfFrame = nullptr;
    QPdfView *m_pdfView = nullptr;
    QMovie *m_movie = nullptr;

    bool m_userSeeking = false;
    double m_imageZoom = 1.0;
    QPixmap *m_originalPixmap = nullptr;
    QPixmap *m_scaledPixmapCache = nullptr;
    QTimer *m_zoomRenderTimer = nullptr;
    QSize m_photoViewport;
    int m_wheelAccumulator = 0;
    QPointF m_panOffset;
    QPointF m_dragStartPos;
    QPointF m_dragStartOffset;
    bool m_draggingImage = false;
    bool m_windowDragging = false;
    bool m_initialGeometryApplied = false;
    bool m_userChangedGeometry = false;
    QPointF m_windowDragStart;
    QPoint m_windowStartPos;
    double m_windowOpacity = 0.92;
    bool m_adaptiveWindow = true;
    QString m_skin = "generic";
};
