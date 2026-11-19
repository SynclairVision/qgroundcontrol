import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    readonly property real _contentHeight: Math.max(0, maximumHeight - selector.height - margin)
    readonly property bool _plotVisible: radarSelected ? radar.plotVisible : nose.plotVisible
    required property var client
    property bool expanded: true
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    property real maximumHeight: parent ? parent.height : height
    readonly property real minimumHeight: selector.height + margin + Math.max(nose.minimumHeight, radar.minimumHeight)
    property bool radarSelected: false

    height: selector.height + margin + (radarSelected ? radar.height : nose.height)
    width: ScreenTools.defaultFontPixelHeight * 15

    RowLayout {
        id: selector

        spacing: root.margin
        width: root.width

        QGCButton {
            Accessible.description: qsTr("Select nose view; select again to show or hide its plot")
            Accessible.name: qsTr("Nose view")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            autoExclusive: true
            checkable: true
            checked: !root.radarSelected
            text: !root.radarSelected ? (root._plotVisible ? qsTr("Nose ▾") : qsTr("Nose ▸")) : qsTr("Nose")

            onClicked: {
                root.expanded = root.radarSelected || !root.expanded;
                root.radarSelected = false;
            }
        }

        QGCButton {
            Accessible.description: qsTr("Select target radar; select again to show or hide its plot")
            Accessible.name: qsTr("Target radar")
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            autoExclusive: true
            checkable: true
            checked: root.radarSelected
            text: root.radarSelected ? (root._plotVisible ? qsTr("Radar ▾") : qsTr("Radar ▸")) : qsTr("Radar")

            onClicked: {
                root.expanded = !root.radarSelected || !root.expanded;
                root.radarSelected = true;
            }
        }
    }

    SVDCLinkNoseView {
        id: nose

        client: root.client
        expanded: root.expanded
        maximumHeight: root._contentHeight
        showHeader: false
        visible: !root.radarSelected
        width: root.width
        y: selector.height + root.margin
    }

    SVDCLinkTargetRadar {
        id: radar

        client: root.client
        expanded: root.expanded
        maximumHeight: root._contentHeight
        visible: root.radarSelected
        width: root.width
        y: selector.height + root.margin
    }
}
