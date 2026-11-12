#pragma once
#include "protocol.h"
#include "keyvault.h"
#include <QBluetoothDeviceDiscoveryAgent>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>

// Центральная модель приложения, доступная QML под именем backend.
// Все изменения карточек выполняются в потоке Qt: Android передаёт данные
// через ограниченную очередь, которую этот объект периодически опрашивает.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList meters READ meters NOTIFY changed)
    Q_PROPERTY(QVariantList captures READ captures NOTIFY capturesChanged)
    Q_PROPERTY(QVariantList latestCaptures READ latestCaptures NOTIFY capturesChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
    Q_PROPERTY(bool demo READ demo WRITE setDemo NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    // Счётчик имеет собственный сигнал: посторонний эфир не должен заставлять
    // QML заново строить список приборов при каждом событии Bluetooth.
    Q_PROPERTY(int packetCount READ packetCount NOTIFY packetCountChanged)
    Q_PROPERTY(QString keyStorageInfo READ keyStorageInfo CONSTANT)
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;
    QVariantList meters() const;
    QVariantList captures() const { return m_captures; }
    QVariantList latestCaptures() const;
    bool scanning() const;
    bool demo() const { return m_demo; }
    QString status() const { return m_status; }
    int packetCount() const { return m_packetCount; }
    QString keyStorageInfo() const;
    void setDemo(bool value);
    Q_INVOKABLE void toggleScan();
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    // Методы возвращают пустую строку при успехе, иначе — сообщение для интерфейса.
    // Управления каталогом протоколов здесь нет: он изменяется только разработчиком.
    Q_INVOKABLE QString saveKey(const QString &id, const QString &hex);
    Q_INVOKABLE QString forgetKey(const QString &id);
    Q_INVOKABLE QString renameMeter(const QString &id, const QString &name);
    Q_INVOKABLE QString deleteMeter(const QString &id);
    Q_INVOKABLE QString exportReadings(const QUrl &url) const;
    Q_INVOKABLE QUrl suggestedExportFile() const;
    Q_INVOKABLE void clearCaptures();
    void applicationActive(bool active);
signals:
    void changed();
    void stateChanged();
    void capturesChanged();
    void packetCountChanged();
private:
    friend class BackendTests;
    friend class EtoUiTests;
    QBluetoothDeviceDiscoveryAgent agent;
    // Назначения таймеров разделены: генерация демо, возраст карточек, запись
    // показаний на диск и ограничение частоты уведомлений интерфейса.
    QTimer demoTimer;
    QTimer ageTimer;
    QTimer saveTimer;
    QTimer uiTimer;
#ifdef Q_OS_ANDROID
    QTimer androidScanTimer;
    bool androidScanning = false;
    void pollAndroidScan();
    void stopAndroidScan();
#endif
    QVector<Protocol> m_protocols;
    QVariantList m_meters;
    // Общая история ограничена 200 событиями. Отдельный кэш хранит последний
    // пакет до 1024 Company ID / Service UUID, чтобы редкий ID не терялся в шуме.
    QVariantList m_captures;
    QHash<QString,QVariantMap> m_latestCaptures;
    quint64 captureSequence = 0;
    bool capturesPending = false;
    bool metersPending = false;
    KeyVault vault;
    bool m_demo = false;
    // Намерение пользователя слушать эфир сохраняется при сворачивании,
    // хотя фактический сканер в фоне останавливается.
    bool wantedScan = false;
    bool isForeground = true;
    bool permissionPending = false;
    // dirty означает, что показания изменились после последнего сохранения.
    // storageError запрещает автоматически перезаписывать повреждённое хранилище.
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
    void setStatus(const QString &message);
    int meterIndex(const QString &id) const;
    void retryMeter(int index);
    void flushUi();
    void processScanBatch(const QByteArray &json);
};
