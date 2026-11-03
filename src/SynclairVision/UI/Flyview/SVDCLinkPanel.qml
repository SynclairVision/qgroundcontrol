import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id: root

    width: ScreenTools.defaultFontPixelHeight * 15
    height: Math.min(maximumHeight, implicitHeight)
    implicitHeight: menuButton.implicitHeight + margin * 2
        + (menuButton.checked ? body.implicitHeight + margin : 0)

    property real maximumHeight: parent ? parent.height : implicitHeight
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    readonly property var client: SVBackend.dclink

    QGCPalette { id: palette }

    Rectangle {
        anchors.fill: parent
        radius: root.margin
        color: palette.window
        border.color: palette.windowShadeLight
    }

    QGCButton {
        id: menuButton

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.margin
        text: root.client.connected ? qsTr("DCLink · Online") : qsTr("DCLink")
        checkable: true
    }

    Flickable {
        id: scroll

        anchors.top: menuButton.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.margin
        anchors.topMargin: menuButton.checked ? root.margin : 0
        height: Math.max(0, root.height - y - root.margin)
        visible: menuButton.checked
        clip: true
        contentWidth: width
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: body

            width: scroll.width
            spacing: root.margin

            QGCLabel {
                Layout.fillWidth: true
                text: root.client.statusText
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.client.vehicleText
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Command: %1").arg(root.client.commandText)
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.client.targetText
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.client.availabilityReason
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.client.actions

                delegate: ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: root.margin * 0.5

                    QGCButton {
                        Layout.fillWidth: true
                        text: modelData.label
                        enabled: modelData.enabled
                        onClicked: root.client.startAction(modelData.command)
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: modelData.reason
                        visible: text.length > 0 && text !== root.client.availabilityReason
                        wrapMode: Text.WordWrap
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root.client.feedback
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }
    }

    onVisibleChanged: {
        if (!visible) {
            menuButton.checked = false
        }
    }
}
