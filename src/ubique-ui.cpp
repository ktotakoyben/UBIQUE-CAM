#include "ubique-common.h"
#include <obs-frontend-api.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <vector>

// Ryoku 1.0 Amaterasu / Lithic Design System QSS
static const char *RYOKU_LITHIC_QSS = R"(
QDialog, QWidget#UbiqueDockRoot {
    background-color: #111214;
    color: #ECE9E2;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 12px;
}
QScrollArea {
    background: transparent;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background: transparent;
}
QScrollBar:vertical {
    background: #141518;
    width: 8px;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: #2E3036;
    border-radius: 4px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover {
    background: #FF9E64;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
QLabel {
    color: #ECE9E2;
    background: transparent;
}
QLabel[role="muted"] {
    color: #8E8C86;
    font-size: 11px;
}
QLabel[role="section"] {
    color: #8E8C86;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 10px;
    font-weight: bold;
    letter-spacing: 1.5px;
    padding-top: 2px;
    padding-bottom: 2px;
}
QLabel[role="mono"] {
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
    color: #ECE9E2;
    background-color: #111214;
    border: 1px solid #2A2B2E;
    border-radius: 4px;
    padding: 3px 6px;
}
QFrame[role="rail"] {
    background-color: #141518;
    border-right: 1px solid #26272B;
}
QFrame[role="card"] {
    background-color: #18191C;
    border: 1px solid #26272B;
    border-radius: 6px;
}
QFrame[role="divider"] {
    background-color: #26272B;
    max-height: 1px;
    min-height: 1px;
}
QPushButton {
    background-color: #1E2024;
    color: #ECE9E2;
    border: 1px solid #2E3036;
    border-radius: 5px;
    padding: 6px 12px;
    font-size: 12px;
    font-weight: 500;
}
QPushButton:hover {
    background-color: #26282E;
    border-color: #3E414A;
}
QPushButton:pressed {
    background-color: #16171A;
}
QPushButton[role="railBtn"] {
    background-color: transparent;
    color: #8E8C86;
    border: none;
    border-left: 2px solid transparent;
    border-radius: 0px;
    text-align: left;
    padding: 9px 14px;
    font-size: 12px;
    font-weight: 500;
}
QPushButton[role="railBtn"]:hover {
    background-color: #1C1D21;
    color: #ECE9E2;
}
QPushButton[role="railBtn"]:checked {
    background-color: #1C1D21;
    color: #ECE9E2;
    border-left: 2px solid #FF9E64;
    font-weight: 600;
}
QPushButton[role="modePill"] {
    background-color: #18191C;
    color: #8E8C86;
    border: 1px solid #2A2B2E;
    border-radius: 5px;
    padding: 7px 12px;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
    font-weight: bold;
    letter-spacing: 0.8px;
}
QPushButton[role="modePill"]:hover {
    color: #ECE9E2;
    border-color: #3E414A;
}
QPushButton[role="modePill"]:checked {
    background-color: #25211E;
    color: #FF9E64;
    border: 1px solid #FF9E64;
}
QPushButton[role="preset"] {
    background-color: #141518;
    color: #B8B5AD;
    border: 1px solid #2A2B2E;
    border-radius: 4px;
    padding: 5px 8px;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 10px;
    font-weight: bold;
}
QPushButton[role="preset"]:hover {
    color: #FF9E64;
    border-color: #FF9E64;
    background-color: #1E1C1A;
}
QComboBox {
    background-color: #141518;
    color: #ECE9E2;
    border: 1px solid #2E3036;
    border-radius: 5px;
    padding: 5px 28px 5px 10px;
    min-height: 22px;
}
QComboBox:hover {
    border-color: #FF9E64;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: top right;
    width: 24px;
    border-left: 1px solid #2A2B2E;
    background: transparent;
}
QComboBox::down-arrow {
    width: 0px;
    height: 0px;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid #8E8C86;
}
QSlider::groove:horizontal {
    height: 4px;
    background: #2A2B2E;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #FF9E64;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #ECE9E2;
    border: 2px solid #111214;
    width: 12px;
    height: 12px;
    border-radius: 6px;
    margin: -5px 0;
}
QSlider::handle:horizontal:hover {
    background: #FF9E64;
}
QCheckBox {
    color: #ECE9E2;
    spacing: 8px;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    background-color: #141518;
    border: 1px solid #3A3C42;
    border-radius: 4px;
}
QCheckBox::indicator:checked {
    background-color: #FF9E64;
    border-color: #FF9E64;
}
)";

static void styleLithicCombo(QComboBox *combo)
{
    QPalette pal = combo->palette();
    pal.setColor(QPalette::Window, QColor("#18191C"));
    pal.setColor(QPalette::Base, QColor("#18191C"));
    pal.setColor(QPalette::Text, QColor("#ECE9E2"));
    pal.setColor(QPalette::WindowText, QColor("#ECE9E2"));
    pal.setColor(QPalette::ButtonText, QColor("#ECE9E2"));
    pal.setColor(QPalette::Highlight, QColor("#25211E"));
    pal.setColor(QPalette::HighlightedText, QColor("#FF9E64"));
    combo->setPalette(pal);

    if (auto *v = combo->view()) {
        v->setPalette(pal);
        v->setStyleSheet(
            "QAbstractItemView { background-color: #18191C; color: #ECE9E2; border: 1px solid #2E3036; outline: none; padding: 3px; }"
            "QAbstractItemView::item { background-color: #18191C; color: #ECE9E2; padding: 6px 10px; min-height: 22px; border-radius: 3px; }"
            "QAbstractItemView::item:selected, QAbstractItemView::item:hover { background-color: #25211E; color: #FF9E64; }");
    }
}

class LithicVuMeter : public QWidget {
public:
    explicit LithicVuMeter(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedHeight(20);
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
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setBrush(QColor("#111214"));
        p.setPen(QColor("#26272B"));
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

        int pad = 4;
        int innerW = width() - pad * 2;
        int innerH = height() - pad * 2;
        int segments = 32;
        float segW = (float)innerW / (float)segments;

        float normDb = (m_db + 80.0f) / 80.0f;
        if (normDb < 0.0f) normDb = 0.0f;
        if (normDb > 1.0f) normDb = 1.0f;
        int activeSegs = (int)(normDb * segments);

        for (int i = 0; i < segments; i++) {
            QRectF r(pad + i * segW + 1, pad, segW - 2.0, innerH);
            bool lit = (i < activeSegs);
            QColor col;
            if (i < 21) {
                col = lit ? QColor("#ECE9E2") : QColor("#222428");
            } else if (i < 27) {
                col = lit ? QColor("#FF9E64") : QColor("#2C2520");
            } else {
                col = lit ? QColor("#E05A47") : QColor("#2E1F1E");
            }
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawRoundedRect(r, 1.5, 1.5);
        }

        float normT = (m_thresh + 80.0f) / 80.0f;
        if (normT < 0.0f) normT = 0.0f;
        if (normT > 1.0f) normT = 1.0f;
        int tx = pad + (int)(normT * innerW);
        p.setPen(QPen(m_talking ? QColor("#FF9E64") : QColor("#8E8C86"), 2));
        p.drawLine(tx, 2, tx, height() - 3);
    }

private:
    float m_db = -100.0f;
    float m_thresh = -38.0f;
    bool m_talking = false;
};

class AvatarDropSlot : public QFrame {
public:
    AvatarDropSlot(const QString &labelText, std::function<void(const QString &)> onChanged, QWidget *parent = nullptr)
        : QFrame(parent), m_onChanged(std::move(onChanged))
    {
        setAcceptDrops(true);
        setFixedSize(114, 130);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(
            "QFrame { background-color: #141518; border: 1px solid #2A2B2E; border-radius: 6px; }"
            "QFrame:hover { border-color: #FF9E64; background-color: #1A1B1F; }");

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(6, 6, 6, 6);
        lay->setSpacing(5);

        m_preview = new QLabel("PNG\nперетащи\nили кликни", this);
        m_preview->setAlignment(Qt::AlignCenter);
        m_preview->setStyleSheet(
            "QLabel { border: 1px dashed #2E3036; border-radius: 4px; background-color: #111214; "
            "color: #6B6A66; font-family: 'Cascadia Mono', Consolas, monospace; font-size: 9px; }");
        m_preview->setFixedSize(100, 90);

        m_caption = new QLabel(labelText, this);
        m_caption->setAlignment(Qt::AlignCenter);
        m_caption->setStyleSheet(
            "QLabel { border: none; background: transparent; color: #ECE9E2; "
            "font-family: 'Cascadia Mono', Consolas, monospace; font-size: 10px; font-weight: bold; }");

        lay->addWidget(m_preview);
        lay->addWidget(m_caption);
    }

    void setImagePath(const QString &path)
    {
        m_path = path;
        if (!path.isEmpty()) {
            QPixmap pix(path);
            if (!pix.isNull()) {
                m_preview->setPixmap(pix.scaled(94, 84, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                return;
            }
        }
        m_preview->setPixmap(QPixmap());
        m_preview->setText("PNG\nперетащи\nили кликни");
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

// Keys saved into custom user preset slots
static const char *PRESET_DOUBLE_KEYS[] = {
    "corner_radius", "zoom", "pan_x", "pan_y",
    "crop_left", "crop_right", "crop_top", "crop_bottom",
    "vignette", "contrast", "warmth",
    "border_width", "border_grad_speed", "glow_size", "glow_intensity",
    "border_react_strength", "shadow_blur", "shadow_offset_x", "shadow_offset_y",
    "spec_sensitivity", "spec_height", "spec_opacity",
    "avatar_scale", "avatar_offset_x", "avatar_offset_y",
    "anim_breath", "anim_bounce", "anim_sway", "bg_blur"
};

static const char *PRESET_INT_KEYS[] = {
    "mode", "transition_type", "border_style",
    "border_color", "border_color2", "glow_color", "peak_color", "shadow_color",
    "spec_layer", "spec_design", "spec_color", "bg_mode", "bg_color"
};

static const char *PRESET_BOOL_KEYS[] = {
    "flip_h", "border_audio_react", "shadow_enable"
};

class UbiqueCamDialog : public QDialog {
public:
    UbiqueCamDialog(struct ubique_cam_data *filter, QWidget *parent)
        : QDialog(parent), m_filter(filter)
    {
        setWindowTitle("UBIQUE CAM — Настройка");
        setStyleSheet(RYOKU_LITHIC_QSS);
        setMinimumSize(720, 560);

        auto *outer = new QHBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        // 1. Left Navigation Rail (Ryoku Lithic)
        auto *rail = new QFrame(this);
        rail->setProperty("role", "rail");
        rail->setFixedWidth(174);
        auto *railLay = new QVBoxLayout(rail);
        railLay->setContentsMargins(0, 16, 0, 14);
        railLay->setSpacing(4);

        auto *brand = new QLabel("UBIQUE CAM", rail);
        brand->setStyleSheet(
            "font-family: 'Cascadia Mono', Consolas, monospace; font-size: 13px; "
            "font-weight: bold; letter-spacing: 2px; color: #ECE9E2; padding: 0 14px;");
        auto *subBrand = new QLabel("v0.5  •  MAX EDITION", rail);
        subBrand->setStyleSheet(
            "font-family: 'Cascadia Mono', Consolas, monospace; font-size: 9px; "
            "letter-spacing: 1px; color: #FF9E64; padding: 0 14px 10px 14px;");
        railLay->addWidget(brand);
        railLay->addWidget(subBrand);

        auto *div = new QFrame(rail);
        div->setProperty("role", "divider");
        railLay->addWidget(div);
        railLay->addSpacing(8);

        auto *navGroup = new QButtonGroup(this);
        navGroup->setExclusive(true);

        const QStringList navNames = {
            "Камера и оптика",
            "Рамка и тень",
            "Аудио-спектр",
            "PNGTuber",
            "Фон и слоты"
        };
        for (int i = 0; i < navNames.size(); i++) {
            auto *btn = new QPushButton(navNames[i], rail);
            btn->setProperty("role", "railBtn");
            btn->setCheckable(true);
            if (i == 0) btn->setChecked(true);
            navGroup->addButton(btn, i);
            railLay->addWidget(btn);
        }

        railLay->addStretch();

        auto *hotkeyHint = new QLabel("Хоткей:\nНастройки OBS →\nГорячие клавиши\n\nДок-панель:\nДок-панели →\nUBIQUE CAM", rail);
        hotkeyHint->setStyleSheet("color: #6B6A66; font-size: 10px; padding: 0 14px;");
        railLay->addWidget(hotkeyHint);

        outer->addWidget(rail);

        // 2. Right Content Area
        auto *rightCol = new QWidget(this);
        auto *rightLay = new QVBoxLayout(rightCol);
        rightLay->setContentsMargins(18, 14, 18, 14);
        rightLay->setSpacing(10);

        // Top Mode Switcher Bar
        auto *modeBar = new QHBoxLayout();
        modeBar->setSpacing(8);
        m_btnWebcam = new QPushButton("●  КАМЕРА", rightCol);
        m_btnPng = new QPushButton("○  PNGTUBER", rightCol);
        m_btnWebcam->setProperty("role", "modePill");
        m_btnPng->setProperty("role", "modePill");
        m_btnWebcam->setCheckable(true);
        m_btnPng->setCheckable(true);
        modeBar->addWidget(m_btnWebcam);
        modeBar->addWidget(m_btnPng);
        rightLay->addLayout(modeBar);

        // Stacked Scrollable Pages
        auto *pages = new QStackedWidget(rightCol);
        pages->addWidget(wrapScroll(buildWebcamPage()));
        pages->addWidget(wrapScroll(buildBorderPage()));
        pages->addWidget(wrapScroll(buildSpectrumPage()));
        pages->addWidget(wrapScroll(buildPngPage()));
        pages->addWidget(wrapScroll(buildBgAndSlotsPage()));
        rightLay->addWidget(pages, 1);

        // Bottom Bar
        auto *bottom = new QHBoxLayout();
        auto *tip = new QLabel("ПКМ по слоту PNG — очистить  •  Все изменения применяются в реальном времени", rightCol);
        tip->setProperty("role", "muted");
        auto *btnClose = new QPushButton("Готово", rightCol);
        connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
        bottom->addWidget(tip);
        bottom->addStretch();
        bottom->addWidget(btnClose);
        rightLay->addLayout(bottom);

        outer->addWidget(rightCol, 1);

        connect(navGroup, &QButtonGroup::idClicked, pages, &QStackedWidget::setCurrentIndex);
        connect(m_btnWebcam, &QPushButton::clicked, this, [this]() { setMode(MODE_WEBCAM); });
        connect(m_btnPng, &QPushButton::clicked, this, [this]() { setMode(MODE_PNGTUBER); });

        loadFromSettings();

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            if (!m_filter) return;
            float db = ubique_get_current_db(m_filter);
            bool talk = ubique_is_talking(m_filter);
            obs_source_t *src = ubique_get_source(m_filter);
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
    struct ubique_cam_data *m_filter;
    QTimer *m_timer = nullptr;

    QPushButton *m_btnWebcam = nullptr;
    QPushButton *m_btnPng = nullptr;
    LithicVuMeter *m_vuMeter = nullptr;

    std::vector<std::function<void(obs_data_t *)>> m_loaders;

    QScrollArea *wrapScroll(QWidget *content)
    {
        auto *sa = new QScrollArea(this);
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setWidget(content);
        return sa;
    }

    void applyDouble(const char *key, double val)
    {
        if (!m_filter) return;
        obs_source_t *src = ubique_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_double(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyInt(const char *key, long long val)
    {
        if (!m_filter) return;
        obs_source_t *src = ubique_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_int(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyBool(const char *key, bool val)
    {
        if (!m_filter) return;
        obs_source_t *src = ubique_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        obs_data_set_bool(s, key, val);
        obs_source_update(src, s);
        obs_data_release(s);
    }

    void applyString(const char *key, const QString &val)
    {
        if (!m_filter) return;
        obs_source_t *src = ubique_get_source(m_filter);
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
        m_btnWebcam->setText(isWeb ? "●  КАМЕРА" : "○  КАМЕРА");
        m_btnPng->setText(!isWeb ? "●  PNGTUBER" : "○  PNGTUBER");
    }

    QFrame *makeCard(QWidget *parent, QVBoxLayout **outLay)
    {
        auto *card = new QFrame(parent);
        card->setProperty("role", "card");
        auto *lay = new QVBoxLayout(card);
        lay->setContentsMargins(14, 12, 14, 12);
        lay->setSpacing(8);
        *outLay = lay;
        return card;
    }

    QLabel *makeSectionLabel(const QString &text, QWidget *parent)
    {
        auto *l = new QLabel(text, parent);
        l->setProperty("role", "section");
        return l;
    }

    QWidget *addSliderRow(QGridLayout *grid, int row, const QString &label,
                          const char *key, double minV, double maxV, double scale,
                          const QString &suffix)
    {
        auto *lbl = new QLabel(label, this);
        auto *sl = new QSlider(Qt::Horizontal, this);
        sl->setRange((int)std::round(minV * scale), (int)std::round(maxV * scale));
        auto *valLbl = new QLabel(this);
        valLbl->setProperty("role", "mono");
        valLbl->setFixedWidth(68);
        valLbl->setAlignment(Qt::AlignCenter);

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

    QCheckBox *addCheckRow(QGridLayout *grid, int row, const QString &label, const char *key)
    {
        auto *chk = new QCheckBox(label, this);
        grid->addWidget(chk, row, 0, 1, 3);
        connect(chk, &QCheckBox::toggled, this, [this, key](bool v) { applyBool(key, v); });
        m_loaders.push_back([chk, key](obs_data_t *s) {
            chk->blockSignals(true);
            chk->setChecked(obs_data_get_bool(s, key));
            chk->blockSignals(false);
        });
        return chk;
    }

    QPushButton *addColorRow(QGridLayout *grid, int row, const QString &label, const char *key)
    {
        grid->addWidget(new QLabel(label, this), row, 0);
        auto *btn = new QPushButton("Выбрать цвет", this);
        grid->addWidget(btn, row, 1, 1, 2);

        auto updateBtnStyle = [btn](const QColor &c) {
            btn->setText(QString("#%1%2%3  •  A:%4")
                             .arg(c.red(), 2, 16, QChar('0'))
                             .arg(c.green(), 2, 16, QChar('0'))
                             .arg(c.blue(), 2, 16, QChar('0'))
                             .arg(c.alpha())
                             .toUpper());
            btn->setStyleSheet(
                QString("background-color: rgba(%1,%2,%3,255); color: %4; border: 1px solid #3A3C42; "
                        "border-radius: 4px; font-family: 'Cascadia Mono', Consolas, monospace; font-size: 11px; padding: 5px;")
                    .arg(c.red()).arg(c.green()).arg(c.blue())
                    .arg((c.lightness() < 128) ? "#ECE9E2" : "#111214"));
        };

        connect(btn, &QPushButton::clicked, this, [this, key, updateBtnStyle]() {
            if (!m_filter) return;
            obs_source_t *src = ubique_get_source(m_filter);
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

    QComboBox *addComboRow(QGridLayout *grid, int row, const QString &label, const char *key,
                           const std::vector<std::pair<QString, int>> &items)
    {
        grid->addWidget(new QLabel(label, this), row, 0);
        auto *combo = new QComboBox(this);
        for (const auto &it : items) combo->addItem(it.first, it.second);
        styleLithicCombo(combo);
        grid->addWidget(combo, row, 1, 1, 2);

        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, key](int idx) {
            applyInt(key, combo->itemData(idx).toInt());
        });
        m_loaders.push_back([combo, key](obs_data_t *s) {
            int val = (int)obs_data_get_int(s, key);
            int idx = combo->findData(val);
            combo->blockSignals(true);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            combo->blockSignals(false);
        });
        return combo;
    }

    // PAGE 1: WEBCAM, SHAPE PRESETS, OPTICS & TRANSITIONS
    QWidget *buildWebcamPage()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(0, 0, 6, 0);
        lay->setSpacing(10);

        // Expanded Shape Presets Card (2 rows x 5)
        QVBoxLayout *pLay = nullptr;
        auto *pCard = makeCard(w, &pLay);
        pLay->addWidget(makeSectionLabel("ПРЕСЕТЫ ФОРМЫ", pCard));
        auto *pGrid = new QGridLayout();
        pGrid->setSpacing(6);

        struct ShapePreset { QString name; double cl, cr, ct, cb, rad, zoom; };
        const std::vector<ShapePreset> presets = {
            {"КРУГ",        21.8, 21.8, 0.0,  0.0,  500.0, 1.0},
            {"КВАДРАТ 1:1", 21.8, 21.8, 0.0,  0.0,  40.0,  1.0},
            {"КЛАССИКА 4:3",12.5, 12.5, 0.0,  0.0,  28.0,  1.0},
            {"ПОРТРЕТ 3:4", 29.0, 29.0, 0.0,  0.0,  32.0,  1.0},
            {"КИНО 16:9",   0.0,  0.0,  0.0,  0.0,  32.0,  1.0},
            {"ШОРТС 9:16",  34.0, 34.0, 0.0,  0.0,  28.0,  1.0},
            {"УЛЬТРА 21:9", 0.0,  0.0,  14.0, 14.0, 24.0,  1.0},
            {"КАПСУЛА",     12.5, 12.5, 12.0, 12.0, 500.0, 1.0},
            {"КРУПНЫЙ ПЛАН",21.8, 21.8, 0.0,  0.0,  500.0, 1.38},
            {"СБРОС",       0.0,  0.0,  0.0,  0.0,  32.0,  1.0}
        };

        for (int i = 0; i < (int)presets.size(); i++) {
            const auto &pr = presets[i];
            auto *btn = new QPushButton(pr.name, pCard);
            btn->setProperty("role", "preset");
            pGrid->addWidget(btn, i / 5, i % 5);
            connect(btn, &QPushButton::clicked, this, [this, pr]() {
                applyDouble("crop_left", pr.cl);
                applyDouble("crop_right", pr.cr);
                applyDouble("crop_top", pr.ct);
                applyDouble("crop_bottom", pr.cb);
                applyDouble("corner_radius", pr.rad);
                applyDouble("zoom", pr.zoom);
                applyDouble("pan_x", 0.0);
                applyDouble("pan_y", 0.0);
                loadFromSettings();
            });
        }
        pLay->addLayout(pGrid);
        lay->addWidget(pCard);

        // Geometry Card
        QVBoxLayout *gLay = nullptr;
        auto *gCard = makeCard(w, &gLay);
        gLay->addWidget(makeSectionLabel("ГЕОМЕТРИЯ И КРОП", gCard));
        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(7);
        int r = 0;
        addSliderRow(grid, r++, "Скругление углов", "corner_radius", 0, 500, 1, " px");
        addSliderRow(grid, r++, "Зум", "zoom", 1.0, 4.0, 100, "x");
        addSliderRow(grid, r++, "Сдвиг по X", "pan_x", -1.0, 1.0, 100, "");
        addSliderRow(grid, r++, "Сдвиг по Y", "pan_y", -1.0, 1.0, 100, "");
        addSliderRow(grid, r++, "Кроп слева", "crop_left", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп справа", "crop_right", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп сверху", "crop_top", 0, 45, 2, "%");
        addSliderRow(grid, r++, "Кроп снизу", "crop_bottom", 0, 45, 2, "%");
        gLay->addLayout(grid);
        lay->addWidget(gCard);

        // Optics & Transition Card
        QVBoxLayout *oLay = nullptr;
        auto *oCard = makeCard(w, &oLay);
        oLay->addWidget(makeSectionLabel("ОПТИКА И ПЕРЕХОД РЕЖИМОВ", oCard));
        auto *oGrid = new QGridLayout();
        oGrid->setVerticalSpacing(7);
        int orow = 0;
        addCheckRow(oGrid, orow++, "Отзеркалить камеру по горизонтали (Flip H)", "flip_h");
        addSliderRow(oGrid, orow++, "Виньетка", "vignette", 0.0, 1.0, 100, "");
        addSliderRow(oGrid, orow++, "Контраст", "contrast", 0.5, 1.6, 100, "");
        addSliderRow(oGrid, orow++, "Теплота цвета", "warmth", -1.0, 1.0, 100, "");
        addComboRow(oGrid, orow++, "Эффект перехода", "transition_type", {
            {"Плавный (Crossfade)", TRANS_CROSSFADE},
            {"Размытие (Blur Morph)", TRANS_BLUR},
            {"Глитч (Glitch / RGB Split)", TRANS_GLITCH},
            {"CRT-схлопывание", TRANS_CRT}
        });
        addSliderRow(oGrid, orow++, "Скорость перехода", "transition_speed", 0.0, 2.0, 20, " с");
        oLay->addLayout(oGrid);
        lay->addWidget(oCard);

        lay->addStretch();
        return w;
    }

    // PAGE 2: BORDER, GRADIENT, AUDIO-REACTIVE GLOW & SHADOW
    QWidget *buildBorderPage()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(0, 0, 6, 0);
        lay->setSpacing(10);

        // Visual Style Presets
        QVBoxLayout *stLay = nullptr;
        auto *stCard = makeCard(w, &stLay);
        stLay->addWidget(makeSectionLabel("ПРЕСЕТЫ СТИЛЯ", stCard));
        auto *stRow = new QHBoxLayout();
        stRow->setSpacing(6);

        auto addStyleBtn = [&](const QString &name, std::function<void()> fn) {
            auto *b = new QPushButton(name, stCard);
            b->setProperty("role", "preset");
            stRow->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, fn]() { fn(); loadFromSettings(); });
        };

        addStyleBtn("LITHIC", [this]() {
            applyDouble("border_width", 2.5); applyInt("border_style", BORDER_SOLID);
            applyInt("border_color", 0xFFE2E9EC);
            applyDouble("glow_size", 20.0); applyDouble("glow_intensity", 0.7);
            applyInt("glow_color", 0xFF649EFF);
            applyBool("shadow_enable", true); applyDouble("shadow_blur", 26.0);
        });
        addStyleBtn("CLEAN", [this]() {
            applyDouble("border_width", 0.0); applyDouble("glow_size", 0.0);
            applyBool("shadow_enable", true); applyDouble("shadow_blur", 30.0);
            applyDouble("corner_radius", 24.0);
        });
        addStyleBtn("НЕОН", [this]() {
            applyDouble("border_width", 4.0); applyInt("border_style", BORDER_ANIM_GRADIENT);
            applyInt("border_color", 0xFFFF77FF); applyInt("border_color2", 0xFFFFFF00);
            applyDouble("glow_size", 42.0); applyDouble("glow_intensity", 1.3);
            applyInt("glow_color", 0xFFFF66AA);
        });
        addStyleBtn("ТАКТИК", [this]() {
            applyDouble("corner_radius", 6.0); applyDouble("border_width", 1.5);
            applyInt("border_style", BORDER_SOLID); applyInt("border_color", 0xFF9EA2A8);
            applyDouble("glow_size", 0.0); applyBool("shadow_enable", true);
            applyDouble("shadow_blur", 10.0);
        });
        addStyleBtn("СТИКЕР", [this]() {
            applyDouble("border_width", 0.0); applyDouble("glow_size", 0.0);
            applyBool("shadow_enable", false); applyInt("bg_mode", BG_TRANSPARENT);
        });

        stLay->addLayout(stRow);
        lay->addWidget(stCard);

        // Border & Gradient Card
        QVBoxLayout *bLay = nullptr;
        auto *bCard = makeCard(w, &bLay);
        bLay->addWidget(makeSectionLabel("РАМКА И ГРАДИЕНТ", bCard));
        auto *bGrid = new QGridLayout();
        bGrid->setVerticalSpacing(7);
        int r = 0;
        addSliderRow(bGrid, r++, "Толщина рамки", "border_width", 0, 40, 2, " px");
        addComboRow(bGrid, r++, "Тип рамки", "border_style", {
            {"Сплошной цвет", BORDER_SOLID},
            {"Градиент (2 цвета)", BORDER_GRADIENT},
            {"Анимированный градиент", BORDER_ANIM_GRADIENT}
        });
        addColorRow(bGrid, r++, "Основной цвет рамки", "border_color");
        addColorRow(bGrid, r++, "Второй цвет градиента", "border_color2");
        addSliderRow(bGrid, r++, "Скорость вращения", "border_grad_speed", 0.1, 6.0, 10, "");
        bLay->addLayout(bGrid);
        lay->addWidget(bCard);

        // Glow & Audio Reactivity Card
        QVBoxLayout *gLay = nullptr;
        auto *gCard = makeCard(w, &gLay);
        gLay->addWidget(makeSectionLabel("СВЕЧЕНИЕ И АУДИО-РЕАКЦИЯ", gCard));
        auto *gGrid = new QGridLayout();
        gGrid->setVerticalSpacing(7);
        int gr = 0;
        addSliderRow(gGrid, gr++, "Радиус свечения", "glow_size", 0, 100, 1, " px");
        addSliderRow(gGrid, gr++, "Яркость свечения", "glow_intensity", 0.0, 2.0, 20, "");
        addColorRow(gGrid, gr++, "Цвет свечения", "glow_color");
        addCheckRow(gGrid, gr++, "Аудио-реактивное свечение (реакция на микрофон)", "border_audio_react");
        addSliderRow(gGrid, gr++, "Сила реакции", "border_react_strength", 0.1, 2.5, 20, "x");
        addSliderRow(gGrid, gr++, "Порог пика (крик)", "peak_threshold_db", -40, 0, 1, " дБ");
        addColorRow(gGrid, gr++, "Цвет вспышки на пиках", "peak_color");
        gLay->addLayout(gGrid);
        lay->addWidget(gCard);

        // Shadow Card
        QVBoxLayout *sLay = nullptr;
        auto *sCard = makeCard(w, &sLay);
        sLay->addWidget(makeSectionLabel("ТЕНЬ (DROP SHADOW)", sCard));
        auto *sGrid = new QGridLayout();
        sGrid->setVerticalSpacing(7);
        int sr = 0;
        addCheckRow(sGrid, sr++, "Включить тень", "shadow_enable");
        addSliderRow(sGrid, sr++, "Мягкость тени", "shadow_blur", 0, 100, 1, " px");
        addSliderRow(sGrid, sr++, "Сдвиг X", "shadow_offset_x", -100, 100, 1, " px");
        addSliderRow(sGrid, sr++, "Сдвиг Y", "shadow_offset_y", -100, 100, 1, " px");
        addColorRow(sGrid, sr++, "Цвет тени", "shadow_color");
        sLay->addLayout(sGrid);
        lay->addWidget(sCard);

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

    // PAGE 3: AUDIO SPECTRUM
    QWidget *buildSpectrumPage()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(0, 0, 6, 0);
        lay->setSpacing(10);

        QVBoxLayout *cLay = nullptr;
        auto *card = makeCard(w, &cLay);
        cLay->addWidget(makeSectionLabel("AUDIO SPECTRUM (ВИЗУАЛИЗАТОР)", card));

        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(8);
        int r = 0;

        grid->addWidget(new QLabel("Источник звука", card), r, 0);
        auto *spCombo = new QComboBox(card);
        spCombo->addItem("(Как у микрофона PNGTuber)", "");
        obs_enum_sources(enumAudioCb, spCombo);
        styleLithicCombo(spCombo);
        grid->addWidget(spCombo, r++, 1, 1, 2);

        connect(spCombo, &QComboBox::currentIndexChanged, this, [this, spCombo](int idx) {
            applyString("spec_source", spCombo->itemData(idx).toString());
        });
        m_loaders.push_back([spCombo](obs_data_t *s) {
            QString cur = QString::fromUtf8(obs_data_get_string(s, "spec_source"));
            int idx = spCombo->findData(cur);
            spCombo->blockSignals(true);
            spCombo->setCurrentIndex(idx >= 0 ? idx : 0);
            spCombo->blockSignals(false);
        });

        addComboRow(grid, r++, "Расположение в слое", "spec_layer", {
            {"Выключен", SPEC_LAYER_OFF},
            {"За моделькой (PNGTuber)", SPEC_LAYER_BEHIND_AVATAR},
            {"Поверх изображения", SPEC_LAYER_OVERLAY},
            {"По периметру рамки", SPEC_LAYER_BORDER}
        });

        addComboRow(grid, r++, "Дизайн волны", "spec_design", {
            {"Классические столбики (Bars)", SPEC_DESIGN_BARS},
            {"Плавная неоновая волна (Wave)", SPEC_DESIGN_WAVE},
            {"Зеркальная от центра (Mirrored)", SPEC_DESIGN_MIRROR},
            {"LED-сегменты (Lithic)", SPEC_DESIGN_LED}
        });

        addColorRow(grid, r++, "Цвет волны", "spec_color");
        addSliderRow(grid, r++, "Чувствительность", "spec_sensitivity", 0.2, 4.0, 20, "x");
        addSliderRow(grid, r++, "Высота / амплитуда", "spec_height", 0.05, 1.0, 100, "");
        addSliderRow(grid, r++, "Непрозрачность", "spec_opacity", 0.0, 1.0, 100, "");

        cLay->addLayout(grid);

        auto *info = new QLabel(
            "Совет: можно выбрать отдельный источник звука (например, Chrome Audio / Spotify), "
            "чтобы волна реагировала на музыку, пока персонаж реагирует на микрофон.", card);
        info->setProperty("role", "muted");
        info->setWordWrap(true);
        cLay->addWidget(info);

        lay->addWidget(card);
        lay->addStretch();
        return w;
    }

    // PAGE 4: PNGTUBER
    QWidget *buildPngPage()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(0, 0, 6, 0);
        lay->setSpacing(10);

        auto *tilesRow = new QHBoxLayout();
        tilesRow->setSpacing(8);
        auto *sIdle = new AvatarDropSlot("01 МОЛЧИТ", [this](const QString &p) { applyString("img_idle", p); }, w);
        auto *sTalk = new AvatarDropSlot("02 ГОВОРИТ", [this](const QString &p) { applyString("img_talk", p); }, w);
        auto *sBlink = new AvatarDropSlot("03 МОРГАЕТ", [this](const QString &p) { applyString("img_blink", p); }, w);
        auto *sTB = new AvatarDropSlot("04 ГОВ+МОРГ", [this](const QString &p) { applyString("img_talk_blink", p); }, w);
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

        // Mic Card
        QVBoxLayout *mLay = nullptr;
        auto *mCard = makeCard(w, &mLay);
        mLay->addWidget(makeSectionLabel("МИКРОФОН И СРАБАТЫВАНИЕ", mCard));
        auto *mGrid = new QGridLayout();
        mGrid->setVerticalSpacing(7);
        mGrid->addWidget(new QLabel("Источник", mCard), 0, 0);
        auto *micCombo = new QComboBox(mCard);
        micCombo->addItem("(Не выбран)", "");
        obs_enum_sources(enumAudioCb, micCombo);
        styleLithicCombo(micCombo);
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

        m_vuMeter = new LithicVuMeter(mCard);
        mGrid->addWidget(m_vuMeter, 1, 0, 1, 3);

        addSliderRow(mGrid, 2, "Порог голоса", "audio_threshold", -80, 0, 2, " дБ");
        addSliderRow(mGrid, 3, "Удержание рта", "talk_hold_ms", 0, 600, 1, " мс");
        addSliderRow(mGrid, 4, "Интервал моргания", "blink_interval", 0.5, 10.0, 10, " с");
        addSliderRow(mGrid, 5, "Длит. моргания", "blink_duration", 40, 400, 1, " мс");
        mLay->addLayout(mGrid);
        lay->addWidget(mCard);

        // Position, Animations, Scream Shake & Idle Dim Card
        QVBoxLayout *aLay = nullptr;
        auto *aCard = makeCard(w, &aLay);
        aLay->addWidget(makeSectionLabel("ПОЗИЦИЯ, АНИМАЦИИ И ЭФФЕКТЫ", aCard));
        auto *aGrid = new QGridLayout();
        aGrid->setVerticalSpacing(7);
        int ar = 0;
        addSliderRow(aGrid, ar++, "Масштаб модельки", "avatar_scale", 0.2, 2.5, 100, "x");
        addSliderRow(aGrid, ar++, "Сдвиг модельки X", "avatar_offset_x", -0.5, 0.5, 100, "");
        addSliderRow(aGrid, ar++, "Сдвиг модельки Y", "avatar_offset_y", -0.5, 0.5, 100, "");
        addSliderRow(aGrid, ar++, "Дыхание", "anim_breath", 0.0, 0.12, 200, "");
        addSliderRow(aGrid, ar++, "Подпрыгивание", "anim_bounce", 0.0, 0.25, 200, "");
        addSliderRow(aGrid, ar++, "Покачивание", "anim_sway", 0.0, 10.0, 10, "°");
        addSliderRow(aGrid, ar++, "Порог тряски (крик)", "scream_threshold_db", -30, 0, 1, " дБ");
        addSliderRow(aGrid, ar++, "Сила тряски", "scream_shake_amp", 0.0, 0.08, 250, "");
        addSliderRow(aGrid, ar++, "Затемнение при молчании", "idle_dim_amp", 0.0, 0.8, 100, "");
        aLay->addLayout(aGrid);
        lay->addWidget(aCard);

        lay->addStretch();
        return w;
    }

    // PAGE 5: BACKGROUND & 3 CUSTOM USER PRESET SLOTS
    QWidget *buildBgAndSlotsPage()
    {
        auto *w = new QWidget(this);
        auto *lay = new QVBoxLayout(w);
        lay->setContentsMargins(0, 0, 6, 0);
        lay->setSpacing(10);

        QVBoxLayout *cLay = nullptr;
        auto *card = makeCard(w, &cLay);
        cLay->addWidget(makeSectionLabel("ФОН ДЛЯ PNGTUBER", card));

        auto *topRow = new QHBoxLayout();
        topRow->addWidget(new QLabel("Тип фона", card));
        auto *bgCombo = new QComboBox(card);
        bgCombo->addItem("Сплошной цвет", BG_COLOR);
        bgCombo->addItem("Картинка", BG_IMAGE);
        bgCombo->addItem("Размытая вебкамера", BG_BLUR_CAM);
        bgCombo->addItem("Прозрачный фон", BG_TRANSPARENT);
        styleLithicCombo(bgCombo);
        topRow->addWidget(bgCombo, 1);
        cLay->addLayout(topRow);

        auto *stack = new QStackedWidget(card);

        auto *pColor = new QWidget(stack);
        auto *cGrid = new QGridLayout(pColor);
        cGrid->setContentsMargins(0, 8, 0, 0);
        addColorRow(cGrid, 0, "Цвет фона", "bg_color");
        cGrid->setRowStretch(1, 1);
        stack->addWidget(pColor);

        auto *pImg = new QWidget(stack);
        auto *iLay = new QVBoxLayout(pImg);
        iLay->setContentsMargins(0, 8, 0, 0);
        auto *btnPickBg = new QPushButton("Выбрать файл фона (PNG / JPG)...", pImg);
        auto *lblBgPath = new QLabel("(Файл не выбран)", pImg);
        lblBgPath->setProperty("role", "muted");
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

        auto *pBlur = new QWidget(stack);
        auto *bGrid = new QGridLayout(pBlur);
        bGrid->setContentsMargins(0, 8, 0, 0);
        addSliderRow(bGrid, 0, "Сила размытия", "bg_blur", 1, 50, 1, " px");
        bGrid->setRowStretch(1, 1);
        stack->addWidget(pBlur);

        auto *pTrans = new QWidget(stack);
        auto *tLay = new QVBoxLayout(pTrans);
        tLay->setContentsMargins(0, 8, 0, 0);
        auto *tLbl = new QLabel("Фон полностью прозрачный — поверх сцены видна только моделька.", pTrans);
        tLbl->setProperty("role", "muted");
        tLbl->setWordWrap(true);
        tLay->addWidget(tLbl);
        tLay->addStretch();
        stack->addWidget(pTrans);

        cLay->addWidget(stack);
        lay->addWidget(card);

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

        // Custom User Preset Slots (3 slots persisted in obs_data_t)
        QVBoxLayout *slLay = nullptr;
        auto *slCard = makeCard(w, &slLay);
        slLay->addWidget(makeSectionLabel("ПОЛЬЗОВАТЕЛЬСКИЕ СЛОТЫ (ПРОФИЛИ)", slCard));

        auto *slGrid = new QGridLayout();
        slGrid->setSpacing(8);
        for (int i = 1; i <= 3; i++) {
            QString slotKey = QString("user_slot_%1").arg(i);
            auto *btnLoad = new QPushButton(QString("ЗАГРУЗИТЬ СЛОТ %1").arg(i), slCard);
            auto *btnSave = new QPushButton("СОХРАНИТЬ", slCard);
            btnLoad->setProperty("role", "preset");
            btnSave->setProperty("role", "preset");
            slGrid->addWidget(btnLoad, i - 1, 0, 1, 2);
            slGrid->addWidget(btnSave, i - 1, 2);

            connect(btnSave, &QPushButton::clicked, this, [this, slotKey, btnSave]() {
                if (!m_filter) return;
                obs_source_t *src = ubique_get_source(m_filter);
                if (!src) return;
                obs_data_t *s = obs_source_get_settings(src);
                QJsonObject obj;
                for (const char *k : PRESET_DOUBLE_KEYS) obj[k] = obs_data_get_double(s, k);
                for (const char *k : PRESET_INT_KEYS) obj[k] = (double)obs_data_get_int(s, k);
                for (const char *k : PRESET_BOOL_KEYS) obj[k] = obs_data_get_bool(s, k);
                QString json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
                obs_data_set_string(s, slotKey.toUtf8().constData(), json.toUtf8().constData());
                obs_data_release(s);
                btnSave->setText("СОХРАНЕНО ✓");
                QTimer::singleShot(1200, btnSave, [btnSave]() { btnSave->setText("СОХРАНИТЬ"); });
            });

            connect(btnLoad, &QPushButton::clicked, this, [this, slotKey]() {
                if (!m_filter) return;
                obs_source_t *src = ubique_get_source(m_filter);
                if (!src) return;
                obs_data_t *s = obs_source_get_settings(src);
                QString json = QString::fromUtf8(obs_data_get_string(s, slotKey.toUtf8().constData()));
                if (!json.isEmpty()) {
                    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();
                        for (const char *k : PRESET_DOUBLE_KEYS) {
                            if (obj.contains(k)) obs_data_set_double(s, k, obj[k].toDouble());
                        }
                        for (const char *k : PRESET_INT_KEYS) {
                            if (obj.contains(k)) obs_data_set_int(s, k, (long long)obj[k].toDouble());
                        }
                        for (const char *k : PRESET_BOOL_KEYS) {
                            if (obj.contains(k)) obs_data_set_bool(s, k, obj[k].toBool());
                        }
                        obs_source_update(src, s);
                    }
                }
                obs_data_release(s);
                loadFromSettings();
            });
        }
        slLay->addLayout(slGrid);
        lay->addWidget(slCard);

        lay->addStretch();
        return w;
    }

    void loadFromSettings()
    {
        if (!m_filter) return;
        obs_source_t *src = ubique_get_source(m_filter);
        if (!src) return;
        obs_data_t *s = obs_source_get_settings(src);
        updateModeButtons((int)obs_data_get_int(s, "mode"));
        for (auto &fn : m_loaders) fn(s);
        obs_data_release(s);
    }
};

// Global filter registry & Dock Panel
static std::vector<struct ubique_cam_data *> g_filters;
static std::map<struct ubique_cam_data *, UbiqueCamDialog *> g_dialogs;

class UbiqueDockWidget : public QWidget {
public:
    explicit UbiqueDockWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("UbiqueDockRoot");
        setStyleSheet(RYOKU_LITHIC_QSS);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->setSpacing(8);

        auto *hdr = new QLabel("UBIQUE CAM", this);
        hdr->setStyleSheet(
            "font-family: 'Cascadia Mono', Consolas, monospace; font-size: 11px; "
            "font-weight: bold; letter-spacing: 1.5px; color: #FF9E64;");
        lay->addWidget(hdr);

        auto *modeRow = new QHBoxLayout();
        modeRow->setSpacing(6);
        m_btnCam = new QPushButton("● КАМЕРА", this);
        m_btnPng = new QPushButton("○ PNGTUBER", this);
        m_btnCam->setProperty("role", "modePill");
        m_btnPng->setProperty("role", "modePill");
        m_btnCam->setCheckable(true);
        m_btnPng->setCheckable(true);
        modeRow->addWidget(m_btnCam);
        modeRow->addWidget(m_btnPng);
        lay->addLayout(modeRow);

        m_vu = new LithicVuMeter(this);
        lay->addWidget(m_vu);

        auto *btnOpen = new QPushButton("Настройка UBIQUE CAM", this);
        lay->addWidget(btnOpen);

        m_status = new QLabel("Нет активного фильтра", this);
        m_status->setProperty("role", "muted");
        lay->addWidget(m_status);
        lay->addStretch();

        connect(m_btnCam, &QPushButton::clicked, this, [this]() { applyModeAll(MODE_WEBCAM); });
        connect(m_btnPng, &QPushButton::clicked, this, [this]() { applyModeAll(MODE_PNGTUBER); });
        connect(btnOpen, &QPushButton::clicked, this, [this]() {
            if (!g_filters.empty()) ubique_ui_open(g_filters.front());
        });

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            if (g_filters.empty()) {
                m_status->setText("Добавь фильтр UBIQUE CAM на вебку");
                m_vu->setValues(-100.0f, -38.0f, false);
                return;
            }
            auto *f = g_filters.front();
            m_status->setText(QString("Активных фильтров: %1").arg(g_filters.size()));
            obs_source_t *src = ubique_get_source(f);
            float thresh = -38.0f;
            int mode = MODE_WEBCAM;
            if (src) {
                obs_data_t *s = obs_source_get_settings(src);
                thresh = (float)obs_data_get_double(s, "audio_threshold");
                mode = (int)obs_data_get_int(s, "mode");
                obs_data_release(s);
            }
            m_vu->setValues(ubique_get_current_db(f), thresh, ubique_is_talking(f));
            bool isCam = (mode == MODE_WEBCAM);
            m_btnCam->setChecked(isCam);
            m_btnPng->setChecked(!isCam);
            m_btnCam->setText(isCam ? "● КАМЕРА" : "○ КАМЕРА");
            m_btnPng->setText(!isCam ? "● PNGTUBER" : "○ PNGTUBER");
        });
        timer->start(40);
    }

private:
    QPushButton *m_btnCam;
    QPushButton *m_btnPng;
    LithicVuMeter *m_vu;
    QLabel *m_status;

    void applyModeAll(int mode)
    {
        for (auto *f : g_filters) {
            obs_source_t *src = ubique_get_source(f);
            if (!src) continue;
            obs_data_t *s = obs_source_get_settings(src);
            obs_data_set_int(s, "mode", mode);
            obs_source_update(src, s);
            obs_data_release(s);
        }
    }
};

static bool g_dock_registered = false;

extern "C" void ubique_ui_register_filter(struct ubique_cam_data *filter)
{
    if (!filter) return;
    if (std::find(g_filters.begin(), g_filters.end(), filter) == g_filters.end()) {
        g_filters.push_back(filter);
    }
}

extern "C" void ubique_ui_unregister_filter(struct ubique_cam_data *filter)
{
    if (!filter) return;
    g_filters.erase(std::remove(g_filters.begin(), g_filters.end(), filter), g_filters.end());

    auto it = g_dialogs.find(filter);
    if (it != g_dialogs.end() && it->second) {
        it->second->detachFilter();
        g_dialogs.erase(it);
    }
}

extern "C" void ubique_ui_open(struct ubique_cam_data *filter)
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
    auto *dlg = new UbiqueCamDialog(filter, mainWin);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(dlg, &QObject::destroyed, [filter]() {
        g_dialogs.erase(filter);
    });
    g_dialogs[filter] = dlg;
    dlg->show();
}

extern "C" void ubique_ui_init_dock(void)
{
    if (g_dock_registered) return;
    auto *dock = new UbiqueDockWidget();
    if (obs_frontend_add_dock_by_id("ubique_cam_dock", "UBIQUE CAM", dock)) {
        g_dock_registered = true;
    } else {
        delete dock;
    }
}

extern "C" void ubique_ui_destroy_dock(void)
{
    if (g_dock_registered) {
        obs_frontend_remove_dock("ubique_cam_dock");
        g_dock_registered = false;
    }
}
