#include "StickPage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QSlider *makeSlider(int minimum, int maximum, QWidget *parent)
{
    auto *slider = new QSlider(Qt::Horizontal, parent);
    slider->setRange(minimum, maximum);
    return slider;
}

}

StickPage::StickPage(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Joystick (left to the system and games)"), static_cast<int>(StickSettings::Mode::Passthrough));
    m_mode->addItem(tr("Keys (four directions)"), static_cast<int>(StickSettings::Mode::Keys));
    m_mode->addItem(tr("Mouse pointer"), static_cast<int>(StickSettings::Mode::Mouse));
    m_mode->addItem(tr("Scroll wheel"), static_cast<int>(StickSettings::Mode::Scroll));
    form->addRow(tr("Stick:"), m_mode);
    m_modeHint = new QLabel(this);
    m_modeHint->setWordWrap(true);
    form->addRow(m_modeHint);

    const auto row = [this](QSlider *slider, QLabel *&value) {
        auto *widget = new QWidget(this);
        auto *box = new QHBoxLayout(widget);
        box->setContentsMargins(0, 0, 0, 0);
        value = new QLabel(widget);
        value->setMinimumWidth(48);
        box->addWidget(slider, 1);
        box->addWidget(value);
        return widget;
    };
    m_deadzone = makeSlider(5, 90, this);
    form->addRow(tr("Dead zone:"), row(m_deadzone, m_deadzoneValue));
    m_speed = makeSlider(1, StickSettings::MaxSpeed, this);
    form->addRow(tr("Pointer speed:"), row(m_speed, m_speedValue));
    m_scrollSpeed = makeSlider(1, StickSettings::MaxSpeed, this);
    form->addRow(tr("Scroll speed:"), row(m_scrollSpeed, m_scrollValue));
    layout->addLayout(form);

    m_wasd = new QPushButton(tr("Bind W, A, S, D to the stick directions"), this);
    m_wasd->setToolTip(tr("Sets the four stick directions of the current layer to W, A, S and D."));
    layout->addWidget(m_wasd);

    auto *calibration = new QGroupBox(tr("Rest position"), this);
    auto *calibrationLayout = new QVBoxLayout(calibration);
    auto *calibrationHint = new QLabel(
        tr("The dead zone is centred on the position the stick rests in. Let go of the stick and click "
           "the button to measure it."),
        calibration);
    calibrationHint->setWordWrap(true);
    m_rest = new QLabel(calibration);
    m_calibrate = new QPushButton(tr("Measure rest position"), calibration);
    calibrationLayout->addWidget(calibrationHint);
    calibrationLayout->addWidget(m_rest);
    calibrationLayout->addWidget(m_calibrate, 0, Qt::AlignLeft);
    layout->addWidget(calibration);
    layout->addStretch();

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);   // sliders: save once they come to rest
    connect(m_debounce, &QTimer::timeout, this, [this] { emit settingsChanged(current()); });

    const auto changed = [this] {
        m_deadzoneValue->setText(QStringLiteral("%1 %").arg(m_deadzone->value()));
        m_speedValue->setText(QString::number(m_speed->value()));
        m_scrollValue->setText(QString::number(m_scrollSpeed->value()));
        updateEnabled();
        if (!m_updating)
            m_debounce->start();
    };
    connect(m_mode, &QComboBox::currentIndexChanged, this, changed);
    connect(m_deadzone, &QSlider::valueChanged, this, changed);
    connect(m_speed, &QSlider::valueChanged, this, changed);
    connect(m_scrollSpeed, &QSlider::valueChanged, this, changed);
    connect(m_wasd, &QPushButton::clicked, this, &StickPage::wasdRequested);
    connect(m_calibrate, &QPushButton::clicked, this, &StickPage::calibrateRequested);

    setSettings(StickSettings());
    setRestPosition({});
}

void StickPage::setSettings(const StickSettings &settings)
{
    m_updating = true;
    m_debounce->stop();
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(settings.mode)));
    m_deadzone->setValue(qRound(settings.deadzone * 100));
    m_speed->setValue(settings.speed);
    m_scrollSpeed->setValue(settings.scrollSpeed);
    m_deadzoneValue->setText(QStringLiteral("%1 %").arg(m_deadzone->value()));
    m_speedValue->setText(QString::number(m_speed->value()));
    m_scrollValue->setText(QString::number(m_scrollSpeed->value()));
    updateEnabled();
    m_updating = false;
}

void StickPage::setRestPosition(const QString &text)
{
    m_rest->setText(text.isEmpty() ? tr("Current rest position: unknown (device not connected)")
                                   : tr("Current rest position: %1 (middle: 128 / 128)").arg(text));
}

void StickPage::setDeviceAvailable(bool available)
{
    m_calibrate->setEnabled(available);
}

StickSettings StickPage::current() const
{
    StickSettings settings;
    settings.mode = static_cast<StickSettings::Mode>(m_mode->currentData().toInt());
    settings.deadzone = m_deadzone->value() / 100.0;
    settings.speed = m_speed->value();
    settings.scrollSpeed = m_scrollSpeed->value();
    return settings;
}

void StickPage::updateEnabled()
{
    const auto mode = static_cast<StickSettings::Mode>(m_mode->currentData().toInt());
    m_deadzone->setEnabled(mode != StickSettings::Mode::Passthrough);
    m_speed->setEnabled(mode == StickSettings::Mode::Mouse);
    m_scrollSpeed->setEnabled(mode == StickSettings::Mode::Scroll);
    m_wasd->setEnabled(mode == StickSettings::Mode::Keys);
    switch (mode) {
    case StickSettings::Mode::Passthrough:
        m_modeHint->setText(tr("Games see the stick as a joystick; the stick buttons are not bound."));
        break;
    case StickSettings::Mode::Keys:
        m_modeHint->setText(tr("The four directions work like keys and are bound in the picture (arrows around the "
                               "stick); diagonals press two of them."));
        break;
    case StickSettings::Mode::Mouse:
        m_modeHint->setText(tr("The stick moves the mouse pointer."));
        break;
    case StickSettings::Mode::Scroll:
        m_modeHint->setText(tr("The stick scrolls vertically and horizontally."));
        break;
    }
}
