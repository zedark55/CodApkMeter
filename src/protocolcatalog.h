#pragma once
#include "protocol.h"
#include <QVector>

// Developer-controlled resource catalog: never loads external/user JSON files.
class ProtocolCatalog {
public:
    static bool load(QVector<Protocol> &out, QString &error);
    static bool validate(const QVector<Protocol> &items, QString &error);
};
