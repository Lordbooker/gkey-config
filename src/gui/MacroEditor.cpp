// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "MacroEditor.h"

#include "DaemonClient.h"
#include "KeyCaptureEdit.h"
#include "KeyNames.h"
#include "MacroStore.h"

#include <QApplication>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int StepRole = Qt::UserRole;

QString typeText(Macro::Step::Type type)
{
    switch (type) {
    case Macro::Step::Type::Press:
        return MacroEditor::tr("Press");
    case Macro::Step::Type::Release:
        return MacroEditor::tr("Release");
    case Macro::Step::Type::Tap:
        return MacroEditor::tr("Tap");
    case Macro::Step::Type::Delay:
        return MacroEditor::tr("Wait");
    }
    return {};
}

}

MacroEditor::MacroEditor(MacroStore *store, DaemonClient *client, QWidget *parent)
    : QWidget(parent), m_store(store), m_client(client)
{
    auto *layout = new QHBoxLayout(this);

    // --- Library -------------------------------------------------------------
    auto *libraryLayout = new QVBoxLayout;
    m_list = new QListWidget(this);
    m_list->setMinimumWidth(140);
    libraryLayout->addWidget(m_list);
    auto *libraryButtons = new QHBoxLayout;
    m_new = new QPushButton(tr("New"), this);
    m_delete = new QPushButton(tr("Delete"), this);
    libraryButtons->addWidget(m_new);
    libraryButtons->addWidget(m_delete);
    libraryLayout->addLayout(libraryButtons);
    layout->addLayout(libraryLayout, 1);

    // --- Editor ----------------------------------------------------------------
    auto *editorLayout = new QVBoxLayout;
    auto *form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setReadOnly(true);
    m_description = new QLineEdit(this);
    form->addRow(tr("Name:"), m_name);
    form->addRow(tr("Description:"), m_description);
    editorLayout->addLayout(form);

    auto *recordRow = new QHBoxLayout;
    m_record = new QPushButton(tr("Start recording"), this);
    m_record->setCheckable(true);
    m_recordHint = new QLabel(tr("Type on your keyboard; MR on the device starts and stops as well."), this);
    m_recordHint->setWordWrap(true);
    recordRow->addWidget(m_record);
    recordRow->addWidget(m_recordHint, 1);
    editorLayout->addLayout(recordRow);

    m_steps = new QTreeWidget(this);
    m_steps->setColumnCount(3);
    m_steps->setHeaderLabels({tr("Step"), tr("Key"), tr("Time (ms)")});
    m_steps->setRootIsDecorated(false);
    m_steps->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_steps->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_steps->setEditTriggers(QAbstractItemView::NoEditTriggers);   // only the time, see below
    editorLayout->addWidget(m_steps, 1);

    auto *stepButtons = new QHBoxLayout;
    m_up = new QPushButton(tr("Up"), this);
    m_down = new QPushButton(tr("Down"), this);
    m_remove = new QPushButton(tr("Remove"), this);
    stepButtons->addWidget(m_up);
    stepButtons->addWidget(m_down);
    stepButtons->addWidget(m_remove);
    stepButtons->addStretch();
    editorLayout->addLayout(stepButtons);

    auto *insertBox = new QGroupBox(tr("Insert step"), this);
    auto *insertLayout = new QGridLayout(insertBox);
    m_stepKey = new KeyCaptureEdit(KeyCaptureEdit::Mode::Single, insertBox);
    m_addPress = new QPushButton(tr("Press"), insertBox);
    m_addRelease = new QPushButton(tr("Release"), insertBox);
    m_addTap = new QPushButton(tr("Tap"), insertBox);
    m_delay = new QSpinBox(insertBox);
    m_delay->setRange(0, Macro::MaxDelay);
    m_delay->setValue(50);
    m_delay->setSuffix(tr(" ms"));
    m_addDelay = new QPushButton(tr("Wait"), insertBox);
    m_allDelays = new QPushButton(tr("Set all waits"), insertBox);
    insertLayout->addWidget(new QLabel(tr("Key:"), insertBox), 0, 0);
    insertLayout->addWidget(m_stepKey, 0, 1, 1, 3);
    insertLayout->addWidget(m_addPress, 1, 1);
    insertLayout->addWidget(m_addRelease, 1, 2);
    insertLayout->addWidget(m_addTap, 1, 3);
    insertLayout->addWidget(new QLabel(tr("Time:"), insertBox), 2, 0);
    insertLayout->addWidget(m_delay, 2, 1);
    insertLayout->addWidget(m_addDelay, 2, 2);
    insertLayout->addWidget(m_allDelays, 2, 3);
    editorLayout->addWidget(insertBox);

    auto *bottom = new QHBoxLayout;
    m_status = new QLabel(this);
    m_play = new QPushButton(tr("Play on device"), this);
    m_save = new QPushButton(tr("Save"), this);
    bottom->addWidget(m_status, 1);
    bottom->addWidget(m_play);
    bottom->addWidget(m_save);
    editorLayout->addLayout(bottom);
    layout->addLayout(editorLayout, 3);

    connect(m_list, &QListWidget::currentTextChanged, this, [this](const QString &name) {
        if (name == m_current)
            return;
        if (!confirmDiscard()) {
            const QSignalBlocker blocker(m_list);
            const auto items = m_list->findItems(m_current, Qt::MatchExactly);
            if (!items.isEmpty())
                m_list->setCurrentItem(items.first());
            return;
        }
        selectMacro(name);
    });
    connect(m_new, &QPushButton::clicked, this, &MacroEditor::onNew);
    connect(m_delete, &QPushButton::clicked, this, &MacroEditor::onDelete);
    connect(m_save, &QPushButton::clicked, this, &MacroEditor::onSave);
    connect(m_play, &QPushButton::clicked, this, &MacroEditor::onPlay);
    connect(m_record, &QPushButton::toggled, this, [this](bool on) {
        if (on)
            startRecording();
        else
            stopRecording();
    });
    connect(m_description, &QLineEdit::textEdited, this, [this] { setDirty(true); });
    connect(m_addPress, &QPushButton::clicked, this, [this] { insertStep(Macro::Step::Type::Press); });
    connect(m_addRelease, &QPushButton::clicked, this, [this] { insertStep(Macro::Step::Type::Release); });
    connect(m_addTap, &QPushButton::clicked, this, [this] { insertStep(Macro::Step::Type::Tap); });
    connect(m_addDelay, &QPushButton::clicked, this, [this] { insertStep(Macro::Step::Type::Delay); });
    connect(m_allDelays, &QPushButton::clicked, this, &MacroEditor::setAllDelays);
    connect(m_up, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(m_remove, &QPushButton::clicked, this, [this] {
        qDeleteAll(m_steps->selectedItems());
        setDirty(true);
    });
    connect(m_steps, &QTreeWidget::itemSelectionChanged, this, &MacroEditor::updateButtons);
    // Double-click on the time of a wait or tap edits it in place.
    connect(m_steps, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        const Macro::Step step = stepFromItem(item);
        if (column == 2 && !m_recording
            && (step.type == Macro::Step::Type::Delay || step.type == Macro::Step::Type::Tap))
            m_steps->editItem(item, 2);
    });
    connect(m_steps, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != 2 || m_recording)
            return;
        const Macro::Step step = stepFromItem(item);   // validates the edited text
        const QSignalBlocker blocker(m_steps);
        applyToItem(item, step);
        setDirty(true);
    });
    connect(m_stepKey, &KeyCaptureEdit::keysCaptured, this, &MacroEditor::updateButtons);
    connect(m_store, &MacroStore::changed, this, &MacroEditor::reloadList);

    reloadList();
}

void MacroEditor::reloadList()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    m_list->addItems(m_store->names());
    const auto items = m_list->findItems(m_current, Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_list->setCurrentItem(items.first());
        if (!m_dirty)
            showMacro(m_store->macro(m_current));
    } else if (!m_dirty) {
        // Nothing (or a deleted macro) selected: show the first one.
        m_current = m_list->count() > 0 ? m_list->item(0)->text() : QString();
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        showMacro(m_store->macro(m_current));
    }
    updateButtons();
}

void MacroEditor::selectMacro(const QString &name)
{
    m_current = name;
    showMacro(m_store->macro(name));
}

void MacroEditor::showMacro(const Macro *macro)
{
    m_name->setText(macro ? macro->name() : QString());
    m_description->setText(macro ? macro->description() : QString());
    setSteps(macro ? macro->steps() : QList<Macro::Step>());
    setDirty(false);
}

void MacroEditor::applyToItem(QTreeWidgetItem *item, const Macro::Step &step)
{
    // Data first: setText() below may trigger itemChanged, which reads it.
    item->setData(0, StepRole, QVariantList{static_cast<int>(step.type), step.code, step.duration});
    item->setText(0, typeText(step.type));
    item->setText(1, step.type == Macro::Step::Type::Delay ? QString() : KeyNames::nameFor(step.code));
    item->setText(2, step.type == Macro::Step::Type::Delay || step.type == Macro::Step::Type::Tap
                         ? QString::number(step.duration)
                         : QString());
    item->setFlags(item->flags() | Qt::ItemIsEditable);
}

Macro::Step MacroEditor::stepFromItem(const QTreeWidgetItem *item)
{
    const QVariantList data = item->data(0, StepRole).toList();
    Macro::Step step;
    step.type = static_cast<Macro::Step::Type>(data.value(0).toInt());
    step.code = data.value(1).toInt();
    step.duration = data.value(2).toInt();
    // The time column can be edited in place.
    if (step.type == Macro::Step::Type::Delay || step.type == Macro::Step::Type::Tap) {
        bool ok = false;
        const int edited = item->text(2).toInt(&ok);
        if (ok && edited >= 0 && edited <= Macro::MaxDelay)
            step.duration = edited;
    }
    return step;
}

QList<Macro::Step> MacroEditor::steps() const
{
    QList<Macro::Step> list;
    for (int i = 0; i < m_steps->topLevelItemCount(); ++i)
        list.append(stepFromItem(m_steps->topLevelItem(i)));
    return list;
}

void MacroEditor::setSteps(const QList<Macro::Step> &steps)
{
    m_steps->clear();
    for (const Macro::Step &step : steps)
        appendStep(step);
}

void MacroEditor::appendStep(const Macro::Step &step)
{
    const QSignalBlocker blocker(m_steps);
    auto *item = new QTreeWidgetItem(m_steps);
    applyToItem(item, step);
    m_steps->scrollToItem(item);
}

void MacroEditor::setDirty(bool dirty)
{
    m_dirty = dirty;
    m_status->setText(dirty ? tr("Unsaved changes") : QString());
    updateButtons();
}

bool MacroEditor::confirmDiscard()
{
    if (!m_dirty)
        return true;
    const auto answer = QMessageBox::question(this, tr("Unsaved changes"),
                                              tr("Discard the changes to the macro \"%1\"?").arg(m_current),
                                              QMessageBox::Discard | QMessageBox::Cancel);
    if (answer != QMessageBox::Discard)
        return false;
    setDirty(false);
    return true;
}

void MacroEditor::updateButtons()
{
    const bool haveMacro = !m_current.isEmpty();
    const bool haveSelection = !m_steps->selectedItems().isEmpty();
    const bool haveKey = !m_stepKey->keyCodes().isEmpty();
    m_delete->setEnabled(haveMacro && !m_recording);
    m_description->setEnabled(haveMacro);
    m_steps->setEnabled(haveMacro);
    m_record->setEnabled(haveMacro);
    m_addPress->setEnabled(haveMacro && haveKey && !m_recording);
    m_addRelease->setEnabled(haveMacro && haveKey && !m_recording);
    m_addTap->setEnabled(haveMacro && haveKey && !m_recording);
    m_addDelay->setEnabled(haveMacro && !m_recording);
    m_allDelays->setEnabled(haveMacro && !m_recording);
    m_up->setEnabled(haveSelection && !m_recording);
    m_down->setEnabled(haveSelection && !m_recording);
    m_remove->setEnabled(haveSelection && !m_recording);
    m_save->setEnabled(haveMacro && m_dirty && !m_recording);
    m_play->setEnabled(haveMacro && !m_dirty && m_client->available());
    m_list->setEnabled(!m_recording);
    m_new->setEnabled(!m_recording);
}

void MacroEditor::onNew()
{
    if (!confirmDiscard())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New macro"), tr("Name:"), QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (m_store->macro(name)) {
        QMessageBox::warning(this, tr("New macro"), tr("A macro \"%1\" already exists.").arg(name));
        return;
    }
    Macro macro;
    macro.setName(name);
    QString error;
    if (!m_store->save(macro, &error)) {
        QMessageBox::warning(this, tr("New macro"), error);
        return;
    }
    m_current = name;
    reloadList();
    showMacro(m_store->macro(name));
}

void MacroEditor::onDelete()
{
    if (m_current.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete macro"),
        tr("Delete the macro \"%1\"? Keys bound to it will do nothing.").arg(m_current));
    if (answer != QMessageBox::Yes)
        return;
    QString error;
    if (!m_store->remove(m_current, &error)) {
        QMessageBox::warning(this, tr("Delete macro"), error);
        return;
    }
    setDirty(false);
    m_current.clear();
    reloadList();
}

void MacroEditor::onSave()
{
    if (m_current.isEmpty())
        return;
    const QList<Macro::Step> list = steps();
    // A key pressed but never released stays down until the macro ends.
    QSet<int> held;
    for (const Macro::Step &step : list) {
        if (step.type == Macro::Step::Type::Press)
            held.insert(step.code);
        else if (step.type == Macro::Step::Type::Release)
            held.remove(step.code);
    }
    if (!held.isEmpty()) {
        QStringList names;
        for (int code : std::as_const(held))
            names.append(KeyNames::nameFor(code));
        const auto answer = QMessageBox::question(
            this, tr("Save macro"),
            tr("These keys are pressed but never released: %1. They are released when the macro ends. Save anyway?")
                .arg(names.join(QStringLiteral(", "))));
        if (answer != QMessageBox::Yes)
            return;
    }

    Macro macro = m_store->macro(m_current) ? *m_store->macro(m_current) : Macro();
    macro.setName(m_current);
    macro.setDescription(m_description->text());
    macro.setSteps(list);
    QString error;
    if (!m_store->save(macro, &error)) {
        QMessageBox::warning(this, tr("Save macro"), error);
        return;
    }
    setDirty(false);
    m_status->setText(tr("Saved"));
}

void MacroEditor::onPlay()
{
    if (!m_client->runMacro(m_current))
        m_status->setText(tr("The daemon could not play the macro."));
}

// --- Recording ------------------------------------------------------------------------

void MacroEditor::setRecordingFromDevice(bool recording)
{
    if (recording == m_recording)
        return;
    if (recording && m_current.isEmpty()) {
        m_client->setRecording(false);   // nothing to record into
        m_status->setText(tr("Select or create a macro before recording."));
        return;
    }
    m_record->setChecked(recording);   // triggers start/stopRecording
}

void MacroEditor::startRecording()
{
    if (m_recording || m_current.isEmpty())
        return;
    m_recording = true;
    m_heldWhileRecording.clear();
    m_sinceLastEvent.invalidate();
    m_record->setText(tr("Stop recording"));
    qApp->installEventFilter(this);
    window()->activateWindow();
    m_steps->setFocus();
    if (!m_client->recording())
        m_client->setRecording(true);   // MR LED
    m_status->setText(tr("Recording..."));
    updateButtons();
}

void MacroEditor::stopRecording()
{
    if (!m_recording)
        return;
    m_recording = false;
    qApp->removeEventFilter(this);
    // Release what is still held, so the macro does not leave keys down.
    for (int code : std::as_const(m_heldWhileRecording))
        appendStep({Macro::Step::Type::Release, code, 0});
    m_heldWhileRecording.clear();
    m_record->setChecked(false);
    m_record->setText(tr("Start recording"));
    if (m_client->recording())
        m_client->setRecording(false);
    setDirty(true);
}

bool MacroEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (m_recording && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease
                        || event->type() == QEvent::ShortcutOverride)) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();   // no shortcuts while recording
            return true;
        }
        if (!keyEvent->isAutoRepeat()) {
            const int code = KeyCaptureEdit::linuxKeyCode(keyEvent->nativeScanCode());
            if (code > 0) {
                if (event->type() == QEvent::KeyPress)
                    KeyNames::learnLabel(code, keyEvent);
                recordKey(code, event->type() == QEvent::KeyPress);
            }
        }
        return true;   // keys typed while recording do not reach other widgets
    }
    return QWidget::eventFilter(watched, event);
}

void MacroEditor::recordKey(int code, bool pressed)
{
    if (pressed == m_heldWhileRecording.contains(code))
        return;   // e.g. release of a key that was held before recording started
    if (m_sinceLastEvent.isValid()) {
        const int elapsed = static_cast<int>(qMin<qint64>(m_sinceLastEvent.elapsed(), Macro::MaxDelay));
        if (elapsed > 0)
            appendStep({Macro::Step::Type::Delay, 0, elapsed});
    }
    m_sinceLastEvent.start();
    appendStep({pressed ? Macro::Step::Type::Press : Macro::Step::Type::Release, code, 0});
    if (pressed)
        m_heldWhileRecording.insert(code);
    else
        m_heldWhileRecording.remove(code);
}

// --- Manual editing ---------------------------------------------------------------------

void MacroEditor::insertStep(Macro::Step::Type type)
{
    Macro::Step step;
    step.type = type;
    if (type == Macro::Step::Type::Delay) {
        step.duration = m_delay->value();
    } else {
        if (m_stepKey->keyCodes().isEmpty())
            return;
        step.code = m_stepKey->keyCodes().first();
        if (type == Macro::Step::Type::Tap)
            step.duration = Macro::DefaultTapHold;
    }
    const QSignalBlocker blocker(m_steps);
    auto *item = new QTreeWidgetItem;
    applyToItem(item, step);
    const QList<QTreeWidgetItem *> selected = m_steps->selectedItems();
    const int index = selected.isEmpty() ? m_steps->topLevelItemCount()
                                         : m_steps->indexOfTopLevelItem(selected.last()) + 1;
    m_steps->insertTopLevelItem(index, item);
    m_steps->setCurrentItem(item);
    setDirty(true);
}

void MacroEditor::moveSelected(int delta)
{
    QList<QTreeWidgetItem *> selected = m_steps->selectedItems();
    if (selected.isEmpty())
        return;
    std::ranges::sort(selected, [this](QTreeWidgetItem *a, QTreeWidgetItem *b) {
        return m_steps->indexOfTopLevelItem(a) < m_steps->indexOfTopLevelItem(b);
    });
    const int first = m_steps->indexOfTopLevelItem(selected.first());
    const int last = m_steps->indexOfTopLevelItem(selected.last());
    if ((delta < 0 && first == 0) || (delta > 0 && last == m_steps->topLevelItemCount() - 1))
        return;
    if (delta > 0)
        std::ranges::reverse(selected);
    for (QTreeWidgetItem *item : selected) {
        const int index = m_steps->indexOfTopLevelItem(item);
        m_steps->takeTopLevelItem(index);
        m_steps->insertTopLevelItem(index + delta, item);
    }
    for (QTreeWidgetItem *item : selected)
        item->setSelected(true);
    setDirty(true);
}

void MacroEditor::setAllDelays()
{
    const QSignalBlocker blocker(m_steps);
    for (int i = 0; i < m_steps->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_steps->topLevelItem(i);
        Macro::Step step = stepFromItem(item);
        if (step.type == Macro::Step::Type::Delay) {
            step.duration = m_delay->value();
            applyToItem(item, step);
        }
    }
    setDirty(true);
}
