#include "backend.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>
#include <QtEndian>

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
    void namelessMeterWithUnknownDeviceTypeIsReceived() {
        // Android's BluetoothDevice.getType() is metadata, not proof that an
        // advertising callback contains no LE data. Do not discard its payload.
        QBluetoothDeviceInfo info(QBluetoothAddress("2B:80:03:42:8B:90"),QString(),0);
        info.setCoreConfigurations(QBluetoothDeviceInfo::UnknownCoreConfiguration);
        info.setRssi(-73);
        info.setManufacturerData(0x168f,QByteArray::fromHex("64325476980603e1bcf26a580101007831000000003c015e190000"));
        Backend b;
        b.discovered(info);
        QCOMPARE(b.captures().size(),1);
        QCOMPARE(b.captures().first().toMap().value("company").toInt(),0x168f);
        QCOMPARE(b.meters().size(),1);
        const auto meter=b.meters().first().toMap();
        QVERIFY(meter.value("valid").toBool());
        QCOMPARE(meter.value("serial").toString(),QString("98765432"));
        QCOMPARE(meter.value("reading").toDouble(),6.588);
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
    void etoArchivesDoNotReplaceLiveReading() {
        const auto live=QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803");
        auto archive=QByteArray::fromHex("64785634120605a0e6e06a40e20100db030000c00001");
        {
            Backend b; b.ingest("manufacturer",5775,{},live,-60,"one");
            QCOMPARE(b.meters().size(),1);
            const auto current=b.meters().first().toMap();
            QCOMPARE(current.value("reading").toDouble(),12.3456);
            QCOMPARE(current.value("serial").toString(),QString("12345678"));
            QCOMPARE(current.value("readingDecimals").toInt(),4);
            b.ingest("manufacturer",5775,{},archive,-61,"rotated");
            b.ingest("manufacturer",5775,{},archive,-62,"rotated-again");
            auto m=b.meters().first().toMap();
            QCOMPARE(b.meters().size(),1); QCOMPARE(m.value("archives").toList().size(),1);
            QCOMPARE(m.value("reading"),current.value("reading"));
            QCOMPARE(m.value("lastReadingAt"),current.value("lastReadingAt"));
            QCOMPARE(m.value("fields"),current.value("fields")); QCOMPARE(m.value("history"),current.value("history"));
            // An older live sample must not roll the current counter back either.
            auto old=live; qToLittleEndian<quint32>(1,old.data()+7); qToLittleEndian<quint32>(5,old.data()+11);
            b.ingest("manufacturer",5775,{},old,-63,{});
            QCOMPARE(b.meters().first().toMap().value("reading"),current.value("reading"));
        }
        Backend restored; QCOMPARE(restored.meters().size(),1);
        auto m=restored.meters().first().toMap(); QVERIFY(m.value("valid").toBool());
        QCOMPARE(m.value("reading").toDouble(),12.3456); QCOMPARE(m.value("archives").toList().size(),1);
        restored.ingest("manufacturer",5775,{},archive,-63,{});
        QCOMPARE(restored.meters().first().toMap().value("archives").toList().size(),1);
        const auto file=QUrl::fromLocalFile(temp.filePath("eto-export.json"));
        QCOMPARE(restored.exportReadings(file),QString());
        QFile out(file.toLocalFile()); QVERIFY(out.open(QIODevice::ReadOnly));
        auto doc=QJsonDocument::fromJson(out.readAll());
        QCOMPARE(doc.object()["meters"].toArray().first().toObject()["archives"].toArray().size(),1);
    }
    void etoArchiveOnlyAndRetention() {
        auto archive=QByteArray::fromHex("64785634120605a0e6e06a40e20100db030000c00001");
        Backend b;
        for (quint32 i=0;i<51;i++) {
            qToLittleEndian<quint32>(1000+i,archive.data()+7);
            b.ingest("manufacturer",5775,{},archive,-60,{});
        }
        auto m=b.meters().first().toMap();
        QVERIFY(!m.value("hasReading").toBool()); QVERIFY(!m.contains("reading"));
        QCOMPARE(m.value("history").toList().size(),0); QCOMPARE(m.value("archives").toList().size(),48);
        QCOMPARE(m.value("archives").toList().first().toMap().value("timestamp").toLongLong(),1050LL);
        QCOMPARE(m.value("archives").toList().last().toMap().value("timestamp").toLongLong(),1003LL);
    }
    void malformedEtoDoesNotCreateOrCorruptMeter() {
        auto good=QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803");
        auto bad=good; bad[23]=char(101);
        Backend b; b.ingest("manufacturer",5775,{},bad,-60,{}); QVERIFY(b.meters().isEmpty());
        b.ingest("manufacturer",5775,{},good,-60,{});
        auto before=b.meters().first().toMap();
        b.ingest("manufacturer",5775,{},bad,-60,{});
        auto after=b.meters().first().toMap();
        QCOMPARE(after.value("serial"),before.value("serial")); QCOMPARE(after.value("reading"),before.value("reading"));
        QCOMPARE(after.value("fields"),before.value("fields")); QVERIFY(!after.value("valid").toBool());
    }
    void deleteMeterPersistsAndPreservesOtherMetersAndKey() {
        const auto first=QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803");
        const auto second=QByteArray::fromHex("64325476980603e1bcf26a580101007831000000003c015e190000");
        Backend b;
        b.ingest("manufacturer",0x168f,{},first,-60,{});
        b.ingest("manufacturer",0x168f,{},second,-61,{});
        const auto id=b.meters().first().toMap().value("id").toString();
        const auto other=b.meters().last().toMap().value("id").toString();
        const QByteArray key(16,'k'); QVERIFY(b.vault.put(id,key));
        QVERIFY(b.deleteMeter(id).isEmpty());
        QCOMPARE(b.meters().size(),1);
        QCOMPARE(b.meters().first().toMap().value("id").toString(),other);
        QCOMPARE(b.vault.get(id),key);
        Backend reloaded;
        QCOMPARE(reloaded.meters().size(),1);
        QCOMPARE(reloaded.meters().first().toMap().value("id").toString(),other);
        const auto path=temp.filePath("after-delete.json");
        QVERIFY(b.exportReadings(QUrl::fromLocalFile(path)).isEmpty());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains(id.toUtf8()));
        QVERIFY(!b.deleteMeter(id).isEmpty());
        b.ingest("manufacturer",0x168f,{},first,-60,{});
        QCOMPARE(b.meters().size(),2);
    }
    void deleteMeterRollsBackOnStorageFailure() {
        Backend b;
        b.ingest("manufacturer",0x168f,{},QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803"),-60,{});
        const auto before=b.m_meters;
        b.storageError="test storage failure";
        QVERIFY(!b.deleteMeter(before.first().toMap().value("id").toString()).isEmpty());
        QCOMPARE(b.m_meters,before);
    }
    void deletingDemoMeterDoesNotDeleteRealMeter() {
        Backend b;
        b.ingest("manufacturer",0x168f,{},QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803"),-60,{});
        QVERIFY(b.persist());
        b.setDemo(true); b.stopScan();
        QVERIFY(b.deleteMeter(b.meters().first().toMap().value("id").toString()).isEmpty());
        QCOMPARE(b.meters().size(),2);
        Backend reloaded; QCOMPARE(reloaded.meters().size(),1);
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
