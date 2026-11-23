import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    readonly property real _contentHeight: Math.max(0, maximumHeight - selector.height
        - detailsButton.implicitHeight - margin * 2)
    readonly property bool _plotVisible: radarSelected ? radar.plotVisible : nose.plotVisible
    required property var client
    property bool rangeRings: false
    property bool mapAvailable: false
    property bool headingUp: false
    property real aircraftHeadingDeg: NaN
    property var _detailsVehicleId: null
    readonly property var _target: client ? client.targetView : ({})

    signal viewPreferenceChanged(bool radarSelected, bool expanded)
    signal rangeRingsRequested(bool enabled)

    function openDetails() {
        if (!_target || !_target.available || !_target.present)
            return
        _detailsVehicleId = _target.vehicleSystemId
        detailsPopup.open()
    }

    on_TargetChanged: {
        if (detailsPopup.opened && (!_target || !_target.available || !_target.present
                || _target.vehicleSystemId !== _detailsVehicleId))
            detailsPopup.close()
    }
    onVisibleChanged: {
        if (!visible)
            detailsPopup.close()
    }
    property bool expanded: true
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    property real maximumHeight: parent ? parent.height : height
    readonly property real minimumHeight: selector.height + detailsButton.implicitHeight + margin * 2
        + Math.max(nose.minimumHeight, radar.minimumHeight)
    property bool radarSelected: false

    height: selector.height + detailsButton.implicitHeight + margin * 2 + (radarSelected ? radar.height : nose.height)
    width: ScreenTools.defaultFontPixelHeight * 15

    RowLayout {
        id: selector

        spacing: root.margin
        width: root.width

        SVDCLinkButton {
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
                root.viewPreferenceChanged(root.radarSelected, root.expanded);
            }
        }

        SVDCLinkButton {
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
                root.viewPreferenceChanged(root.radarSelected, root.expanded);
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
        headingUp: root.headingUp
        aircraftHeadingDeg: root.aircraftHeadingDeg
        maximumHeight: root._contentHeight
        visible: root.radarSelected
        width: root.width
        y: selector.height + root.margin
    }

    RowLayout {
        id: detailsButton

        y: root.height - height
        width: root.width
        spacing: root.margin

        SVDCLinkButton {
            Layout.fillWidth: true
            text: qsTr("Details")
            enabled: !!root._target && root._target.available === true && root._target.present === true
            onClicked: root.openDetails()
        }
    }

    Popup {
        id: detailsPopup

        parent: Overlay.overlay
        width: Math.min(ScreenTools.defaultFontPixelHeight * 22, parent ? parent.width - root.margin * 2 : 0)
        height: Math.min(detailsContents.implicitHeight + padding * 2,
                         parent ? parent.height - root.margin * 2 : 0)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: root.margin
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: detailsPalette.window
            border.color: detailsPalette.windowShadeLight
            radius: root.margin
        }

        QGCPalette { id: detailsPalette }

        contentItem: ScrollView {
            id: detailsScroll

            clip: true
            contentWidth: availableWidth
            contentHeight: detailsContents.implicitHeight

            ColumnLayout {
                id: detailsContents

                width: detailsScroll.availableWidth
                spacing: root.margin

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Target details")
                    font.bold: true
                    wrapMode: Text.Wrap
                }

                SVDCLinkTargetDetails {
                    Layout.fillWidth: true
                    client: root.client
                    rangeRings: root.rangeRings
                    mapAvailable: root.mapAvailable
                    onRangeRingsRequested: (enabled) => root.rangeRingsRequested(enabled)
                }

                SVDCLinkButton {
                    Layout.fillWidth: true
                    text: qsTr("Close")
                    onClicked: detailsPopup.close()
                }
            }
        }
    }
}
