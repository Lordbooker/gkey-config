// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

// Per-user autostart switch (XDG autostart) and group membership check.

#include "Autostart.h"
#include "GroupAccess.h"
#include "SystemError.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cerrno>
#include <memory>

#include <grp.h>
#include <unistd.h>

namespace {

const QString FileName = QStringLiteral("gkey-config-tray.desktop");

void write(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

}

class TestSession : public QObject {
    Q_OBJECT
private slots:
    void init();
    void systemEntry();
    void noSystemEntry();
    void gnomeDisabledEntry();
    void groupStates();
    void errorText();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    Autostart::Paths m_paths;
};

void TestSession::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_paths.userDir = m_dir->filePath(QStringLiteral("home/autostart"));
    m_paths.systemDirs = {m_dir->filePath(QStringLiteral("etc1/autostart")),
                          m_dir->filePath(QStringLiteral("etc2/autostart"))};
}

void TestSession::systemEntry()
{
    write(QDir(m_paths.systemDirs.at(1)).filePath(FileName),
          "[Desktop Entry]\nType=Application\nName=Tray\nExec=gkey-config --tray\n");
    QVERIFY(Autostart::isEnabled(FileName, m_paths));

    // Off: a user file hides the system-wide entry.
    QString error;
    QVERIFY2(Autostart::setEnabled(FileName, false, QStringLiteral("Tray"), QStringLiteral("gkey-config --tray"),
                                   m_paths, &error),
             qPrintable(error));
    QVERIFY(!Autostart::isEnabled(FileName, m_paths));
    QVERIFY(read(QDir(m_paths.userDir).filePath(FileName)).contains("Hidden=true"));

    // On again: the user file is removed, the system-wide entry applies.
    QVERIFY(Autostart::setEnabled(FileName, true, QStringLiteral("Tray"), QStringLiteral("gkey-config --tray"),
                                  m_paths, &error));
    QVERIFY(Autostart::isEnabled(FileName, m_paths));
    QVERIFY(!QFile::exists(QDir(m_paths.userDir).filePath(FileName)));
}

void TestSession::noSystemEntry()
{
    QVERIFY(!Autostart::isEnabled(FileName, m_paths));
    QVERIFY(Autostart::setEnabled(FileName, true, QStringLiteral("Tray"), QStringLiteral("/opt/gkey-config --tray"),
                                  m_paths));
    QVERIFY(Autostart::isEnabled(FileName, m_paths));
    QVERIFY(read(QDir(m_paths.userDir).filePath(FileName)).contains("Exec=/opt/gkey-config --tray"));
    QVERIFY(Autostart::setEnabled(FileName, false, QStringLiteral("Tray"), QStringLiteral("x"), m_paths));
    QVERIFY(!Autostart::isEnabled(FileName, m_paths));
    QVERIFY(!QFile::exists(QDir(m_paths.userDir).filePath(FileName)));
}

void TestSession::gnomeDisabledEntry()
{
    write(QDir(m_paths.userDir).filePath(FileName),
          "[Desktop Entry]\nType=Application\nExec=x\nX-GNOME-Autostart-enabled=false\n");
    QVERIFY(!Autostart::isEnabled(FileName, m_paths));
    // Keys of other groups do not count.
    write(QDir(m_paths.userDir).filePath(FileName), "[Desktop Entry]\nExec=x\n[Desktop Action a]\nHidden=true\n");
    QVERIFY(Autostart::isEnabled(FileName, m_paths));
}

void TestSession::groupStates()
{
    QCOMPARE(GroupAccess::state(QStringLiteral("gkey-no-such-group-4711")), GroupAccess::State::Missing);
    const group *own = getgrgid(getegid());
    if (!own)
        QSKIP("own group not in the group database");
    QCOMPARE(GroupAccess::state(QString::fromLocal8Bit(own->gr_name)), GroupAccess::State::Active);
    QVERIFY(!GroupAccess::userName().isEmpty());
}

// The errno description from strerror_r() (text depends on the locale).
void TestSession::errorText()
{
    const QString denied = SystemError::text(EACCES);
    QVERIFY(!denied.isEmpty());
    QVERIFY(denied != SystemError::text(ENOENT));
    QVERIFY(!SystemError::text(99999).isEmpty());   // unknown number
}

QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
