#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

// Общий результат для JSON-протоколов и встроенных обработчиков.
// ok проверяется раньше остальных полей: нулевое reading само по себе не успех.
// archived отделяет архив от текущего показания; metadata содержит модель,
// серийный номер и время, fields — готовые строки/числа для карточки QML.
struct DecodeResult {
    bool ok = false;
    QString error;
    bool archived = false;
    QVariantMap metadata;
    QVariantList fields;
    double reading = 0;
    QString unit;
    int battery = -1;
};

// Описание формата пакета и единая точка его проверки/декодирования.
// Смещения identity относятся к исходной полезной нагрузке, а смещения fields
// универсального JSON-формата — к выделенному или расшифрованному блоку данных.
class Protocol {
public:
    QJsonObject json;
    static bool parse(const QByteArray &data, Protocol &out, QString &error);
    static bool hex(const QString &text, QByteArray &out);
    QString id() const { return json.value("id").toString(); }
    QString name() const { return json.value("name").toString(); }
    // matches выбирает кандидат по источнику и префиксу; полная проверка — в decode.
    bool matches(const QString &source, int company, const QString &service, const QByteArray &packet) const;
    QString identity(const QByteArray &packet) const;
    DecodeResult decode(const QByteArray &packet, const QByteArray &key = {}) const;
};
