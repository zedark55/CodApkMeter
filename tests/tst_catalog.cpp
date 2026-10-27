#include "protocolcatalog.h"
#include <QJsonDocument>
#include <QtTest>

class CatalogTests : public QObject {
    Q_OBJECT
    Protocol production(const QString &id="meter-a",const QString &prefix="B1",const QString &source="manufacturer") {
        QVector<Protocol> items; QString error;
        if (!ProtocolCatalog::load(items,error)) qFatal("%s",qPrintable(error));
        QJsonObject o;
        for (const auto &p : items) if (p.id()=="demo-water") o=p.json;
        if (o.isEmpty()) qFatal("Missing demo-water fixture");
        o["id"]=id; o["prefix"]=prefix; o["source"]=source;
        o["serviceUuid"]="0000181a-0000-1000-8000-00805f9b34fb";
        Protocol p;
        if (!Protocol::parse(QJsonDocument(o).toJson(),p,error)) qFatal("%s",qPrintable(error));
        return p;
    }
private slots:
    void compiledResourcesLoad() {
        QVector<Protocol> items; QString error;
        QVERIFY2(ProtocolCatalog::load(items,error),qPrintable(error));
        QVERIFY(items.size()>=2);
        QSet<QString> ids; for (const auto &p : items) ids.insert(p.id());
        QVERIFY(ids.contains("demo-water")); QVERIFY(ids.contains("demo-electricity"));
    }
    void duplicateIdRejected() {
        QString error;
        QVERIFY(!ProtocolCatalog::validate({production(),production("meter-a","B2")},error));
        QVERIFY(error.contains("meter-a"));
    }
    void overlappingPrefixesRejected() {
        QString error;
        QVERIFY(!ProtocolCatalog::validate({production(),production("meter-b","B1 00")},error));
        QVERIFY(!ProtocolCatalog::validate({production("meter-a","B1","service"),production("meter-b","B1","service")},error));
    }
    void separateSourcesAndDemoNamespace() {
        QString error;
        QVERIFY(ProtocolCatalog::validate({production(),production("meter-b","B1","service")},error));
        QVERIFY(ProtocolCatalog::validate({production(),production("demo-meter-a")},error));
    }
};
QTEST_APPLESS_MAIN(CatalogTests)
#include "tst_catalog.moc"
