#pragma once
#include "protocol.h"
#include "keyvault.h"
#include <QBluetoothDeviceDiscoveryAgent>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QUrl>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList meters READ meters NOTIFY changed)
    Q_PROPERTY(QVariantList protocols READ protocols NOTIFY protocolsChanged)
    Q_PROPERTY(QVariantList captures READ captures NOTIFY capturesChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool demo READ demo WRITE setDemo NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(int packetCount READ packetCount NOTIFY changed)
    Q_PROPERTY(QString keyStorageInfo READ keyStorageInfo CONSTANT)
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;
    QVariantList meters() const;
    QVariantList protocols() const;
    QVariantList captures() const { return m_captures; }
    bool scanning() const { return m_demo ? demoTimer.isActive() : agent.isActive(); }
    bool demo() const { return m_demo; }
    QString status() const { return m_status; }
    int packetCount() const { return m_packetCount; }
    QString keyStorageInfo() const;
    void setDemo(bool value);
    Q_INVOKABLE void toggleScan();
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE QString protocolText(const QString &id) const;
    Q_INVOKABLE QString templateText() const;
    Q_INVOKABLE QString validateProtocol(const QString &text) const;
    Q_INVOKABLE QString saveProtocol(const QString &text);
    Q_INVOKABLE QString deleteProtocol(const QString &id);
    Q_INVOKABLE QString importProtocol(const QUrl &url);
    Q_INVOKABLE QVariantMap readProtocol(const QUrl &url) const;
    Q_INVOKABLE QString exportProtocol(const QString &text, const QUrl &url);
    Q_INVOKABLE QString testPacket(const QString &text, const QString &hex, const QString &keyHex) const;
    Q_INVOKABLE QString saveKey(const QString &id, const QString &hex);
    Q_INVOKABLE QString forgetKey(const QString &id);
    Q_INVOKABLE QString renameMeter(const QString &id, const QString &name);
    Q_INVOKABLE QString exportReadings(const QUrl &url) const;
    Q_INVOKABLE void clearCaptures();
    void applicationActive(bool active);
signals:
    void changed();
    void stateChanged();
    void protocolsChanged();
    void capturesChanged();
private:
    friend class BackendTests;
    QBluetoothDeviceDiscoveryAgent agent;
    QTimer demoTimer;
    QTimer ageTimer;
    QTimer saveTimer;
    QVector<Protocol> m_protocols;
    QVariantList m_meters;
    QVariantList m_captures;
    KeyVault vault;
    bool m_demo = false;
    bool wantedScan = false;
    bool isForeground = true;
    bool permissionPending = false;
    bool dirty = false;
    QString m_status = "Готов к приёму";
    QString storage;
    QString storageError;
    int m_packetCount = 0;
    quint32 demoCounter = 0;
    void discovered(const QBluetoothDeviceInfo &info);
    void ingest(const QString &source, int company, const QString &service, const QByteArray &packet, int rssi, const QString &address);
    void demoTick();
    void load();
    bool persist();
    bool writeProtocols(const QVector<Protocol> &items);
    void setStatus(const QString &message);
    int meterIndex(const QString &id) const;
    void retryMeter(int index);
    static QString path(const QUrl &url);
};
