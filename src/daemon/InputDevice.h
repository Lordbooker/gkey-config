#pragma once

#include <QObject>
#include <QString>

struct libevdev;
class QSocketNotifier;

// One evdev node of a physical device, read with an exclusive grab
// (EVIOCGRAB): its events reach only gkeyd, never the compositor.
class InputDevice : public QObject {
    Q_OBJECT
public:
    InputDevice(const QString &devNode, const QString &role, QObject *parent = nullptr);
    ~InputDevice() override;

    // Opens and grabs the node. Returns false and sets an error text on failure.
    bool open(QString *error);

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
    libevdev *m_device = nullptr;
    QSocketNotifier *m_notifier = nullptr;
};
