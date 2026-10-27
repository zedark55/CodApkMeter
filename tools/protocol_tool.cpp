#include "protocol.h"
#include "protocolcatalog.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

int main(int argc,char *argv[]) {
    QCoreApplication app(argc,argv);
    QTextStream output(stdout),error(stderr),input(stdin);
    auto args=app.arguments();
    auto fail=[&](const QString &message) { error<<message<<Qt::endl; return 1; };
    if (args.size()==2 && args[1]=="catalog") {
        QVector<Protocol> items; QString message;
        if (!ProtocolCatalog::load(items,message)) return fail(message);
        for (const auto &p : items) output<<p.id()<<" · "<<p.name()<<Qt::endl;
        return 0;
    }
    if (args.size()<3 || (args[1]!="validate" && args[1]!="test") || (args[1]=="test" && args.size()!=4)) {
        error<<"Developer tool (desktop only):\n"
                "  protocol_tool catalog\n"
                "  protocol_tool validate file.json [other.json ...]\n"
                "  protocol_tool test file.json \"PAYLOAD HEX\"\n"
                "Encrypted packets: supply one AES key HEX line on stdin; keys are never command-line arguments.\n";
        return 2;
    }
    QVector<Protocol> items;
    const int count=args[1]=="test" ? 3 : args.size();
    for (int i=2;i<count;i++) {
        QFile file(args[i]); Protocol p; QString message;
        if (!file.open(QIODevice::ReadOnly)) return fail("Cannot open: "+args[i]);
        if (!Protocol::parse(file.read(65537),p,message)) return fail(args[i]+": "+message);
        items.append(p);
    }
    QString message;
    if (!ProtocolCatalog::validate(items,message)) return fail(message);
    if (args[1]=="validate") { output<<"OK: "<<items.size()<<Qt::endl; return 0; }
    const auto &p=items.first();
    QByteArray packet,key;
    if (!Protocol::hex(args[3],packet) || packet.isEmpty()) return fail("Invalid packet HEX");
    if (!p.matches(p.json.value("source").toString(),p.json.value("companyId").toInt(),p.json.value("serviceUuid").toString(),packet)) return fail("Packet prefix does not match protocol");
    const auto mode=p.json.value("encryption").toObject().value("mode").toString();
    if (mode!="none") {
        error<<"AES-128 key HEX from stdin: "<<Qt::flush;
        if (!Protocol::hex(input.readLine(),key) || key.size()!=16) return fail("Expected 32 key HEX characters on stdin");
    }
    auto result=p.decode(packet,key);
    if (!result.ok) return fail(result.error);
    output<<"ID: "<<p.identity(packet)<<Qt::endl;
    for (const auto &v : result.fields) {
        auto f=v.toMap(); output<<f.value("label").toString()<<": "<<QString::number(f.value("value").toDouble(),'g',12)<<" "<<f.value("unit").toString()<<Qt::endl;
    }
    if (mode=="aes-128-ctr" || mode=="aes-128-cbc") error<<"CTR/CBC does not authenticate the packet or verify the key.\n";
    return 0;
}
