#pragma once

#include "Action.h"

#include <QStringList>
#include <QWidget>

#include <optional>

class KeyCaptureEdit;
class QComboBox;
class QLabel;
class QSpinBox;
class QStackedWidget;

// Edits the binding of one key in one layer. Every complete change is
// reported at once (the main window saves it; the daemon reloads).
class BindingEditor : public QWidget {
    Q_OBJECT
public:
    explicit BindingEditor(QWidget *parent = nullptr);

    // Choices offered for macro and profile actions.
    void setMacroNames(const QStringList &names);
    void setProfileNames(const QStringList &names);

    // Shows the binding of a key; an empty keyId shows a hint instead.
    void setKey(const QString &keyId, const QString &title, const std::optional<Action> &action);

signals:
    void bindingChanged(const QString &keyId, const std::optional<Action> &action);

private:
    enum Page { PageNone, PageKey, PageCombo, PageMouse, PageMacro, PageMode, PageProfile, PageLcd };

    void onTypeChanged(int index);
    void emitCurrent();
    std::optional<Action> currentAction() const;
    void showAction(const std::optional<Action> &action);

    QString m_keyId;
    bool m_updating = false;

    QLabel *m_title;
    QLabel *m_hint;
    QWidget *m_editor;
    QComboBox *m_type;
    QStackedWidget *m_pages;
    KeyCaptureEdit *m_key;
    KeyCaptureEdit *m_combo;
    QComboBox *m_mouse;
    QComboBox *m_macro;
    QComboBox *m_repeat;
    QSpinBox *m_count;
    QComboBox *m_mode;
    QComboBox *m_profile;
    QComboBox *m_lcd;
};
