#include "cam-morph-common.h"
#include <obs-frontend-api.h>

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <functional>
#include <map>

// Classic Macintosh OS 8/9 Platinum QSS Theme
static const char *MAC_PLATINUM_QSS = R"(
QDialog {
    background-color: #DDDDDD;
    color: #000000;
    font-family: "Geneva", "Tahoma", "MS Sans Serif", sans-serif;
    font-size: 11px;
}
QLabel {
    color: #000000;
    background: transparent;
}
QTabWidget::pane {
    border-top: 2px solid #FFFFFF;
    border-left: 2px solid #FFFFFF;
    border-right: 2px solid #666666;
    border-bottom: 2px solid #666666;
    background-color: #DDDDDD;
    padding: 6px;
}
QTabBar::tab {
    background-color: #CCCCCC;
    color: #000000;
    border-top: 2px solid #FFFFFF;
    border-left: 2px solid #FFFFFF;
    border-right: 2px solid #666666;
    border-bottom: 1px solid #888888;
    padding: 5px 14px;
    margin-right: 2px;
    font-weight: bold;
}
QTabBar::tab:selected {
    background-color: #DDDDDD;
    border-bottom: 2px solid #DDDDDD;
    margin-bottom: -1px;
}
QPushButton {
    background-color: #DDDDDD;
    color: #000000;
    border-top: 2px solid #FFFFFF;
    border-left: 2px solid #FFFFFF;
    border-right: 2px solid #555555;
    border-bottom: 2px solid #555555;
    padding: 4px 10px;
    font-weight: bold;
    min-height: 18px;
}
QPushButton:hover {
    background-color: #E6E6E6;
}
QPushButton:pressed, QPushButton:checked {
    background-color: #BBBBBB;
    border-top: 2px solid #555555;
    border-left: 2px solid #555555;
    border-right: 2px solid #FFFFFF;
    border-bottom: 2px solid #FFFFFF;
}
QGroupBox {
    border: 1px solid #888888;
    border-top: 1px solid #888888;
    margin-top: 10px;
    padding-top: 8px;
    font-weight: bold;
    color: #000000;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    background-color: #DDDDDD;
}
QComboBox {
    background-color: #FFFFFF;
    color: #000000;
    border-top: 2px solid #666666;
    border-left: 2px solid #666666;
    border-right: 2px solid #FFFFFF;
    border-bottom: 2px solid #FFFFFF;
    padding: 2px 6px;
}
QComboBox QAbstractItemView {
    background-color: #FFFFFF;
    color: #000000;
    selection-background-color: #333399;
    selection-color: #FFFFFF;
}
QSlider::groove:horizontal {
    height: 6px;
    background: #AAAAAA;
    border-top: 1px solid #555555;
    border-left: 1px solid #555555;
    border-bottom: 1px solid #FFFFFF;
    border-right: 1px solid #FFFFFF;
}
QSlider::handle:horizontal {
    background: #DDDDDD;
    border-top: 2px solid #FFFFFF;
    border-left: 2px solid #FFFFFF;
    border-right: 2px solid #444444;
    border-bottom: 2px solid #444444;
    width: 12px;
    margin: -5px 0;
}
QCheckBox {
    color: #000000;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 13px;
    height: 13px;
    background-color: #FFFFFF;
    border-top: 2px solid #555555;
    border-left: 2px solid #555555;
    border-right: 2px solid #FFFFFF;
    border-bottom: 2px solid #FFFFFF;
}
QCheckBox::indicator:checked {
    background-color: #333399;
}
)";

// Classic Macintosh Pinstripe Header Bar
class MacPinstripeHeader : public QWidget {
public:
    explicit MacPinstripeHeader(const QString &title, QWidget *parent = nullptr)
        : QWidget(parent), m_title(title)
    {
        setFixedHeight(26);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor("#DDDDDD"));

        // Classic horizontal pinstripes
        for (int y = 4; y < height() - 4; y += 3) {
            p.setPen(QColor("#888888"));
            p.drawLine(8, y, width() - 8, y);
            p.setPen(QColor("#FFFFFF"));
            p.drawLine(8, y + 1, width() - 8, y + 1);
        }

        // Centered title box interrupting pinstripes
        QFont f = font();
        f.setBold(true);
        p.setFont(f);
        QFontMetrics fm(f);
        int tw = fm.horizontalAdvance(m_title) + 18;
        QRect titleRect((width() - tw) / 2, 2, tw, height() - 4);
        p.fillRect(titleRect, QColor("#DDDDDD"));
        p.setPen(QColor("#000000"));
        p.drawText(titleRect, Qt::AlignCenter, m_title);

        // Bottom separator
        p.setPen(QColor("#666666"));
        p.drawLine(0, height() - 1, width(), height() - 1);
    }

private:
    QString m_title;
};

// Retro Segmented LED VU-Meter for Microphone
class RetroVuMeter : public QWidget {
public:
    explicit RetroVuMeter(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(22);
    }

    void setValues(float db, float thresholdDb, bool talking)
    {
        m_db = db;
        m_thresh = thresholdDb;
        m_talking = talking;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        // Sunken Mac bezel
        p.fillRect(rect(), QColor("#181A18"));
        p.setPen(QColor("#555555"));
        p.drawLine(0, 0, width() - 1, 0);
        p.drawLine(0, 0, 0, height() - 1);
        p.setPen(QColor("#FFFFFF"));
        p.drawLine(0, height() - 1, width() - 1, height() - 1);
        p.drawLine(width() - 1, 0, width() - 1, height() - 1);

        int pad = 4;
        int innerW = width() - pad * 2;
        int innerH = height() - pad * 2;
        int segments = 28;
        float segW = (float)innerW / (float)segments;

        float normDb = (m_db + 80.0f) / 80.0f;
        if (normDb < 0.0f) normDb = 0.0f;
        if (normDb > 1.0f) normDb = 1.0f;
        int activeSegs = (int)(normDb * segments);

        for (int i = 0; i < segments; i++) {
            QRectF r(pad + i * segW + 1, pad, segW - 2, innerH);
            bool lit = (i < activeSegs);
            QColor col;
            if (i < 18) {
                col = lit ? QColor("#33DD55") : QColor("#1E3322");
            } else if (i < 24) {
                col = lit ? QColor("#FFCC22") : QColor("#3A3318");
            } else {
                col = lit ? QColor("#EE3333") : QColor("#3A1A1A");
            }
            p.fillRect(r, col);
        }

        // Threshold marker line
        float normT = (m_thresh + 80.0f) / 80.0f;
        if (normT < 0.0f) normT = 0.0f;
        if (normT > 1.0f) normT = 1.0f;
        int tx = pad + (int)(normT * innerW);
        p.setPen(QPen(m_talking ? QColor("#FFFFFF") : QColor("#FF6666"), 2));
        p.drawLine(tx, 2, tx, height() - 3);
    }

private:
    float m_db = -100.0f;
    float m_thresh = -38.0f;
    bool m_talking = false;
};

// Drag & Drop PNG Avatar Tile
class AvatarDropSlot : public QFrame {
public:
    AvatarDropSlot(const QString &labelText, std::function<void(const QString &)> onChanged, QWidget *parent = nullptr)
        : QFrame(parent), m_onChanged(std::move(onChanged))
    {
        setAcceptDrops(true);
        setFixedSize(118, 128);
        setStyleSheet(
            "QFrame { background-color: #CCCCCC; "
            "border-top: 2px solid #666666; border-left: 2px solid #666666; "
            "border-right: 2px solid #FFFFFF; border-bottom: 2px solid #FFFFFF; }");

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(2);

        m_preview = new QLabel("Перетащи PNG\nили кликни", this);
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setStyleSheet("border: none; color: #444444; font-size: 10px;");
        m_preview->setFixedSize(106, 94);

        m_caption = new QLabel(labelText, this);
        m_caption->setAlignment(Qt::AlignCenter);
        m_caption->setStyleSheet("border: none; font-weight: bold; font-size: 10px;");

        lay->addWidget(m_preview);
        lay->addWidget(m_caption);
    }

    void setImagePath(const QString &path)
    {
        m_path = path;
        if (!path.isEmpty()) {
            QPixmap pix(path);
            if (!pix.isNull()) {
                m_preview->setPixmap(pix.scaled(102, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                return;
            }
        }
        m_preview->setPixmap(QPixmap());
        m_preview->setText("Перетащи PNG\nили кликни");
    }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            QString f = QFileDialog::getOpenFileName(
                this, "Выбрать изображение PNG", m_path,
                "Изображения (*.png *.jpg *.jpeg *.webp *.bmp)");
            if (!f.isEmpty()) {
                setImagePath(f);
                if (m_onChanged) m_onChanged(f);
            }
        } else if (e->button() == Qt::RightButton) {
            setImagePath("");
            if (m_onChanged) m_onChanged("");
        }
    }

    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (e->mimeData()->hasUrls()) e->acceptProposedAction();
    }

    void dropEvent(QDropEvent *e) override
    {
        auto urls = e->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString f = urls.first().toLocalFile();
            if (!f.isEmpty()) {
                setImagePath(f);
                if (m_onChanged) m_onChanged(f);
            }
        }
    }

private:
    QLabel *m_preview;
    QLabel *m_caption;
    QString m_path;
    std::function<void(const QString &)> m_onChanged;
};

static QColor obsIntToQColor(uint32_t abgr)
{
    int r = (abgr) & 0xFF;
    int g = (abgr >> 8) & 0xFF;
    int b = (abgr >> 16) & 0xFF;
    int a = (abgr >> 24) & 0xFF;
    return QColor(r, g, b, a);
}

static uint32_t qColorToObsInt(const QColor &c)
{
    return ((uint32_t)c.alpha() << 24) |
           ((uint32_t)c.blue() << 16) |
           ((uint32_t)c.green() << 8) |
           ((uint32_t)c.red());
}

class CamMorphRetroDialog : public QDialog {
public:
    CamMorphRetroDialog(struct cam_morph_data *filter, QWidget *parent)
        : QDialog(parent), m_filter(filter)
    {
        setWindowTitle("CamMorph 0.3 — Macintosh Platinum");
        setStyleSheet(MAC_PLATINUM_QSS);
        setMinimumWidth(520);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(8, 6, 8, 8);
        root->setSpacing(6);

        // Pinstripe Mac Header
        root->addWidget(new MacPinstripeHeader("CamMorph v0.3 — Панель управления", this));

        // Top Mode Bar (Big Retro Toggle + LED)
        auto *modeBar = new QHBoxLayout();
        m_btnWebcam = new QPushButton("●  ВЕБКАМЕРА (LIVE)", this);
        m_btnPng = new QPushButton("○  PNGTUBER (МОДЕЛЬКА)", this);
        m_btnWebcam->setCheckable(true);
        m_btnPng->setCheckable(true);
        m_btnWebcam->setFixedHeight(30);
        m_btnPng->setFixedHeight(30);

        modeBar->addWidget(m_btnWebcam);
        modeBar->addWidget(m_btnPng);
        root->addLayout(modeBar);

        // Tabs
        auto *tabs = new QTabWidget(this);
        tabs->addTab(buildWebcamTab(), "Вебкамера");
        tabs->addTab(buildBorderTab(), "Рамка и тень");
        tabs->addTab(buildPngTab(), "PNGTuber");
        tabs->addTab(buildBgTab(), "Фон");
        root->addWidget(tabs);

        // Bottom Bar
        auto *bottom = new QHBoxLayout();
        auto *hint = new QLabel("ПКМ по плитке PNG — очистить слот. Хоткей: Настройки → Горячие клавиши.", this);
        hint->setStyleSheet("color: #444444; font-size: 10px;");
        auto *btnClose = new QPushButton("Закрыть", this);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
        bottom->addWidget(hint);
        bottom->addStretch();
        bottom->addWidget(btnClose);
        root->addLayout(bottom);

        connect(m_btnWebcam, &QPushButton::clicked, this, [this]() { setMode(MODE_WEBCAM); });
        connect(m_btnPng, &QPushButton::clicked, this, [this]() { setMode(MODE_PNGTUBER); });

        loadFromSettings();

        // Live VU-meter & mode sync timer (30 FPS)
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (!m_filter) return;
            float db = cam_morph_get_current_db(m_filter);
            bool talk = cam_morph_is_talking(m_filter);
            obs_source_t *src = cam_morph_get_source(m_filter);
            float thresh = -38.0f;
            int curMode = MODE_WEBCAM;
            if (src) {
                obs_data_t *s = obs_source_get_settings(src);
                thresh = (float)obs_data_get_double(s, "audio_threshold");
                curMode = (int)obs_data_get_int(s, "mode");
                obs_data_release(s);
            }
            if (m_vuMeter) m_vuMeter->setValues(db, thresh, talk);
            updateModeButtons(curMode);
        });
        m_timer->start(33);
    }

    void detachFilter()
    {
        m_filter = nullptr;
        if (m_timer) m_timer->stop();
        close();
    }

private:
    struct cam_morph_data *m_filter;
    QTimer *m_timer = nullptr;

    QPushButton *m_btnWebcam = nullptr;
    QPushButton *m_btnPng = nullptr;
    RetroVuMeter *m_vuMeter = nullptr;

    // Controls for sync
    std::vector<std::function<void(obs_data_t *)>> m_loaders;

    void applyDouble(const char *key, double val)
    {
        if (!m_filter) return;
        obs_source_t *src = cam_morph_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_double(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyInt(const char *key, long long val)
    {
        if (!m_filter) return;
        obs_source_t *src = cam_morph_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_int(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyBool(const char *key, bool val)
    {
        if (!m_filter) return;
        obs_source_t *src = cam_morph_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_bool(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyString(const char *key, const QString &val)
    {
        if (!m_filter) return;
        obs_source_t *src = cam_morph_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_string(s, key, val.toUtf8().constData());
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void setMode(int mode)
    {
        applyInt("mode", mode);
        updateModeButtons(mode);
    }

    void updateModeButtons(int mode)
    {
        bool isWeb = (mode == MODE_WEBCAM);
        m_btnWebcam->setChecked(isWeb);
        m_btnPng->setChecked(!isWeb);
        m_btnWebcam->setText(isWeb ? "●  ВЕБКАМЕРА (LIVE)" : "○  ВЕБКАМЕРА (LIVE)");
        m_btnPng->setText(!isWeb ? "●  PNGTUBER (МОДЕЛЬКА)" : "○  PNGTUBER (МОДЕЛЬКА)");
    }

    QWidget *addSliderRow(QGridLayout *grid, int row, const QString &label,
                          const char *key, double minV, double maxV, double scale,
                          const QString &suffix)
    {
        auto *lbl = new QLabel(label, this);
        auto *sl = new QSlider(Qt::Horizontal, this);
        sl->setRange((int)std::round(minV * scale), (int)std::round(maxV * scale));
        auto *valLbl = new QLabel(this);
        valLbl->setFixedWidth(56);
        valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        grid->addWidget(lbl, row, 0);
        grid->addWidget(sl, row, 1);
        grid->addWidget(valLbl, row, 2);

        connect(sl, &QSlider::valueChanged, this, [this, key, scale, valLbl, suffix](int v) {
            double realV = (double)v / scale;
            valLbl->setText(QString::number(realV, 'f', scale >= 100.0 ? 2 : (scale >= 10.0 ? 1 : 0)) + suffix);
            applyDouble(key, realV);
        });

        m_loaders.push_back([sl, valLbl, key, scale, suffix](obs_data_t *s) {
            double realV = obs_data_get_double(s, key);
            sl->blockSignals(true);
            sl->setValue((int)std::round(realV * scale));
            sl->blockSignals(false);
            valLbl->setText(QString::number(realV, 'f', scale >= 100.0 ? 2 : (scale >= 10.0 ? 1 : 0)) + suffix);
        });
        return sl;
    }

    QPushButton *addColorRow(QGridLayout *grid, int row, const QString &label, const char *key)
    {
        grid->addWidget(new QLabel(label, this), row, 0);
        auto *btn = new QPushButton("Выбрать цвет...", this);
        grid->addWidget(btn, row, 1, 1, 2);

        auto updateBtnStyle = [btn](const QColor &c) {
            btn->setText(QString("RGBA(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
            btn->setStyleSheet(QString("background-color: rgba(%1,%2,%3,255); color: %4;")
                                   .arg(c.red()).arg(c.green()).arg(c.blue())
                                   .arg((c.lightness() < 128) ? "#FFFFFF" : "#000000"));
        };

        connect(btn, &QPushButton::clicked, this, [this, key, updateBtnStyle]() {
            if (!m_filter) return;
            obs_source_t *src = cam_morph_get_source(m_filter);
            obs_data_t *s = obs_source_get_settings(src);
            QColor cur = obsIntToQColor((uint32_t)obs_data_get_int(s, key));
            obs_data_release(s);

            QColor picked = QColorDialog::getColor(cur, this, "Выбор цвета", QColorDialog::ShowAlphaChannel);
            if (picked.isValid()) {
                applyInt(key, (long long)qColorToObsInt(picked));
                updateBtnStyle(picked);
            }
        });

        m_loaders.push_back([key, updateBtnStyle](obs_data_t *s) {
            QColor cur = obsIntToQColor((uint32_t)obs_data_get_int(s, key));
            updateBtnStyle(cur);
        });
        return btn;
    }

    QWidget *buildWebcamTab()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);

        // Quick Presets
        auto *presetBox = new QHBoxLayout();
        auto *bCircle = new QPushButton("Круг", this);
        auto *bSquare = new QPushButton("Квадрат 1:1", this);
        auto *bWide = new QPushButton("Кино 16:9", this);
        auto *bReset = new QPushButton("Сброс", this);
        presetBox->addWidget(bCircle);
        presetBox->addWidget(bSquare);
        presetBox->addWidget(bWide);
        presetBox->addWidget(bReset);
        lay->addLayout(presetBox);

        connect(bCircle, &QPushButton::clicked, this, [this]() {
            applyDouble("crop_left", 21.8); applyDouble("crop_right", 21.8);
            applyDouble("crop_top", 0.0); applyDouble("crop_bottom", 0.0);
            applyDouble("corner_radius", 500.0);
            loadFromSettings();
        });
        connect(bSquare, &QPushButton::clicked, this, [this]() {
            applyDouble("crop_left", 21.8); applyDouble("crop_right", 21.8);
            applyDouble("crop_top", 0.0); applyDouble("crop_bottom", 0.0);
            applyDouble("corner_radius", 40.0);
            loadFromSettings();
        });
        connect(bWide, &QPushButton::clicked, this, [this]() {
            applyDouble("crop_left", 0.0); applyDouble("crop_right", 0.0);
            applyDouble("crop_top", 0.0); applyDouble("crop_bottom", 0.0);
            applyDouble("corner_radius", 32.0);
            loadFromSettings();
        });
        connect(bReset, &QPushButton::clicked, this, [this]() {
            applyDouble("crop_left", 0.0); applyDouble("crop_right", 0.0);
            applyDouble("crop_top", 0.0); applyDouble("crop_bottom", 0.0);
            applyDouble("zoom", 1.0); applyDouble("pan_x", 0.0); applyDouble("pan_y", 0.0);
            applyDouble("corner_radius", 32.0);
            loadFromSettings();
        });

        auto *grid = new QGridLayout();
        int r = 0;
        addSliderRow(grid, r++, "Скругление углов", "corner_radius", 0, 500, 1, " px");
        addSliderRow(grid, r++, "Зум", "zoom", 1.0, 4.0, 100, "x");
        addSliderRow(grid, r++, "Сдвиг X", "pan_x", -1.0, 1.0, 100, "");
        addSliderRow(grid, r++, "Сдвиг Y", "pan_y", -1.0, 1.0, 100, "");
        addSliderRow(grid, r++, "Кроп слева", "crop_left", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп справа", "crop_right", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп сверху", "crop_top", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп снизу", "crop_bottom", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Скорость перехода", "transition_speed", 0.0, 2.0, 20, " с");
        lay->addLayout(grid);
        lay->addStretch();
        return w;
    }

    QWidget *buildBorderTab()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);

        auto *gBorder = new QGroupBox("Рамка и свечение", this);
        auto *bGrid = new QGridLayout(gBorder);
        int r = 0;
        addSliderRow(bGrid, r++, "Толщина рамки", "border_width", 0, 40, 2, " px");
        addColorRow(bGrid, r++, "Цвет рамки", "border_color");
        addSliderRow(bGrid, r++, "Радиус свечения", "glow_size", 0, 100, 1, " px");
        addSliderRow(bGrid, r++, "Яркость свечения", "glow_intensity", 0.0, 2.0, 20, "");
        addColorRow(bGrid, r++, "Цвет свечения", "glow_color");
        lay->addWidget(gBorder);

        auto *gShadow = new QGroupBox("Тень (Drop Shadow)", this);
        auto *sGrid = new QGridLayout(gShadow);
        auto *chkShadow = new QCheckBox("Включить тень", this);
        sGrid->addWidget(chkShadow, 0, 0, 1, 3);
        connect(chkShadow, &QCheckBox::toggled, this, [this](bool v) { applyBool("shadow_enable", v); });
        m_loaders.push_back([chkShadow](obs_data_t *s) {
            chkShadow->blockSignals(true);
            chkShadow->setChecked(obs_data_get_bool(s, "shadow_enable"));
            chkShadow->blockSignals(false);
        });
        int sr = 1;
        addSliderRow(sGrid, sr++, "Мягкость тени", "shadow_blur", 0, 100, 1, " px");
        addSliderRow(sGrid, sr++, "Сдвиг X", "shadow_offset_x", -100, 100, 1, " px");
        addSliderRow(sGrid, sr++, "Сдвиг Y", "shadow_offset_y", -100, 100, 1, " px");
        addColorRow(sGrid, sr++, "Цвет тени", "shadow_color");
        lay->addWidget(gShadow);

        lay->addStretch();
        return w;
    }

    static bool enumAudioCb(void *param, obs_source_t *src)
    {
        auto *combo = static_cast<QComboBox *>(param);
        if ((obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO) != 0) {
            QString name = QString::fromUtf8(obs_source_get_name(src));
            combo->addItem(name, name);
        }
        return true;
    }

    QWidget *buildPngTab()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);

        // 4 Drag & Drop Avatar Tiles
        auto *tilesRow = new QHBoxLayout();
        auto *sIdle = new AvatarDropSlot("1. Молчит", [this](const QString &p) { applyString("img_idle", p); }, this);
        auto *sTalk = new AvatarDropSlot("2. Говорит", [this](const QString &p) { applyString("img_talk", p); }, this);
        auto *sBlink = new AvatarDropSlot("3. Моргает", [this](const QString &p) { applyString("img_blink", p); }, this);
        auto *sTB = new AvatarDropSlot("4. Гов+Морг", [this](const QString &p) { applyString("img_talk_blink", p); }, this);
        tilesRow->addWidget(sIdle);
        tilesRow->addWidget(sTalk);
        tilesRow->addWidget(sBlink);
        tilesRow->addWidget(sTB);
        lay->addLayout(tilesRow);

        m_loaders.push_back([sIdle, sTalk, sBlink, sTB](obs_data_t *s) {
            sIdle->setImagePath(QString::fromUtf8(obs_data_get_string(s, "img_idle")));
            sTalk->setImagePath(QString::fromUtf8(obs_data_get_string(s, "img_talk")));
            sBlink->setImagePath(QString::fromUtf8(obs_data_get_string(s, "img_blink")));
            sTB->setImagePath(QString::fromUtf8(obs_data_get_string(s, "img_talk_blink")));
        });

        // Mic & VU Meter
        auto *micBox = new QGroupBox("Микрофон и срабатывание", this);
        auto *mGrid = new QGridLayout(micBox);
        mGrid->addWidget(new QLabel("Микрофон:", this), 0, 0);
        auto *micCombo = new QComboBox(this);
        micCombo->addItem("(Не выбран)", "");
        obs_enum_sources(enumAudioCb, micCombo);
        mGrid->addWidget(micCombo, 0, 1, 1, 2);

        connect(micCombo, &QComboBox::currentIndexChanged, this, [this, micCombo](int idx) {
            applyString("audio_source", micCombo->itemData(idx).toString());
        });
        m_loaders.push_back([micCombo](obs_data_t *s) {
            QString cur = QString::fromUtf8(obs_data_get_string(s, "audio_source"));
            int idx = micCombo->findData(cur);
            micCombo->blockSignals(true);
            micCombo->setCurrentIndex(idx >= 0 ? idx : 0);
            micCombo->blockSignals(false);
        });

        m_vuMeter = new RetroVuMeter(this);
        mGrid->addWidget(m_vuMeter, 1, 0, 1, 3);

        addSliderRow(mGrid, 2, "Порог голоса", "audio_threshold", -80, 0, 2, " дБ");
        addSliderRow(mGrid, 3, "Удержание рта", "talk_hold_ms", 0, 600, 1, " мс");
        lay->addWidget(micBox);

        // Animations & Transform
        auto *animBox = new QGroupBox("Позиция и анимации", this);
        auto *aGrid = new QGridLayout(animBox);
        int ar = 0;
        addSliderRow(aGrid, ar++, "Масштаб модельки", "avatar_scale", 0.2, 2.5, 100, "x");
        addSliderRow(aGrid, ar++, "Дыхание", "anim_breath", 0.0, 0.12, 200, "");
        addSliderRow(aGrid, ar++, "Подпрыгивание", "anim_bounce", 0.0, 0.25, 200, "");
        addSliderRow(aGrid, ar++, "Покачивание", "anim_sway", 0.0, 10.0, 10, "°");
        lay->addWidget(animBox);

        return w;
    }

    QWidget *buildBgTab()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);

        auto *topRow = new QHBoxLayout();
        topRow->addWidget(new QLabel("Тип фона для PNGTuber:", this));
        auto *bgCombo = new QComboBox(this);
        bgCombo->addItem("Сплошной цвет", BG_COLOR);
        bgCombo->addItem("Картинка", BG_IMAGE);
        bgCombo->addItem("Размытая вебкамера", BG_BLUR_CAM);
        bgCombo->addItem("Прозрачный фон", BG_TRANSPARENT);
        topRow->addWidget(bgCombo);
        lay->addLayout(topRow);

        // Smart dynamic stack based on background type
        auto *stack = new QStackedWidget(this);

        // Page 0: Color
        auto *pColor = new QWidget(this);
        auto *cGrid = new QGridLayout(pColor);
        addColorRow(cGrid, 0, "Цвет фона:", "bg_color");
        cGrid->setRowStretch(1, 1);
        stack->addWidget(pColor);

        // Page 1: Image
        auto *pImg = new QWidget(this);
        auto *iLay = new QVBoxLayout(pImg);
        auto *btnPickBg = new QPushButton("Выбрать файл фона (PNG / JPG)...", this);
        auto *lblBgPath = new QLabel("(Файл не выбран)", this);
        lblBgPath->setWordWrap(true);
        iLay->addWidget(btnPickBg);
        iLay->addWidget(lblBgPath);
        iLay->addStretch();
        connect(btnPickBg, &QPushButton::clicked, this, [this, lblBgPath]() {
            QString f = QFileDialog::getOpenFileName(this, "Выбрать фон", "", "Изображения (*.png *.jpg *.jpeg *.webp *.bmp)");
            if (!f.isEmpty()) {
                applyString("bg_image_file", f);
                lblBgPath->setText(f);
            }
        });
        m_loaders.push_back([lblBgPath](obs_data_t *s) {
            QString p = QString::fromUtf8(obs_data_get_string(s, "bg_image_file"));
            lblBgPath->setText(p.isEmpty() ? "(Файл не выбран)" : p);
        });
        stack->addWidget(pImg);

        // Page 2: Blurred Cam
        auto *pBlur = new QWidget(this);
        auto *bGrid = new QGridLayout(pBlur);
        addSliderRow(bGrid, 0, "Сила размытия вебки", "bg_blur", 1, 50, 1, " px");
        bGrid->setRowStretch(1, 1);
        stack->addWidget(pBlur);

        // Page 3: Transparent
        auto *pTrans = new QWidget(this);
        auto *tLay = new QVBoxLayout(pTrans);
        tLay->addWidget(new QLabel("Фон полностью прозрачный — поверх игры видна только моделька.", this));
        tLay->addStretch();
        stack->addWidget(pTrans);

        lay->addWidget(stack);

        connect(bgCombo, &QComboBox::currentIndexChanged, this, [this, bgCombo, stack](int idx) {
            int mode = bgCombo->itemData(idx).toInt();
            applyInt("bg_mode", mode);
            stack->setCurrentIndex(mode);
        });
        m_loaders.push_back([bgCombo, stack](obs_data_t *s) {
            int mode = (int)obs_data_get_int(s, "bg_mode");
            int idx = bgCombo->findData(mode);
            bgCombo->blockSignals(true);
            bgCombo->setCurrentIndex(idx >= 0 ? idx : 0);
            bgCombo->blockSignals(false);
            stack->setCurrentIndex(mode >= 0 && mode <= 3 ? mode : 0);
        });

        lay->addStretch();
        return w;
    }

    void loadFromSettings()
    {
        if (!m_filter) return;
        obs_source_t *src = cam_morph_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        updateModeButtons((int)obs_data_get_int(s, "mode"));
        for (auto &fn : m_loaders) fn(s);
        obs_data_release(s);
    }
};

static std::map<struct cam_morph_data *, CamMorphRetroDialog *> g_dialogs;

extern "C" void cam_morph_ui_open(struct cam_morph_data *filter)
{
    if (!filter) return;
    auto it = g_dialogs.find(filter);
    if (it != g_dialogs.end() && it->second) {
        it->second->show();
        it->second->raise();
        it->second->activateWindow();
        return;
    }

    QMainWindow *mainWin = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    auto *dlg = new CamMorphRetroDialog(filter, mainWin);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dlg, &QObject::destroyed, [filter]() {
        g_dialogs.erase(filter);
    });
    g_dialogs[filter] = dlg;
    dlg->show();
}

extern "C" void cam_morph_ui_close_for_filter(struct cam_morph_data *filter)
{
    auto it = g_dialogs.find(filter);
    if (it != g_dialogs.end() && it->second) {
        it->second->detachFilter();
        g_dialogs.erase(it);
    }
}
