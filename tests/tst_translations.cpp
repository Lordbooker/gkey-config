// Checks that every GUI language is complete: lupdate extracts the current
// texts from the sources, and each translations/gkey-config_<lang>.ts must
// translate all of them (finished, same placeholders and accelerators) and
// nothing else.

#include <QDir>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

namespace {

struct Message {
    QString translation;
    QString type; // "", "unfinished", "vanished", "obsolete"
};

// Key: context + '\n' + source (+ '\n' + comment).
QHash<QString, Message> readTs(const QString &path, QString *error)
{
    QHash<QString, Message> messages;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return messages;
    }
    QXmlStreamReader xml(&file);
    QString context, source, comment;
    Message message;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const auto name = xml.name();
            if (name == u"context") {
                context.clear();
            } else if (name == u"name") {
                context = xml.readElementText();
            } else if (name == u"message") {
                source.clear();
                comment.clear();
                message = {};
            } else if (name == u"source") {
                source = xml.readElementText();
            } else if (name == u"comment") {
                comment = xml.readElementText();
            } else if (name == u"translation") {
                message.type = xml.attributes().value(QLatin1String("type")).toString();
                message.translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
        } else if (xml.isEndElement() && xml.name() == u"message") {
            QString key = context + u'\n' + source;
            if (!comment.isEmpty())
                key += u'\n' + comment;
            messages.insert(key, message);
        }
    }
    if (xml.hasError())
        *error = xml.errorString();
    return messages;
}

QStringList placeholders(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("%\\d+|%n"));
    QStringList result;
    for (auto it = pattern.globalMatch(text); it.hasNext();)
        result.append(it.next().captured());
    result.sort();
    return result;
}

// Menu accelerators ("&File"); "&&" is a literal ampersand, entities do not count.
int accelerators(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("&(?![a-zA-Z#][a-zA-Z0-9]*;)(?!&)"));
    QString plain = text;
    plain.replace(QStringLiteral("&&"), QString());
    return int(plain.count(pattern));
}

QString shortKey(const QString &key)
{
    return QString(key).replace(u'\n', QStringLiteral(" / "));
}

}

class TestTranslations : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void complete_data();
    void complete();

private:
    QTemporaryDir m_dir;
    QHash<QString, Message> m_reference;
};

void TestTranslations::initTestCase()
{
    QVERIFY(m_dir.isValid());
    const QString reference = m_dir.filePath(QStringLiteral("reference.ts"));
    QProcess lupdate;
    lupdate.setProcessChannelMode(QProcess::MergedChannels);
    lupdate.start(QStringLiteral(GKEY_LUPDATE),
                  {QStringLiteral("-silent"), QStringLiteral("-no-obsolete"), QStringLiteral("-locations"),
                   QStringLiteral("none"), QStringLiteral(GKEY_GUI_SOURCE_DIR), QStringLiteral("-ts"), reference});
    QVERIFY2(lupdate.waitForFinished(60000), "lupdate did not finish");
    const QByteArray output = lupdate.readAll();
    // lupdate itself cannot start (e.g. on Arch without qt6-declarative):
    // that is a missing tool, not a translation problem.
    if (lupdate.exitStatus() != QProcess::NormalExit || lupdate.exitCode() == 127
        || output.contains("error while loading shared libraries"))
        QSKIP(qPrintable(QStringLiteral("lupdate cannot run: ") + QString::fromLocal8Bit(output).trimmed()));
    QVERIFY2(lupdate.exitCode() == 0, output.constData());
    QString error;
    m_reference = readTs(reference, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(m_reference.size() > 100, "lupdate found suspiciously few texts");
}

void TestTranslations::complete_data()
{
    QTest::addColumn<QString>("path");
    const QDir dir(QStringLiteral(GKEY_TS_DIR));
    const QStringList files = dir.entryList({QStringLiteral("gkey-config_*.ts")}, QDir::Files, QDir::Name);
    QVERIFY(!files.isEmpty());
    for (const QString &file : files)
        QTest::newRow(qPrintable(file)) << dir.filePath(file);
}

void TestTranslations::complete()
{
    QFETCH(QString, path);
    QString error;
    const QHash<QString, Message> messages = readTs(path, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QStringList problems;
    for (auto it = m_reference.constBegin(); it != m_reference.constEnd(); ++it) {
        const QString source = it.key().section(u'\n', 1, 1);
        const auto found = messages.constFind(it.key());
        if (found == messages.constEnd()) {
            problems << QStringLiteral("missing (run update_translations): ") + shortKey(it.key());
            continue;
        }
        const Message &message = found.value();
        if (!message.type.isEmpty() || message.translation.isEmpty()) {
            problems << QStringLiteral("unfinished: ") + shortKey(it.key());
            continue;
        }
        if (placeholders(source) != placeholders(message.translation))
            problems << QStringLiteral("placeholders differ: ") + shortKey(it.key());
        if (accelerators(source) != accelerators(message.translation))
            problems << QStringLiteral("accelerator (&) differs: ") + shortKey(it.key());
    }
    for (auto it = messages.constBegin(); it != messages.constEnd(); ++it) {
        if (!m_reference.contains(it.key()))
            problems << QStringLiteral("obsolete (run update_translations): ") + shortKey(it.key());
    }
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(u'\n')));
}

QTEST_GUILESS_MAIN(TestTranslations)
#include "tst_translations.moc"
