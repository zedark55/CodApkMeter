#include "protocol.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <QtEndian>
#include <mbedtls/aes.h>
#include <mbedtls/ccm.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace {
bool integer(const QJsonValue &v, int min, int max) {
    return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() == std::floor(v.toDouble())
           && v.toDouble() >= min && v.toDouble() <= max;
}
int width(const QString &type, const QJsonObject &f) {
    if (type == "u8") return 1;
    if (type == "u16" || type == "i16") return 2;
    if (type == "u32" || type == "i32" || type == "float32") return 4;
    if (type == "bcd") return f.value("length").toInt();
    return 0;
}
bool fits(int offset, int size, int total) { return offset >= 0 && size >= 0 && offset <= total && size <= total - offset; }
}

bool Protocol::hex(const QString &text, QByteArray &out) {
    QString s = text;
    s.remove(QRegularExpression("\\s"));
    if (s.size() % 2 || s.size() > 3300 || s.contains(QRegularExpression("[^0-9a-fA-F]"))) return false;
    out = QByteArray::fromHex(s.toLatin1());
    return true;
}

bool Protocol::parse(const QByteArray &data, Protocol &out, QString &error) {
    auto fail = [&](const QString &message) { error = message; return false; };
    if (data.size() > 65536) return fail("Описание протокола больше 64 КБ");
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(data, &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) return fail("Некорректный JSON: " + e.errorString());
    const auto o = doc.object();
    if (o.value("version").toInt() != 1) return fail("Поддерживается version: 1");
    if (!QRegularExpression("^[a-z0-9][a-z0-9._-]{0,63}$").match(o.value("id").toString()).hasMatch()) return fail("id: 1–64 символа a-z, 0-9, . _ -");
    if (o.value("name").toString().trimmed().isEmpty() || o.value("name").toString().size() > 100) return fail("Укажите название до 100 символов");
    if (!QStringList{"water","electricity","gas","heat","other"}.contains(o.value("kind").toString())) return fail("Неизвестный kind");
    const auto source = o.value("source").toString();
    if (source != "manufacturer" && source != "service") return fail("source: manufacturer или service");
    if (source == "manufacturer" && !integer(o.value("companyId"), 0, 65535)) return fail("companyId: целое число 0–65535");
    if (source == "service" && QUuid(o.value("serviceUuid").toString()).isNull()) return fail("Укажите полный serviceUuid");
    QByteArray prefix;
    if (!hex(o.value("prefix").toString(), prefix) || prefix.isEmpty()) return fail("Нужен непустой HEX prefix для однозначного выбора протокола");
    const auto identity = o.value("identity").toObject();
    if (!integer(identity.value("offset"),0,1649) || !integer(identity.value("length"),1,32)) return fail("identity: offset 0–1649, length 1–32");
    const auto enc = o.value("encryption").toObject();
    const auto mode = enc.value("mode").toString();
    if (!QStringList{"none","aes-128-ctr","aes-128-ccm","aes-128-cbc"}.contains(mode)) return fail("encryption.mode: none, aes-128-ctr, aes-128-cbc или aes-128-ccm");
    if (!integer(enc.value("offset"),0,1649) || !integer(enc.value("length"),-1,1650) || enc.value("length").toInt() == 0) return fail("encryption: нужны offset и length (-1 = до конца)");
    if (mode != "none") {
        if (!integer(enc.value("nonceOffset"),0,1649)) return fail("Нужен nonceOffset");
        bool ccm = mode == "aes-128-ccm";
        if (!integer(enc.value("nonceLength"), ccm ? 7 : 16, ccm ? 13 : 16)) return fail("CTR/CBC: nonceLength 16; CCM: 7–13");
        if (mode == "aes-128-ccm" && (!integer(enc.value("tagLength"),4,16) || enc.value("tagLength").toInt()%2 || !integer(enc.value("aadLength"),0,1650))) return fail("CCM: чётный tagLength 4–16 и aadLength 0–1650");
    }
    if (!o.value("fields").isArray() || o.value("fields").toArray().isEmpty() || o.value("fields").toArray().size() > 32) return fail("Нужны 1–32 поля fields");
    QSet<QString> ids;
    int readings = 0;
    for (const auto &v : o.value("fields").toArray()) {
        const auto f = v.toObject();
        const auto id = f.value("id").toString();
        if (id.isEmpty() || id.size()>64 || ids.contains(id)) return fail("id полей должны быть непустыми и уникальными");
        ids.insert(id);
        if (f.value("label").toString().isEmpty() || f.value("label").toString().size()>100 || f.value("unit").toString().size()>16) return fail("Укажите короткие label и unit");
        if (f.value("type").toString() == "bcd" && !integer(f.value("length"),1,6)) return fail("BCD length: 1–6");
        if (!width(f.value("type").toString(),f) || !integer(f.value("offset"),0,1649)) return fail("Неизвестный type или неверный offset поля");
        if (f.value("endian").toString() != "little" && f.value("endian").toString() != "big") return fail("endian: little или big");
        if (!f.value("scale").isDouble() || !std::isfinite(f.value("scale").toDouble()) || f.value("scale").toDouble()==0) return fail("scale должен быть конечным ненулевым числом");
        if (!QStringList{"reading","battery","temperature","status","other"}.contains(f.value("role").toString())) return fail("Неизвестная role поля");
        if (f.value("role") == "reading") ++readings;
    }
    if (readings != 1) return fail("Нужно ровно одно поле с role: reading");
    out.json = o;
    error.clear();
    return true;
}

bool Protocol::matches(const QString &source, int company, const QString &service, const QByteArray &packet) const {
    if (json.value("source").toString() != source) return false;
    if (source == "manufacturer" && json.value("companyId").toInt() != company) return false;
    if (source == "service" && QUuid(json.value("serviceUuid").toString()) != QUuid(service)) return false;
    return packet.startsWith(QByteArray::fromHex(json.value("prefix").toString().remove(QRegularExpression("\\s")).toLatin1()));
}

QString Protocol::identity(const QByteArray &packet) const {
    auto i = json.value("identity").toObject();
    int offset = i.value("offset").toInt(), size = i.value("length").toInt();
    return fits(offset,size,packet.size()) ? QString::fromLatin1(packet.mid(offset,size).toHex()).toUpper() : QString();
}

DecodeResult Protocol::decode(const QByteArray &packet, const QByteArray &key) const {
    DecodeResult result;
    auto fail = [&](const QString &message) { result.error = message; return result; };
    if (packet.size()>1650 || identity(packet).isEmpty()) return fail("Пакет слишком короткий или больше 1650 байт");
    const auto enc = json.value("encryption").toObject();
    const auto mode = enc.value("mode").toString();
    int offset = enc.value("offset").toInt();
    int tag = mode == "aes-128-ccm" ? enc.value("tagLength").toInt() : 0;
    int size = enc.value("length").toInt();
    if (size == -1) size = packet.size() - offset - tag;
    if (!fits(offset,size + tag,packet.size()) || size <= 0) return fail("Шифротекст выходит за границы пакета");
    QByteArray plain = packet.mid(offset,size);
    if (mode != "none") {
        if (key.size()!=16) return fail("Нужен ключ AES-128: 32 HEX-символа");
        int nonceOffset = enc.value("nonceOffset").toInt(), nonceSize = enc.value("nonceLength").toInt();
        if (!fits(nonceOffset,nonceSize,packet.size())) return fail("Nonce выходит за границы пакета");
        QByteArray nonce = packet.mid(nonceOffset,nonceSize);
        int rc;
        if (mode == "aes-128-ctr") {
            mbedtls_aes_context aes;
            mbedtls_aes_init(&aes);
            rc = mbedtls_aes_setkey_enc(&aes,reinterpret_cast<const unsigned char *>(key.constData()),128);
            size_t position = 0;
            unsigned char stream[16] = {};
            if (!rc) rc = mbedtls_aes_crypt_ctr(&aes,size,&position,reinterpret_cast<unsigned char *>(nonce.data()),stream,
                reinterpret_cast<const unsigned char *>(packet.constData()+offset),reinterpret_cast<unsigned char *>(plain.data()));
            mbedtls_aes_free(&aes);
        } else if (mode == "aes-128-cbc") {
            if (size%16) return fail("Длина AES-CBC должна быть кратна 16 байтам");
            mbedtls_aes_context aes;
            mbedtls_aes_init(&aes);
            rc = mbedtls_aes_setkey_dec(&aes,reinterpret_cast<const unsigned char *>(key.constData()),128);
            if (!rc) rc = mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_DECRYPT,size,reinterpret_cast<unsigned char *>(nonce.data()),
                reinterpret_cast<const unsigned char *>(packet.constData()+offset),reinterpret_cast<unsigned char *>(plain.data()));
            mbedtls_aes_free(&aes);
        } else {
            int aad = enc.value("aadLength").toInt();
            if (!fits(0,aad,packet.size())) return fail("AAD выходит за границы пакета");
            mbedtls_ccm_context ccm;
            mbedtls_ccm_init(&ccm);
            rc = mbedtls_ccm_setkey(&ccm,MBEDTLS_CIPHER_ID_AES,reinterpret_cast<const unsigned char *>(key.constData()),128);
            if (!rc) rc = mbedtls_ccm_auth_decrypt(&ccm,size,
                reinterpret_cast<const unsigned char *>(nonce.constData()),nonceSize,
                reinterpret_cast<const unsigned char *>(packet.constData()),aad,
                reinterpret_cast<const unsigned char *>(packet.constData()+offset),reinterpret_cast<unsigned char *>(plain.data()),
                reinterpret_cast<const unsigned char *>(packet.constData()+offset+size),tag);
            mbedtls_ccm_free(&ccm);
        }
        if (rc) return fail("Ошибка расшифровки / проверки подлинности пакета");
    }
    for (const auto &v : json.value("fields").toArray()) {
        const auto f = v.toObject();
        const auto type = f.value("type").toString();
        int start = f.value("offset").toInt(), n = width(type,f);
        if (!fits(start,n,plain.size())) return fail("Поле «" + f.value("label").toString() + "» выходит за границы данных");
        bool little = f.value("endian").toString()=="little";
        quint32 raw = 0;
        double value = 0;
        if (type == "bcd") {
            for (int i=0;i<n;i++) {
                auto b = quint8(plain.at(start+(little ? n-1-i : i)));
                if ((b>>4)>9 || (b&15)>9) return fail("Некорректное BCD поле");
                value = value*100 + (b>>4)*10 + (b&15);
            }
        } else {
            for (int i=0;i<n;i++) raw = (raw<<8) | quint8(plain.at(start+(little ? n-1-i : i)));
            if (type == "i16") value = raw >= 0x8000 ? double(raw)-65536.0 : double(raw);
            else if (type == "i32") value = raw >= 0x80000000u ? double(raw)-4294967296.0 : double(raw);
            else if (type == "float32") { float number; std::memcpy(&number,&raw,4); value = number; }
            else value = raw;
        }
        value *= f.value("scale").toDouble();
        if (!std::isfinite(value)) return fail("Поле содержит нечисловое или бесконечное значение");
        const auto role = f.value("role").toString();
        if (role=="battery" && (value<0 || value>100)) return fail("Заряд батареи вне диапазона 0–100%");
        result.fields.append(QVariantMap{{"label",f.value("label").toString()},{"value",value},{"unit",f.value("unit").toString()},{"role",role}});
        if (role=="reading") { result.reading=value; result.unit=f.value("unit").toString(); }
        if (role=="battery") result.battery=int(value);
    }
    result.ok=true;
    return result;
}
