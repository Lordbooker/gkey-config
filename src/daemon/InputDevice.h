#pragma once

#include <QObject>
#include <QString>

struct libevdev;
class QSocketNotifier;

// One evdev node of a physical device, normally read with an exclusive grab
// (EVIOCGRAB): its events reach only gkeyd, never the compositor.
class InputDevice : public QObject {
    Q_OBJECT
public:
    InputDevice(const QString &devNode, const QString &role, QObject *parent = nullptr);
    ~InputDevice() override;

    // Opens the node and grabs it if grab is true. Returns false and sets an
    // error text on failure.
    bool open(QString *error, bool grab = true);

    // Exclusive access on/off (the stick in passthrough mode is not grabbed).
    bool setGrabbed(bool grabbed, QString *error = nullptr);
    bool isGrabbed() const { return m_grabbed; }

    // Current value of an absolute axis.
    int absValue(int code) const;

    const QString &devNode() const { return m_devNode; }
    const QString &role() const { return m_role; }

signals:
    void keyEvent(const QString &role, int code, int value);
    void absEvent(const QString &role, int code, int value);
    // The node is gone or unreadable; the device will be closed.
    void failed(const QString &devNode);

private:
    void onReadable();
    void close();

    QString m_devNode;
    QString m_role;
    int m_fd = -1;
    bool m_grabbed = false;
    libevdev *m_device = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};
