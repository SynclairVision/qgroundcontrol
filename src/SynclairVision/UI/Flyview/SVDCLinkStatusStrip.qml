import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var client
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5

    height: implicitHeight
    implicitHeight: content.implicitHeight + margin * 2

    QGCPalette {
        id: palette
    }

    Rectangle {
        anchors.fill: parent
        border.color: palette.windowShadeLight
        color: palette.window
        radius: root.margin
    }

    ColumnLayout {
        id: content

        spacing: root.margin
        width: Math.max(0, root.width - root.margin * 2)
        x: root.margin
        y: root.margin

        GridLayout {
            Layout.fillWidth: true
            columnSpacing: root.margin * 2
            columns: {
                if (width >= ScreenTools.defaultFontPixelWidth * 100)
                    return 4;
                if (width >= ScreenTools.defaultFontPixelWidth * 48)
                    return 2;
                return 1;
            }
            rowSpacing: root.margin
            uniformCellWidths: true

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.client.statusText
                wrapMode: Text.Wrap
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Command: %1").arg(root.client.commandText)
                wrapMode: Text.Wrap
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.client.targetText
                wrapMode: Text.Wrap
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.client.navigationText
                wrapMode: Text.Wrap
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.client.availabilityReason
            visible: root.client.connected && text.length > 0
            wrapMode: Text.Wrap
        }
    }
}
