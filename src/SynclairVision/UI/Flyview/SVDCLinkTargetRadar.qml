pragma ComponentBehavior: Bound

import QGroundControl
import QGroundControl.Controls

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    readonly property bool _available: !!_target && _target.available === true
    readonly property real _distanceM: _positionValid ? Math.hypot(_target.relativeNorthM, _target.relativeEastM) : 0
    readonly property bool _positionValid: _present && finiteOffset(_target.relativeNorthM)
        && finiteOffset(_target.relativeEastM)
    readonly property bool _present: _available && _target.present === true
    property real _radiusM: 50
    readonly property bool _rangeValid: _present && _target.rangeSource === 1
        && finiteOffset(_target.horizontalRangeM) && _target.horizontalRangeM >= 0
    readonly property string _stateText: {
        if (!_available)
            return client && client.connected ? qsTr("Target data unavailable") : qsTr("DCLink offline");
        if (!_present)
            return qsTr("No fresh target");
        if (_positionValid)
            return qsTr("Relative target position");
        if (_rangeValid)
            return qsTr("Range only · position unavailable");
        return qsTr("Position unavailable · see Nose");
    }
    readonly property var _target: client ? client.targetView : ({})
    required property var client
    property bool expanded: true
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    property real maximumHeight: parent ? parent.height : implicitHeight
    readonly property real minimumHeight: readout.implicitHeight + stateLabel.implicitHeight + margin * 3
    readonly property bool plotVisible: expanded
        && maximumHeight >= minimumHeight + plot.width + legend.implicitHeight + margin * 2

    function distanceText(value: real): string {
        if (value >= 1000)
            return qsTr("%1 km").arg((value / 1000).toFixed(value < 10000 ? 1 : 0));
        return qsTr("%1 m").arg(value.toFixed(value < 10 ? 1 : 0));
    }

    function finiteOffset(value: var): bool {
        return typeof value === "number" && Number.isFinite(value) && Math.abs(value) <= 1e9;
    }

    function updateScale() {
        if (!_positionValid) {
            _radiusM = 50;
            return;
        }
        // Keep the scale stable while the target stays comfortably inside the outer ring.
        if (_distanceM <= _radiusM * 0.9 && _distanceM >= _radiusM * 0.2)
            return;
        const desired = Math.max(50, _distanceM / 0.75);
        const decade = Math.pow(10, Math.floor(Math.log10(desired)));
        const fraction = desired / decade;
        _radiusM = (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10) * decade;
    }

    height: implicitHeight
    implicitHeight: content.implicitHeight + margin * 2
    width: ScreenTools.defaultFontPixelHeight * 15

    Component.onCompleted: updateScale()
    on_DistanceMChanged: updateScale()
    on_PositionValidChanged: updateScale()

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

        QGCLabel {
            id: readout

            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: {
                if (root._positionValid)
                    return qsTr("Offset %1").arg(root.distanceText(root._distanceM));
                if (root._rangeValid)
                    return qsTr("Est. range %1").arg(root.distanceText(root._target.horizontalRangeM));
                return qsTr("Range unavailable");
            }
            wrapMode: Text.Wrap
        }

        QGCLabel {
            id: stateLabel

            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root._stateText
            wrapMode: Text.Wrap
        }

        Item {
            id: plot

            readonly property real padding: ScreenTools.defaultFontPixelHeight * 0.9
            readonly property real radius: Math.max(0, width / 2 - padding)
            readonly property real targetX: root._positionValid
                ? width / 2 + root._target.relativeEastM / root._radiusM * radius : width / 2
            readonly property real targetY: root._positionValid
                ? height / 2 - root._target.relativeNorthM / root._radiusM * radius : height / 2

            Layout.fillWidth: true
            implicitHeight: width
            visible: root.plotVisible

            Rectangle {
                anchors.centerIn: parent
                color: palette.windowShade
                height: width
                radius: width / 2
                width: plot.radius * 2
            }

            Repeater {
                model: 2

                Rectangle {
                    required property int index

                    anchors.centerIn: parent
                    border.color: palette.windowShadeLight
                    color: "transparent"
                    height: width
                    radius: width / 2
                    width: plot.radius * (index + 1)
                }
            }

            Rectangle {
                anchors.centerIn: parent
                color: palette.windowShadeLight
                height: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.1)
                width: plot.radius * 2
            }

            Rectangle {
                anchors.centerIn: parent
                color: palette.windowShadeLight
                height: plot.radius * 2
                width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.1)
            }

            QGCLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("N")
            }

            QGCLabel {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("E")
            }

            QGCLabel {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("S")
            }

            QGCLabel {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("W")
            }

            QGCLabel {
                Accessible.name: qsTr("Aircraft position")
                anchors.centerIn: parent
                text: "+"
            }

            Rectangle {
                Accessible.name: qsTr("Target relative position")
                border.color: palette.text
                border.width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
                color: palette.brandPrimary
                height: width
                radius: width / 2
                visible: root._positionValid
                width: ScreenTools.defaultFontPixelHeight * 0.8
                x: plot.targetX - width / 2
                y: plot.targetY - height / 2
            }
        }

        QGCLabel {
            id: legend

            Layout.fillWidth: true
            font.pointSize: ScreenTools.smallFontPointSize
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("North-up · outer ring %1").arg(root.distanceText(root._radiusM))
            visible: root.plotVisible
            wrapMode: Text.Wrap
        }
    }
}
