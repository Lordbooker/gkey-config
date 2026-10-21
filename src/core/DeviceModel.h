#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>

#include <expected>
#include <optional>

// Description of one supported device model, loaded from a model file
// (data/models/<id>.json, see docs/CONCEPT.md section 4).
//
// The model maps the kernel's event codes to stable key IDs ("G1", "L1",
// "M1", ...) that are used in profiles, and describes LEDs, stick, LCD and the
// clickable areas of the device image in the GUI.
class DeviceModel {
public:
    struct UsbId {
        quint16 vendor = 0;
        quint16 product = 0;
    };

    // Fixed function of a key. Keys with role Bindable can be bound in profiles.
    enum class KeyRole {
        Bindable,
        Mode1,
        Mode2,
        Mode3,
        Record,
        Hardware,   // handled by the device itself (e.g. G13 light button), never bound
    };

    struct Key {
        QString id;         // key ID used in profiles, e.g. "G1"
        QString codeName;   // evdev name, e.g. "KEY_MACRO1"
        int     code = 0;   // numeric evdev code
        QString input;      // input role the key comes from ("keys" or "stick")
        KeyRole role = KeyRole::Bindable;
    };

    enum class LedType {
        OnOff,   // brightness 0/1
        Level,   // brightness 0..max_brightness
        Rgb,     // multicolor LED (multi_intensity + brightness)
    };

    struct Led {
        QString role;     // "backlight", "mode1", "mode2", "mode3", "record"
        QString suffix;   // end of the LED class name, e.g. "macro_preset_1"
        LedType type = LedType::OnOff;
    };

    struct Stick {
        QString input;     // input role of the stick device
        int     axisX = 0;
        int     axisY = 0;
        int     minimum = 0;
        int     maximum = 255;
    };

    struct Lcd {
        QString type;      // protocol, e.g. "g13-hidraw"
        int     width = 0;
        int     height = 0;
    };

    enum class AreaShape { Rect, Ellipse };

    // Clickable area of a key in the device image (image coordinates).
    struct Area {
        QString   key;
        AreaShape shape = AreaShape::Rect;
        double    x = 0;
        double    y = 0;
        double    width = 0;
        double    height = 0;
    };

    // Key IDs of the four stick directions; they exist when the model has a stick.
    static const QList<QString> &stickDirectionKeys();

    // Parses and validates a model file. sourceName is only used in error texts.
    static std::expected<DeviceModel, QString> fromJson(const QByteArray &json,
                                                        const QString &sourceName);

    const QString &id() const { return m_id; }
    const QString &name() const { return m_name; }
    bool tested() const { return m_tested; }
    const QString &minKernel() const { return m_minKernel; }
    const QList<UsbId> &usbIds() const { return m_usbIds; }
    const QMap<QString, QString> &inputs() const { return m_inputs; }
    const QList<Key> &keys() const { return m_keys; }
    const QList<Led> &leds() const { return m_leds; }
    const std::optional<Stick> &stick() const { return m_stick; }
    const std::optional<Lcd> &lcd() const { return m_lcd; }
    const QString &image() const { return m_image; }
    const QList<Area> &areas() const { return m_areas; }

    bool matchesUsb(quint16 vendor, quint16 product) const;

    // Key that the given input role reports with the given evdev code; nullptr if none.
    const Key *keyForCode(const QString &inputRole, int code) const;

    // Key with the given key ID; nullptr if none.
    const Key *keyById(const QString &id) const;

    // LED with the given role; nullptr if none.
    const Led *ledByRole(const QString &role) const;

    // All key IDs that can be bound in profiles (bindable keys and stick directions).
    QList<QString> bindableKeyIds() const;

private:
    QString m_id;
    QString m_name;
    bool m_tested = false;
    QString m_minKernel;
    QList<UsbId> m_usbIds;
    QMap<QString, QString> m_inputs;   // input role -> kernel input device name
    QList<Key> m_keys;
    QList<Led> m_leds;
    std::optional<Stick> m_stick;
    std::optional<Lcd> m_lcd;
    QString m_image;
    QList<Area> m_areas;
};
