#include "protocol.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
#include <mbedtls/ccm.h>

class ProtocolTests : public QObject {
    Q_OBJECT
    Protocol demo;
    Protocol from(QJsonObject obj) {
        Protocol p; QString error;
        if (!Protocol::parse(QJsonDocument(obj).toJson(),p,error)) qFatal("Invalid fixture: %s",qPrintable(error));
        return p;
    }
    QJsonObject plainDescription() {
        auto obj=demo.json;
        obj["encryption"]=QJsonObject{{"mode","none"},{"offset",3},{"length",-1}};
        return obj;
    }
private slots:
    void initTestCase() {
        QFile file(QStringLiteral(PROTOCOL_DIR "/demo-water.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString error; QVERIFY2(Protocol::parse(file.readAll(),demo,error),qPrintable(error));
    }
    void strictHex() {
        QByteArray bytes;
        QVERIFY(Protocol::hex("AA bb\n01",bytes)); QCOMPARE(bytes,QByteArray::fromHex("aabb01"));
        QVERIFY(!Protocol::hex("0x12",bytes)); QVERIFY(!Protocol::hex("A",bytes));
        QVERIFY(!Protocol::hex("GG",bytes)); QVERIFY(!Protocol::hex(QString(3302,'0'),bytes));
    }
    void matchingAndIdentity() {
        auto packet=QByteArray::fromHex("a103e9");
        QVERIFY(demo.matches("manufacturer",65535,{},packet));
        QVERIFY(!demo.matches("manufacturer",1,{},packet));
        QVERIFY(!demo.matches("manufacturer",65535,{},QByteArray::fromHex("a203e9")));
        QVERIFY(!demo.matches("service",65535,{},packet));
        QCOMPARE(demo.identity(packet),QString("03E9")); QVERIFY(demo.identity(packet.left(2)).isEmpty());
    }
    void serviceMatching() {
        auto obj=demo.json; obj["source"]="service"; obj["serviceUuid"]="0000181a-0000-1000-8000-00805f9b34fb";
        auto p=from(obj);
        QVERIFY(p.matches("service",-1,"{0000181A-0000-1000-8000-00805F9B34FB}",QByteArray::fromHex("a10001")));
        QVERIFY(!p.matches("service",-1,"0000180f-0000-1000-8000-00805f9b34fb",QByteArray::fromHex("a10001")));
    }
    void plainLittleEndianAndSigned() {
        auto p=from(plainDescription());
        auto r=p.decode(QByteArray::fromHex("a103e940e201005efbff"));
        QVERIFY2(r.ok,qPrintable(r.error)); QCOMPARE(r.reading,123.456); QCOMPARE(r.battery,94);
        QCOMPARE(r.fields[2].toMap().value("value").toDouble(),-0.05);
    }
    void numericTypes() {
        auto o=plainDescription();
        for (const auto &type : {"u16","u32","i16","i32","float32","bcd"}) {
            auto field=QJsonObject{{"id","test"},{"label","Test"},{"type",type},{"offset",0},{"endian","big"},{"scale",1},{"unit",""},{"role","reading"},{"length",2}};
            o["fields"]=QJsonArray{field}; auto p=from(o);
            QByteArray bytes=QByteArray::fromHex("a10001");
            if (QString(type)=="u16") { auto r=p.decode(bytes+QByteArray::fromHex("1234")); QVERIFY(r.ok); QCOMPARE(r.reading,4660.0); }
            if (QString(type)=="u32") { auto r=p.decode(bytes+QByteArray::fromHex("ffffffff")); QVERIFY(r.ok); QCOMPARE(r.reading,4294967295.0); }
            if (QString(type)=="i16") { auto r=p.decode(bytes+QByteArray::fromHex("8000")); QVERIFY(r.ok); QCOMPARE(r.reading,-32768.0); }
            if (QString(type)=="i32") { auto r=p.decode(bytes+QByteArray::fromHex("ffffffff")); QVERIFY(r.ok); QCOMPARE(r.reading,-1.0); }
            if (QString(type)=="float32") {
                auto r=p.decode(bytes+QByteArray::fromHex("3fc00000")); QVERIFY(r.ok); QCOMPARE(r.reading,1.5);
                QVERIFY(!p.decode(bytes+QByteArray::fromHex("7fc00000")).ok);
            }
            if (QString(type)=="bcd") {
                auto r=p.decode(bytes+QByteArray::fromHex("1234")); QVERIFY(r.ok); QCOMPARE(r.reading,1234.0);
                QVERIFY(!p.decode(bytes+QByteArray::fromHex("1a34")).ok);
            }
        }
    }
    void nistCtrVector() {
        // NIST SP 800-38A F.5.1: first AES-128-CTR block.
        auto o=demo.json;
        o["encryption"]=QJsonObject{{"mode","aes-128-ctr"},{"offset",19},{"length",16},{"nonceOffset",3},{"nonceLength",16}};
        o["fields"]=QJsonArray{QJsonObject{{"id","value"},{"label","Value"},{"type","u32"},{"offset",0},{"endian","big"},{"scale",1},{"unit",""},{"role","reading"}}};
        auto p=from(o);
        auto packet=QByteArray::fromHex("a10001f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff874d6191b620e3261bef6864990db6ce");
        auto r=p.decode(packet,QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c"));
        QVERIFY2(r.ok,qPrintable(r.error)); QCOMPARE(r.reading,double(0x6bc1bee2));
    }
    void rfc3610CcmVector() {
        // RFC 3610 packet vector #1: 8-byte AAD, 13-byte nonce, 8-byte tag.
        auto o=demo.json;
        o["identity"]=QJsonObject{{"offset",0},{"length",2}};
        o["encryption"]=QJsonObject{{"mode","aes-128-ccm"},{"offset",21},{"length",23},{"nonceOffset",8},{"nonceLength",13},{"tagLength",8},{"aadLength",8}};
        o["fields"]=QJsonArray{QJsonObject{{"id","value"},{"label","Value"},{"type","u32"},{"offset",0},{"endian","big"},{"scale",1},{"unit",""},{"role","reading"}}};
        auto p=from(o);
        auto packet=QByteArray::fromHex("0001020304050607" "00000003020100a0a1a2a3a4a5" "588c979a61c663d2f066d0c2c0f989806d5f6b61dac384" "17e8d12cfdf926e0");
        auto key=QByteArray::fromHex("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf");
        auto r=p.decode(packet,key); QVERIFY2(r.ok,qPrintable(r.error)); QCOMPARE(r.reading,double(0x08090a0b));
        auto bad=packet; bad[22]=char(quint8(bad[22])^1); QVERIFY(!p.decode(bad,key).ok);
        bad=packet; bad[0]=char(quint8(bad[0])^1); QVERIFY(!p.decode(bad,key).ok);
        bad=packet; bad[51]=char(quint8(bad[51])^1); QVERIFY(!p.decode(bad,key).ok);
        key[0]=0; QVERIFY(!p.decode(packet,key).ok);
        QVERIFY(!p.decode(packet).ok);
    }
    void nistCbcVector() {
        // NIST SP 800-38A F.2.1: first CBC-AES128 block.
        auto o=demo.json;
        o["encryption"]=QJsonObject{{"mode","aes-128-cbc"},{"offset",19},{"length",16},{"nonceOffset",3},{"nonceLength",16}};
        o["fields"]=QJsonArray{QJsonObject{{"id","value"},{"label","Value"},{"type","u32"},{"offset",0},{"endian","big"},{"scale",1},{"unit",""},{"role","reading"}}};
        auto p=from(o);
        auto packet=QByteArray::fromHex("a10001000102030405060708090a0b0c0d0e0f7649abac8119b246cee98e9b12e9197d");
        auto r=p.decode(packet,QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c"));
        QVERIFY2(r.ok,qPrintable(r.error)); QCOMPARE(r.reading,double(0x6bc1bee2));
        QVERIFY(!p.decode(packet.left(packet.size()-1),QByteArray(16,0)).ok);
    }
    void truncatedPackets() {
        auto p=from(plainDescription()); auto packet=QByteArray::fromHex("a103e940e201005efbff");
        for (int n=0;n<packet.size();n++) QVERIFY2(!p.decode(packet.left(n)).ok,qPrintable(QString::number(n)));
        QVERIFY(!p.decode(QByteArray(1651,0)).ok);
    }
    void validationRejectsUnsafeOffsets() {
        auto o=demo.json; auto identity=o["identity"].toObject(); identity["offset"]=-1; o["identity"]=identity;
        Protocol p; QString error; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=demo.json; auto enc=o["encryption"].toObject(); enc["nonceLength"]=16; o["encryption"]=enc; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=demo.json; enc=o["encryption"].toObject(); enc["tagLength"]=5; o["encryption"]=enc; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=demo.json; auto fields=o["fields"].toArray(); auto f=fields[0].toObject(); f["scale"]=0; fields[0]=f; o["fields"]=fields; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=demo.json; fields=o["fields"].toArray(); fields.append(fields[0]); o["fields"]=fields; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        o=demo.json; o["prefix"]=""; QVERIFY(!Protocol::parse(QJsonDocument(o).toJson(),p,error));
        QVERIFY(!Protocol::parse("[]",p,error)); QVERIFY(!Protocol::parse("{",p,error));
        QVERIFY(!Protocol::parse(QByteArray(65537,' '),p,error));
    }
};
QTEST_APPLESS_MAIN(ProtocolTests)
#include "tst_protocol.moc"
