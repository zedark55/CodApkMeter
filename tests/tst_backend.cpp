#include "backend.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

class BackendTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    Protocol sample() {
        QFile file(QStringLiteral(PROTOCOL_DIR "/sample-plain.json"));
        if (!file.open(QIODevice::ReadOnly)) qFatal("Missing developer fixture");
        Protocol p; QString error;
        if (!Protocol::parse(file.readAll(),p,error)) qFatal("%s",qPrintable(error));
        p.json["id"]="test-only-archived-meter";
        return p;
    }
    QByteArray packet() { return QByteArray::fromHex("b103e940e201005efbff"); }
private slots:
    void initTestCase() {
        QVERIFY(temp.isValid());
        qputenv("XDG_DATA_HOME",temp.path().toUtf8());
        QCoreApplication::setOrganizationName("CodApkMeterTests");
        QCoreApplication::setApplicationName("Backend");
    }
    void init() {
        const auto storage=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(storage);
        for (const auto &name : {"meters.json","protocols.json"}) QFile::remove(storage+"/"+name);
    }
    void protocolManagementIsNotExposedToQml() {
        Backend b;
        const auto meta=b.metaObject();
        QCOMPARE(meta->indexOfProperty("protocols"),-1);
        const QList<QByteArray> removed={"protocolText","templateText","validateProtocol","saveProtocol","deleteProtocol","importProtocol","readProtocol","exportProtocol","testPacket","protocolsChanged"};
        for (int i=0;i<meta->methodCount();i++) {
            const auto name=meta->method(i).name();
            QVERIFY2(!removed.contains(name),name.constData());
        }
    }
    void externalProtocolFileIsIgnoredAndPreserved() {
        Backend b;
        auto storage=b.storage;
        QFile file(storage+"/protocols.json"); QVERIFY(file.open(QIODevice::WriteOnly));
        auto definition=sample().json;
        definition["id"]="external-only-regression-fixture"; definition["companyId"]=65534; definition["prefix"]="FDEE";
        definition["identity"]=QJsonObject{{"offset",2},{"length",2}};
        definition["encryption"]=QJsonObject{{"mode","none"},{"offset",4},{"length",7}};
        Protocol checked; QString error;
        QVERIFY(Protocol::parse(QJsonDocument(definition).toJson(),checked,error));
        auto external=QJsonDocument(QJsonArray{definition}).toJson();
        QCOMPARE(file.write(external),external.size()); file.close();
        Backend fresh;
        QCOMPARE(fresh.m_protocols.size(),b.m_protocols.size());
        fresh.ingest("manufacturer",65534,{},QByteArray::fromHex("fdee03e940e201005efbff"),-53,{});
        QVERIFY(fresh.meters().isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),external);
    }
    void corruptLegacyCatalogDoesNotBlockStorage() {
        Backend b;
        QFile file(b.storage+"/protocols.json"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{broken"); file.close();
        Backend fresh;
        QVERIFY(fresh.storageError.isEmpty()); QVERIFY(fresh.persist());
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray("{broken"));
    }
    void demoUsesAuthenticatedDecoderAndDoesNotPersist() {
        Backend b; b.setDemo(true);
        QCOMPARE(b.meters().size(),3); QCOMPARE(b.captures().size(),3);
        for (const auto &v : b.meters()) {
            auto m=v.toMap(); QVERIFY(m.value("valid").toBool()); QVERIFY(m.value("demo").toBool());
            QVERIFY(m.value("reading").toDouble()>0); QCOMPARE(m.value("history").toList().size(),1);
        }
        b.stopScan(); b.setDemo(false); QVERIFY(b.meters().isEmpty());
        b.setDemo(true); QVERIFY(b.scanning()); b.applicationActive(false); QVERIFY(!b.scanning());
        b.applicationActive(true); QVERIFY(b.scanning()); b.stopScan();
        Backend fresh; QVERIFY(fresh.meters().isEmpty());
    }
    void compiledDemoProtocolsDoNotMatchRealScanning() {
        Backend b; b.setDemo(true);
        auto capture=b.captures().first().toMap();
        auto bytes=QByteArray::fromHex(capture.value("hex").toString().toLatin1());
        b.setDemo(false);
        b.ingest("manufacturer",65535,{},bytes,-50,{});
        QVERIFY(b.meters().isEmpty());
    }
    void readingsPersistAndAddressRotationDoesNotDuplicate() {
        QString id;
        {
            Backend b;
            // The fixture is injected by the C++ test, never by an app API.
            b.m_protocols={sample()};
            b.ingest("manufacturer",123,{},packet(),-53,"AA:BB:CC:DD:EE:01");
            QCOMPARE(b.meters().size(),1);
            auto m=b.meters().first().toMap(); QVERIFY(m.value("valid").toBool());
            QCOMPARE(m.value("reading").toDouble(),123.456); id=m.value("id").toString();
            b.ingest("manufacturer",123,{},packet(),-60,"AA:BB:CC:DD:EE:02");
            QCOMPARE(b.meters().size(),1); QCOMPARE(b.meters().first().toMap().value("history").toList().size(),1);
            QCOMPARE(b.renameMeter(id,"Кухня"),QString());
        }
        Backend reloaded;
        QCOMPARE(reloaded.meters().size(),1);
        auto m=reloaded.meters().first().toMap();
        QCOMPARE(m.value("name").toString(),QString("Кухня"));
        QCOMPARE(m.value("reading").toDouble(),123.456);
        // Old readings survive, but a missing compiled protocol is not trusted.
        QVERIFY(!m.value("valid").toBool()); QVERIFY(m.value("hasReading").toBool());
        QVERIFY(m.value("error").toString().contains("не поддерживается"));
        auto path=temp.filePath("readings.json");
        QCOMPARE(reloaded.exportReadings(QUrl::fromLocalFile(path)),QString());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        auto data=file.readAll(); QVERIFY(!data.contains("packet")); QVERIFY(!data.contains("0011223344556677"));
        QCOMPARE(QJsonDocument::fromJson(data).object()["meters"].toArray().size(),1);
    }
    void captureBufferIsBounded() {
        Backend b;
        for (int i=0;i<250;i++) b.ingest("manufacturer",42,{},QByteArray::fromHex("ffff"),-70,{});
        QCOMPARE(b.captures().size(),200); QCOMPARE(b.packetCount(),250);
        b.clearCaptures(); QVERIFY(b.captures().isEmpty());
        b.ingest("manufacturer",42,{},QByteArray(1651,0),-70,{}); QVERIFY(b.captures().isEmpty());
    }
};
QTEST_GUILESS_MAIN(BackendTests)
#include "tst_backend.moc"
