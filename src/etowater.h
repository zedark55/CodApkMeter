#pragma once
#include "protocol.h"

// ETO, «Протей» / «СВЭУ», Bluetooth protocol revision 2 (2026).
// Input is Qt Manufacturer Data: no AD length, AD type or Company ID bytes.
class EtoWater {
public:
    static bool matches(const QByteArray &payload);
    static QString identity(const QByteArray &payload);
    static DecodeResult decode(const QByteArray &payload);
};
