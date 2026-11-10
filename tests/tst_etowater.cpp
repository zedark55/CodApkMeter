#include "protocol.h"
#include "protocolcatalog.h"
#include <QJsonDocument>
#include <QtEndian>
#include <QtTest>

class EtoTests : public QObject {
    Q_OBJECT
    Protocol protocol;
    QByteArray live() { return QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803"); }
    QByteArray archive() { return QByteArray::fromHex("0a214365870706a0e6e06a40e20100db030000c00001"); }
    double field(const DecodeResult &r,const QString &label) {
        for (const auto &v : r.fields) if (v.toMap().value("label")==label) return v.toMap().value("value").toDouble();
        return qQNaN();
    }
private slots:
    void initTestCase() {
        QVector<Protocol> items; QString error;
        QVERIFY2(ProtocolCatalog::load(items,error),qPrintable(error));
        for (const auto &p : items) if (p.id()=="eto-water-v2") protocol=p;
        QVERIFY(!protocol.json.isEmpty());
    }
    void pdfLiveExample() {
        auto r=protocol.decode(live()); QVERIFY2(r.ok,qPrintable(r.error)); QVERIFY(!r.archived);
        QCOMPARE(protocol.identity(live()),QString("64-12345678"));
        QCOMPARE(r.reading,12.3456); QCOMPARE(r.metadata.value("reverse").toDouble(),0.0987);
        QCOMPARE(r.metadata.value("serial").toString(),QString("12345678"));
        QCOMPARE(r.metadata.value("deviceTime").toString(),QString("28.10.2026 15:14:09 UTC+5"));
        QCOMPARE(r.metadata.value("deviceTimestamp").toLongLong(),1793182449LL);
        QCOMPARE(r.metadata.value("software").toInt(),6); QCOMPARE(r.battery,90);
        QCOMPARE(field(r,"Напряжение батареи"),3.11); QCOMPARE(field(r,"Температура электронного блока"),27.0);
        QCOMPARE(field(r,"Мгновенный расход"),0.1);
        QCOMPARE(r.metadata.value("statusCode").toInt(),2);
        QCOMPARE(r.metadata.value("statusText").toStringList(),QStringList{"Был сброс по питанию"});
    }
    void pdfArchiveAccordingToTable() {
        // Page 4 prints only C0-01 at the end: it misses the high 00 byte
        // of status. Table 2 + the textual C0-00 description define C0-00-01.
        auto r=protocol.decode(archive()); QVERIFY2(r.ok,qPrintable(r.error)); QVERIFY(r.archived);
        QCOMPARE(protocol.identity(archive()),QString("0a-87654321"));
        QCOMPARE(r.reading,123.456); QCOMPARE(r.metadata.value("reverse").toDouble(),0.987);
        QCOMPARE(r.metadata.value("deviceTime").toString(),QString("28.10.2026 00:00:00 UTC+6"));
        QCOMPARE(r.metadata.value("archiveType").toInt(),1);
        QCOMPARE(r.metadata.value("archiveTypeName").toString(),QString("На начало суток"));
        QCOMPARE(r.metadata.value("statusCode").toInt(),192);
        auto flags=r.metadata.value("statusText").toStringList(); QCOMPARE(flags.size(),2);
        QVERIFY(flags[0].contains("Hall1")); QVERIFY(flags[1].contains("Hall2"));
        QCOMPARE(r.battery,-1);
        auto malformed=archive(); malformed.remove(20,1); QVERIFY(!protocol.decode(malformed).ok);
    }
    void everyDocumentedModel_data() {
        QTest::addColumn<int>("code"); QTest::addColumn<double>("reading"); QTest::addColumn<double>("flow");
        for (int code : {1,2,9,10,11,12,13,14}) QTest::newRow(qPrintable(QString::number(code)))<<code<<123.456<<1.0;
        QTest::newRow("Protey50-table3")<<6<<1234.56<<1.0;
        for (int code : {0x41,0x42,0x51,0x52,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x71,0x72})
            QTest::newRow(qPrintable(QString::number(code)))<<code<<12.3456<<0.1;
    }
    void everyDocumentedModel() {
        QFETCH(int,code); QFETCH(double,reading); QFETCH(double,flow);
        auto p=live(); p[0]=char(code);
        QVERIFY(protocol.matches("manufacturer",5775,{},p));
        auto r=protocol.decode(p); QVERIFY2(r.ok,qPrintable(r.error)); QCOMPARE(r.reading,reading);
        QCOMPARE(field(r,"Мгновенный расход"),flow);
        auto a=archive(); a[0]=char(code); r=protocol.decode(a); QVERIFY(r.ok); QCOMPARE(r.reading,reading);
    }
    void sourceAndUnknownModels() {
        QVERIFY(!protocol.matches("manufacturer",1,{},live()));
        QVERIFY(!protocol.matches("service",5775,{},live()));
        auto p=live(); p[0]=char(0x7f);
        QVERIFY(!protocol.matches("manufacturer",5775,{},p)); QVERIFY(!protocol.decode(p).ok);
    }
    void strictBoundsAndBcd() {
        const auto p=live();
        for (int n=0;n<p.size();n++) QVERIFY2(!protocol.decode(p.left(n)).ok,qPrintable(QString::number(n)));
        for (int n=0;n<archive().size();n++) QVERIFY(!protocol.decode(archive().left(n)).ok);
        QVERIFY(!protocol.decode(p+QByteArray(1,0)).ok);
        auto bad=p; bad[1]=char(0xfa); QVERIFY(protocol.identity(bad).isEmpty()); QVERIFY(!protocol.decode(bad).ok);
        bad=p; bad[6]=16; QVERIFY(!protocol.decode(bad).ok);
        bad=p; bad[23]=101; QVERIFY(!protocol.decode(bad).ok);
        bad=archive(); bad[21]=3; QVERIFY(!protocol.decode(bad).ok);
        bad=p; bad[1]=0; bad[2]=0; bad[3]=0; bad[4]=1;
        QCOMPARE(protocol.decode(bad).metadata.value("serial").toString(),QString("01000000"));
        bad=p; bad[6]=15; QVERIFY(protocol.decode(bad).ok);
    }
    void allArchiveTypes() {
        for (int type=0;type<3;type++) {
            auto p=archive(); p[21]=char(type); auto r=protocol.decode(p);
            QVERIFY(r.ok); QVERIFY(r.archived); QCOMPARE(r.metadata.value("archiveType").toInt(),type);
        }
    }
    void statusBits() {
        for (const auto &v : QList<QPair<int,QString>>{{1,"магнитного"},{2,"сброс"},{4,"обратного"},{5,"Протечка"},{6,"Прорыв"},{7,"Hall1"},{8,"Hall2"},{12,"Разряд"},{13,"32768"}}) {
            auto p=live(); qToLittleEndian<quint16>(quint16(1u<<(v.first-1)),p.data()+19);
            auto r=protocol.decode(p); QVERIFY(r.ok);
            auto flags=r.metadata.value("statusText").toStringList(); QCOMPARE(flags.size(),1); QVERIFY(flags[0].contains(v.second));
        }
        auto p=live(); qToLittleEndian<quint16>(4,p.data()+19);
        auto r=protocol.decode(p); QVERIFY(r.ok); QVERIFY(r.metadata.value("statusText").toStringList().join(' ').contains("0x0004"));
        qToLittleEndian<quint16>(0,p.data()+19); QVERIFY(protocol.decode(p).metadata.value("statusText").toStringList().isEmpty());
    }
    void decoderValidationAndCatalogConflict() {
        auto o=protocol.json; QString error; Protocol p;
        o["companyId"]=1; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=protocol.json; o["decoder"]="eto-water-typo"; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        auto other=protocol; other.json["id"]="conflicting-eto-copy";
        QVERIFY(!ProtocolCatalog::validate({protocol,other},error));
        QVector<Protocol> items; QVERIFY(ProtocolCatalog::load(items,error));
        for (const auto &item : items) if (item.id()=="demo-water") {
            auto generic=item.json; generic["id"]="another-eto-family"; generic["companyId"]=5775; generic["prefix"]="64";
            QVERIFY(Protocol::parse(QJsonDocument(generic).toJson(),other,error));
            QVERIFY(!ProtocolCatalog::validate({protocol,other},error));
            generic["prefix"]="FE";
            QVERIFY(Protocol::parse(QJsonDocument(generic).toJson(),other,error));
            QVERIFY(ProtocolCatalog::validate({protocol,other},error));
        }
    }
};
QTEST_APPLESS_MAIN(EtoTests)
#include "tst_etowater.moc"
