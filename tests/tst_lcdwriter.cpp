// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "LcdWriter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// LcdWriter against a fake sysfs/dev tree: the "hidraw node" is a plain file.
class TestLcdWriter : public QObject {
    Q_OBJECT

private slots:
    void init();
    void writesLastReport();
    void nodeAppearsLater();

private:
    QByteArray node() const;
    void createNode();

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_hid;
    QString m_dev;
};

void TestLcdWriter::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_hid = m_dir->filePath(QStringLiteral("sys/0003:046D:C21C.0001"));
    m_dev = m_dir->filePath(QStringLiteral("dev"));
    QVERIFY(QDir().mkpath(m_hid));
    QVERIFY(QDir().mkpath(m_dev));
}

void TestLcdWriter::createNode()
{
    QVERIFY(QDir().mkpath(m_hid + QStringLiteral("/hidraw/hidraw7")));
    QFile file(m_dev + QStringLiteral("/hidraw7"));
    QVERIFY(file.open(QIODevice::WriteOnly));
}

QByteArray TestLcdWriter::node() const
{
    QFile file(m_dev + QStringLiteral("/hidraw7"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}

void TestLcdWriter::writesLastReport()
{
    createNode();
    {
        LcdWriter writer(QStringLiteral("Test"), m_hid, m_dev);
        writer.submit("first");
        writer.submit("second");
    }   // the destructor writes what is pending before the thread ends
    // Either both reports or only the newest was written, never an older one last.
    QVERIFY2(node().endsWith("second"), node().constData());
}

void TestLcdWriter::nodeAppearsLater()
{
    LcdWriter writer(QStringLiteral("Test"), m_hid, m_dev);
    writer.submit("lost");   // no hidraw node yet: nothing written, no crash
    QTest::qWait(50);
    createNode();
    writer.submit("found");
    QTRY_COMPARE(node(), QByteArray("found"));
}

QTEST_GUILESS_MAIN(TestLcdWriter)
#include "tst_lcdwriter.moc"
