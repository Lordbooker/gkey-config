#pragma once

#include "Macro.h"

#include <QElapsedTimer>
#include <QSet>
#include <QWidget>

class DaemonClient;
class KeyCaptureEdit;
class MacroStore;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

// Macro library: list of macros, step editor and recorder.
//
// Recording captures key presses and releases with their timing while the
// window has the keyboard focus (only the GUI sees the normal keyboard). MR on
// the device starts and stops the recording as well.
class MacroEditor : public QWidget {
    Q_OBJECT
public:
    MacroEditor(MacroStore *store, DaemonClient *client, QWidget *parent = nullptr);

    bool isRecording() const { return m_recording; }

public slots:
    // Called when MR on the device toggles the recording state.
    void setRecordingFromDevice(bool recording);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reloadList();
    void selectMacro(const QString &name);
    void showMacro(const Macro *macro);
    QList<Macro::Step> steps() const;
    void setSteps(const QList<Macro::Step> &steps);
    void appendStep(const Macro::Step &step);
    void setDirty(bool dirty);
    bool confirmDiscard();
    void updateButtons();

    void onNew();
    void onDelete();
    void onSave();
    void onPlay();
    void startRecording();
    void stopRecording();
    void recordKey(int code, bool pressed);
    void insertStep(Macro::Step::Type type);
    void moveSelected(int delta);
    void setAllDelays();

    static void applyToItem(QTreeWidgetItem *item, const Macro::Step &step);
    static Macro::Step stepFromItem(const QTreeWidgetItem *item);

    MacroStore *m_store;
    DaemonClient *m_client;
    QString m_current;   // name of the macro being edited
    bool m_dirty = false;
    bool m_recording = false;
    QElapsedTimer m_sinceLastEvent;
    QSet<int> m_heldWhileRecording;

    QListWidget *m_list;
    QPushButton *m_new;
    QPushButton *m_delete;
    QLineEdit *m_name;
    QLineEdit *m_description;
    QTreeWidget *m_steps;
    QPushButton *m_record;
    QLabel *m_recordHint;
    KeyCaptureEdit *m_stepKey;
    QPushButton *m_addPress;
    QPushButton *m_addRelease;
    QPushButton *m_addTap;
    QSpinBox *m_delay;
    QPushButton *m_addDelay;
    QPushButton *m_allDelays;
    QPushButton *m_up;
    QPushButton *m_down;
    QPushButton *m_remove;
    QPushButton *m_save;
    QPushButton *m_play;
    QLabel *m_status;
};
