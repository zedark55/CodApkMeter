#include "backend.h"
#include "protocolcatalog.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QBluetoothPermission>
#include <QLocationPermission>
#include <QPermissions>
#include <QRandomGenerator>
#include <QtEndian>
#include <mbedtls/ccm.h>
#include <limits>
#ifdef Q_OS_ANDROID
#include <QOperatingSystemVersion>
#endif

namespace {
const QByteArray demoKey = QByteArray::fromHex("00112233445566778899aabbccddeeff");
QString when() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool writeJson(const QString &file, const QJsonDocument &doc) {
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) return false;
    QByteArray data=doc.toJson();
    if (out.write(data)!=data.size()) return false;
    if (!out.commit()) return false;
    QFile::setPermissions(file,QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}
bool writeSelected(const QUrl &url, const QByteArray &data) {
    if (!url.isLocalFile() && url.scheme()!="content") return false;
    QFile file(url.isLocalFile() ? url.toLocalFile() : url.toString());
    return file.open(QIODevice::WriteOnly|QIODevice::Truncate) && file.write(data)==data.size() && file.flush();
}
QByteArray demoPacket(int index, quint32 counter) {
    QByteArray header(10,0), plain(7,0), cipher(7,0), tag(4,0);
    header[0]=char(index==2 ? 0xa2 : 0xa1);
    qToBigEndian<quint16>(quint16(1001+index),header.data()+1);
    // A new random session prefix plus monotonically increasing counter: demo only.
    static quint16 session=quint16(QRandomGenerator::global()->generate());
    header[3]=char(index);
    qToBigEndian<quint16>(session,header.data()+4);
    qToBigEndian<quint32>(counter,header.data()+6);
    qToLittleEndian<quint32>((index==0 ? 128456 : index==1 ? 86312 : 248192) + counter*(index+1),plain.data());
    plain[4]=char(index==0 ? 94 : index==1 ? 76 : 100);
    qToLittleEndian<qint16>(index==0 ? 1840 : index==1 ? 5230 : 2460,plain.data()+5);
    mbedtls_ccm_context ctx;
    mbedtls_ccm_init(&ctx);
    int rc=mbedtls_ccm_setkey(&ctx,MBEDTLS_CIPHER_ID_AES,reinterpret_cast<const unsigned char *>(demoKey.constData()),128);
    if (!rc) rc=mbedtls_ccm_encrypt_and_tag(&ctx,plain.size(),
        reinterpret_cast<const unsigned char *>(header.constData()+3),7,
        reinterpret_cast<const unsigned char *>(header.constData()),header.size(),
        reinterpret_cast<const unsigned char *>(plain.constData()),reinterpret_cast<unsigned char *>(cipher.data()),
        reinterpret_cast<unsigned char *>(tag.data()),tag.size());
    mbedtls_ccm_free(&ctx);
    return rc ? QByteArray() : header+cipher+tag;
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    storage=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(storage)) storageError="Не удалось создать хранилище приложения";
    load();
    agent.setLowEnergyDiscoveryTimeout(0);
    connect(&agent,&QBluetoothDeviceDiscoveryAgent::deviceDiscovered,this,&Backend::discovered);
    connect(&agent,&QBluetoothDeviceDiscoveryAgent::deviceUpdated,this,[this](const QBluetoothDeviceInfo &info,QBluetoothDeviceInfo::Fields) { discovered(info); });
    connect(&agent,&QBluetoothDeviceDiscoveryAgent::errorOccurred,this,[this](QBluetoothDeviceDiscoveryAgent::Error e) {
        wantedScan=false;
        setStatus(e==QBluetoothDeviceDiscoveryAgent::PoweredOffError ? "Включите Bluetooth в настройках телефона" : "Ошибка Bluetooth: "+agent.errorString());
    });
    connect(&agent,&QBluetoothDeviceDiscoveryAgent::canceled,this,&Backend::stateChanged);
    connect(&agent,&QBluetoothDeviceDiscoveryAgent::finished,this,[this] {
        setStatus("Сканирование завершено"); wantedScan=false;
    });
    demoTimer.setInterval(4000);
    connect(&demoTimer,&QTimer::timeout,this,&Backend::demoTick);
    ageTimer.setInterval(10000);
    connect(&ageTimer,&QTimer::timeout,this,&Backend::changed);
    ageTimer.start();
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(700);
    connect(&saveTimer,&QTimer::timeout,this,[this] {
        if (!persist()) setStatus("Не удалось сохранить показания; проверьте хранилище");
        else dirty=false;
    });
    if (!storageError.isEmpty()) setStatus(storageError);
}
Backend::~Backend() { if (dirty) persist(); }

void Backend::setStatus(const QString &message) { m_status=message; emit stateChanged(); }
QString Backend::keyStorageInfo() const {
#ifdef Q_OS_ANDROID
    return "Ключи защищены Android Keystore и не попадают в экспорт. Резервное копирование приложения отключено.";
#else
    return "Настольная версия: ключи хранятся только до закрытия приложения. На Android используется Keystore.";
#endif
}

void Backend::load() {
    QString catalogError;
    if (!ProtocolCatalog::load(m_protocols,catalogError)) storageError=catalogError;
    // Legacy protocols.json is intentionally ignored and preserved on disk.
    // Protocols can now enter the application only through compiled resources.
    QFile metersFile(storage+"/meters.json");
    if (metersFile.exists()) {
        if (!metersFile.open(QIODevice::ReadOnly) || metersFile.size()>8*1024*1024) { storageError="Не удалось прочитать meters.json"; return; }
        QJsonParseError e;
        auto doc=QJsonDocument::fromJson(metersFile.readAll(),&e);
        if (e.error!=QJsonParseError::NoError || !doc.isArray() || doc.array().size()>200) { storageError="Повреждён meters.json; файл сохранён для восстановления"; return; }
        for (const auto &v : doc.array()) {
            auto m=v.toObject().toVariantMap();
            if (!m.value("id").toString().isEmpty() && !m.value("demo").toBool()) {
                bool supported=false;
                for (const auto &p : m_protocols) if (!p.id().startsWith("demo-") && p.id()==m.value("protocolId").toString()) supported=true;
                if (!supported) { m["valid"]=false; m["error"]="Протокол прибора не поддерживается этой версией приложения"; }
                m_meters.append(m);
            }
        }
    }
}

bool Backend::persist() {
    if (!storageError.isEmpty()) return false; // Never overwrite a corrupt user file automatically.
    QVariantList real;
    for (const auto &v : m_meters) if (!v.toMap().value("demo").toBool()) real.append(v);
    return writeJson(storage+"/meters.json",QJsonDocument(QJsonArray::fromVariantList(real)));
}
QVariantList Backend::meters() const {
    QVariantList list;
    for (const auto &v : m_meters) {
        auto m=v.toMap();
        if (m.value("demo").toBool()!=m_demo) continue;
        auto date=QDateTime::fromString(m.value("lastSeen").toString(),Qt::ISODateWithMs);
        qint64 age=qMax<qint64>(0,date.secsTo(QDateTime::currentDateTimeUtc()));
        m["age"]=age<60 ? "только что" : age<3600 ? QString::number(age/60)+" мин назад" : age<86400 ? QString::number(age/3600)+" ч назад" : QString::number(age/86400)+" дн назад";
        m["stale"]=age>180;
        list.append(m);
    }
    return list;
}
void Backend::setDemo(bool value) {
    if (value==m_demo) return;
    stopScan();
    m_demo=value;
    m_packetCount=0;
    m_captures.clear(); emit capturesChanged();
    setStatus(value ? "Демонстрация · синтетические данные" : "Готов к приёму");
    emit changed();
    if (value) startScan();
}
void Backend::toggleScan() { if (scanning() || wantedScan) stopScan(); else startScan(); }
void Backend::startScan() {
    if (scanning()) return;
    wantedScan=true;
    if (!isForeground) return;
    if (m_demo) { demoTimer.start(); demoTick(); setStatus("Демонстрация · синтетические данные"); return; }
    QBluetoothPermission bluetooth;
    bluetooth.setCommunicationModes(QBluetoothPermission::Access);
    if (qApp->checkPermission(bluetooth)!=Qt::PermissionStatus::Granted) {
        if (permissionPending) return;
        permissionPending=true;
        qApp->requestPermission(bluetooth,this,[this](const QPermission &permission) {
            permissionPending=false;
            if (!wantedScan || !isForeground) return;
            if (permission.status()==Qt::PermissionStatus::Granted) startScan();
            else { wantedScan=false; setStatus("Разрешите доступ к Bluetooth в настройках приложения"); }
        });
        return;
    }
#ifdef Q_OS_ANDROID
    if (QOperatingSystemVersion::current().majorVersion()<12) {
        QLocationPermission location;
        location.setAccuracy(QLocationPermission::Precise);
        if (qApp->checkPermission(location)!=Qt::PermissionStatus::Granted) {
            if (permissionPending) return;
            permissionPending=true;
            qApp->requestPermission(location,this,[this](const QPermission &permission) {
                permissionPending=false;
                if (!wantedScan || !isForeground) return;
                if (permission.status()==Qt::PermissionStatus::Granted) startScan();
                else { wantedScan=false; setStatus("Android 8–11 требует разрешение геолокации для BLE-сканирования"); }
            });
            return;
        }
    }
#endif
    agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    if (agent.isActive()) setStatus("Слушаем Bluetooth LE · без подключения");
    emit stateChanged();
}
void Backend::stopScan() {
    wantedScan=false;
    demoTimer.stop();
    if (agent.isActive()) agent.stop();
    setStatus("Приём приостановлен");
}
void Backend::applicationActive(bool active) {
    isForeground=active;
    if (!active && dirty) { if (persist()) dirty=false; else setStatus("Не удалось сохранить показания"); }
    if (!active && wantedScan) {
        demoTimer.stop();
        if (agent.isActive()) agent.stop();
        setStatus("Приём приостановлен до возврата в приложение");
    } else if (active && wantedScan) startScan();
}
void Backend::discovered(const QBluetoothDeviceInfo &info) {
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) return;
    const auto manufacturer=info.manufacturerData();
    for (auto it=manufacturer.cbegin();it!=manufacturer.cend();++it) ingest("manufacturer",it.key(),{},it.value(),info.rssi(),info.address().toString());
    const auto services=info.serviceData();
    for (auto it=services.cbegin();it!=services.cend();++it) ingest("service",-1,it.key().toString(),it.value(),info.rssi(),info.address().toString());
}
int Backend::meterIndex(const QString &id) const {
    for (int i=0;i<m_meters.size();i++) if (m_meters[i].toMap().value("id").toString()==id) return i;
    return -1;
}
void Backend::ingest(const QString &source, int company, const QString &service, const QByteArray &packet, int rssi, const QString &address) {
    if (packet.isEmpty() || packet.size()>1650) return;
    if (m_packetCount<std::numeric_limits<int>::max()) ++m_packetCount;
    QVector<const Protocol *> matches;
    for (const auto &p : m_protocols) {
        if (p.id().startsWith("demo-")!=m_demo) continue;
        if (p.matches(source,company,service,packet)) matches.append(&p);
    }
    const Protocol *protocol=matches.size()==1 ? matches.first() : nullptr;
    QVariantMap capture{{"time",QDateTime::currentDateTime().toString("HH:mm:ss")},{"source",source},
        {"company",company},{"service",service},{"hex",QString::fromLatin1(packet.toHex(' ')).toUpper()},
        {"rssi",rssi},{"address",address},{"protocol",protocol ? protocol->name() : matches.size()>1 ? "Конфликт протоколов" : "Неизвестный формат"}};
    m_captures.prepend(capture);
    if (m_captures.size()>200) m_captures.removeLast();
    emit capturesChanged();
    if (!protocol) { emit changed(); return; }
    auto serial=protocol->identity(packet);
    if (serial.isEmpty()) { emit changed(); return; }
    auto id=(m_demo ? "demo/" : "real/")+protocol->id()+"/"+serial;
    auto result=protocol->decode(packet,m_demo ? demoKey : vault.get(id));
    int index=meterIndex(id);
    if (index<0 && m_meters.size()>=200) { setStatus("Достигнут лимит: 200 приборов"); emit changed(); return; }
    QVariantMap meter=index<0 ? QVariantMap() : m_meters[index].toMap();
    if (meter.isEmpty()) meter={{"id",id},{"name",m_demo ? (serial=="03E9" ? "Холодная вода" : serial=="03EA" ? "Горячая вода" : "Электричество") : protocol->name()+" · "+serial},{"history",QVariantList()},{"hasReading",false}};
    meter["serial"]=serial;
    meter["protocolId"]=protocol->id(); meter["protocolName"]=protocol->name();
    meter["kind"]=protocol->json.value("kind").toString();
    meter["mode"]=protocol->json.value("encryption").toObject().value("mode").toString();
    meter["demo"]=m_demo; meter["lastSeen"]=when(); meter["rssi"]=rssi;
    meter["packet"]=QString::fromLatin1(packet.toHex());
    meter["error"]=result.error;
    meter["valid"]=result.ok;
    if (result.ok) {
        meter["hasReading"]=true; meter["reading"]=result.reading; meter["unit"]=result.unit;
        meter["fields"]=result.fields; meter["battery"]=result.battery; meter["lastReadingAt"]=when();
        auto history=meter.value("history").toList();
        // Avoid duplicate samples emitted by RSSI-only updates from Qt discovery.
        if (history.isEmpty() || history.last().toMap().value("value").toDouble()!=result.reading) history.append(QVariantMap{{"time",when()},{"value",result.reading}});
        while (history.size()>80) history.removeFirst();
        meter["history"]=history;
    }
    if (index<0) m_meters.append(meter); else m_meters[index]=meter;
    if (!m_demo) { dirty=true; if (!saveTimer.isActive()) saveTimer.start(); }
    emit changed();
}
void Backend::demoTick() {
    ++demoCounter;
    for (int i=0;i<3;i++) ingest("manufacturer",65535,{},demoPacket(i,demoCounter),-48-i*9-((demoCounter+i)%7),"DEMO");
}

void Backend::retryMeter(int index) {
    auto m=m_meters[index].toMap();
    for (const auto &p : m_protocols) if (p.id()==m.value("protocolId").toString()) {
        auto result=p.decode(QByteArray::fromHex(m.value("packet").toString().toLatin1()),vault.get(m.value("id").toString()));
        m["valid"]=result.ok; m["error"]=result.error;
        if (result.ok) {
            m["hasReading"]=true; m["reading"]=result.reading; m["unit"]=result.unit; m["fields"]=result.fields; m["battery"]=result.battery;
            m["lastReadingAt"]=m.value("lastSeen");
        }
        m_meters[index]=m;
        if (!persist()) setStatus("Ошибка сохранения показаний");
        emit changed(); break;
    }
}
QString Backend::saveKey(const QString &id, const QString &hex) {
    int index=meterIndex(id);
    if (index<0 || m_meters[index].toMap().value("demo").toBool()) return "Выберите реальный прибор";
    QByteArray key;
    if (!Protocol::hex(hex,key) || key.size()!=16) return "Ключ AES-128: ровно 32 HEX-символа";
    if (!vault.put(id,key)) return "Не удалось сохранить ключ в защищённом хранилище";
    retryMeter(index); return {};
}
QString Backend::forgetKey(const QString &id) {
    if (!vault.remove(id)) return "Не удалось удалить ключ";
    int index=meterIndex(id);
    if (index>=0) retryMeter(index);
    return {};
}
QString Backend::renameMeter(const QString &id, const QString &name) {
    int index=meterIndex(id);
    if (index<0) return "Прибор не найден";
    if (name.trimmed().isEmpty() || name.size()>80) return "Название: 1–80 символов";
    auto old=m_meters[index]; auto m=old.toMap(); m["name"]=name.trimmed(); m_meters[index]=m;
    if (!m_demo && !persist()) { m_meters[index]=old; return "Не удалось сохранить название"; }
    emit changed(); return {};
}
QString Backend::exportReadings(const QUrl &url) const {
    QVariantList output;
    for (const auto &v : meters()) {
        auto m=v.toMap(); m.remove("packet"); m.remove("age"); m.remove("stale"); output.append(m);
    }
    QJsonObject doc{{"application","CodApkMeter"},{"version",1},{"exportedAt",when()},{"demo",m_demo},{"meters",QJsonArray::fromVariantList(output)}};
    return writeSelected(url,QJsonDocument(doc).toJson()) ? QString() : "Не удалось экспортировать показания";
}
void Backend::clearCaptures() { m_captures.clear(); emit capturesChanged(); }
