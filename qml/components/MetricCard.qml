import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Panel {
    id: card
    required property var meter
    signal tapped()
    property color accent: meter.kind === "electricity" ? "#f2c27a" : meter.name.indexOf("Горячая") >= 0 ? "#f19d8d" : "#83d6e9"
    implicitHeight: 222
    border.color: area.containsMouse ? "#52657c" : "#243142"
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: card.tapped() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 22; spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                width: 36; height: 36; radius: 11; color: Qt.rgba(card.accent.r,card.accent.g,card.accent.b,0.12)
                Text { textFormat: Text.PlainText; anchors.centerIn: parent; text: card.meter.kind === "electricity" ? "ϟ" : "≈"; font.pixelSize: 28; color: card.accent }
            }
            Item { Layout.fillWidth: true }
            Rectangle { width: 6; height: 6; radius: 3; color: !card.meter.valid ? "#f2c27a" : card.meter.stale ? "#63758e" : "#83e4ce" }
            Text { textFormat: Text.PlainText; text: card.meter.age || ""; font.pixelSize: 11; color: "#91a2b8" }
        }
        Text { textFormat: Text.PlainText; text: card.meter.name; Layout.fillWidth: true; elide: Text.ElideRight; color: "#dce5f2"; font.pixelSize: 15; font.weight: Font.Medium }
        RowLayout {
            spacing: 8
            Text { textFormat: Text.PlainText; text: card.meter.hasReading ? Number(card.meter.reading).toLocaleString(Qt.locale("ru_RU"),'f',card.meter.kind === "electricity" ? 2 : 3) : "—"; color: "#f1f6ff"; font.pixelSize: 34; font.weight: Font.DemiBold; font.letterSpacing: -1 }
            Text { textFormat: Text.PlainText; text: card.meter.unit || ""; color: "#91a2b8"; font.pixelSize: 14; Layout.alignment: Qt.AlignBottom; bottomPadding: 6 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#263345" }
        RowLayout {
            Layout.fillWidth: true
            Text { textFormat: Text.PlainText; text: card.meter.valid ? "Батарея " + (card.meter.battery >= 0 ? card.meter.battery + "%" : "—") : "Нужен ключ / проверьте пакет"; color: card.meter.valid ? "#91a2b8" : "#f2c27a"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { textFormat: Text.PlainText; text: card.meter.rssi + " dBm   ↗"; color: "#91a2b8"; font.pixelSize: 11 }
        }
    }
    Accessible.role: Accessible.Button
    Accessible.name: meter.name + ", " + (meter.hasReading ? meter.reading + " " + meter.unit : "Нет показаний")
    Accessible.onPressAction: card.tapped()
    activeFocusOnTab: true
    Keys.onReturnPressed: card.tapped()
}
