pragma ComponentBehavior: Bound

import QGroundControl
import QGroundControl.Controls

import QtQuick
import QtQuick.Layouts

Item {
    id: root

    readonly property bool _available: !!_target && _target.available === true
    readonly property real _distanceM: _positionValid ? Math.hypot(_target.relativeNorthM, _target.relativeEastM) : 0
    readonly property bool _headingValid: Number.isFinite(aircraftHeadingDeg)
        && aircraftHeadingDeg >= 0 && aircraftHeadingDeg <= 360
    readonly property bool _orientationAvailable: !headingUp || _headingValid
    readonly property real _rotationDeg: headingUp && _headingValid ? aircraftHeadingDeg : 0
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
            return qsTr("No target");
        if (!root._orientationAvailable)
            return qsTr("Heading unavailable");
        if (_positionValid)
            return "";
        if (_rangeValid)
            return qsTr("Range only");
        return qsTr("Position unavailable");
    }
    readonly property var _target: client ? client.targetView : ({})
    property real aircraftHeadingDeg: NaN
    required property var client
    property bool expanded: true
    property bool headingUp: false
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    property real maximumHeight: parent ? parent.height : implicitHeight
    readonly property real minimumHeight: readout.implicitHeight + margin * 2
        + (stateLabel.visible ? stateLabel.implicitHeight + margin : 0)
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
                    return root.distanceText(root._distanceM);
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
            visible: text.length > 0
            wrapMode: Text.Wrap
        }

        Item {
            id: plot

            readonly property real padding: ScreenTools.defaultFontPixelHeight * 0.9
            readonly property real radius: Math.max(0, width / 2 - padding)
            readonly property real headingRadians: root._rotationDeg * Math.PI / 180
            readonly property real targetRightM: root._positionValid
                ? root._target.relativeEastM * Math.cos(headingRadians)
                    - root._target.relativeNorthM * Math.sin(headingRadians) : 0
            readonly property real targetForwardM: root._positionValid
                ? root._target.relativeNorthM * Math.cos(headingRadians)
                    + root._target.relativeEastM * Math.sin(headingRadians) : 0
            readonly property real targetX: width / 2 + targetRightM / root._radiusM * radius
            readonly property real targetY: height / 2 - targetForwardM / root._radiusM * radius

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

            Repeater {
                model: [qsTr("N"), qsTr("E"), qsTr("S"), qsTr("W")]

                QGCLabel {
                    required property int index
                    required property string modelData
                    readonly property real bearingRadians: (index * 90 - root._rotationDeg) * Math.PI / 180
                    readonly property real labelRadius: plot.radius + plot.padding * 0.55

                    font.pointSize: ScreenTools.smallFontPointSize
                    text: modelData
                    visible: root._orientationAvailable
                    x: plot.width / 2 + Math.sin(bearingRadians) * labelRadius - width / 2
                    y: plot.height / 2 - Math.cos(bearingRadians) * labelRadius - height / 2
                }
            }

            QGCLabel {
                Accessible.name: root._headingValid ? qsTr("Aircraft position and heading") : qsTr("Aircraft position")
                anchors.centerIn: parent
                rotation: root.headingUp || !root._headingValid ? 0 : root.aircraftHeadingDeg
                text: root._headingValid ? "▲" : "+"
            }

            Rectangle {
                Accessible.name: qsTr("Target relative position")
                border.color: palette.text
                border.width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
                color: palette.brandPrimary
                height: width
                radius: width / 2
                visible: root._positionValid && root._orientationAvailable
                width: ScreenTools.defaultFontPixelHeight * 0.8
                x: plot.targetX - width / 2
                y: plot.targetY - height / 2
            }
        }

        QGCLabel {
            id: legend

            Layout.fillWidth: true
            Accessible.name: qsTr("Outer ring %1").arg(root.distanceText(root._radiusM))
            font.pointSize: ScreenTools.smallFontPointSize
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("%1 · %2")
                .arg(root.headingUp ? (root._headingValid ? qsTr("HDG ↑") : qsTr("HDG —")) : qsTr("N ↑"))
                .arg(root.distanceText(root._radiusM))
            visible: root.plotVisible
            wrapMode: Text.Wrap
        }
    }
}
