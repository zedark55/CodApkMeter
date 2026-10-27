import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    property string label: ""
    property alias text: input.text
    property string placeholder: ""
    property bool secret: false
    property bool readOnly: false
    signal edited(string value)
    spacing: 6
    Text { textFormat: Text.PlainText; text: root.label; color: "#91a2b8"; font.pixelSize: 12; visible: text.length > 0 }
    TextField {
        id: input
        Layout.fillWidth: true
        implicitHeight: 44
        color: "#e8eef7"
        selectionColor: "#316f68"
        placeholderTextColor: "#63758e"
        placeholderText: root.placeholder
        readOnly: root.readOnly
        echoMode: root.secret ? TextInput.Password : TextInput.Normal
        selectByMouse: true
        font.pixelSize: 13
        onEditingFinished: root.edited(text)
        background: Rectangle { radius: 10; color: "#0e1724"; border.color: input.activeFocus ? "#83e4ce" : "#2b3b50" }
    }
}
