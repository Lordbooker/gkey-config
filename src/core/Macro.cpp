#include "Macro.h"

#include "EventCodes.h"
#include "OutputCodes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace {

std::expected<int, QString> outputCode(const QString &name)
{
    const auto code = EventCodes::keyCode(name);
    if (!code)
        return std::unexpected(QStringLiteral("unknown key \"%1\"").arg(name));
    if (!OutputCodes::isKeyboardKey(*code) && !OutputCodes::isMouseButton(*code))
        return std::unexpected(QStringLiteral("\"%1\" is neither a keyboard key nor a mouse button").arg(name));
    return *code;
}

}

std::expected<Macro, QString> Macro::fromJson(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return std::unexpected(QStringLiteral("invalid JSON at offset %1: %2")
                                   .arg(parseError.offset)
                                   .arg(parseError.errorString()));
    if (!document.isObject())
        return std::unexpected(QStringLiteral("top level must be an object"));
    QJsonObject root = document.object();

    Macro macro;
    macro.m_name = root.take(QStringLiteral("name")).toString().trimmed();
    if (macro.m_name.isEmpty())
        return std::unexpected(QStringLiteral("\"name\" is missing"));
    macro.m_description = root.take(QStringLiteral("description")).toString();

    const QJsonArray steps = root.take(QStringLiteral("steps")).toArray();
    for (qsizetype i = 0; i < steps.size(); ++i) {
        const QJsonObject object = steps.at(i).toObject();
        const QString where = QStringLiteral("step %1: ").arg(i + 1);
        Step step;
        if (object.contains(u"delay")) {
            step.type = Step::Type::Delay;
            step.duration = object.value(u"delay").toInt(-1);
            if (step.duration < 0 || step.duration > MaxDelay)
                return std::unexpected(where + QStringLiteral("\"delay\" must be 0-%1 ms").arg(MaxDelay));
        } else {
            QString name;
            if (object.contains(u"press")) {
                step.type = Step::Type::Press;
                name = object.value(u"press").toString();
            } else if (object.contains(u"release")) {
                step.type = Step::Type::Release;
                name = object.value(u"release").toString();
            } else if (object.contains(u"tap")) {
                step.type = Step::Type::Tap;
                name = object.value(u"tap").toString();
                step.duration = object.value(u"hold").toInt(DefaultTapHold);
                if (step.duration < 0 || step.duration > MaxDelay)
                    return std::unexpected(where + QStringLiteral("\"hold\" must be 0-%1 ms").arg(MaxDelay));
            } else {
                return std::unexpected(where + QStringLiteral("needs \"press\", \"release\", \"tap\" or \"delay\""));
            }
            const auto code = outputCode(name);
            if (!code)
                return std::unexpected(where + code.error());
            step.code = *code;
        }
        macro.m_steps.append(step);
    }
    macro.m_unknown = root;
    return macro;
}

QByteArray Macro::toJson() const
{
    QJsonObject root = m_unknown;
    root.insert(QStringLiteral("name"), m_name);
    if (!m_description.isEmpty())
        root.insert(QStringLiteral("description"), m_description);
    QJsonArray steps;
    for (const Step &step : m_steps) {
        QJsonObject object;
        switch (step.type) {
        case Step::Type::Press:
            object.insert(QStringLiteral("press"), EventCodes::keyName(step.code));
            break;
        case Step::Type::Release:
            object.insert(QStringLiteral("release"), EventCodes::keyName(step.code));
            break;
        case Step::Type::Tap:
            object.insert(QStringLiteral("tap"), EventCodes::keyName(step.code));
            if (step.duration != DefaultTapHold)
                object.insert(QStringLiteral("hold"), step.duration);
            break;
        case Step::Type::Delay:
            object.insert(QStringLiteral("delay"), step.duration);
            break;
        }
        steps.append(object);
    }
    root.insert(QStringLiteral("steps"), steps);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
