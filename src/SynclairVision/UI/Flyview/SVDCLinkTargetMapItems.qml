import QGroundControl
import QGroundControl.Controls
import QtLocation
import QtPositioning
import QtQuick

MapItemGroup {
    id: root

    readonly property real _contextRange: _lineValid ? aircraftCoordinate.distanceTo(_coordinate) : 0
    readonly property var _coordinate: _valid
        ? QtPositioning.coordinate(_target.latitudeDeg, _target.longitudeDeg) : QtPositioning.coordinate()
    readonly property bool _lineValid: _valid && !!aircraftCoordinate && aircraftCoordinate.isValid
    readonly property var _target: client ? client.targetView : ({})
    readonly property bool _valid: !!_target && _target.available === true && _target.present === true
        && typeof _target.latitudeDeg === "number" && Number.isFinite(_target.latitudeDeg)
        && typeof _target.longitudeDeg === "number" && Number.isFinite(_target.longitudeDeg)
        && Math.abs(_target.latitudeDeg) <= 90 && Math.abs(_target.longitudeDeg) <= 180
    property var aircraftCoordinate: QtPositioning.coordinate()
    required property var client
    required property var mapControl
    property bool rangeRings: false
    property bool compact: false

    signal detailsRequested

    property var _attachedMap: null
    property bool _ready: false

    function attachMap() {
        if (_attachedMap === mapControl)
            return
        detachMap()
        if (mapControl && typeof mapControl.addMapItemGroup === "function") {
            root.parent = mapControl
            mapControl.addMapItemGroup(root)
            _attachedMap = mapControl
        }
    }

    function detachMap() {
        // QtLocation requires removing groups before their children are destroyed.
        if (_attachedMap && typeof _attachedMap.removeMapItemGroup === "function")
            _attachedMap.removeMapItemGroup(root)
        _attachedMap = null
    }

    onMapControlChanged: { if (_ready) attachMap() }
    Component.onCompleted: {
        _ready = true
        attachMap()
    }
    Component.onDestruction: detachMap()

    QGCPalette {
        id: palette
    }

    MapPolyline {
        path: root._lineValid ? [root.aircraftCoordinate, root._coordinate] : []
        line.color: palette.text
        line.width: Math.max(2, ScreenTools.defaultFontPixelWidth * 0.5)
        visible: root._lineValid
    }

    MapPolyline {
        line.color: palette.brandPrimary
        line.width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.25)
        path: root._lineValid ? [root.aircraftCoordinate, root._coordinate] : []
        visible: root._lineValid
    }

    MapCircle {
        border.color: palette.brandPrimary
        border.width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
        center: root._lineValid ? root.aircraftCoordinate : QtPositioning.coordinate()
        color: "transparent"
        radius: root._contextRange
        visible: root.rangeRings && root._lineValid && root._contextRange > 0
    }

    MapCircle {
        border.color: palette.brandPrimary
        border.width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
        center: root._lineValid ? root.aircraftCoordinate : QtPositioning.coordinate()
        color: "transparent"
        radius: root._contextRange / 2
        visible: root.rangeRings && root._lineValid && root._contextRange > 0
    }

    MapQuickItem {
        anchorPoint.x: marker.width / 2
        anchorPoint.y: marker.height / 2
        coordinate: root._coordinate
        visible: root._valid
        z: QGroundControl.zOrderMapItems + 1

        sourceItem: Rectangle {
            id: marker

            Accessible.name: root.compact ? qsTr("DCLink target; enlarge map for details")
                : qsTr("DCLink target; open read-only details")
            border.color: palette.text
            color: palette.brandPrimary
            height: root.compact ? ScreenTools.defaultFontPixelHeight * 0.9
                : targetLabel.implicitHeight + ScreenTools.defaultFontPixelHeight
            radius: height / 2
            width: root.compact ? height : targetLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 2

            QGCLabel {
                id: targetLabel

                anchors.centerIn: parent
                color: palette.text
                text: qsTr("Target")
                visible: !root.compact
            }

            MouseArea {
                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                enabled: !root.compact
                preventStealing: false

                onClicked: root.detailsRequested()
            }
        }
    }
}
