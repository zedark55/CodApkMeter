#include "etowater.h"
#include <QDateTime>
#include <QTimeZone>
#include <QStringList>
#include <QtEndian>

namespace {
struct Model { quint8 code; const char *name; double volumeScale; int decimals; };
const Model models[]={
    {0x01,"Протей 15, ДУ15",0.001,3}, {0x02,"Протей 20, ДУ20",0.001,3},
    {0x06,"Протей 50, ДУ50",0.01,2}, {0x09,"Протей 15-2, ДУ15",0.001,3},
    {0x0a,"Протей 15-3, ДУ15",0.001,3}, {0x0b,"Протей 15-4, ДУ15",0.001,3},
    {0x0c,"Протей 20-3, ДУ20",0.001,3}, {0x0d,"Протей 20-4, ДУ20",0.001,3},
    {0x0e,"Протей 20-5, ДУ20",0.001,3},
    {0x41,"СВЭУ-15-1, ДУ15",0.0001,4}, {0x42,"СВЭУ-20-1, ДУ20",0.0001,4},
    {0x51,"СВЭУ-15-2, ДУ15",0.0001,4}, {0x52,"СВЭУ-20-2, ДУ20",0.0001,4},
    {0x61,"СВЭУ-15-3, ДУ15",0.0001,4}, {0x62,"СВЭУ-20-3, ДУ20",0.0001,4},
    {0x63,"СВЭУ-15-3-2, ДУ15",0.0001,4}, {0x64,"СВЭУ-15-3-3, ДУ15",0.0001,4},
    {0x65,"СВЭУ-20-3-2, ДУ20",0.0001,4}, {0x66,"СВЭУ-20-3-3, ДУ20",0.0001,4},
    {0x67,"СВЭУ-20-3-4, ДУ20",0.0001,4}, {0x71,"СВЭУ-15-4, ДУ15",0.0001,4},
    {0x72,"СВЭУ-20-4, ДУ20",0.0001,4}
};
const Model *model(const QByteArray &p) {
    if (p.isEmpty()) return nullptr;
    for (const auto &m : models) if (m.code==quint8(p[0])) return &m;
    return nullptr;
}
QString serial(const QByteArray &p) {
    if (p.size()<5) return {};
    QString result;
    for (int i=4;i>=1;i--) {
        const auto b=quint8(p[i]);
        if ((b>>4)>9 || (b&15)>9) return {};
        result+=QChar('0'+(b>>4)); result+=QChar('0'+(b&15));
    }
    return result;
}
QStringList statusText(quint16 status) {
    struct Flag { int bit; const char *text; };
    const Flag flags[]={
        {1,"Длительное воздействие внешнего магнитного поля"}, {2,"Был сброс по питанию"},
        {4,"Наличие обратного потока"}, {5,"Протечка воды (резерв)"},
        {6,"Прорыв воды (резерв)"}, {7,"Нарушение работы датчика расхода Hall1"},
        {8,"Нарушение работы датчика расхода Hall2"}, {12,"Разряд элемента питания"},
        {13,"Ошибка кварцевого генератора 32768 Гц"}
    };
    QStringList result; quint16 known=0;
    for (const auto &f : flags) {
        const auto mask=quint16(1u<<(f.bit-1)); known|=mask;
        if (status&mask) result.append(QString::fromUtf8(f.text));
    }
    const auto reserved=quint16(status & ~known);
    if (reserved) result.append(QString("Зарезервированные биты: 0x%1").arg(reserved,4,16,QChar('0')));
    return result;
}
}

bool EtoWater::matches(const QByteArray &p) { return model(p)!=nullptr; }
QString EtoWater::identity(const QByteArray &p) {
    auto m=model(p); auto number=serial(p);
    return m && !number.isEmpty() ? QString("%1-%2").arg(m->code,2,16,QChar('0')).arg(number) : QString();
}
DecodeResult EtoWater::decode(const QByteArray &p) {
    DecodeResult r;
    auto fail=[&](const QString &error) { r.error=error; return r; };
    const auto m=model(p);
    if (!m) return fail("Неизвестная модель ЭТО / Протей / СВЭУ");
    // Table 1: 27 manufacturer bytes; table 2: 22. The printed archive
    // hex example on page 4 misses a status byte; do not repair it silently.
    if (p.size()!=27 && p.size()!=22) return fail("ЭТО: ожидается 27 байт текущих или 22 байта архивных данных");
    const auto number=serial(p);
    if (number.isEmpty()) return fail("ЭТО: некорректный заводской номер BCD");
    const auto tz=quint8(p[6]);
    if (tz>15) return fail("ЭТО: часовой пояс должен быть UTC+0…UTC+15");
    r.archived=p.size()==22;
    const auto u16=[&](int offset) { return qFromLittleEndian<quint16>(p.constData()+offset); };
    const auto u32=[&](int offset) { return qFromLittleEndian<quint32>(p.constData()+offset); };
    const quint32 timestamp=u32(7);
    const auto wall=QDateTime::fromSecsSinceEpoch(timestamp,QTimeZone::UTC).addSecs(tz*3600);
    const auto deviceTime=wall.toString("dd.MM.yyyy HH:mm:ss")+QString(" UTC+%1").arg(tz);
    const auto status=u16(19); const auto messages=statusText(status);
    const QStringList archiveNames={"На начало часа","На начало суток","На начало месяца"};
    if (r.archived && quint8(p[21])>2) return fail("ЭТО: неизвестный тип архивных данных");
    if (!r.archived && quint8(p[23])>100) return fail("ЭТО: заряд батареи вне диапазона 0–100%");
    r.reading=u32(11)*m->volumeScale; r.unit="м³";
    r.metadata={{"modelName",QString::fromUtf8(m->name)},{"modelCode",m->code},{"serial",number},
        {"software",quint8(p[5])},{"timezoneHours",tz},{"deviceTimestamp",qint64(timestamp)},
        {"deviceTime",deviceTime},{"statusCode",status},{"statusText",messages},
        {"readingDecimals",m->decimals},{"reverse",u32(15)*m->volumeScale}};
    auto add=[&](const QString &label,double value,const QString &unit,const QString &role="other",int decimals=0,const QString &text=QString()) {
        QVariantMap f{{"label",label},{"value",value},{"unit",unit},{"role",role},{"decimals",decimals}};
        if (!text.isNull()) f["text"]=text;
        r.fields.append(f);
    };
    add("Модель",m->code,"","other",0,QString::fromUtf8(m->name));
    add("Заводской номер",number.toLongLong(),"","other",0,number);
    add("Вариант ПО",quint8(p[5]),"");
    add("Часовой пояс",tz,"","other",0,QString("UTC+%1").arg(tz));
    add(r.archived ? "Время архивной записи" : "Время прибора",timestamp,"","other",0,deviceTime);
    add(r.archived ? "Архивный объём" : "Объём воды",r.reading,"м³","reading",m->decimals);
    add("Обратный поток",u32(15)*m->volumeScale,"м³","other",m->decimals);
    add("Состояние",status,"","status",0,messages.isEmpty() ? "Нет событий" : messages.join("; "));
    if (r.archived) {
        const auto type=quint8(p[21]);
        r.metadata["archiveType"]=type; r.metadata["archiveTypeName"]=archiveNames[type];
        add("Тип архива",type,"","other",0,archiveNames[type]);
    } else {
        r.battery=quint8(p[23]);
        add("Напряжение батареи",u16(21)*0.01,"В","other",2);
        add("Остаточная ёмкость",r.battery,"%","battery");
        // The PDF does not define signed temperature encoding. Preserve the
        // specified byte as unsigned instead of inventing negative semantics.
        add("Температура электронного блока",quint8(p[24]),"°C","temperature");
        // Table 1 specifies flow independently from the volume scale:
        // Protey: 1 l/h, SVEU: 0.1 l/h (including Protey 50's volume exception).
        add("Мгновенный расход",u16(25)*(m->code>=0x40 ? 0.0001 : 0.001),"м³/ч","other",m->code>=0x40 ? 4 : 3);
    }
    r.ok=true; return r;
}
