// gkeyd - user session daemon for Logitech G keyboards driven by hid-lg-g15.
//
// Milestone 1: only loads and validates the model files. Device handling
// follows in milestone 2 (see docs/CONCEPT.md section 13).

#include "ModelRegistry.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

namespace {

int listModels(const ModelRegistry &registry)
{
    QTextStream out(stdout);
    for (const DeviceModel &model : registry.models()) {
        QStringList ids;
        for (const DeviceModel::UsbId &usb : model.usbIds())
            ids.append(QStringLiteral("%1:%2").arg(usb.vendor, 4, 16, QChar(u'0'))
                                              .arg(usb.product, 4, 16, QChar(u'0')));
        out << model.id() << '\t' << model.name() << '\t' << ids.join(u',')
            << (model.tested() ? "" : "\t(untested)") << '\n';
    }
    return registry.errors().isEmpty() ? 0 : 1;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("gkeyd"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GKEY_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Daemon for the gaming keys of Logitech G keyboards (hid-lg-g15)."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listOption(QStringLiteral("list-models"),
                                        QStringLiteral("List the supported models and exit."));
    parser.addOption(listOption);
    parser.process(app);

    ModelRegistry registry;
    registry.load();
    for (const QString &error : registry.errors())
        qWarning("Invalid model file: %s", qPrintable(error));

    if (parser.isSet(listOption))
        return listModels(registry);

    if (registry.models().isEmpty()) {
        qCritical("No model files found in: %s",
                  qPrintable(ModelRegistry::defaultSearchPaths().join(QStringLiteral(", "))));
        return 1;
    }

    qInfo("gkeyd %s: %lld model(s) loaded. Device handling is not implemented yet.",
          GKEY_VERSION, static_cast<long long>(registry.models().size()));
    return 0;
}
