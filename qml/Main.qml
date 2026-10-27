import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"

ApplicationWindow {
    id: window
    width: 1100
    height: 860
    minimumWidth: 360
    minimumHeight: 640
    visible: true
    title: "CodApkMeter · Приборы учёта"
    color: "#0c1420"
    font.family: "Sans Serif"
    palette.window: "#0c1420"
    palette.windowText: "#e8eef7"
    palette.text: "#e8eef7"
    palette.base: "#0e1724"
    palette.button: "#202e40"
    palette.buttonText: "#e8eef7"
    palette.highlight: "#316f68"
    palette.highlightedText: "#ffffff"
    property bool wide: width >= 800
    property int page: 0
    property string filterKind: "all"
    property string query: ""
    property string selectedId: ""
    property var selected: {
        const list = backend.meters
        for (let i = 0; i < list.length; i++) if (list[i].id === selectedId) return list[i]
        return ({})
    }
    property var filteredMeters: backend.meters.filter(function(m) {
        return (filterKind === "all" || m.kind === filterKind || (filterKind === "other" && ["water","electricity"].indexOf(m.kind) < 0)) &&
            (m.name.toLowerCase().indexOf(query.toLowerCase()) >= 0 || m.serial.toLowerCase().indexOf(query.toLowerCase()) >= 0)
    })
    property string toastText: ""
    function notify(error, success) { toastText = error.length ? error : success; toast.open(); toastTimer.restart() }

    Rectangle {
        id: sidebar
        visible: window.wide
        width: 206; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        color: "#101a28"
        Rectangle { anchors.right: parent.right; height: parent.height; width: 1; color: "#213044" }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 24; spacing: 10
            RowLayout {
                Layout.topMargin: 12; spacing: 9
                Rectangle { width: 30; height: 30; radius: 9; color: "#83e4ce"; Text { textFormat: Text.PlainText; anchors.centerIn: parent; text: "≈"; color: "#123c36"; font.pixelSize: 25 } }
                Text { textFormat: Text.PlainText; text: "meter"; color: "#f1f6ff"; font.pixelSize: 26; font.weight: Font.DemiBold; font.letterSpacing: -1 }
                Rectangle { width: 5; height: 5; radius: 3; color: "#83e4ce"; Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 6 }
            }
            SectionLabel { text: "BLUETOOTH LE"; Layout.topMargin: 6; Layout.bottomMargin: 38; font.pixelSize: 9 }
            Repeater {
                model: ["Обзор", "Эфир", "Настройки"]
                delegate: Button {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true; implicitHeight: 46
                    onClicked: window.page = index
                    contentItem: Text { textFormat: Text.PlainText; text: ["◫  ", "◎  ", "⚙  "][index] + modelData; color: window.page === index ? "#83e4ce" : "#91a2b8"; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; leftPadding: 12 }
                    background: Rectangle { radius: 10; color: window.page === index ? "#1b3537" : "transparent" }
                }
            }
            Item { Layout.fillHeight: true }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#263345" }
            Text { textFormat: Text.PlainText; text: "Ближе к данным."; color: "#b8c8da"; font.pixelSize: 12; Layout.topMargin: 14 }
            Text { textFormat: Text.PlainText; text: "CodApkMeter / " + Qt.application.version; color: "#63758e"; font.pixelSize: 10 }
        }
    }

    ColumnLayout {
        anchors.left: window.wide ? sidebar.right : parent.left
        anchors.right: parent.right; anchors.top: parent.top
        anchors.bottom: window.wide ? parent.bottom : bottomNav.top
        anchors.margins: window.wide ? 32 : 20
        spacing: 20
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 5
            Text { textFormat: Text.PlainText; text: window.wide ? "МОЙ ОБЪЕКТ  /  ПРИБОРЫ УЧЁТА" : "meter."; color: window.wide ? "#91a2b8" : "#83e4ce"; font.pixelSize: window.wide ? 10 : 24; font.weight: Font.DemiBold; font.letterSpacing: window.wide ? 1.5 : -1 }
            Item { Layout.fillWidth: true }
            Rectangle { width: 7; height: 7; radius: 4; color: backend.scanning ? "#83e4ce" : "#63758e"; opacity: backend.scanning ? 1 : 0.6; SequentialAnimation on opacity { running: backend.scanning; loops: Animation.Infinite; NumberAnimation { to: 0.35; duration: 950 } NumberAnimation { to: 1; duration: 950 } } }
            Text { textFormat: Text.PlainText; text: backend.demo ? "ДЕМО" : backend.scanning ? "LIVE" : "ПАУЗА"; color: backend.demo ? "#f2c27a" : "#91a2b8"; font.pixelSize: 10; font.weight: Font.DemiBold }
        }
        Rectangle {
            visible: backend.demo; Layout.fillWidth: true; implicitHeight: 40; radius: 10; color: "#30291f"
            Text { textFormat: Text.PlainText; anchors.fill: parent; anchors.margins: 10; text: "Демонстрация · данные сгенерированы"; color: "#f2c27a"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
        }
        StackLayout {
            currentIndex: window.page; Layout.fillWidth: true; Layout.fillHeight: true
            // Overview
            ScrollView {
                id: overview
                clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: overview.availableWidth; spacing: 22
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            spacing: 8
                            Text { textFormat: Text.PlainText; text: "Ваши показания.\nВ одном месте."; color: "#f1f6ff"; font.pixelSize: window.wide ? 38 : 31; font.weight: Font.DemiBold; font.letterSpacing: -1; lineHeight: 1.1 }
                            Text { textFormat: Text.PlainText; text: "Приём данных по воздуху, без подключения."; color: "#91a2b8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                        Item { Layout.fillWidth: true; visible: window.wide }
                        Rectangle {
                            visible: window.wide; width: 90; height: 90; color: "transparent"
                            Repeater { model: 3; Rectangle { required property int index; anchors.centerIn: parent; width: 30+index*27; height: width; radius: width/2; color: "transparent"; border.color: Qt.rgba(0.51,0.89,0.81,0.5-index*0.15) } }
                            Rectangle { anchors.centerIn: parent; width: 9; height: 9; radius: 5; color: "#83e4ce" }
                        }
                    }
                    Panel {
                        Layout.fillWidth: true; implicitHeight: window.wide ? 94 : 138; color: "#162b30"; border.color: "#2b4e4d"
                        GridLayout {
                            anchors.fill: parent; anchors.margins: 18; columns: window.wide ? 2 : 1
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 6
                                Text { textFormat: Text.PlainText; text: backend.scanning ? "Эфир под контролем" : "Начнём с эфира"; color: "#e1f8f2"; font.pixelSize: 16; font.weight: Font.DemiBold }
                                Text { textFormat: Text.PlainText; text: backend.status; color: "#9dbcb8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                            ActionButton { text: backend.scanning ? "Приостановить" : "Начать приём"; Layout.fillWidth: !window.wide; onClicked: backend.toggleScan() }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        SectionLabel { text: "ПРИБОРЫ  /  " + backend.meters.length }
                        Item { Layout.fillWidth: true }
                        Text { textFormat: Text.PlainText; text: backend.packetCount + " событий приёма"; color: "#63758e"; font.pixelSize: 11 }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Repeater {
                            model: [{name:"Все",kind:"all"},{name:"Вода",kind:"water"},{name:"Энергия",kind:"electricity"},{name:"Другие",kind:"other"}]
                            Button {
                                required property var modelData
                                implicitHeight: 34; implicitWidth: Math.max(58, filterText.implicitWidth + 22)
                                onClicked: window.filterKind = modelData.kind
                                contentItem: Text { textFormat: Text.PlainText; id: filterText; text: modelData.name; font.pixelSize: 11; color: window.filterKind === modelData.kind ? "#e1f8f2" : "#91a2b8"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                background: Rectangle { radius: 9; color: window.filterKind === modelData.kind ? "#2b4148" : "#141e2c" }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    TextField {
                        Layout.fillWidth: true; implicitHeight: 42; placeholderText: "Поиск по названию или серийному номеру"; color: "#e8eef7"; placeholderTextColor: "#63758e"; font.pixelSize: 12; selectByMouse: true
                        onTextChanged: window.query = text
                        background: Rectangle { radius: 10; color: "#141e2c"; border.color: "#243142" }
                    }
                    GridLayout {
                        Layout.fillWidth: true; columns: window.wide ? 2 : 1; columnSpacing: 14; rowSpacing: 14
                        Repeater {
                            model: window.filteredMeters
                            MetricCard { required property var modelData; Layout.fillWidth: true; meter: modelData; onTapped: { window.selectedId = modelData.id; meterDialog.open() } }
                        }
                    }
                    Panel {
                        visible: window.filteredMeters.length === 0; Layout.fillWidth: true; implicitHeight: 208
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 24; spacing: 12
                            Text { textFormat: Text.PlainText; text: backend.meters.length ? "Ничего не найдено" : "Здесь появятся ваши приборы"; color: "#e8eef7"; font.pixelSize: 18; font.weight: Font.Medium; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Text { textFormat: Text.PlainText; text: backend.meters.length ? "Попробуйте другой запрос или фильтр." : "Включите приём рядом с поддерживаемым прибором.\nИли посмотрите, как всё работает, в деморежиме."; color: "#91a2b8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true; lineHeight: 1.4 }
                            ActionButton { text: "Посмотреть демо"; visible: !backend.demo && backend.meters.length === 0; secondary: true; onClicked: backend.demo = true }
                            Item { Layout.fillHeight: true }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { textFormat: Text.PlainText; text: "Локально. Без облака. Ваши данные у вас."; color: "#63758e"; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Button { text: "Экспорт ↗"; flat: true; onClicked: readingsExport.open(); palette.buttonText: "#83e4ce" }
                    }
                }
            }
            // Raw advertising events
            ColumnLayout {
                spacing: 16
                Text { textFormat: Text.PlainText; text: "Эфир"; color: "#f1f6ff"; font.pixelSize: 32; font.weight: Font.DemiBold }
                Text { textFormat: Text.PlainText; text: "Manufacturer и Service Data, которые передал Qt. Последние 200 событий. Ключи здесь не отображаются."; color: "#91a2b8"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    ActionButton { text: backend.scanning ? "Пауза" : "Начать приём"; onClicked: backend.toggleScan() }
                    ActionButton { text: "Очистить"; secondary: true; onClicked: backend.clearCaptures() }
                }
                Text { textFormat: Text.PlainText; text: backend.status; color: "#83e4ce"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                ListView {
                    id: captureList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 10; model: backend.captures
                    delegate: Panel {
                        required property var modelData
                        width: captureList.width; implicitHeight: captureColumn.implicitHeight + 28
                        ColumnLayout {
                            id: captureColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 14; spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                Text { textFormat: Text.PlainText; text: modelData.time + "  ·  " + modelData.rssi + " dBm"; color: "#83e4ce"; font.pixelSize: 11 }
                                Item { Layout.fillWidth: true }
                                Text { textFormat: Text.PlainText; text: modelData.source === "manufacturer" ? "ID " + modelData.company.toString(16).toUpperCase() : "SERVICE"; color: "#91a2b8"; font.pixelSize: 10 }
                            }
                            Text { textFormat: Text.PlainText; text: modelData.protocol; color: "#dce5f2"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Text { textFormat: Text.PlainText; visible: modelData.source === "service"; text: modelData.service; color: "#91a2b8"; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                            TextArea { textFormat: TextEdit.PlainText; text: modelData.hex; readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere; color: "#91a2b8"; font.family: "monospace"; font.pixelSize: 11; Layout.fillWidth: true; padding: 0; background: null }
                        }
                    }
                    Text { textFormat: Text.PlainText; anchors.centerIn: parent; visible: captureList.count === 0; text: "Пока тихо. Включите приём пакетов."; color: "#63758e"; font.pixelSize: 12 }
                }
            }
            // Settings
            ScrollView {
                id: settings; clip: true; contentWidth: availableWidth
                ColumnLayout {
                    width: settings.availableWidth; spacing: 20
                    Text { textFormat: Text.PlainText; text: "Всё под контролем"; color: "#f1f6ff"; font.pixelSize: window.wide ? 32 : 27; font.weight: Font.DemiBold }
                    Panel {
                        Layout.fillWidth: true; implicitHeight: 118
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 20; spacing: 10
                            RowLayout {
                                Text { textFormat: Text.PlainText; text: "Демонстрационный режим"; color: "#e8eef7"; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                Switch { checked: backend.demo; onToggled: backend.demo = checked; Accessible.name: "Демонстрационный режим" }
                            }
                            Text { textFormat: Text.PlainText; text: "Три виртуальных прибора. Пакеты проходят тот же AES-CCM декодер. Реальный Bluetooth в демо выключен."; color: "#91a2b8"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                    }
                    SectionLabel { text: "ПРИВАТНОСТЬ И КЛЮЧИ" }
                    Text { textFormat: Text.PlainText; text: backend.keyStorageInfo; color: "#c0cedf"; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    Text { textFormat: Text.PlainText; text: "Ключ задаётся отдельно в карточке каждого прибора. Экспорт показаний не содержит ключей. Данные не отправляются на сервер."; color: "#91a2b8"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap; lineHeight: 1.5 }
                    SectionLabel { text: "КАК РАБОТАЕТ ПРИЁМ"; Layout.topMargin: 8 }
                    Text { textFormat: Text.PlainText; text: "Приложение слушает Bluetooth LE advertising. Прямое соединение и сопряжение не нужны. При сворачивании приём приостанавливается и возобновляется после возврата.\n\nAndroid 12+ запрашивает доступ к устройствам поблизости. Android 8–11 требует разрешение и включённую геолокацию.\n\nQt может объединять повторные advertising-пакеты и scan response. Счётчик событий не равен числу радиопакетов.\n\nСовместимость с реальными моделями требует документации производителя и проверки на телефоне. Произвольный зашифрованный пакет нельзя расшифровать без правильного формата и ключа."; color: "#91a2b8"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap; lineHeight: 1.6 }
                    Text { textFormat: Text.PlainText; text: "CodApkMeter " + Qt.application.version + "\nQt 6 / C++ / QML"; color: "#63758e"; font.pixelSize: 11; lineHeight: 1.6; Layout.topMargin: 20 }
                }
            }
        }
    }

    Rectangle {
        id: bottomNav; visible: !window.wide
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 76; color: "#101a28"
        Rectangle { width: parent.width; height: 1; color: "#263345" }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 0
            Repeater {
                model: ["Обзор", "Эфир", "Настройки"]
                Button {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true; Layout.fillHeight: true
                    onClicked: window.page = index
                    background: Rectangle { color: "transparent" }
                    contentItem: ColumnLayout {
                        spacing: 3
                        Text { textFormat: Text.PlainText; text: ["◫", "◎", "⚙"][index]; color: window.page === index ? "#83e4ce" : "#63758e"; font.pixelSize: 21; Layout.alignment: Qt.AlignHCenter }
                        Text { textFormat: Text.PlainText; text: modelData; color: window.page === index ? "#83e4ce" : "#91a2b8"; font.pixelSize: 9; Layout.alignment: Qt.AlignHCenter }
                    }
                }
            }
        }
    }

    Popup {
        id: meterDialog; parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(window.width - 24, 530); height: Math.min(window.height - 32, 760)
        modal: true; padding: 24
        background: Panel { color: "#141e2c" }
        ScrollView {
            id: detailScroll; anchors.fill: parent; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: detailScroll.availableWidth; spacing: 18
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { text: "КАРТОЧКА ПРИБОРА" }
                    Item { Layout.fillWidth: true }
                    Button { text: "✕"; flat: true; onClicked: meterDialog.close(); Accessible.name: "Закрыть" }
                }
                Text { textFormat: Text.PlainText; text: window.selected.name || ""; color: "#f1f6ff"; font.pixelSize: 25; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { textFormat: Text.PlainText; text: "ID " + (window.selected.serial || "") + "  ·  " + (window.selected.protocolName || ""); color: "#91a2b8"; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { textFormat: Text.PlainText; text: window.selected.hasReading ? Number(window.selected.reading).toLocaleString(Qt.locale("ru_RU"),'f',3) + " " + window.selected.unit : "Нет показаний"; color: "#83e4ce"; font.pixelSize: 32; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Text { textFormat: Text.PlainText; text: (window.selected.valid ? "Принято: " : "Последний успешный приём: ") + (window.selected.lastReadingAt || "—"); color: "#91a2b8"; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { textFormat: Text.PlainText; visible: window.selected.error !== ""; text: window.selected.error || ""; color: "#f2c27a"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                SectionLabel { text: "ИСТОРИЯ ПОКАЗАНИЙ" }
                Canvas {
                    id: chart; Layout.fillWidth: true; height: 100
                    property var samples: window.selected.history || []
                    onSamplesChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d"); ctx.reset()
                        ctx.strokeStyle = "#263345"; ctx.lineWidth = 1
                        for (let j = 0; j < 3; j++) { ctx.beginPath(); ctx.moveTo(0, 10 + j * 40); ctx.lineTo(width,10+j*40); ctx.stroke() }
                        if (samples.length < 2) return
                        const values = samples.map(function(s) { return s.value })
                        const low = Math.min.apply(null,values), high = Math.max.apply(null,values), span = Math.max(high-low,0.001)
                        ctx.strokeStyle = "#83e4ce"; ctx.lineWidth = 2; ctx.beginPath()
                        for (let i = 0; i < values.length; i++) { const x = i * width/(values.length-1), y = 90 - (values[i]-low)/span*80; if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y) }
                        ctx.stroke()
                    }
                }
                Text { textFormat: Text.PlainText; text: "Последние " + (window.selected.history || []).length + " изменений · шкала по диапазону значений"; color: "#63758e"; font.pixelSize: 10 }
                Repeater {
                    model: window.selected.fields || []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Text { textFormat: Text.PlainText; text: modelData.label; color: "#91a2b8"; font.pixelSize: 12; Layout.fillWidth: true }
                        Text { textFormat: Text.PlainText; text: Number(modelData.value).toLocaleString(Qt.locale("ru_RU"),'f',2) + " " + modelData.unit; color: "#e8eef7"; font.pixelSize: 13 }
                    }
                }
                Text { textFormat: Text.PlainText; visible: ["aes-128-ctr","aes-128-cbc"].indexOf(window.selected.mode) >= 0; text: "AES-CTR/CBC не проверяет подлинность пакета. Неверный ключ может дать правдоподобные показания."; color: "#f2c27a"; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11 }
                FormField { id: meterName; Layout.fillWidth: true; label: "Название прибора"; text: window.selected.name || "" }
                ActionButton { text: "Сохранить название"; secondary: true; onClicked: window.notify(backend.renameMeter(window.selectedId,meterName.text),"Название сохранено") }
                SectionLabel { visible: !window.selected.demo; text: "КЛЮЧ AES-128" }
                FormField { id: meterKey; visible: !window.selected.demo; Layout.fillWidth: true; label: "32 HEX-символа · ключ не отображается после сохранения"; secret: true; placeholder: "Введите ключ прибора" }
                Flow {
                    visible: !window.selected.demo; Layout.fillWidth: true; spacing: 10
                    ActionButton { text: "Сохранить ключ"; onClicked: { let error = backend.saveKey(window.selectedId,meterKey.text); if (!error) meterKey.text = ""; window.notify(error,"Ключ сохранён; результат расшифровки — в карточке") } }
                    ActionButton { text: "Удалить ключ"; secondary: true; danger: true; onClicked: window.notify(backend.forgetKey(window.selectedId),"Ключ удалён") }
                }
                Text { textFormat: Text.PlainText; visible: window.selected.demo === true; text: "Учебный прибор · показания сгенерированы для демонстрации."; color: "#f2c27a"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; font.pixelSize: 10 }
                Item { height: 8 }
            }
        }
        onClosed: meterKey.text = ""
    }

    FileDialog { id: readingsExport; title: "Экспорт показаний"; fileMode: FileDialog.SaveFile; nameFilters: ["JSON (*.json)"]; defaultSuffix: "json"; onAccepted: window.notify(backend.exportReadings(selectedFile),"Показания экспортированы") }
    Popup {
        id: toast; parent: Overlay.overlay; x: (parent.width-width)/2; y: parent.height-height-92
        width: Math.min(window.width-32,520); padding: 16; closePolicy: Popup.NoAutoClose
        background: Rectangle { color: "#2b4148"; radius: 12; border.color: "#4c7872" }
        contentItem: Text { textFormat: Text.PlainText; text: window.toastText; color: "#e1f8f2"; font.pixelSize: 12; wrapMode: Text.WordWrap }
    }
    Timer { id: toastTimer; interval: 4200; onTriggered: toast.close() }
}
