#include "KeyCaptureEdit.h"

#include "KeyNames.h"

#include <QContextMenuEvent>
#include <QKeyEvent>

KeyCaptureEdit::KeyCaptureEdit(Mode mode, QWidget *parent) : QLineEdit(parent), m_mode(mode)
{
    setReadOnly(true);   // no text cursor, no pasting
    setFocusPolicy(Qt::StrongFocus);
    if (m_mode == Mode::Single) {
        setPlaceholderText(tr("Click here and press a key"));
        setToolTip(tr("Click into the field and press a key on your keyboard."));
    } else {
        setPlaceholderText(tr("Click here and press the key combination"));
        setToolTip(tr("Click into the field and press the keys together, e.g. Ctrl+Shift+T."));
    }
}

int KeyCaptureEdit::linuxKeyCode(quint32 nativeScanCode)
{
    constexpr quint32 XkbOffset = 8;
    return nativeScanCode > XkbOffset ? static_cast<int>(nativeScanCode - XkbOffset) : 0;
}

void KeyCaptureEdit::setKeyCodes(const QList<int> &codes)
{
    m_codes = codes;
    updateText();
}

void KeyCaptureEdit::updateText()
{
    QStringList names;
    for (int code : std::as_const(m_codes))
        names.append(KeyNames::nameFor(code));
    setText(names.join(QStringLiteral(" + ")));
}

bool KeyCaptureEdit::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        // Accepting suppresses shortcuts (e.g. Ctrl+S); the KeyPress follows.
        event->accept();
        return true;

    case QEvent::KeyPress: {
        // Intercepted before QWidget::event, so Tab/Backtab are captured as
        // well instead of moving the focus.
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->isAutoRepeat())
            return true;
        const int code = linuxKeyCode(keyEvent->nativeScanCode());
        if (code <= 0)
            return true;
        KeyNames::learnLabel(code, keyEvent);
        if (m_mode == Mode::Single) {
            setKeyCodes({code});
            emit keysCaptured(m_codes);
        } else {
            if (m_down.isEmpty())
                m_chord.clear();
            m_down.insert(code);
            if (!m_chord.contains(code))
                m_chord.append(code);
            m_codes = m_chord;
            updateText();
        }
        return true;
    }

    case QEvent::KeyRelease: {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (m_mode == Mode::Chord && !keyEvent->isAutoRepeat()) {
            m_down.remove(linuxKeyCode(keyEvent->nativeScanCode()));
            if (m_down.isEmpty() && !m_chord.isEmpty())
                emit keysCaptured(m_codes);
        }
        return true;
    }

    default:
        return QLineEdit::event(event);
    }
}

void KeyCaptureEdit::focusOutEvent(QFocusEvent *event)
{
    // Keys released outside the field (e.g. after Alt+Tab) must not leave the chord open.
    if (m_mode == Mode::Chord && !m_down.isEmpty()) {
        m_down.clear();
        if (!m_chord.isEmpty())
            emit keysCaptured(m_codes);
    }
    QLineEdit::focusOutEvent(event);
}

void KeyCaptureEdit::contextMenuEvent(QContextMenuEvent *event)
{
    event->accept();   // no copy/paste menu
}
