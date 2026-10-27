#include "protocolcatalog.h"
#include <QDir>
#include <QFile>
#include <QResource>
#include <QSet>
#include <QUuid>

bool ProtocolCatalog::validate(const QVector<Protocol> &items, QString &error) {
    QSet<QString> ids;
    for (int i=0;i<items.size();i++) {
        const auto &p=items[i];
        if (ids.contains(p.id())) { error="Повторяющийся id протокола: "+p.id(); return false; }
        ids.insert(p.id());
        for (int j=0;j<i;j++) {
            const auto &other=items[j];
            // Demo formats and production formats are separate namespaces.
            if (p.id().startsWith("demo-")!=other.id().startsWith("demo-")) continue;
            const auto &a=p.json; const auto &b=other.json;
            if (a.value("source")!=b.value("source")) continue;
            if (a.value("source")=="manufacturer" && a.value("companyId")!=b.value("companyId")) continue;
            if (a.value("source")=="service" && QUuid(a.value("serviceUuid").toString())!=QUuid(b.value("serviceUuid").toString())) continue;
            QByteArray x,y; Protocol::hex(a.value("prefix").toString(),x); Protocol::hex(b.value("prefix").toString(),y);
            if (x.startsWith(y) || y.startsWith(x)) {
                error="Пересечение prefix: "+p.id()+" / "+other.id(); return false;
            }
        }
    }
    error.clear(); return true;
}

bool ProtocolCatalog::load(QVector<Protocol> &out, QString &error) {
    static const bool initialized=[] { Q_INIT_RESOURCE(bundled_protocols); return true; }();
    Q_UNUSED(initialized);
    const QDir dir(":/meter/protocols");
    auto files=dir.entryList({"*.json"},QDir::Files,QDir::Name);
    if (files.isEmpty()) { error="Встроенный каталог протоколов пуст"; return false; }
    QVector<Protocol> loaded;
    for (const auto &name : files) {
        QFile file(dir.filePath(name)); Protocol p;
        if (!file.open(QIODevice::ReadOnly) || !Protocol::parse(file.read(65537),p,error)) {
            error="Ошибка встроенного протокола "+name+": "+error; return false;
        }
        loaded.append(p);
    }
    if (!validate(loaded,error)) return false;
    out=loaded; return true;
}
