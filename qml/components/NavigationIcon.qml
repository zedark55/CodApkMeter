// Компонент значка навигации. pageIndex выбирает рисунок, tint задаёт
// активный/неактивный цвет без хранения собственного состояния выбранной страницы.
import QtQuick

Item {
    id: root
    property int pageIndex: 0
    property color tint: "#63758e"
    implicitWidth: 24
    implicitHeight: 24

    // Сохраняем прежний символ «Эфир». Остальные значки рисуются линиями,
    // чтобы их вид не зависел от формы символов в системном шрифте Android.
    Text {
        visible: root.pageIndex === 1
        anchors.centerIn: parent
        text: "◎"; color: root.tint; font.pixelSize: root.height - 3
    }
    Canvas {
        id: drawing
        anchors.fill: parent
        visible: root.pageIndex !== 1
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            // Все координаты заданы на условной сетке 24×24 и масштабируются
            // для бокового меню. requestPaint нужен при смене активного цвета.
            const ctx = getContext("2d")
            ctx.reset()
            ctx.scale(width / 24, height / 24)
            ctx.strokeStyle = root.tint
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            if (root.pageIndex === 0) {
                ctx.strokeRect(4, 4, 6, 6)
                ctx.strokeRect(14, 4, 6, 6)
                ctx.strokeRect(4, 14, 6, 6)
                ctx.strokeRect(14, 14, 6, 6)
            } else {
                ctx.beginPath()
                ctx.moveTo(4, 8); ctx.lineTo(6, 8)
                ctx.moveTo(12, 8); ctx.lineTo(20, 8)
                ctx.moveTo(4, 16); ctx.lineTo(12, 16)
                ctx.moveTo(18, 16); ctx.lineTo(20, 16)
                ctx.stroke()
                ctx.beginPath(); ctx.arc(9, 8, 3, 0, 2 * Math.PI); ctx.stroke()
                ctx.beginPath(); ctx.arc(15, 16, 3, 0, 2 * Math.PI); ctx.stroke()
            }
        }
    }
    onTintChanged: drawing.requestPaint()
    onPageIndexChanged: drawing.requestPaint()
}
