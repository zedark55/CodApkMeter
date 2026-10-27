#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

struct DecodeResult {
    bool ok = false;
    QString error;
    QVariantList fields;
    double reading = 0;
    QString unit;
    int battery = -1;
};

class Protocol {
public:
    QJsonObject json;
    static bool parse(const QByteArray &data, Protocol &out, QString &error);
    static bool hex(const QString &text, QByteArray &out);
    QString id() const { return json.value("id").toString(); }
    QString name() const { return json.value("name").toString(); }
    bool matches(const QString &source, int company, const QString &service, const QByteArray &packet) const;
    QString identity(const QByteArray &packet) const;
    DecodeResult decode(const QByteArray &packet, const QByteArray &key = {}) const;
};
