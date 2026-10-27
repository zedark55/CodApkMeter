# Как открыть, изменить и собрать CodApkMeter на чистом компьютере

Основной маршрут ниже — **Windows 10/11 x64 + Qt Creator**. Для Linux приведены отдельные замечания в конце. Сам проект и команды CMake общие для обеих систем.

Набор, с которым уже собран Android APK: **Qt 6.8.3, Android SDK 35, Build Tools 35.0.0, NDK r27c (27.2.12479018)**. Рекомендуемый JDK — **17**; облачная сборка также прошла с JDK 21. Инструкция для Windows опирается на стандартную настройку Qt Creator; на чистом Windows-компьютере проект в рамках этой работы не собирался.

## 1. Что понадобится и где скачать

Практический ориентир для компьютера: 64-битная Windows, от 8 ГБ RAM (удобнее 16 ГБ), примерно 25–35 ГБ свободного места и интернет для первоначальной установки и первой сборки. Телефон — **Android 8.0+**, с поддержкой **arm64-v8a**; для реального приёма нужен Bluetooth LE.

| Что | Для чего | Где взять |
| --- | --- | --- |
| Git for Windows | Скачать проект, сохранять изменения в GitHub | https://git-scm.com/downloads/win |
| Qt Online Installer, Open Source | Установить Qt Creator, Qt и инструменты сборки | https://www.qt.io/download-qt-installer-oss |
| Qt Creator | Редактор проекта и запуск сборки | Устанавливается через Qt Online Installer |
| Qt 6.8.3 for Android arm64-v8a | Библиотеки приложения для телефона | Через Qt Online Installer / Maintenance Tool; запасной способ — aqt ниже |
| Qt 6.8.3 Desktop MinGW 64-bit | Инструменты Qt для Windows и запуск интерфейса на ПК | Через тот же Qt Installer |
| Qt Connectivity / Qt Bluetooth | Приём Bluetooth LE | Компонент соответствующей версии Qt |
| MinGW 64-bit | Сборка настольной версии C++ | Developer and Designer Tools в Qt Installer; версия должна соответствовать выбранному Desktop kit |
| CMake и Ninja | Подготовка и выполнение сборки | Developer and Designer Tools в Qt Installer; CMake также https://cmake.org/download/ и Ninja https://github.com/ninja-build/ninja/releases |
| JDK 17 x64, **именно JDK** | Сборка Java-части Android и запуск Gradle | https://adoptium.net/temurin/releases/?version=17 |
| Android Studio | Удобно установить Android SDK/NDK через SDK Manager | https://developer.android.com/studio |
| Android SDK/NDK | Android-компилятор, упаковка и установка APK | SDK Manager в Android Studio либо настройка Android в Qt Creator |

Mbed TLS и Gradle отдельно вручную устанавливать не нужно: проект/Android-сборка загружают их при первой сборке. Не отключайте проверку TLS или контрольных сумм при ошибке загрузки.

Для CMake нужен минимум 3.22; проверялась **3.31.6**. Её установщики доступны на официальной странице https://github.com/Kitware/CMake/releases/tag/v3.31.6 . При нескольких версиях выберите нужную в настройках Qt Creator.

## 2. Установите Qt и инструменты

1. Установите Git for Windows с обычными настройками, включая Git Credential Manager.
2. Установите Temurin **JDK 17**. Запомните каталог установки, например `C:\Java\jdk-17`. В настройках далее указывается корень JDK, не папка `bin`.
3. Запустите Qt Online Installer. Для онлайн-установщика может потребоваться бесплатная учётная запись Qt.
4. Выберите **Custom installation / выбор компонентов**. Установите Qt Creator, Qt **6.8.3 Android arm64-v8a**, соответствующий **Qt 6.8.3 Desktop MinGW 64-bit**, Qt Connectivity/Bluetooth, MinGW, CMake и Ninja. Qt Quick и Quick Controls 2 должны входить в выбранные комплекты Qt.
5. Удобный каталог Qt: `C:\Qt`. Для проекта и инструментов используйте короткие пути латиницей.

MinGW нужен для запуска версии на компьютере. Android C++ компилирует Clang из **Android NDK**. Даже при сборке только для телефона Qt нужны соответствующие инструменты хоста той же версии Qt.

### Если Qt 6.8.3 не виден в установщике

Проверьте фильтр архивных версий в списке компонентов Maintenance Tool. Если нужного комплекта там нет, установите Creator и инструменты через Installer, а Qt 6.8.3 — через `aqtinstall`:

1. Установите Python 3 с https://www.python.org/downloads/windows/ .
2. Откройте PowerShell и выполните:

```powershell
py -m pip install --user aqtinstall==3.3.0
py -m aqt install-qt windows android 6.8.3 android_arm64_v8a -O C:/Qt -m qtconnectivity --autodesktop
```

`--autodesktop` устанавливает соответствующий комплект Qt для инструментов Windows. Компилятор MinGW, Qt Creator, CMake и Ninja устанавливаются отдельно через Qt Installer.

После установки через aqt добавьте Qt в **Preferences → Kits → Qt Versions** (в некоторых версиях Creator этот раздел называется Build & Run → Qt Versions). Выберите `qmake.exe` / `qmake.bat` из каталогов `C:\Qt\6.8.3\mingw_64\bin` и `C:\Qt\6.8.3\android_arm64_v8a\bin` соответственно. Название Desktop-папки зависит от установленного комплекта: указывайте фактический путь. Затем проверьте Android kit после настройки SDK.

## 3. Установите Android SDK и NDK

Установите Android Studio, запустите его и откройте **More Actions → SDK Manager**. Если уже открыт проект — **Tools → SDK Manager**.

Укажите каталог SDK, например `C:\Android\Sdk`, и установите:

| Раздел | Компонент |
| --- | --- |
| SDK Platforms | **Android 15 / API 35 — Android SDK Platform 35** |
| SDK Tools | **Android SDK Build-Tools 35.0.0** |
| SDK Tools | **NDK (Side by side) 27.2.12479018** — это r27c |
| SDK Tools | **Android SDK Platform-Tools**, в том числе `adb` |
| SDK Tools | **Android SDK Command-line Tools**; для воспроизведения настройки подходит **19.0** |

Для выбора конкретных версий включите **Show Package Details**. Примите лицензии предлагаемых пакетов и дождитесь окончания установки. Отдельный Android Emulator для сборки APK и установки на физический телефон не нужен.

Пример ожидаемых путей:

```text
C:\Java\jdk-17
C:\Android\Sdk
C:\Android\Sdk\ndk\27.2.12479018
C:\Android\Sdk\platform-tools\adb.exe
C:\Qt\6.8.3\android_arm64_v8a
```

Если SDK уже установлен в `%LOCALAPPDATA%\Android\Sdk`, можно оставить этот путь и выбрать его в Qt Creator.

## 4. Настройте Android в Qt Creator

Откройте **Edit → Preferences → Devices → Android**. В некоторых версиях меню настроек называется **Tools → Options**.

1. Укажите **JDK location** — корень установленного JDK 17.
2. Укажите **Android SDK location** — корень SDK.
3. Проверьте, что Creator обнаружил SDK Platform 35, Build Tools 35.0.0 и NDK 27.2.12479018. Если доступны несколько NDK, выберите r27c для Android kit; в зависимости от версии Creator выбор находится в Android settings или свойствах kit.
4. Откройте **Kits** и проверьте наличие комплекта вида **Android Qt 6.8.3 Clang arm64-v8a**. Он не должен иметь ошибок конфигурации.
5. В разделе CMake выберите установленный CMake; в свойствах kit проверьте доступность Ninja.

Если Creator предлагает автоматически установить Android prerequisites, можно использовать эту функцию. Затем всё равно сверьте версии с таблицей выше.

## 5. Скачайте и откройте проект

В PowerShell:

```powershell
New-Item -ItemType Directory -Force C:/Dev | Out-Null
Set-Location C:/Dev
git clone https://github.com/zedark55/CodApkMeter.git
Set-Location C:/Dev/CodApkMeter
```

Либо скачайте **Code → Download ZIP** на странице репозитория и распакуйте архив. Git удобнее для последующего получения обновлений и отправки изменений.

В Qt Creator:

1. **File → Open File or Project**.
2. Выберите `C:\Dev\CodApkMeter\CMakeLists.txt`.
3. Выберите **Android Qt 6.8.3 Clang arm64-v8a**.
4. Нажмите **Configure Project** и дождитесь настройки CMake. При первом запуске загрузится Mbed TLS.
5. В **Projects → Build → CMake** проверьте параметры Android. Для воспроизведения текущей сборки используются:

```text
CMAKE_BUILD_TYPE = Debug
BUILD_TESTING = OFF
ANDROID_ABI = arm64-v8a
ANDROID_PLATFORM = android-26
ANDROID_SDK_ROOT = C:/Android/Sdk
ANDROID_NDK_ROOT = C:/Android/Sdk/ndk/27.2.12479018
ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES = ON
QT_HOST_PATH = C:/Qt/6.8.3/mingw_64
```

Часть значений Creator задаёт автоматически из kit. Если путь Desktop Qt отличается, используйте его фактический каталог в `QT_HOST_PATH`. Для сборки Android нельзя подставлять Desktop Qt в качестве целевой Qt-библиотеки.

Используйте разные каталоги сборки для Desktop и Android, например `build-desktop` и `build-android`. При смене NDK или комплекта Qt настройте новый каталог сборки, чтобы CMake не использовал старый кэш.

## 6. Как менять приложение

| Что изменить | Где |
| --- | --- |
| Основные экраны и навигация | `qml/Main.qml` |
| Вид карточек приборов | `qml/components/MetricCard.qml` |
| Кнопки, панели и поля ввода | `qml/components/` |
| Bluetooth, хранение показаний, демогенератор | `src/backend.cpp` |
| Разбор пакетов и криптография | `src/protocol.cpp` |
| Защищённое хранение ключей на Android | `src/keyvault.cpp`, `android/src/org/codapkmeter/app/KeyVault.java` |
| Учебные описания протоколов | `protocols/` |
| Разрешения Android | `android/AndroidManifest.xml` |
| Версия приложения | `CMakeLists.txt`: `project(... VERSION ...)`, `QT_ANDROID_VERSION_CODE` |

Простой первый опыт: найдите в `qml/Main.qml` текст **«Ваши показания.»**, замените его, сохраните файл через **Ctrl+S** и соберите приложение повторно. После изменения `.qml` или `.cpp` APK нужно пересобрать и установить на телефон.

Для быстрой проверки интерфейса можно выбрать **Desktop MinGW kit**, собрать настольную версию и в **Projects → Run → Command line arguments** указать:

```text
--demo --mobile
```

Реальное Android-поведение всё равно проверяется на телефоне.

Новые протоколы добавляет **только разработчик в исходниках**: создайте JSON в `protocols/`, зарегистрируйте его в `METER_PROTOCOL_FILES` в `CMakeLists.txt`, проверьте через настольный `protocol_tool`, затем пересоберите APK. Пользовательского редактора и импорта нет. Команды и описание: [PROTOCOLS.md](PROTOCOLS.md). Образец для разработчика: [sample-plain.json](../protocols/sample-plain.json).

## 7. Соберите Debug APK

1. В селекторе kit выберите **Android arm64-v8a**, режим **Debug**.
2. В **Projects → Build → Build Steps** проверьте шаг **Build Android APK**. Если его нет, добавьте через **Add Build Step**. Названия и расположение могут немного отличаться в версии Creator.
3. Проект уже содержит собственный Android package source directory `android/`. Его задаёт CMake; оставьте этот каталог и существующий манифест.
4. Нажмите **Build / Ctrl+B**. Для проверки без подключённого телефона достаточно сборки.
5. Дождитесь успешного завершения. При первом запуске Gradle загрузит Android-зависимости.

APK ищите **в выбранном каталоге сборки**, а не обязательно рядом с исходниками:

```text
<каталог-сборки>\android-build\build\outputs\apk\debug\android-build-debug.apk
```

Qt Creator / androiddeployqt выводит фактический путь в журнал сборки. В разных комплектах название выходного APK может отличаться.

**Debug APK уже подписан отладочным ключом** и устанавливается на телефон. Для первой проверки создавать собственный release-keystore не требуется.

## 8. Установите APK на телефон

### Вариант A: через Qt Creator и USB

1. На телефоне включите режим разработчика: обычно несколько нажатий на «Номер сборки» в сведениях об устройстве.
2. Включите **USB debugging / Отладка по USB**.
3. Подключите телефон кабелем с поддержкой передачи данных и подтвердите доверие компьютеру на экране телефона.
4. В Qt Creator нажмите **Run / Ctrl+R**, выберите телефон. Creator соберёт, установит и запустит приложение.

Если Windows не видит телефон, установите USB-драйвер производителя. Официальная инструкция: https://developer.android.com/studio/run/oem-usb .

### Вариант B: установить файл APK

Скопируйте APK на телефон, откройте его и разрешите установку из этого источника для используемого файлового менеджера/браузера.

### Вариант C: через adb

Пример для PowerShell; подставьте фактический путь к APK:

```powershell
& "C:/Android/Sdk/platform-tools/adb.exe" devices
& "C:/Android/Sdk/platform-tools/adb.exe" install -r "C:/Dev/CodApkMeter/build-android/android-build/build/outputs/apk/debug/android-build-debug.apk"
```

В списке `adb devices` состояние должно быть `device`, а не `unauthorized`.

В приложении откройте «Настройки» и включите деморежим. Должны появиться три учебных прибора. Для реального приёма выключите демо, включите Bluetooth и нажмите «Начать приём». Пока нет образцов конкретных приборов, встроенные учебные протоколы не подтверждают совместимость с реальными моделями.

## 9. Debug, Release и обновления

Для разработки используйте Debug. Для собственного постоянного подписанного APK выберите Release и настройте подпись в шаге **Build Android APK**. Создайте один keystore и сохраняйте его для всех следующих обновлений.

Храните keystore и его пароль вне репозитория, например в защищённом каталоге `C:\Keys`. Сделайте резервную копию: при потере ключа подписи обновить уже установленное приложение с той же подписью не получится. В `CMakeLists.txt` увеличивайте `QT_ANDROID_VERSION_CODE` при выпуске обновлений; отображаемую версию меняйте через `project(... VERSION ...)`.

Если вы уже установили APK из этой облачной среды, отладочный ключ на новом компьютере будет другим. Ошибка `INSTALL_FAILED_UPDATE_INCOMPATIBLE` означает конфликт подписи. Перед удалением прежней установки экспортируйте показания; ключи приборов после переустановки нужно ввести заново. Затем удалите старое приложение и установите локальную сборку. Дальнейшие обновления делайте с одним и тем же ключом подписи.

## 10. Как сохранять изменения в ваш GitHub

В каталоге исходников после редактирования:

```powershell
git status
git add .
git commit -m "Update meter interface"
git push origin main
```

На чистом компьютере Git может попросить задать автора:

```powershell
git config --global user.name "Ваше имя"
git config --global user.email "Ваш GitHub email или noreply-адрес"
```

После настройки автора повторите `git commit` и `git push`. При первом `push` Git Credential Manager предложит вход в GitHub через браузер. Сборочные каталоги исключены в `.gitignore`. Ключи приборов и подписи держите вне исходников.

Для получения последующих изменений до начала своей работы используйте `git pull --ff-only`. Если Git сообщает о локальных изменениях или расхождении веток, сначала сохраните свою работу; не делайте сброс с потерей файлов.

## 11. Частые затруднения

| Симптом | Что проверить |
| --- | --- |
| Нет Android kit | Установлены ли Android Qt, JDK и NDK; зарегистрирована ли Qt Version; нет ли ошибок в Devices → Android и Kits |
| `Qt6Bluetooth` / `Qt6Quick` не найден | Нужные модули установлены именно для выбранной версии и платформы Qt; Desktop и Android-комплекты не перепутаны |
| `No CMAKE_CXX_COMPILER` / Clang не найден | Установлен NDK и kit указывает на `ndk/27.2.12479018` |
| JDK / Java compiler missing | Выбран JDK, а не JRE; путь указывает на корень JDK 17 |
| CMake не запускает Ninja | Ninja установлен и выбран в kit; проверена версия CMake |
| Ошибка загрузки Mbed TLS или Gradle | Есть доступ к GitHub, services.gradle.org и Android/Maven-репозиториям; настройки прокси/сертификатов компьютера корректны |
| Телефон `unauthorized` | Разблокируйте телефон и подтвердите RSA-запрос USB debugging |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | Телефон поддерживает arm64-v8a; для 32-битного устройства нужен отдельный комплект и сборка, текущий APK arm64 |
| Конфликт подписи при обновлении | Смотрите раздел 9; отладочные ключи разных компьютеров различаются |
| Приём пустой | Демо выключен, Bluetooth включён, разрешения выданы, рядом BLE-прибор и в сборку включён его реальный протокол |

## 12. Если у вас Linux

На Ubuntu 24.04 установите Git и стандартные инструменты C/C++, JDK 17, Qt Creator и те же Qt/Android-компоненты. Для запуска Qt Installer/Creator в графическом сеансе понадобятся системные XCB/OpenGL-библиотеки; следуйте зависимостям установщика вашей системы.

Пример базовых пакетов:

```bash
sudo apt update
sudo apt install git build-essential openjdk-17-jdk libgl1-mesa-dev libxcb-cursor0 libxkbcommon-x11-0
```

Qt Desktop обычно находится в `~/Qt/6.8.3/gcc_64`; его путь укажите как `QT_HOST_PATH`. SDK — в выбранном вами каталоге, часто `~/Android/Sdk`. Дальше следуйте тем же шагам Qt Creator. Командная Android-сборка приведена в основном [README](../README.md#сборка-android-apk).

Скрипты `bootstrap-cloud.sh`, `bootstrap-android-cloud.sh`, `build-android-cloud.py` предназначены для нашей Debian-облачной среды с `/workspace`. На обычном Windows/Linux-компьютере используйте настройку выше.

## 13. APK через GitHub Actions

Если нужен результат без локальной установки всех Android-инструментов, откройте https://github.com/zedark55/CodApkMeter/actions , выберите **Build and test → Run workflow → main**. После успешного задания `android` скачайте артефакт **CodApkMeter-android-arm64-debug** и распакуйте APK.

Этот маршрут использует workflow из репозитория. Его успех смотрите по фактическому статусу задания; наличие workflow не гарантирует успешную сборку. Для редактирования проекта в Qt Creator по-прежнему удобнее локальный набор инструментов.

Официальные справочники: [Qt for Android](https://doc.qt.io/qt-6.8/android.html), [Qt Creator: Android](https://doc.qt.io/qtcreator/creator-developing-android.html), [Android SDK Manager](https://developer.android.com/studio/intro/update#sdk-manager).
