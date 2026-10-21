#include "DeviceModel.h"

#include "EventCodes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

namespace {

// Error collector: the first error ends parsing; it is reported with the file name.
class ModelError {
public:
    explicit ModelError(const QString &source) : m_source(source) {}

    std::unexpected<QString> operator()(const QString &message) const
    {
        return std::unexpected(QStringLiteral("%1: %2").arg(m_source, message));
    }

private:
    QString m_source;
};

std::optional<quint16> parseHex16(const QJsonValue &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{4}$"));
    const QString text = value.toString();
    if (!pattern.match(text).hasMatch())
        return std::nullopt;
    return static_cast<quint16>(text.toUShort(nullptr, 16));
}

std::optional<DeviceModel::KeyRole> parseKeyRole(const QString &text)
{
    if (text.isEmpty() || text == u"bindable")
        return DeviceModel::KeyRole::Bindable;
    if (text == u"mode1")
        return DeviceModel::KeyRole::Mode1;
    if (text == u"mode2")
        return DeviceModel::KeyRole::Mode2;
    if (text == u"mode3")
        return DeviceModel::KeyRole::Mode3;
    if (text == u"record")
        return DeviceModel::KeyRole::Record;
    if (text == u"hardware")
        return DeviceModel::KeyRole::Hardware;
    return std::nullopt;
}

std::optional<DeviceModel::LedType> parseLedType(const QString &text)
{
    if (text.isEmpty() || text == u"onoff")
        return DeviceModel::LedType::OnOff;
    if (text == u"level")
        return DeviceModel::LedType::Level;
    if (text == u"rgb")
        return DeviceModel::LedType::Rgb;
    return std::nullopt;
}

const QSet<QString> &knownLedRoles()
{
    static const QSet<QString> roles = {
        QStringLiteral("backlight"), QStringLiteral("mode1"), QStringLiteral("mode2"),
        QStringLiteral("mode3"), QStringLiteral("record"),
    };
    return roles;
}

}

const QList<QString> &DeviceModel::stickDirectionKeys()
{
    static const QList<QString> keys = {
        QStringLiteral("STICK_UP"), QStringLiteral("STICK_DOWN"),
        QStringLiteral("STICK_LEFT"), QStringLiteral("STICK_RIGHT"),
    };
    return keys;
}

std::expected<DeviceModel, QString> DeviceModel::fromJson(const QByteArray &json,
                                                          const QString &sourceName)
{
    const ModelError error(sourceName);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return error(QStringLiteral("invalid JSON at offset %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString()));
    if (!document.isObject())
        return error(QStringLiteral("top level must be an object"));
    const QJsonObject root = document.object();

    DeviceModel model;

    // --- Identity -------------------------------------------------------------
    static const QRegularExpression idPattern(QStringLiteral("^[a-z0-9][a-z0-9-]*$"));
    model.m_id = root.value(u"id").toString();
    if (!idPattern.match(model.m_id).hasMatch())
        return error(QStringLiteral("\"id\" must be lower case letters, digits and '-'"));
    model.m_name = root.value(u"name").toString();
    if (model.m_name.isEmpty())
        return error(QStringLiteral("\"name\" is missing"));
    model.m_tested = root.value(u"tested").toBool(false);
    model.m_minKernel = root.value(u"minKernel").toString();

    // --- USB IDs ----------------------------------------------------------------
    const QJsonArray usb = root.value(u"usb").toArray();
    if (usb.isEmpty())
        return error(QStringLiteral("\"usb\" must list at least one vendor/product pair"));
    for (const QJsonValue &entry : usb) {
        const auto vendor = parseHex16(entry.toObject().value(u"vendor"));
        const auto product = parseHex16(entry.toObject().value(u"product"));
        if (!vendor || !product)
            return error(QStringLiteral("\"usb\" entries need 4-digit hex \"vendor\" and \"product\""));
        model.m_usbIds.append({*vendor, *product});
    }

    // --- Input devices ----------------------------------------------------------
    const QJsonObject inputs = root.value(u"inputs").toObject();
    for (auto it = inputs.begin(); it != inputs.end(); ++it) {
        const QString deviceName = it.value().toString();
        if (deviceName.isEmpty())
            return error(QStringLiteral("input \"%1\" needs a device name").arg(it.key()));
        model.m_inputs.insert(it.key(), deviceName);
    }
    if (!model.m_inputs.contains(QStringLiteral("keys")))
        return error(QStringLiteral("\"inputs\" must contain \"keys\""));

    // --- Keys -------------------------------------------------------------------
    const QJsonArray keys = root.value(u"keys").toArray();
    if (keys.isEmpty())
        return error(QStringLiteral("\"keys\" is empty"));
    QSet<QString> keyIds;
    QSet<QString> inputCodes;   // "<input>/<code>" to detect duplicate mappings
    for (const QJsonValue &entry : keys) {
        const QJsonObject object = entry.toObject();
        Key key;
        key.id = object.value(u"id").toString();
        key.codeName = object.value(u"code").toString();
        key.input = object.value(u"input").toString(QStringLiteral("keys"));

        if (key.id.isEmpty())
            return error(QStringLiteral("a key without \"id\""));
        if (keyIds.contains(key.id))
            return error(QStringLiteral("duplicate key id \"%1\"").arg(key.id));
        if (stickDirectionKeys().contains(key.id))
            return error(QStringLiteral("key id \"%1\" is reserved for the stick directions").arg(key.id));
        const auto code = EventCodes::keyCode(key.codeName);
        if (!code)
            return error(QStringLiteral("key \"%1\": unknown event code \"%2\"").arg(key.id, key.codeName));
        key.code = *code;
        if (!model.m_inputs.contains(key.input))
            return error(QStringLiteral("key \"%1\": unknown input \"%2\"").arg(key.id, key.input));
        const QString inputCode = key.input + u'/' + QString::number(key.code);
        if (inputCodes.contains(inputCode))
            return error(QStringLiteral("key \"%1\": code %2 is already mapped for input \"%3\"")
                             .arg(key.id, key.codeName, key.input));
        const auto role = parseKeyRole(object.value(u"role").toString());
        if (!role)
            return error(QStringLiteral("key \"%1\": unknown role \"%2\"")
                             .arg(key.id, object.value(u"role").toString()));
        key.role = *role;

        keyIds.insert(key.id);
        inputCodes.insert(inputCode);
        model.m_keys.append(key);
    }

    // --- LEDs -------------------------------------------------------------------
    const QJsonObject leds = root.value(u"leds").toObject();
    for (auto it = leds.begin(); it != leds.end(); ++it) {
        if (!knownLedRoles().contains(it.key()))
            return error(QStringLiteral("unknown LED role \"%1\"").arg(it.key()));
        const QJsonObject object = it.value().toObject();
        Led led;
        led.role = it.key();
        led.suffix = object.value(u"suffix").toString();
        if (led.suffix.isEmpty())
            return error(QStringLiteral("LED \"%1\" needs a \"suffix\"").arg(led.role));
        const auto type = parseLedType(object.value(u"type").toString());
        if (!type)
            return error(QStringLiteral("LED \"%1\": unknown type \"%2\"")
                             .arg(led.role, object.value(u"type").toString()));
        led.type = *type;
        model.m_leds.append(led);
    }

    // --- Stick ------------------------------------------------------------------
    if (root.contains(u"stick")) {
        const QJsonObject object = root.value(u"stick").toObject();
        Stick stick;
        stick.input = object.value(u"input").toString();
        if (!model.m_inputs.contains(stick.input))
            return error(QStringLiteral("stick: unknown input \"%1\"").arg(stick.input));
        const auto axisX = EventCodes::absCode(object.value(u"axisX").toString());
        const auto axisY = EventCodes::absCode(object.value(u"axisY").toString());
        if (!axisX || !axisY)
            return error(QStringLiteral("stick: \"axisX\" and \"axisY\" must be ABS_* names"));
        stick.axisX = *axisX;
        stick.axisY = *axisY;
        stick.minimum = object.value(u"min").toInt(0);
        stick.maximum = object.value(u"max").toInt(255);
        if (stick.minimum >= stick.maximum)
            return error(QStringLiteral("stick: \"min\" must be lower than \"max\""));
        model.m_stick = stick;
    }

    // --- LCD --------------------------------------------------------------------
    if (root.contains(u"lcd")) {
        const QJsonObject object = root.value(u"lcd").toObject();
        Lcd lcd;
        lcd.type = object.value(u"type").toString();
        lcd.width = object.value(u"width").toInt();
        lcd.height = object.value(u"height").toInt();
        if (lcd.type.isEmpty() || lcd.width <= 0 || lcd.height <= 0)
            return error(QStringLiteral("lcd: needs \"type\", \"width\" and \"height\""));
        model.m_lcd = lcd;
    }

    // --- Image and clickable areas ----------------------------------------------
    model.m_image = root.value(u"image").toString();
    const QList<QString> bindableIds = model.bindableKeyIds();
    const QSet<QString> bindable(bindableIds.begin(), bindableIds.end());
    for (const QJsonValue &entry : root.value(u"areas").toArray()) {
        const QJsonObject object = entry.toObject();
        Area area;
        area.key = object.value(u"key").toString();
        if (!keyIds.contains(area.key) && !bindable.contains(area.key))
            return error(QStringLiteral("area for unknown key \"%1\"").arg(area.key));
        const QString shape = object.value(u"shape").toString();
        if (shape == u"rect")
            area.shape = AreaShape::Rect;
        else if (shape == u"ellipse")
            area.shape = AreaShape::Ellipse;
        else
            return error(QStringLiteral("area \"%1\": shape must be \"rect\" or \"ellipse\"").arg(area.key));
        area.x = object.value(u"x").toDouble();
        area.y = object.value(u"y").toDouble();
        area.width = object.value(u"w").toDouble();
        area.height = object.value(u"h").toDouble();
        if (area.width <= 0 || area.height <= 0)
            return error(QStringLiteral("area \"%1\": \"w\" and \"h\" must be positive").arg(area.key));
        model.m_areas.append(area);
    }

    return model;
}

bool DeviceModel::matchesUsb(quint16 vendor, quint16 product) const
{
    for (const UsbId &id : m_usbIds) {
        if (id.vendor == vendor && id.product == product)
            return true;
    }
    return false;
}

const DeviceModel::Key *DeviceModel::keyForCode(const QString &inputRole, int code) const
{
    for (const Key &key : m_keys) {
        if (key.code == code && key.input == inputRole)
            return &key;
    }
    return nullptr;
}

const DeviceModel::Key *DeviceModel::keyById(const QString &id) const
{
    for (const Key &key : m_keys) {
        if (key.id == id)
            return &key;
    }
    return nullptr;
}

const DeviceModel::Led *DeviceModel::ledByRole(const QString &role) const
{
    for (const Led &led : m_leds) {
        if (led.role == role)
            return &led;
    }
    return nullptr;
}

QList<QString> DeviceModel::bindableKeyIds() const
{
    QList<QString> ids;
    for (const Key &key : m_keys) {
        if (key.role == KeyRole::Bindable)
            ids.append(key.id);
    }
    if (m_stick)
        ids.append(stickDirectionKeys());
    return ids;
}
