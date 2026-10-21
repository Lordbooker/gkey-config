// gkey-config - configuration tool.
//
// Milestone 1: placeholder window that lists the loaded models. The real GUI
// follows in milestone 4 (see docs/CONCEPT.md section 13).

#include "ModelRegistry.h"

#include <QApplication>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gkey-config"));
    QApplication::setApplicationVersion(QStringLiteral(GKEY_VERSION));

    ModelRegistry registry;
    registry.load();

    QStringList lines;
    lines.append(QStringLiteral("gkey-config %1 - development version").arg(QStringLiteral(GKEY_VERSION)));
    lines.append(QString());
    lines.append(QStringLiteral("Supported models:"));
    for (const DeviceModel &model : registry.models())
        lines.append(QStringLiteral("  %1%2").arg(model.name(), model.tested() ? QString()
                                                                               : QStringLiteral(" (untested)")));
    for (const QString &error : registry.errors())
        lines.append(QStringLiteral("Invalid model file: %1").arg(error));

    QMainWindow window;
    auto *label = new QLabel(lines.join(u'\n'));
    label->setMargin(16);
    window.setCentralWidget(label);
    window.setWindowTitle(QStringLiteral("gkey-config"));
    window.show();

    return app.exec();
}
