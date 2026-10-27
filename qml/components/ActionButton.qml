import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool secondary: false
    property bool danger: false
    implicitHeight: 46
    implicitWidth: Math.max(100, label.implicitWidth + 32)
    leftPadding: 16
    rightPadding: 16
    opacity: enabled ? 1 : 0.4
    contentItem: Text { textFormat: Text.PlainText;
        id: label
        text: control.text
        font.pixelSize: 13
        font.weight: Font.DemiBold
        color: control.danger ? "#ff9b94" : control.secondary ? "#dce5f2" : "#0a2623"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 12
        color: control.down ? "#51baa7" : control.secondary ? (control.hovered ? "#2b3c50" : "#202e40") : "#83e4ce"
        border.color: control.activeFocus ? "#e4fff8" : "transparent"
        border.width: 2
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
