import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var client
    property string layoutWarning: ""
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.25
    readonly property real minimumWidth: ScreenTools.defaultFontPixelWidth * 28 + _rowHeight + margin * 2
    readonly property real _rowHeight: ScreenTools.defaultFontPixelHeight * 1.5
    readonly property var _target: client ? client.targetView : ({})
    readonly property bool _connected: !!client && client.connected === true
    readonly property string _controlText: {
        if (!_target || !_target.available || _target.vehicleBound === false)
            return qsTr("Unavailable")
        switch (_target.controlOwner) {
        case 1: return qsTr("Manual")
        case 2: return qsTr("Autopilot")
        case 3: return qsTr("DCLink")
        default: return qsTr("Unknown")
        }
    }
    readonly property string _commandAlert: {
        if (!client)
            return ""
        const states = []
        if (client.commandStateText.length > 0)
            states.push(client.commandStateText)
        if (client.commandOutcomeUnknown)
            states.push(qsTr("Check command"))
        else if (client.commandPending)
            states.push(qsTr("Pending"))
        return states.join(" · ")
    }
    readonly property bool _navigationWarning: _connected && !client.navigationHealthy
    readonly property string _alertText: [_commandAlert,
        _navigationWarning ? client.navigationText : ""]
        .filter((text) => text.length > 0).join(" · ")
    readonly property bool _warning: _navigationWarning || (!!client && (client.commandOutcomeUnknown
        || client.commandStateText.length > 0))

    height: implicitHeight
    implicitHeight: content.implicitHeight + margin * 2
    implicitWidth: connectionIndicator.implicitWidth + commandLabel.implicitWidth + controlLabel.implicitWidth
        + targetLabel.implicitWidth + optionsButton.width + margin * 10

    signal optionsRequested()

    function openDetails() {
        detailsPopup.open()
    }

    onVisibleChanged: {
        if (!visible)
            detailsPopup.close()
    }

    QGCPalette {
        id: palette
    }

    Rectangle {
        anchors.fill: parent
        border.color: palette.windowShadeLight
        color: palette.window
        radius: root.margin
    }

    Column {
        id: content

        spacing: root.margin
        width: Math.max(0, root.width - root.margin * 2)
        x: root.margin
        y: root.margin

        Flow {
            id: statusFields

            spacing: root.margin * 2
            width: Math.max(0, parent.width - optionsButton.width - root.margin * 2)

            RowLayout {
                id: connectionIndicator

                height: root._rowHeight
                spacing: root.margin

                QGCButton {
                    id: connectionButton

                    Accessible.description: root._connected ? qsTr("Connected; open DCLink details")
                        : qsTr("Disconnected; open DCLink details")
                    Layout.preferredHeight: root._rowHeight
                    bottomPadding: 0
                    implicitWidth: connectionMetrics.width + leftPadding + rightPadding
                    leftPadding: root.margin
                    rightPadding: root.margin
                    text: qsTr("DCLink")
                    topPadding: 0

                    background: Rectangle {
                        color: connectionButton.hovered ? palette.windowShadeLight : "transparent"
                        radius: root.margin
                    }

                    onClicked: root.openDetails()

                    ToolTip.delay: 500
                    ToolTip.text: root.client ? root.client.statusText : qsTr("DCLink disconnected")
                    ToolTip.visible: hovered
                }

                Rectangle {
                    Accessible.name: root._connected ? qsTr("DCLink connected") : qsTr("DCLink disconnected")
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 0.5
                    Layout.preferredWidth: height
                    border.color: palette.text
                    color: root._connected ? palette.colorGreen : palette.colorRed
                    radius: height / 2
                }
            }

            QGCLabel {
                id: commandLabel

                height: Math.max(root._rowHeight, implicitHeight)
                text: qsTr("Command: %1").arg(root.client ? root.client.commandNameText : qsTr("Unavailable"))
                textFormat: Text.PlainText
                verticalAlignment: Text.AlignVCenter
                width: Math.min(implicitWidth, statusFields.width)
                wrapMode: Text.Wrap
            }

            QGCLabel {
                id: controlLabel

                height: Math.max(root._rowHeight, implicitHeight)
                text: qsTr("Control: %1").arg(root._controlText)
                textFormat: Text.PlainText
                verticalAlignment: Text.AlignVCenter
                width: Math.min(implicitWidth, statusFields.width)
                wrapMode: Text.Wrap
            }

            QGCLabel {
                id: targetLabel

                height: Math.max(root._rowHeight, implicitHeight)
                text: qsTr("Target: %1").arg(root.client ? root.client.targetStateText : qsTr("Unavailable"))
                textFormat: Text.PlainText
                verticalAlignment: Text.AlignVCenter
                width: Math.min(implicitWidth, statusFields.width)
                wrapMode: Text.Wrap
            }
        }

        QGCLabel {
            color: root._warning ? palette.warningText : palette.text
            text: root._alertText
            textFormat: Text.PlainText
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.Wrap
        }
    }

    SVDCLinkButton {
        id: optionsButton

        Accessible.name: qsTr("DCLink options")
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.margin
        width: root._rowHeight
        height: root._rowHeight
        topPadding: 0
        bottomPadding: 0
        leftPadding: 0
        rightPadding: 0
        iconSource: "qrc:/qmlimages/settings_general.svg"
        description: root.layoutWarning.length > 0
            ? qsTr("DCLink options · %1").arg(root.layoutWarning) : qsTr("DCLink options")

        QGCLabel {
            anchors.right: parent.right
            anchors.top: parent.top
            text: "!"
            color: palette.warningText
            font.bold: true
            visible: root.layoutWarning.length > 0
        }
        onClicked: root.optionsRequested()
    }

    TextMetrics {
        id: connectionMetrics

        font.family: ScreenTools.normalFontFamily
        font.pointSize: ScreenTools.defaultFontPointSize
        text: qsTr("DCLink")
    }

    Popup {
        id: detailsPopup

        parent: Overlay.overlay
        width: Math.max(0, Math.min(ScreenTools.defaultFontPixelHeight * 24,
                                   parent ? parent.width - root.margin * 4 : 0))
        height: Math.max(0, Math.min(detailsContents.implicitHeight + padding * 2,
                                    parent ? parent.height - root.margin * 4 : 0))
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: root.margin * 2
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            border.color: palette.windowShadeLight
            color: palette.window
            radius: root.margin
        }

        contentItem: ScrollView {
            id: detailsScroll

            clip: true
            contentWidth: availableWidth
            contentHeight: detailsContents.implicitHeight

            ColumnLayout {
                id: detailsContents

                spacing: root.margin * 2
                width: detailsScroll.availableWidth

                QGCLabel {
                    Layout.fillWidth: true
                    font.bold: true
                    text: qsTr("DCLink status")
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.client ? root.client.statusText : qsTr("DCLink disconnected")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.client ? root.client.vehicleText : ""
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Command: %1").arg(root.client ? root.client.commandText : qsTr("Unavailable"))
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.client ? root.client.navigationText : ""
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.client ? root.client.availabilityReason : ""
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root.client ? root.client.feedback : ""
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    wrapMode: Text.Wrap
                }

                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("Close")
                    onClicked: detailsPopup.close()
                }
            }
        }
    }
}
