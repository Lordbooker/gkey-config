#pragma once

#include "ModelRegistry.h"
#include "Profile.h"

#include <QMainWindow>
#include <QSystemTrayIcon>

#include <optional>

class Action;
class BindingEditor;
class DaemonClient;
class DeviceView;
class LcdPage;
class Settings;
class StickPage;
struct StickSettings;
class MacroEditor;
class MacroStore;
class ProfileStore;
class SessionSetup;
class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;

// Main window: device picture, profile and layer selection, binding editor,
// backlight colour and macro library. Profiles and macros are edited as files
// (the daemon reloads them); switching profile/layer and live feedback go
// through the daemon's D-Bus API.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Creates the tray icon when a system tray is available; returns true if it exists.
    bool setupTray();
    bool hasTray() const { return m_tray != nullptr; }

    void showWindow();

    // Selects a key and a tab (0 key, 1 backlight, 2 stick, 3 LCD, 4 macros), e.g. for screenshots.
    void showKey(const QString &keyId, int tab);

    // Renders the window to an image file (documentation, tests).
    bool saveScreenshot(const QString &path);

    static QIcon appIcon();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void selectModel(const QString &modelId);
    void reloadProfiles();
    void reloadView();
    void selectKey(const QString &keyId);
    const Profile *currentProfile() const;
    bool saveProfile(const Profile &profile);

    void onProfileActivated(int index);
    void onModeClicked(int mode);
    void onBindingChanged(const QString &keyId, const std::optional<Action> &action);
    void onNewProfile();
    void onDuplicateProfile();
    void onRenameProfile();
    void onDeleteProfile();
    void onStickSettingsChanged(const StickSettings &settings);
    void onStickWasd();
    void onStickCalibrate();
    void onBacklightToggled(bool enabled);
    void onChooseBacklight();
    void updateDaemonState();
    void runSystemctl(const QString &verb);
    void updateLedHint();
    void onLedAccessFinished(bool success, const QString &message);
    void onDaemonAutostartChanged();
    void askFirstRunSetup();

    ModelRegistry m_registry;
    DaemonClient *m_client = nullptr;
    MacroStore *m_macros = nullptr;
    ProfileStore *m_store = nullptr;
    DeviceModel m_model;
    QString m_profileName;
    int m_mode = 1;
    QString m_selectedKey;

    QComboBox *m_modelCombo = nullptr;
    QComboBox *m_profileCombo = nullptr;
    QToolButton *m_profileMenuButton = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    DeviceView *m_view = nullptr;
    BindingEditor *m_bindingEditor = nullptr;
    MacroEditor *m_macroEditor = nullptr;
    StickPage *m_stickPage = nullptr;
    LcdPage *m_lcdPage = nullptr;
    class QTabWidget *m_tabs = nullptr;
    Settings *m_settings = nullptr;
    QCheckBox *m_backlightEnabled = nullptr;
    QPushButton *m_backlightButton = nullptr;
    QLabel *m_ledHint = nullptr;
    QPushButton *m_ledButton = nullptr;
    SessionSetup *m_session = nullptr;
    QAction *m_daemonAutostartAction = nullptr;
    QAction *m_trayAutostartAction = nullptr;
    QAction *m_ledAccessAction = nullptr;
    bool m_firstRunPending = false;
    QLabel *m_daemonLabel = nullptr;
    QPushButton *m_daemonButton = nullptr;

    QSystemTrayIcon *m_tray = nullptr;
    QAction *m_trayStatus = nullptr;
    QAction *m_trayStartStop = nullptr;
    bool m_quitting = false;
    bool m_trayHintShown = false;
};
