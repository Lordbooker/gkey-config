// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "MainWindow.h"

#include "Action.h"
#include "BindingEditor.h"
#include "ConfigPaths.h"
#include "DaemonClient.h"
#include "DeviceDiagnosis.h"
#include "DeviceView.h"
#include "LcdPage.h"
#include "Settings.h"
#include "StickPage.h"
#include "MacroEditor.h"
#include "MacroStore.h"
#include "ProfileStore.h"
#include "SessionSetup.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <linux/input-event-codes.h>

namespace {

QString keyTitle(const DeviceModel &model, const QString &keyId)
{
    for (const auto &area : model.view().areas) {
        if (area.key == keyId && !area.label.isEmpty() && area.label != keyId)
            return QStringLiteral("%1 (%2)").arg(keyId, area.label);
    }
    return keyId;
}

QIcon colorIcon(const QColor &color)
{
    QPixmap pixmap(32, 20);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::gray, 1));
    painter.setBrush(color.isValid() ? color : QColor(Qt::transparent));
    painter.drawRoundedRect(QRectF(0.5, 0.5, 31, 19), 4, 4);
    return QIcon(pixmap);
}

}

QIcon MainWindow::appIcon()
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("gkey-config"));
    if (!icon.isNull())
        return icon;
    // Fallback when the icon is not installed: a drawn key cap with a "G".
    QIcon drawn;
    for (int size : {16, 22, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal m = size * 0.06;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x2b, 0x2d, 0x31));
        painter.drawRoundedRect(QRectF(m, m, size - 2 * m, size - 2 * m), size * 0.2, size * 0.2);
        painter.setBrush(QColor(0x3d, 0xae, 0xe9));
        painter.drawRoundedRect(QRectF(size * 0.2, size * 0.16, size * 0.6, size * 0.56), size * 0.1, size * 0.1);
        QFont font;
        font.setBold(true);
        font.setPixelSize(qMax(8, static_cast<int>(size * 0.42)));
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(size * 0.2, size * 0.16, size * 0.6, size * 0.56), Qt::AlignCenter, QStringLiteral("G"));
        drawn.addPixmap(pixmap);
    }
    return drawn;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_registry.load();
    m_client = new DaemonClient(this);
    m_macros = new MacroStore(ConfigPaths::macroDir(), this);
    m_settings = new Settings(ConfigPaths::settingsFile(), this);
    m_session = new SessionSetup(this);
    m_firstRunPending = !QSettings().value(QStringLiteral("setup/done"), false).toBool();

    setWindowTitle(tr("G-Key Configuration"));
    setWindowIcon(appIcon());
    setupUi();

    connect(m_client, &DaemonClient::availableChanged, this, &MainWindow::updateDaemonState);
    connect(m_client, &DaemonClient::statusChanged, this, &MainWindow::updateDaemonState);
    connect(m_client, &DaemonClient::profileChanged, this, [this](const QString &model, const QString &name) {
        if (model == m_model.id() && name != m_profileName) {
            m_profileName = name;
            reloadProfiles();
        }
    });
    connect(m_client, &DaemonClient::modeChanged, this, [this](const QString &model, int mode) {
        if (model == m_model.id() && mode != m_mode) {
            m_mode = mode;
            m_modeGroup->button(mode)->setChecked(true);
            reloadView();
        }
    });
    connect(m_client, &DaemonClient::keyEvent, this, [this](const QString &model, const QString &key, bool pressed) {
        if (model != m_model.id())
            return;
        m_view->setKeyPressed(key, pressed);
        // Pressing a key on the device selects it (while the window is in use and not recording).
        if (pressed && isActiveWindow() && !m_macroEditor->isRecording() && m_model.bindableKeyIds().contains(key))
            selectKey(key);
    });
    connect(m_client, &DaemonClient::recordingChanged, m_macroEditor, &MacroEditor::setRecordingFromDevice);
    connect(m_client, &DaemonClient::deviceAdded, this, [this](const QString &model) {
        if (m_model.id().isEmpty() || model != m_model.id())
            updateDaemonState();
    });
    connect(m_macros, &MacroStore::changed, this, [this] { m_bindingEditor->setMacroNames(m_macros->names()); });
    connect(m_session, &SessionSetup::daemonAutostartChanged, this, &MainWindow::onDaemonAutostartChanged);
    connect(m_session, &SessionSetup::failed, this,
            [this](const QString &message) { statusBar()->showMessage(message, 8000); });
    connect(m_session, &SessionSetup::ledAccessFinished, this, &MainWindow::onLedAccessFinished);
    m_session->refreshDaemonAutostart();
    // Plugging in or replugging a device that the kernel does not provide
    // raises no daemon signal: re-check the reason while the window is shown.
    auto *diagnosisTimer = new QTimer(this);
    diagnosisTimer->setInterval(3000);
    connect(diagnosisTimer, &QTimer::timeout, this, [this] {
        if (isVisible())
            updateStatusText();
    });
    diagnosisTimer->start();

    // Start with a connected model, else the first one.
    const QStringList connected = m_client->connectedModels();
    if (!connected.isEmpty())
        selectModel(connected.first());
    else if (!m_registry.models().isEmpty())
        selectModel(m_registry.models().first().id());
    updateDaemonState();
    resize(1100, 720);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    // --- Toolbar: device, profile, layer ------------------------------------------
    auto *toolbar = addToolBar(tr("Profile"));
    toolbar->setMovable(false);

    m_modelCombo = new QComboBox(this);
    for (const DeviceModel &model : m_registry.models())
        m_modelCombo->addItem(model.tested() ? model.name() : tr("%1 (untested)").arg(model.name()), model.id());
    toolbar->addWidget(new QLabel(tr(" Device: "), this));
    toolbar->addWidget(m_modelCombo);
    connect(m_modelCombo, &QComboBox::activated, this,
            [this](int index) { selectModel(m_modelCombo->itemData(index).toString()); });

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(tr(" Profile: "), this));
    m_profileCombo = new QComboBox(this);
    m_profileCombo->setMinimumContentsLength(16);
    m_profileCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    toolbar->addWidget(m_profileCombo);
    connect(m_profileCombo, &QComboBox::activated, this, &MainWindow::onProfileActivated);

    m_profileMenuButton = new QToolButton(this);
    m_profileMenuButton->setText(tr("Manage"));
    m_profileMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto *profileMenu = new QMenu(m_profileMenuButton);
    profileMenu->addAction(tr("New..."), this, &MainWindow::onNewProfile);
    profileMenu->addAction(tr("Duplicate..."), this, &MainWindow::onDuplicateProfile);
    profileMenu->addAction(tr("Rename..."), this, &MainWindow::onRenameProfile);
    profileMenu->addSeparator();
    profileMenu->addAction(tr("Delete"), this, &MainWindow::onDeleteProfile);
    m_profileMenuButton->setMenu(profileMenu);
    toolbar->addWidget(m_profileMenuButton);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(tr(" Layer: "), this));
    m_modeGroup = new QButtonGroup(this);
    for (int mode = 1; mode <= Profile::ModeCount; ++mode) {
        auto *button = new QToolButton(this);
        button->setText(QStringLiteral("M%1").arg(mode));
        button->setCheckable(true);
        button->setToolTip(tr("Show and edit layer M%1; switches the device as well").arg(mode));
        m_modeGroup->addButton(button, mode);
        toolbar->addWidget(button);
    }
    m_modeGroup->button(1)->setChecked(true);
    connect(m_modeGroup, &QButtonGroup::idClicked, this, &MainWindow::onModeClicked);

    // --- Centre: device picture and editors -------------------------------------------
    auto *splitter = new QSplitter(this);
    m_view = new DeviceView(splitter);
    connect(m_view, &DeviceView::keyClicked, this, &MainWindow::selectKey);
    splitter->addWidget(m_view);

    auto *tabs = new QTabWidget(splitter);
    m_tabs = tabs;
    m_bindingEditor = new BindingEditor(tabs);
    m_bindingEditor->setMacroNames(m_macros->names());
    connect(m_bindingEditor, &BindingEditor::bindingChanged, this, &MainWindow::onBindingChanged);
    tabs->addTab(m_bindingEditor, tr("Key"));

    auto *profileTab = new QWidget(tabs);
    auto *profileLayout = new QVBoxLayout(profileTab);
    auto *backlightForm = new QFormLayout;
    m_backlightEnabled = new QCheckBox(tr("This profile sets the backlight colour"), profileTab);
    m_backlightButton = new QPushButton(tr("Choose colour..."), profileTab);
    backlightForm->addRow(m_backlightEnabled);
    backlightForm->addRow(tr("Colour:"), m_backlightButton);
    profileLayout->addLayout(backlightForm);
    m_ledHint = new QLabel(profileTab);
    m_ledHint->setWordWrap(true);
    profileLayout->addWidget(m_ledHint);
    m_ledButton = new QPushButton(tr("Allow LED control..."), profileTab);
    connect(m_ledButton, &QPushButton::clicked, m_session, &SessionSetup::requestLedAccess);
    auto *ledButtonRow = new QHBoxLayout;
    ledButtonRow->addWidget(m_ledButton);
    ledButtonRow->addStretch();
    profileLayout->addLayout(ledButtonRow);
    profileLayout->addStretch();
    connect(m_backlightEnabled, &QCheckBox::toggled, this, &MainWindow::onBacklightToggled);
    connect(m_backlightButton, &QPushButton::clicked, this, &MainWindow::onChooseBacklight);
    tabs->addTab(profileTab, tr("Backlight"));

    m_stickPage = new StickPage(tabs);
    connect(m_stickPage, &StickPage::settingsChanged, this, &MainWindow::onStickSettingsChanged);
    connect(m_stickPage, &StickPage::wasdRequested, this, &MainWindow::onStickWasd);
    connect(m_stickPage, &StickPage::calibrateRequested, this, &MainWindow::onStickCalibrate);
    tabs->addTab(m_stickPage, tr("Stick"));

    m_lcdPage = new LcdPage(tabs);
    m_lcdPage->setPages(m_settings->lcdPages());
    connect(m_lcdPage, &LcdPage::pagesChanged, this, [this](const QStringList &pages) {
        m_settings->setLcdPages(pages);
        QString error;
        if (!m_settings->save(&error))
            QMessageBox::warning(this, tr("LCD"), error);
    });
    connect(m_lcdPage, &LcdPage::showOnDevice, m_client, &DaemonClient::lcdPage);
    connect(m_settings, &Settings::changed, this, [this] { m_lcdPage->setPages(m_settings->lcdPages()); });
    tabs->addTab(m_lcdPage, tr("LCD"));

    m_macroEditor = new MacroEditor(m_macros, m_client, tabs);
    tabs->addTab(m_macroEditor, tr("Macros"));

    splitter->addWidget(tabs);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 4);
    splitter->setSizes({600, 480});
    setCentralWidget(splitter);

    // --- Status bar: daemon state ---------------------------------------------------------
    m_daemonLabel = new QLabel(this);
    // Long diagnosis texts must not widen the window (a word-wrapping label
    // would make the status bar several lines high); the tooltip has the full text.
    m_daemonLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_daemonButton = new QPushButton(this);
    statusBar()->addWidget(m_daemonLabel, 1);
    statusBar()->addPermanentWidget(m_daemonButton);
    connect(m_daemonButton, &QPushButton::clicked, this,
            [this] { runSystemctl(m_client->available() ? QStringLiteral("restart") : QStringLiteral("start")); });

    // --- Menu ------------------------------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Open profile folder"), this, [this] {
        if (m_store)
            openFolder(m_store->directory());
    });
    fileMenu->addAction(tr("Open macro folder"), this, [this] { openFolder(m_macros->directory()); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, [this] {
        m_quitting = true;
        qApp->quit();
    });
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_daemonAutostartAction = settingsMenu->addAction(tr("Start the daemon at login"));
    m_daemonAutostartAction->setCheckable(true);
    m_daemonAutostartAction->setEnabled(false);
    connect(m_daemonAutostartAction, &QAction::triggered, m_session, &SessionSetup::setDaemonAutostart);
    m_trayAutostartAction = settingsMenu->addAction(tr("Show the tray icon at login"));
    m_trayAutostartAction->setCheckable(true);
    m_trayAutostartAction->setChecked(m_session->trayAutostart());
    connect(m_trayAutostartAction, &QAction::triggered, this, [this](bool enabled) {
        QString error;
        if (!m_session->setTrayAutostart(enabled, &error))
            QMessageBox::warning(this, tr("Tray icon at login"), error);
        m_trayAutostartAction->setChecked(m_session->trayAutostart());
    });
    settingsMenu->addSeparator();
    m_ledAccessAction = settingsMenu->addAction(tr("Allow LED control..."), m_session, &SessionSetup::requestLedAccess);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("About"), this, [this] {
        QMessageBox::about(this, tr("About gkey-config"),
                           tr("<b>gkey-config</b> %1<br>Configuration of the gaming keys of Logitech G keyboards "
                              "driven by the Linux kernel driver hid-lg-g15.<br><br>License: GPL-2.0-only<br>"
                              "<a href=\"https://github.com/Lordbooker/gkey-config\">github.com/Lordbooker/gkey-config</a>")
                               .arg(QStringLiteral(GKEY_VERSION)));
    });
}

// --- Model and profiles --------------------------------------------------------------------

void MainWindow::selectModel(const QString &modelId)
{
    const DeviceModel *model = m_registry.findById(modelId);
    if (!model)
        return;
    m_model = *model;
    m_modelCombo->setCurrentIndex(m_modelCombo->findData(modelId));
    m_view->setModel(m_model);

    delete m_store;
    m_store = new ProfileStore(m_model, ConfigPaths::profileDir(modelId), this);
    m_store->ensureDefault();
    connect(m_store, &ProfileStore::changed, this, &MainWindow::reloadProfiles);

    m_profileName = m_client->activeProfile(modelId);
    m_mode = m_client->available() ? m_client->activeMode(modelId) : 1;
    m_modeGroup->button(m_mode)->setChecked(true);
    m_selectedKey.clear();
    reloadProfiles();
    updateDaemonState();
}

void MainWindow::reloadProfiles()
{
    if (!m_store)
        return;
    const QStringList names = m_store->names();
    if (!m_store->profile(m_profileName))
        m_profileName = names.value(0);

    const QSignalBlocker blocker(m_profileCombo);
    m_profileCombo->clear();
    m_profileCombo->addItems(names);
    m_profileCombo->setCurrentText(m_profileName);
    m_bindingEditor->setProfileNames(names);
    reloadView();
}

const Profile *MainWindow::currentProfile() const
{
    return m_store ? m_store->profile(m_profileName) : nullptr;
}

void MainWindow::reloadView()
{
    const Profile *profile = currentProfile();
    QSet<QString> bound;
    if (profile) {
        const auto &bindings = profile->bindings(m_mode);
        for (auto it = bindings.begin(); it != bindings.end(); ++it)
            bound.insert(it.key());
    }
    m_view->setBoundKeys(bound);

    QColor accent;
    if (profile && profile->backlight())
        accent = QColor(profile->backlight()->red, profile->backlight()->green, profile->backlight()->blue);
    m_view->setAccentColor(accent);

    {
        const QSignalBlocker blocker(m_backlightEnabled);
        m_backlightEnabled->setChecked(profile && profile->backlight());
    }
    m_backlightButton->setEnabled(profile && profile->backlight());
    m_backlightButton->setIcon(colorIcon(accent));
    m_backlightEnabled->setEnabled(profile && m_model.ledByRole(QStringLiteral("backlight")));

    // Stick and LCD tabs exist only for models that have them.
    m_tabs->setTabEnabled(m_tabs->indexOf(m_stickPage), m_model.stick().has_value());
    m_tabs->setTabEnabled(m_tabs->indexOf(m_lcdPage), m_model.lcd().has_value());
    m_stickPage->setSettings(profile ? profile->stickSettings() : StickSettings());
    m_stickPage->setRestPosition(m_client->stickCenter(m_model.id()));
    m_stickPage->setDeviceAvailable(m_client->connectedModels().contains(m_model.id()));
    m_lcdPage->setStatus({m_model.name(), m_profileName, m_mode, m_client->recording()});

    selectKey(m_selectedKey);
}

void MainWindow::selectKey(const QString &keyId)
{
    // Keys with a fixed function (M1-M3, MR, BD, light) have nothing to edit.
    if (!keyId.isEmpty() && !m_model.bindableKeyIds().contains(keyId))
        return;
    m_selectedKey = keyId;
    m_view->setSelectedKey(keyId);
    std::optional<Action> action;
    if (const Profile *profile = currentProfile()) {
        const auto &bindings = profile->bindings(m_mode);
        if (bindings.contains(keyId))
            action = bindings.value(keyId);
    }
    const QString title = keyId.isEmpty() ? QString()
                                          : tr("%1 — layer M%2").arg(keyTitle(m_model, keyId)).arg(m_mode);
    m_bindingEditor->setKey(keyId, title, action);
}

bool MainWindow::saveProfile(const Profile &profile)
{
    QString error;
    if (!m_store->save(profile, &error)) {
        QMessageBox::warning(this, tr("Save profile"), error);
        return false;
    }
    return true;
}

void MainWindow::onProfileActivated(int index)
{
    m_profileName = m_profileCombo->itemText(index);
    if (m_client->available())
        m_client->setProfile(m_model.id(), m_profileName);
    reloadView();
}

void MainWindow::onModeClicked(int mode)
{
    m_mode = mode;
    if (m_client->available())
        m_client->setMode(m_model.id(), mode);
    reloadView();
}

void MainWindow::onBindingChanged(const QString &keyId, const std::optional<Action> &action)
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    Profile profile = *current;
    if (action)
        profile.setBinding(m_mode, keyId, *action);
    else
        profile.removeBinding(m_mode, keyId);
    if (saveProfile(profile)) {
        statusBar()->showMessage(tr("Saved"), 2000);
        reloadView();
    }
}

void MainWindow::onNewProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New profile"), tr("Name:"), QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (m_store->profile(name)) {
        QMessageBox::warning(this, tr("New profile"), tr("A profile \"%1\" already exists.").arg(name));
        return;
    }
    Profile profile;
    profile.setName(name);
    if (saveProfile(profile)) {
        m_profileName = name;
        reloadProfiles();
        if (m_client->available())
            m_client->setProfile(m_model.id(), name);
    }
}

void MainWindow::onDuplicateProfile()
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Duplicate profile"), tr("Name of the copy:"),
                                               QLineEdit::Normal, tr("%1 (copy)").arg(current->name()), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    if (m_store->profile(name)) {
        QMessageBox::warning(this, tr("Duplicate profile"), tr("A profile \"%1\" already exists.").arg(name));
        return;
    }
    Profile copy = *current;
    copy.setName(name);
    if (saveProfile(copy)) {
        m_profileName = name;
        reloadProfiles();
        if (m_client->available())
            m_client->setProfile(m_model.id(), name);
    }
}

void MainWindow::onRenameProfile()
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    const QString oldName = current->name();
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Rename profile"), tr("New name:"), QLineEdit::Normal,
                                               oldName, &ok)
                             .trimmed();
    if (!ok || name.isEmpty() || name == oldName)
        return;
    QString error;
    if (!m_store->rename(oldName, name, &error)) {
        QMessageBox::warning(this, tr("Rename profile"), error);
        return;
    }
    m_profileName = name;
    reloadProfiles();
    if (m_client->available())
        m_client->setProfile(m_model.id(), name);
}

void MainWindow::onDeleteProfile()
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    if (m_store->names().size() <= 1) {
        QMessageBox::information(this, tr("Delete profile"), tr("The last profile cannot be deleted."));
        return;
    }
    const QString name = current->name();
    if (QMessageBox::question(this, tr("Delete profile"), tr("Delete the profile \"%1\"?").arg(name))
        != QMessageBox::Yes)
        return;
    QString error;
    if (!m_store->remove(name, &error)) {
        QMessageBox::warning(this, tr("Delete profile"), error);
        return;
    }
    m_profileName.clear();
    reloadProfiles();
    if (m_client->available())
        m_client->setProfile(m_model.id(), m_profileName);
}

// --- Stick ------------------------------------------------------------------------------------

void MainWindow::onStickSettingsChanged(const StickSettings &settings)
{
    const Profile *current = currentProfile();
    if (!current || current->stickSettings() == settings)
        return;
    Profile profile = *current;
    profile.setStickSettings(settings);
    if (saveProfile(profile))
        statusBar()->showMessage(tr("Saved"), 2000);
}

void MainWindow::onStickWasd()
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    Profile profile = *current;
    profile.setBinding(m_mode, QStringLiteral("STICK_UP"), Action::key(KEY_W));
    profile.setBinding(m_mode, QStringLiteral("STICK_LEFT"), Action::key(KEY_A));
    profile.setBinding(m_mode, QStringLiteral("STICK_DOWN"), Action::key(KEY_S));
    profile.setBinding(m_mode, QStringLiteral("STICK_RIGHT"), Action::key(KEY_D));
    if (saveProfile(profile)) {
        statusBar()->showMessage(tr("Stick directions of layer M%1 bound to W, A, S, D").arg(m_mode), 4000);
        reloadView();
    }
}

void MainWindow::onStickCalibrate()
{
    if (m_client->calibrateStick(m_model.id()))
        statusBar()->showMessage(tr("Rest position measured: %1").arg(m_client->stickCenter(m_model.id())), 4000);
    else
        statusBar()->showMessage(tr("The rest position could not be measured (device connected?)"), 4000);
    m_stickPage->setRestPosition(m_client->stickCenter(m_model.id()));
}

// --- Backlight --------------------------------------------------------------------------------

void MainWindow::onBacklightToggled(bool enabled)
{
    const Profile *current = currentProfile();
    if (!current)
        return;
    Profile profile = *current;
    profile.setBacklight(enabled ? std::optional<Profile::Rgb>(Profile::Rgb{51, 153, 255}) : std::nullopt);
    if (saveProfile(profile))
        reloadView();
}

void MainWindow::onChooseBacklight()
{
    const Profile *current = currentProfile();
    if (!current || !current->backlight())
        return;
    const Profile::Rgb rgb = *current->backlight();
    QColorDialog dialog(QColor(rgb.red, rgb.green, rgb.blue), this);
    dialog.setWindowTitle(tr("Backlight colour"));
    // Live preview on the device while choosing.
    connect(&dialog, &QColorDialog::currentColorChanged, this,
            [this](const QColor &color) { m_client->setBacklight(m_model.id(), color); });
    if (dialog.exec() != QDialog::Accepted) {
        m_client->resetBacklight(m_model.id());
        return;
    }
    const QColor color = dialog.selectedColor();
    Profile profile = *currentProfile();
    profile.setBacklight(Profile::Rgb{color.red(), color.green(), color.blue()});
    if (saveProfile(profile))
        reloadView();
}

// --- Daemon state, tray ---------------------------------------------------------------------

void MainWindow::updateDaemonState()
{
    const bool available = m_client->available();
    updateStatusText();
    m_daemonButton->setText(available ? tr("Restart daemon") : tr("Start daemon"));
    updateLedHint();
    m_stickPage->setRestPosition(m_client->stickCenter(m_model.id()));
    m_stickPage->setDeviceAvailable(m_client->connectedModels().contains(m_model.id()));
    m_lcdPage->setStatus({m_model.name(), m_profileName, m_mode, m_client->recording()});

    if (m_trayStatus) {
        m_trayStatus->setText(available ? tr("Daemon: running") : tr("Daemon: stopped"));
        m_trayStartStop->setText(available ? tr("Stop daemon") : tr("Start daemon"));
    }
}

QString MainWindow::diagnosisText() const
{
    const DeviceDiagnosis::Result result = DeviceDiagnosis::diagnose(m_model);
    switch (result.problem) {
    case DeviceDiagnosis::Problem::KernelTooOld:
        return tr("%1 is plugged in, but the kernel %2 is too old: Linux %3 or newer is needed.")
            .arg(m_model.name(), result.detail, m_model.minKernel());
    case DeviceDiagnosis::Problem::UsedByOtherProgram:
        return tr("%1 is used directly by another program (e.g. linux-g13-driver). Stop it and replug the device.")
            .arg(m_model.name());
    case DeviceDiagnosis::Problem::NoKernelDriver:
        return tr("%1 is plugged in, but the kernel driver hid-lg-g15 does not drive it (driver: %2).")
            .arg(m_model.name(), result.detail.isEmpty() ? tr("none") : result.detail);
    case DeviceDiagnosis::Problem::None:
        break;
    }
    if (result.plugged && m_client->available() && !m_client->connectedModels().contains(m_model.id()))
        return tr("%1 is plugged in, but the daemon cannot use it. Replug the device; if that does not help, "
                  "check the udev rule and the daemon log (journalctl --user -u gkeyd).")
            .arg(m_model.name());
    return {};
}

// Status bar: daemon state and, while the device is not usable, the reason.
void MainWindow::updateStatusText()
{
    const bool available = m_client->available();
    const bool connected = available && m_client->connectedModels().contains(m_model.id());
    QString text;
    if (!available) {
        text = tr("The daemon gkeyd is not running: changes are saved but not active.");
    } else if (connected) {
        text = tr("Daemon running, %1 connected.").arg(m_model.name());
        if (m_client->activeProfile(m_model.id()) != m_profileName && !m_profileName.isEmpty())
            text += u' ' + tr("Active on the device: profile \"%1\".").arg(m_client->activeProfile(m_model.id()));
    } else {
        text = tr("Daemon running, %1 not connected.").arg(m_model.name());
    }
    if (!connected && !m_model.id().isEmpty()) {
        const QString reason = diagnosisText();
        if (!reason.isEmpty())
            text = available ? reason : text + u' ' + reason;
    }
    m_daemonLabel->setText(text);
    m_daemonLabel->setToolTip(text);
}

void MainWindow::openFolder(const QString &path)
{
    QDir().mkpath(path);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        // No file manager handler (e.g. xdg-utils missing): show the path instead.
        QMessageBox::information(this, tr("Open folder"),
                                 tr("The folder could not be opened. It is located at:\n%1").arg(path));
    }
}

void MainWindow::updateLedHint()
{
    const GroupAccess::State group = m_session->ledGroupState();
    const bool canRequest = group == GroupAccess::State::NotMember || group == GroupAccess::State::Missing;
    QString text;
    if (canRequest) {
        text = tr("LED control (backlight colour, M1-M3 lights) needs membership in the group \"gkey\". "
                  "The button adds your user; the system asks for the administrator password.");
    } else if (group == GroupAccess::State::NeedsRelogin) {
        text = tr("Your user is in the group \"gkey\". This takes effect after you log out completely and "
                  "back in (or restart the computer).");
    } else if (m_client->available() && !m_client->ledPermission(m_model.id())) {
        text = tr("The daemon may not change the LEDs although your user is in the group \"gkey\". "
                  "Replug the device; if that does not help, log out completely and back in.");
    }
    m_ledHint->setText(text);
    m_ledHint->setVisible(!text.isEmpty());
    m_ledButton->setVisible(canRequest);
    m_ledAccessAction->setEnabled(canRequest);
}

void MainWindow::onLedAccessFinished(bool success, const QString &message)
{
    updateLedHint();
    if (success) {
        QMessageBox::information(this, tr("LED control"),
                                 tr("Your user was added to the group \"gkey\". Log out completely and back in "
                                    "(or restart the computer) so that the change takes effect."));
    } else {
        QMessageBox::warning(this, tr("LED control"), message);
    }
}

void MainWindow::onDaemonAutostartChanged()
{
    const SessionSetup::DaemonAutostart state = m_session->daemonAutostart();
    const bool installed = state != SessionSetup::DaemonAutostart::NotInstalled
                           && state != SessionSetup::DaemonAutostart::Unknown;
    m_daemonAutostartAction->setEnabled(installed);
    m_daemonAutostartAction->setChecked(state == SessionSetup::DaemonAutostart::Enabled);
    m_daemonAutostartAction->setToolTip(installed ? QString() : tr("The systemd user unit gkeyd.service is not installed."));
    if (m_firstRunPending && isVisible())
        QTimer::singleShot(300, this, &MainWindow::askFirstRunSetup);
}

// First start of the window: offer the daemon autostart and LED access in one dialog.
void MainWindow::askFirstRunSetup()
{
    if (!m_firstRunPending || m_session->daemonAutostart() == SessionSetup::DaemonAutostart::Unknown)
        return;
    m_firstRunPending = false;
    QSettings().setValue(QStringLiteral("setup/done"), true);

    const SessionSetup::DaemonAutostart daemon = m_session->daemonAutostart();
    const bool offerDaemon =
        daemon == SessionSetup::DaemonAutostart::Disabled || daemon == SessionSetup::DaemonAutostart::Masked;
    const GroupAccess::State group = m_session->ledGroupState();
    const bool offerLed = group == GroupAccess::State::NotMember || group == GroupAccess::State::Missing;
    if (!offerDaemon && !offerLed)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set up gkey-config"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(tr("Recommended for everyday use (can be changed later in the Settings menu):"), &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *daemonBox = new QCheckBox(tr("Start the daemon gkeyd at every login"), &dialog);
    daemonBox->setChecked(true);
    daemonBox->setVisible(offerDaemon);
    layout->addWidget(daemonBox);
    auto *ledBox = new QCheckBox(tr("Allow LED control (group \"gkey\"; asks for the administrator password)"), &dialog);
    ledBox->setChecked(true);
    ledBox->setVisible(offerLed);
    layout->addWidget(ledBox);
    auto *buttons = new QDialogButtonBox(&dialog);
    buttons->addButton(tr("Set up"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Later"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (offerDaemon && daemonBox->isChecked())
        m_session->setDaemonAutostart(true);
    if (offerLed && ledBox->isChecked())
        m_session->requestLedAccess();
}

void MainWindow::runSystemctl(const QString &verb)
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, verb](int exitCode) {
        if (exitCode != 0)
            statusBar()->showMessage(tr("systemctl --user %1 gkeyd failed: %2")
                                         .arg(verb, QString::fromLocal8Bit(process->readAllStandardError()).trimmed()),
                                     8000);
        process->deleteLater();
        QTimer::singleShot(500, m_client, &DaemonClient::refreshStatus);
    });
    process->start(QStringLiteral("systemctl"), {QStringLiteral("--user"), verb, QStringLiteral("gkeyd")});
}

bool MainWindow::setupTray()
{
    if (m_tray)
        return true;
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return false;

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(tr("G-Key Configuration"));
    auto *menu = new QMenu(this);
    menu->addAction(tr("Open configuration"), this, &MainWindow::showWindow);
    menu->addSeparator();
    m_trayStatus = menu->addAction(QString());
    m_trayStatus->setEnabled(false);
    m_trayStartStop = menu->addAction(QString(), this, [this] {
        runSystemctl(m_client->available() ? QStringLiteral("stop") : QStringLiteral("start"));
    });
    menu->addAction(tr("Restart daemon"), this, [this] { runSystemctl(QStringLiteral("restart")); });
    menu->addAction(m_daemonAutostartAction);
    menu->addSeparator();
    menu->addAction(tr("Quit tray icon"), this, [this] {
        m_quitting = true;
        qApp->quit();
    });
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (isVisible() && !isMinimized())
                hide();
            else
                showWindow();
        }
    });
    m_tray->show();
    updateDaemonState();
    return true;
}

void MainWindow::showWindow()
{
    if (isMinimized())
        showNormal();
    else
        show();
    raise();
    activateWindow();
    if (m_firstRunPending)
        QTimer::singleShot(300, this, &MainWindow::askFirstRunSetup);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_macroEditor->isRecording())
        m_macroEditor->setRecordingFromDevice(false);
    if (m_tray && !m_quitting) {
        hide();
        if (!m_trayHintShown) {
            m_trayHintShown = true;
            m_tray->showMessage(tr("G-Key Configuration"),
                                tr("Still running in the system tray. The daemon keeps working without it."),
                                QSystemTrayIcon::Information, 4000);
        }
        event->ignore();
        return;
    }
    event->accept();
    qApp->quit();
}

void MainWindow::showKey(const QString &keyId, int tab)
{
    selectKey(keyId);
    m_tabs->setCurrentIndex(tab);
}

bool MainWindow::saveScreenshot(const QString &path)
{
    return grab().save(path);
}
