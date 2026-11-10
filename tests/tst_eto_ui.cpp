#include "backend.h"
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QtTest>

class EtoUiTests : public QObject {
    Q_OBJECT
    QStringList visibleText(QQuickItem *item) {
        QStringList result;
        if (!item || !item->isVisible()) return result;
        if (item->property("text").isValid()) result.append(item->property("text").toString());
        for (auto child : item->childItems()) result+=visibleText(child);
        return result;
    }
private slots:
    void deleteCardRequiresConfirmation() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); qputenv("XDG_DATA_HOME",temp.path().toUtf8());
        QQuickStyle::setStyle("Basic");
        Backend b;
        b.ingest("manufacturer",5775,{},QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803"),-55,{});
        QQmlApplicationEngine engine; QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors) {
            for (const auto &e : errors) warnings.append(e.toString());
        });
        engine.rootContext()->setContextProperty("backend",&b);
        engine.load(QUrl::fromLocalFile(QStringLiteral(PROJECT_DIR "/qml/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(),qPrintable(warnings.join('\n')));
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->resize(420,860);
        window->setProperty("selectedId",b.meters().first().toMap().value("id"));
        auto popup=window->findChild<QObject *>("meterDialog"); QVERIFY(popup);
        auto remove=window->findChild<QObject *>("deleteMeterButton"); QVERIFY(remove);
        auto confirm=window->findChild<QObject *>("confirmDeleteMeterButton"); QVERIFY(confirm);
        auto cancel=window->findChild<QObject *>("cancelDeleteMeterButton"); QVERIFY(cancel);
        auto dialog=window->findChild<QObject *>("deleteMeterConfirmation"); QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(popup,"open"));
        QVERIFY(QMetaObject::invokeMethod(remove,"clicked"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(b.meters().size(),1);
        QVERIFY(QMetaObject::invokeMethod(cancel,"clicked"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(b.meters().size(),1);
        QVERIFY(QMetaObject::invokeMethod(remove,"clicked"));
        QVERIFY(QMetaObject::invokeMethod(confirm,"clicked"));
        QVERIFY(b.meters().isEmpty());
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(window->property("selectedId").toString(),QString());
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void liveAndArchiveCard() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); qputenv("XDG_DATA_HOME",temp.path().toUtf8());
        QQuickStyle::setStyle("Basic");
        Backend b;
        b.ingest("manufacturer",5775,{},QByteArray::fromHex("64785634120605f1cae16a40e20100db030000020037015a1be803"),-55,{});
        b.ingest("manufacturer",5775,{},QByteArray::fromHex("64785634120605a0e6e06a40e20100db030000c00001"),-56,{});
        QQmlApplicationEngine engine; QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors) {
            for (const auto &e : errors) warnings.append(e.toString());
        });
        engine.rootContext()->setContextProperty("backend",&b);
        engine.load(QUrl::fromLocalFile(QStringLiteral(PROJECT_DIR "/qml/Main.qml")));
        QVERIFY2(!engine.rootObjects().isEmpty(),qPrintable(warnings.join('\n')));
        auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(window);
        window->resize(420,860);
        window->setProperty("selectedId",b.meters().first().toMap().value("id"));
        auto popup=window->findChild<QObject *>("meterDialog"); QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup,"open"));
        QTRY_VERIFY(visibleText(window->contentItem()).join('\n').contains("На начало суток"));
        auto text=visibleText(window->contentItem()).join('\n');
        QVERIFY(text.contains("12,3456")); QVERIFY(text.contains("12345678"));
        QVERIFY(text.contains("15:14:09 UTC+5")); QVERIFY(text.contains("Был сброс по питанию"));
        QVERIFY(text.contains("Hall1")); QVERIFY(text.contains("Hall2"));
        QVERIFY(!text.contains("NaN")); QVERIFY(!text.contains("КЛЮЧ AES-128"));
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
};
QTEST_MAIN(EtoUiTests)
#include "tst_eto_ui.moc"
