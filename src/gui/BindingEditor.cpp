// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "BindingEditor.h"

#include "KeyCaptureEdit.h"
#include "KeyNames.h"
#include "OutputCodes.h"

#include <QComboBox>
#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <linux/input-event-codes.h>

BindingEditor::BindingEditor(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_title = new QLabel(this);
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);

    m_hint = new QLabel(tr("Click a key in the picture or press it on the device."), this);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    m_editor = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(m_editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    auto *typeForm = new QFormLayout;
    m_type = new QComboBox(m_editor);
    m_type->addItem(tr("Not bound"));
    m_type->addItem(tr("Key"));
    m_type->addItem(tr("Key combination"));
    m_type->addItem(tr("Mouse button"));
    m_type->addItem(tr("Macro"));
    m_type->addItem(tr("Switch layer"));
    m_type->addItem(tr("Switch profile"));
    m_type->addItem(tr("LCD page"));
    typeForm->addRow(tr("Action:"), m_type);
    editorLayout->addLayout(typeForm);

    m_pages = new QStackedWidget(m_editor);
    auto addPage = [this](QWidget *content) {
        auto *page = new QWidget(m_pages);
        auto *pageLayout = new QFormLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        if (content)
            pageLayout->addRow(content);
        m_pages->addWidget(page);
        return pageLayout;
    };

    // None
    addPage(new QLabel(tr("The key does nothing in this layer."), m_pages));

    // Key
    m_key = new KeyCaptureEdit(KeyCaptureEdit::Mode::Single, m_pages);
    addPage(nullptr)->addRow(tr("Key:"), m_key);
    connect(m_key, &KeyCaptureEdit::keysCaptured, this, &BindingEditor::emitCurrent);

    // Combination
    m_combo = new KeyCaptureEdit(KeyCaptureEdit::Mode::Chord, m_pages);
    QFormLayout *comboForm = addPage(nullptr);
    comboForm->addRow(tr("Keys:"), m_combo);
    auto *comboHint = new QLabel(tr("Press all keys together, e.g. Ctrl+C."), m_pages);
    comboHint->setWordWrap(true);
    comboForm->addRow(comboHint);
    connect(m_combo, &KeyCaptureEdit::keysCaptured, this, &BindingEditor::emitCurrent);

    // Mouse
    m_mouse = new QComboBox(m_pages);
    for (int code = BTN_LEFT; code <= BTN_TASK; ++code)
        m_mouse->addItem(KeyNames::nameFor(code), code);
    addPage(nullptr)->addRow(tr("Button:"), m_mouse);
    connect(m_mouse, &QComboBox::currentIndexChanged, this, &BindingEditor::emitCurrent);

    // Macro
    m_macro = new QComboBox(m_pages);
    m_repeat = new QComboBox(m_pages);
    m_repeat->addItem(tr("Run once"), QStringLiteral("once"));
    m_repeat->addItem(tr("Repeat while held"), QStringLiteral("while_held"));
    m_repeat->addItem(tr("Start/stop with each press"), QStringLiteral("toggle"));
    m_repeat->addItem(tr("Run several times"), QStringLiteral("count"));
    m_count = new QSpinBox(m_pages);
    m_count->setRange(1, 999);
    QFormLayout *macroForm = addPage(nullptr);
    macroForm->addRow(tr("Macro:"), m_macro);
    macroForm->addRow(tr("Run:"), m_repeat);
    macroForm->addRow(tr("Times:"), m_count);
    auto *macroHint = new QLabel(tr("Pressing the key again stops a running macro."), m_pages);
    macroHint->setWordWrap(true);
    macroForm->addRow(macroHint);
    connect(m_macro, &QComboBox::currentIndexChanged, this, &BindingEditor::emitCurrent);
    connect(m_repeat, &QComboBox::currentIndexChanged, this, [this] {
        m_count->setEnabled(m_repeat->currentData().toString() == u"count");
        emitCurrent();
    });
    connect(m_count, &QSpinBox::valueChanged, this, &BindingEditor::emitCurrent);

    // Mode
    m_mode = new QComboBox(m_pages);
    for (int mode = 1; mode <= 3; ++mode)
        m_mode->addItem(QStringLiteral("M%1").arg(mode), mode);
    addPage(nullptr)->addRow(tr("Layer:"), m_mode);
    connect(m_mode, &QComboBox::currentIndexChanged, this, &BindingEditor::emitCurrent);

    // Profile
    m_profile = new QComboBox(m_pages);
    addPage(nullptr)->addRow(tr("Profile:"), m_profile);
    connect(m_profile, &QComboBox::currentIndexChanged, this, &BindingEditor::emitCurrent);

    // LCD
    m_lcd = new QComboBox(m_pages);
    m_lcd->addItem(tr("Next page"), QStringLiteral("next"));
    m_lcd->addItem(tr("Previous page"), QStringLiteral("previous"));
    m_lcd->addItem(tr("Status"), QStringLiteral("status"));
    m_lcd->addItem(tr("System"), QStringLiteral("system"));
    m_lcd->addItem(tr("Clock"), QStringLiteral("clock"));
    m_lcd->addItem(tr("Text"), QStringLiteral("text"));
    addPage(nullptr)->addRow(tr("Page:"), m_lcd);
    connect(m_lcd, &QComboBox::currentIndexChanged, this, &BindingEditor::emitCurrent);

    editorLayout->addWidget(m_pages);
    layout->addWidget(m_editor);
    layout->addStretch();

    connect(m_type, &QComboBox::currentIndexChanged, this, &BindingEditor::onTypeChanged);
    setMacroNames({});
    setProfileNames({});
    setKey({}, {}, std::nullopt);
}

void BindingEditor::setMacroNames(const QStringList &names)
{
    m_updating = true;
    const QString current = m_macro->currentText();
    m_macro->clear();
    m_macro->addItems(names);
    m_macro->setCurrentText(current);
    m_updating = false;
}

void BindingEditor::setProfileNames(const QStringList &names)
{
    m_updating = true;
    const QVariant current = m_profile->currentData();
    m_profile->clear();
    m_profile->addItem(tr("Next profile"), QStringLiteral("next"));
    m_profile->addItem(tr("Previous profile"), QStringLiteral("previous"));
    for (const QString &name : names)
        m_profile->addItem(name, name);
    const int index = m_profile->findData(current);
    m_profile->setCurrentIndex(index >= 0 ? index : 0);
    m_updating = false;
}

void BindingEditor::setKey(const QString &keyId, const QString &title, const std::optional<Action> &action)
{
    m_keyId = keyId;
    m_hint->setVisible(keyId.isEmpty());
    m_editor->setVisible(!keyId.isEmpty());
    m_title->setText(keyId.isEmpty() ? tr("No key selected") : title);
    if (!keyId.isEmpty())
        showAction(action);
}

void BindingEditor::showAction(const std::optional<Action> &action)
{
    m_updating = true;
    Page page = PageNone;
    m_key->setKeyCodes({});
    m_combo->setKeyCodes({});
    if (action) {
        switch (action->type()) {
        case Action::Type::Key:
            page = PageKey;
            m_key->setKeyCodes(action->codes());
            break;
        case Action::Type::Combo:
            page = PageCombo;
            m_combo->setKeyCodes(action->codes());
            break;
        case Action::Type::Mouse:
            page = PageMouse;
            m_mouse->setCurrentIndex(qMax(0, m_mouse->findData(action->codes().value(0))));
            break;
        case Action::Type::Macro:
            page = PageMacro;
            if (m_macro->findText(action->macro()) < 0)
                m_macro->addItem(action->macro());   // keep a binding to a missing macro visible
            m_macro->setCurrentText(action->macro());
            m_repeat->setCurrentIndex(static_cast<int>(action->repeat()));
            m_count->setValue(action->count());
            break;
        case Action::Type::Mode:
            page = PageMode;
            m_mode->setCurrentIndex(action->modeNumber() - 1);
            break;
        case Action::Type::Profile: {
            page = PageProfile;
            if (m_profile->findData(action->target()) < 0)
                m_profile->addItem(action->target(), action->target());
            m_profile->setCurrentIndex(m_profile->findData(action->target()));
            break;
        }
        case Action::Type::Lcd:
            page = PageLcd;
            m_lcd->setCurrentIndex(qMax(0, m_lcd->findData(action->page())));
            break;
        }
    }
    m_count->setEnabled(m_repeat->currentData().toString() == u"count");
    m_type->setCurrentIndex(page);
    m_pages->setCurrentIndex(page);
    m_updating = false;
}

void BindingEditor::onTypeChanged(int index)
{
    m_pages->setCurrentIndex(index);
    if (m_updating)
        return;
    // Start capturing right away when a key type was chosen.
    if (index == PageKey)
        m_key->setFocus();
    else if (index == PageCombo)
        m_combo->setFocus();
    emitCurrent();
}

std::optional<Action> BindingEditor::currentAction() const
{
    QJsonObject json;
    switch (m_type->currentIndex()) {
    case PageKey:
        if (m_key->keyCodes().isEmpty() || !OutputCodes::isKeyboardKey(m_key->keyCodes().first()))
            return std::nullopt;
        return Action::key(m_key->keyCodes().first());
    case PageCombo: {
        const QList<int> codes = m_combo->keyCodes();
        if (codes.size() < 2)
            return std::nullopt;
        for (int code : codes) {
            if (!OutputCodes::isKeyboardKey(code))
                return std::nullopt;
        }
        return Action::combo(codes);
    }
    case PageMouse:
        return Action::mouse(m_mouse->currentData().toInt());
    case PageMacro:
        if (m_macro->currentText().isEmpty())
            return std::nullopt;
        json.insert(QStringLiteral("type"), QStringLiteral("macro"));
        json.insert(QStringLiteral("macro"), m_macro->currentText());
        json.insert(QStringLiteral("repeat"), m_repeat->currentData().toString());
        json.insert(QStringLiteral("count"), m_count->value());
        break;
    case PageMode:
        return Action::mode(m_mode->currentData().toInt());
    case PageProfile:
        return Action::profile(m_profile->currentData().toString());
    case PageLcd:
        json.insert(QStringLiteral("type"), QStringLiteral("lcd"));
        json.insert(QStringLiteral("page"), m_lcd->currentData().toString());
        break;
    default:
        return std::nullopt;
    }
    const auto action = Action::fromJson(json);
    return action ? std::optional<Action>(*action) : std::nullopt;
}

void BindingEditor::emitCurrent()
{
    if (m_updating || m_keyId.isEmpty())
        return;
    if (m_type->currentIndex() == PageNone) {
        emit bindingChanged(m_keyId, std::nullopt);
        return;
    }
    // Incomplete input (e.g. no key captured yet) changes nothing.
    if (const auto action = currentAction())
        emit bindingChanged(m_keyId, action);
}
