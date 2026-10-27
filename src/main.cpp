#include "backend.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QImage>
#include <QTimer>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc,argv);
    QCoreApplication::setApplicationName("CodApkMeter");
    QCoreApplication::setOrganizationName("CodApkMeter");
    QCoreApplication::setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    Backend backend;
    QQmlApplicationEngine engine;
    bool qmlWarnings=false;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[&](const QList<QQmlError> &) { qmlWarnings=true; });
    engine.rootContext()->setContextProperty("backend",&backend);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[] { QCoreApplication::exit(1); },Qt::QueuedConnection);
    QObject::connect(&app,&QGuiApplication::applicationStateChanged,&backend,[&](Qt::ApplicationState state) { backend.applicationActive(state==Qt::ApplicationActive); });
    engine.loadFromModule("CodApkMeter","Main");
    if (app.arguments().contains("--mobile") && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().first()->setProperty("width",420);
        engine.rootObjects().first()->setProperty("height",860);
    }
    if (app.arguments().contains("--demo")) backend.setDemo(true);
    const int smoke=app.arguments().indexOf("--smoke-test");
    if (smoke>=0) {
        QTimer::singleShot(600,&app,[&] {
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto root=engine.rootObjects().first();
            root->setProperty("page",1);
            QMetaObject::invokeMethod(root,"openEditor",Q_ARG(QVariant,QVariant("demo-water")));
        });
        QTimer::singleShot(1200,&app,[&] {
            if (engine.rootObjects().isEmpty()) { app.exit(2); return; }
            auto root=engine.rootObjects().first();
            auto popup=root->findChild<QObject *>("protocolEditor");
            if (popup) popup->setProperty("visible",false);
            root->setProperty("page",2);
        });
        QTimer::singleShot(1500,&app,[&] { if (!engine.rootObjects().isEmpty()) engine.rootObjects().first()->setProperty("page",3); });
        QTimer::singleShot(1800,&app,[&] { if (!engine.rootObjects().isEmpty()) engine.rootObjects().first()->setProperty("page",0); });
        QTimer::singleShot(2200,&app,[&] {
            bool ok=!engine.rootObjects().isEmpty() && !qmlWarnings;
            if (backend.demo()) {
                ok=ok && backend.meters().size()==3;
                for (const auto &v : backend.meters()) ok=ok && v.toMap().value("valid").toBool();
            }
            int screenshot=app.arguments().indexOf("--screenshot");
            if (ok && screenshot>=0 && screenshot+1<app.arguments().size()) {
                auto view=qobject_cast<QQuickWindow *>(engine.rootObjects().first());
                ok=view && view->grabWindow().save(app.arguments().at(screenshot+1));
            }
            app.exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
