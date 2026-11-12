#pragma once
#include "protocol.h"

// ЭТО «Протей» / «СВЭУ», протокол Bluetooth редакции 2 (2026).
// На вход поступают только данные производителя: длина AD, тип FF и байты
// Company ID уже удалены Bluetooth API. Первый байт payload — код модели.
// Текущий пакет содержит 27 байт, архивный — 22; прямое подключение не нужно.
class EtoWater {
public:
    static bool matches(const QByteArray &payload);
    static QString identity(const QByteArray &payload);
    static DecodeResult decode(const QByteArray &payload);
};
