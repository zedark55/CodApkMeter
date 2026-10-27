#include "backend.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

class BackendTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    QString custom(Backend &b, const QString &id="my-meter") {
        auto o=QJsonDocument::fromJson(b.templateText().toUtf8()).object();
        o["id"]=id;
        o["prefix"]="B1";
        o["companyId"]=123;
        o["encryption"]=QJsonObject{{"mode","none"},{"offset",3},{"length",7}};
        return QString::fromUtf8(QJsonDocument(o).toJson());
    }
    QByteArray packet() { return QByteArray::fromHex("b103e940e201005efbff"); }
private slots:
    void initTestCase() {
        QVERIFY(temp.isValid());
        qputenv("XDG_DATA_HOME",temp.path().toUtf8());
        QCoreApplication::setOrganizationName("CodApkMeterTests");
        QCoreApplication::setApplicationName("Backend");
    }
    void builtinAndTemplate() {
        Backend b; QCOMPARE(b.protocols().size(),2);
        QCOMPARE(b.validateProtocol(b.templateText()),QString());
        QVERIFY(!b.validateProtocol(b.protocolText("demo-water")).isEmpty());
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
    void invalidProtocolDoesNotChangeLibrary() {
        Backend b; int size=b.protocols().size();
        QVERIFY(!b.saveProtocol("{}").isEmpty()); QCOMPARE(b.protocols().size(),size);
        QVERIFY(!b.saveProtocol(b.protocolText("demo-water")).isEmpty());
    }
    void userProtocolAndReadingsPersist() {
        {
            Backend b;
            QCOMPARE(b.saveProtocol(custom(b)),QString());
            b.ingest("manufacturer",123,{},packet(),-53,"AA:BB:CC:DD:EE:01");
            QCOMPARE(b.meters().size(),1);
            auto m=b.meters().first().toMap(); QVERIFY(m.value("valid").toBool());
            QCOMPARE(m.value("reading").toDouble(),123.456);
            // A rotating Bluetooth address must not create another device.
            b.ingest("manufacturer",123,{},packet(),-60,"AA:BB:CC:DD:EE:02");
            QCOMPARE(b.meters().size(),1); QCOMPARE(b.meters().first().toMap().value("history").toList().size(),1);
            QCOMPARE(b.renameMeter(m.value("id").toString(),"Кухня"),QString());
            QVERIFY(!b.deleteProtocol("my-meter").isEmpty());
        }
        Backend reloaded;
        QCOMPARE(reloaded.protocols().size(),3); QCOMPARE(reloaded.meters().size(),1);
        QCOMPARE(reloaded.meters().first().toMap().value("name").toString(),QString("Кухня"));
        auto path=temp.filePath("readings.json");
        QCOMPARE(reloaded.exportReadings(QUrl::fromLocalFile(path)),QString());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        auto data=file.readAll(); QVERIFY(!data.contains("packet")); QVERIFY(!data.contains("0011223344556677"));
        auto doc=QJsonDocument::fromJson(data); QVERIFY(doc.isObject());
        QCOMPARE(doc.object()["meters"].toArray().size(),1);
    }
    void conflictsAndSchemaUpdates() {
        Backend b;
        QVERIFY(!b.saveProtocol(custom(b,"conflict")).isEmpty());
        auto o=QJsonDocument::fromJson(custom(b).toUtf8()).object();
        auto fields=o["fields"].toArray(); auto f=fields[0].toObject(); f["scale"]=1; fields[0]=f; o["fields"]=fields;
        QCOMPARE(b.saveProtocol(QString::fromUtf8(QJsonDocument(o).toJson())),QString());
        QCOMPARE(b.meters().first().toMap().value("reading").toDouble(),123456.0);
        QVERIFY(b.meters().first().toMap().value("history").toList().isEmpty());
    }
    void captureBufferIsBounded() {
        Backend b;
        for (int i=0;i<250;i++) b.ingest("manufacturer",42,{},QByteArray::fromHex("ffff"),-70,{});
        QCOMPARE(b.captures().size(),200); QCOMPARE(b.packetCount(),250);
        b.clearCaptures(); QVERIFY(b.captures().isEmpty());
        b.ingest("manufacturer",42,{},QByteArray(1651,0),-70,{}); QVERIFY(b.captures().isEmpty());
    }
    void protocolImportExport() {
        Backend b;
        auto o=QJsonDocument::fromJson(custom(b,"imported").toUtf8()).object(); o["companyId"]=456;
        QString text=QString::fromUtf8(QJsonDocument(o).toJson());
        auto path=QUrl::fromLocalFile(temp.filePath("protocol.json"));
        QCOMPARE(b.exportProtocol(text,path),QString());
        QCOMPARE(b.readProtocol(path).value("error").toString(),QString());
        QVERIFY(!b.readProtocol(path).value("text").toString().isEmpty());
        QCOMPARE(b.importProtocol(path),QString());
        QVERIFY(!b.importProtocol(path).isEmpty());
        QCOMPARE(b.deleteProtocol("imported"),QString());
        QVERIFY(b.protocolText("imported").isEmpty());
    }
    void testPacketDoesNotPersistKeys() {
        Backend b;
        auto result=b.testPacket(custom(b),QString::fromLatin1(packet().toHex()),"");
        QVERIFY(result.startsWith("Успешно"));
        QVERIFY(b.testPacket(custom(b),"ZZ","").startsWith("Ошибка"));
        auto wrongPrefix=packet(); wrongPrefix[0]=char(0xa1);
        QVERIFY(b.testPacket(custom(b),QString::fromLatin1(wrongPrefix.toHex()),"").startsWith("Ошибка"));
        QVERIFY(!b.saveKey("unknown","00112233445566778899aabbccddeeff").isEmpty());
    }
};
QTEST_GUILESS_MAIN(BackendTests)
#include "tst_backend.moc"
