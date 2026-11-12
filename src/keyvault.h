#pragma once
#include <QByteArray>
#include <QHash>
#include <QString>

// Единый доступ к ключам AES-128 по устойчивому идентификатору карточки.
// Android хранит зашифрованные значения через Java/Keystore; настольная сборка
// держит ключи только в памяти текущего сеанса и не записывает их на диск.
class KeyVault {
public:
    QByteArray get(const QString &id) const;
    bool put(const QString &id, const QByteArray &key);
    bool remove(const QString &id);
private:
    QHash<QString,QByteArray> sessionKeys;
};
