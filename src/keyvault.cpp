#include "keyvault.h"
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QCoreApplication>
#include <QtCore/qcoreapplication_platform.h>
namespace {
QJniObject context() { return QNativeInterface::QAndroidApplication::context(); }
bool exception() {
    QJniEnvironment env;
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}
}
#endif

QByteArray KeyVault::get(const QString &id) const {
#ifdef Q_OS_ANDROID
    auto result = QJniObject::callStaticObjectMethod("org/codapkmeter/app/KeyVault", "get",+        "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        context().object(), QJniObject::fromString(id).object<jstring>());
    if (exception() || !result.isValid()) return {};
    return QByteArray::fromHex(result.toString().toLatin1());
#else
    return sessionKeys.value(id);
#endif
}
bool KeyVault::put(const QString &id, const QByteArray &key) {
    if (key.size()!=16) return false;
#ifdef Q_OS_ANDROID
    jboolean ok = QJniObject::callStaticMethod<jboolean>("org/codapkmeter/app/KeyVault", "put",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z", context().object(),
        QJniObject::fromString(id).object<jstring>(), QJniObject::fromString(QString::fromLatin1(key.toHex())).object<jstring>());
    return !exception() && ok;
#else
    sessionKeys.insert(id,key);
    return true;
#endif
}
bool KeyVault::remove(const QString &id) {
#ifdef Q_OS_ANDROID
    jboolean ok = QJniObject::callStaticMethod<jboolean>("org/codapkmeter/app/KeyVault", "remove",
        "(Landroid/content/Context;Ljava/lang/String;)Z", context().object(), QJniObject::fromString(id).object<jstring>());
    return !exception() && ok;
#else
    sessionKeys.remove(id);
    return true;
#endif
}
