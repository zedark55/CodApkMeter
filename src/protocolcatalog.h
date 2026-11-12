#pragma once
#include "protocol.h"
#include <QVector>

// Каталог разработчика: загружает только JSON, встроенные в ресурсы при сборке.
// Пользовательские файлы не читаются. validate запрещает неоднозначный выбор
// обработчика, включая пересечение префиксов и моделей встроенного протокола ЭТО.
class ProtocolCatalog {
public:
    static bool load(QVector<Protocol> &out, QString &error);
    static bool validate(const QVector<Protocol> &items, QString &error);
};
