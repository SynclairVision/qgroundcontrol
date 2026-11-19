import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var client
    readonly property var _target: client ? client.targetView : ({})
    readonly property string _controlText: {
        if (!_target || !_target.available)
            return qsTr("Unavailable")
        switch (_target.controlOwner) {
        case 1: return qsTr("Manual")
        case 2: return qsTr("Autopilot")
        case 3: return qsTr("DCLink")
        default: return qsTr("Unknown")
        }
    }
    readonly property string _targetText: {
        if (!_target || !_target.supported)
            return client.targetText
        if (!_target.available)
            return qsTr("Target data unavailable")
        if (!_target.present)
            return qsTr("No fresh target")
        return _target.driving && _target.controlOwner === 3
            ? qsTr("Target driving control") : qsTr("Target observed")
    }
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
                text: qsTr("Command: %1 · Control: %2").arg(root.client.commandText).arg(root._controlText)
                wrapMode: Text.Wrap
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root._targetText
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
