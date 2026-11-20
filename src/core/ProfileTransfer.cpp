// SPDX-FileCopyrightText: 2026 Lordbooker
// SPDX-License-Identifier: GPL-2.0-only

#include "ProfileTransfer.h"

#include "MacroStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

namespace ProfileTransfer {

namespace {

QJsonObject toObject(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}

QByteArray toBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool sameMacro(const Macro &a, const Macro &b)
{
    return a.steps() == b.steps() && a.description() == b.description();
}

}

QByteArray exportProfile(const QString &modelId, const Profile &profile, const MacroStore &macros, QStringList *missing)
{
    QJsonArray macroArray;
    for (const QString &name : profile.macros()) {
        if (const Macro *macro = macros.macro(name))
            macroArray.append(toObject(macro->toJson()));
        else if (missing)
            missing->append(name);
    }
    const QJsonObject root{
        {QStringLiteral("gkeyConfigExport"), FormatVersion},
        {QStringLiteral("model"), modelId},
        {QStringLiteral("profile"), toObject(profile.toJson())},
        {QStringLiteral("macros"), macroArray},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::expected<Import, QString> parse(const QByteArray &data, const QString &fallbackModelId)
{
    if (data.size() > MaxFileSize)
        return std::unexpected(QStringLiteral("the file is larger than %1 MiB").arg(MaxFileMiB));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return std::unexpected(QStringLiteral("not valid JSON: %1").arg(parseError.errorString()));
    if (!document.isObject())
        return std::unexpected(QStringLiteral("not a profile"));
    const QJsonObject root = document.object();

    Import result;
    if (!root.contains(u"gkeyConfigExport")) {
        // A plain profile file.
        auto profile = Profile::fromJson(data);
        if (!profile)
            return std::unexpected(profile.error());
        result.modelId = fallbackModelId;
        result.profile = *profile;
        return result;
    }

    const int version = root.value(u"gkeyConfigExport").toInt(0);
    if (version < 1 || version > FormatVersion)
        return std::unexpected(QStringLiteral("export format %1 is not supported (this version reads %2)")
                                   .arg(root.value(u"gkeyConfigExport").toVariant().toString())
                                   .arg(FormatVersion));
    result.modelId = root.value(u"model").toString();
    if (result.modelId.isEmpty())
        return std::unexpected(QStringLiteral("the export names no device model"));
    auto profile = Profile::fromJson(toBytes(root.value(u"profile").toObject()));
    if (!profile)
        return std::unexpected(QStringLiteral("profile: %1").arg(profile.error()));
    result.profile = *profile;

    QSet<QString> names;
    const QJsonArray macroArray = root.value(u"macros").toArray();
    for (const auto &value : macroArray) {
        auto macro = Macro::fromJson(toBytes(value.toObject()));
        if (!macro)
            return std::unexpected(QStringLiteral("macro: %1").arg(macro.error()));
        if (names.contains(macro->name()))
            return std::unexpected(QStringLiteral("macro \"%1\" appears twice").arg(macro->name()));
        names.insert(macro->name());
        result.macros.append(*macro);
    }
    return result;
}

QString uniqueName(const QString &name, const std::function<bool(const QString &)> &taken)
{
    if (!taken(name))
        return name;
    for (int number = 2;; ++number) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(name).arg(number);
        if (!taken(candidate))
            return candidate;
    }
}

MacroPlan planMacros(const QList<Macro> &imported, const MacroStore &macros)
{
    QSet<QString> inFile;
    for (const Macro &macro : imported)
        inFile.insert(macro.name());

    MacroPlan plan;
    QSet<QString> assigned;
    for (const Macro &macro : imported) {
        const Macro *existing = macros.macro(macro.name());
        if (!existing) {
            plan.save.append(macro);
        } else if (sameMacro(*existing, macro)) {
            plan.reused.append(macro.name());
        } else {
            // A name that neither exists nor is used in the file: renaming the
            // profile's bindings one after the other cannot mix them up.
            const QString name = uniqueName(macro.name(), [&](const QString &candidate) {
                return macros.macro(candidate) || inFile.contains(candidate) || assigned.contains(candidate);
            });
            assigned.insert(name);
            Macro copy = macro;
            copy.setName(name);
            plan.save.append(copy);
            plan.renamed.insert(macro.name(), name);
        }
    }
    return plan;
}

bool applyMacros(const MacroPlan &plan, MacroStore &macros, Profile &profile, QString *error)
{
    for (const Macro &macro : plan.save) {
        if (!macros.save(macro, error))
            return false;
    }
    for (auto it = plan.renamed.constBegin(); it != plan.renamed.constEnd(); ++it)
        profile.renameMacro(it.key(), it.value());
    return true;
}

}
