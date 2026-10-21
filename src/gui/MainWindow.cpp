#include "MainWindow.h"

#include "Action.h"
#include "BindingEditor.h"
#include "ConfigPaths.h"
#include "DaemonClient.h"
#include "DeviceView.h"
#include "LcdPage.h"
#include "Settings.h"
#include "StickPage.h"
#include "MacroEditor.h"
#include "MacroStore.h"
#include "ProfileStore.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
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
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
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
    m_ledHint->setText(tr("The daemon may not change the LEDs. Add your user to the group \"gkey\" "
                          "(sudo usermod -aG gkey $USER), log out and back in, and replug the device."));
    profileLayout->addWidget(m_ledHint);
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
    m_daemonButton = new QPushButton(this);
    statusBar()->addWidget(m_daemonLabel, 1);
    statusBar()->addPermanentWidget(m_daemonButton);
    connect(m_daemonButton, &QPushButton::clicked, this,
            [this] { runSystemctl(m_client->available() ? QStringLiteral("restart") : QStringLiteral("start")); });

    // --- Menu ------------------------------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Open profile folder"), this, [this] {
        if (m_store)
            QProcess::startDetached(QStringLiteral("xdg-open"), {m_store->directory()});
    });
    fileMenu->addAction(tr("Open macro folder"), this,
                        [this] { QProcess::startDetached(QStringLiteral("xdg-open"), {m_macros->directory()}); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, [this] {
        m_quitting = true;
        qApp->quit();
    });
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
    QString text;
    if (!available) {
        text = tr("The daemon gkeyd is not running: changes are saved but not active.");
    } else {
        const bool connected = m_client->connectedModels().contains(m_model.id());
        text = connected ? tr("Daemon running, %1 connected.").arg(m_model.name())
                         : tr("Daemon running, %1 not connected.").arg(m_model.name());
        if (connected && m_client->activeProfile(m_model.id()) != m_profileName && !m_profileName.isEmpty())
            text += u' ' + tr("Active on the device: profile \"%1\".").arg(m_client->activeProfile(m_model.id()));
    }
    m_daemonLabel->setText(text);
    m_daemonButton->setText(available ? tr("Restart daemon") : tr("Start daemon"));
    m_ledHint->setVisible(available && !m_client->ledPermission(m_model.id()));
    m_stickPage->setRestPosition(m_client->stickCenter(m_model.id()));
    m_stickPage->setDeviceAvailable(m_client->connectedModels().contains(m_model.id()));
    m_lcdPage->setStatus({m_model.name(), m_profileName, m_mode, m_client->recording()});

    if (m_trayStatus) {
        m_trayStatus->setText(available ? tr("Daemon: running") : tr("Daemon: stopped"));
        m_trayStartStop->setText(available ? tr("Stop daemon") : tr("Start daemon"));
    }
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
