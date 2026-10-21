#pragma once

#include <QLineEdit>
#include <QList>
#include <QSet>

// Input field that captures keys: click into it and press a key (Single) or a
// key combination such as Ctrl+Shift+T (Chord: all keys pressed until the
// last one is released). Linux input key codes are stored. Keys that usually
// control the user interface (Tab, Enter, Esc, Ctrl+S, ...) are captured too.
class KeyCaptureEdit : public QLineEdit {
    Q_OBJECT
public:
    enum class Mode { Single, Chord };

    explicit KeyCaptureEdit(Mode mode = Mode::Single, QWidget *parent = nullptr);

    const QList<int> &keyCodes() const { return m_codes; }
    void setKeyCodes(const QList<int> &codes);

    // Linux key code from QKeyEvent::nativeScanCode() (XKB key code = Linux code + 8);
    // 0 if the platform provides no scan code.
    static int linuxKeyCode(quint32 nativeScanCode);

signals:
    // Emitted when a key (Single) or a complete combination (Chord) was captured.
    void keysCaptured(const QList<int> &codes);

protected:
    bool event(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updateText();

    Mode m_mode;
    QList<int> m_codes;
    QList<int> m_chord;      // chord being captured
    QSet<int> m_down;        // keys currently held during chord capture
};
