#pragma once
#include <QByteArray>
#include <QHash>
#include <QString>

class KeyVault {
public:
    QByteArray get(const QString &id) const;
    bool put(const QString &id, const QByteArray &key);
    bool remove(const QString &id);
private:
    QHash<QString,QByteArray> sessionKeys;
};
