import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    property bool headingUp: false
    property bool rangeRings: false
    property bool mapAvailable: false
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5

    signal headingUpRequested(bool enabled)
    signal rangeRingsRequested(bool enabled)

    parent: Overlay.overlay
    width: Math.min(ScreenTools.defaultFontPixelHeight * 21, parent ? parent.width - margin * 2 : 0)
    height: Math.min(contents.implicitHeight + padding * 2, parent ? parent.height - margin * 2 : 0)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: margin
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    QGCPalette { id: palette }

    background: Rectangle {
        color: palette.window
        border.color: palette.windowShadeLight
        radius: root.margin
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: contents.implicitHeight

        ColumnLayout {
            id: contents

            width: scroll.availableWidth
            spacing: root.margin

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Display options")
                font.bold: true
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Radar orientation")
            }

            QGCComboBox {
                Layout.fillWidth: true
                model: [qsTr("North up"), qsTr("Aircraft heading up")]
                currentIndex: root.headingUp ? 1 : 0
                onActivated: (index) => root.headingUpRequested(index === 1)
            }

            QGCCheckBox {
                Layout.fillWidth: true
                text: qsTr("Map range rings")
                checked: root.rangeRings
                enabled: root.mapAvailable
                onClicked: root.rangeRingsRequested(checked)
            }

            QGCLabel {
                Layout.fillWidth: true
                visible: !root.mapAvailable
                text: qsTr("Target overlays require the standard QGC map.")
                wrapMode: Text.Wrap
                font.pointSize: ScreenTools.smallFontPointSize
            }

            QGCButton {
                Layout.fillWidth: true
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
