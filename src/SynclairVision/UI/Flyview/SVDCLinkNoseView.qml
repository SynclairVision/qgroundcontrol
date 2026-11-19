pragma ComponentBehavior: Bound

import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    readonly property bool _available: !!_target && _target.available === true
    readonly property bool _azimuthValid: _present && validAngle(_target.azimuthDeg, 180)
    readonly property bool _behind: _azimuthValid && (_target.behind === true || Math.abs(_target.azimuthDeg) > 90)
    readonly property bool _elevationValid: _present && validAngle(_target.elevationDeg, 90)
    readonly property real _expandedHeight: minimumHeight + plot.width + legend.implicitHeight + margin * 2
    readonly property real _horizontal: {
        if (!_azimuthValid)
            return 0;
        if (_behind)
            return _target.azimuthDeg >= 0 ? 1 : -1;
        return Math.max(-1, Math.min(1, _target.azimuthDeg / 60));
    }
    readonly property bool _outside: {
        if (_azimuthValid && Math.abs(_target.azimuthDeg) > 60)
            return true;
        return _elevationValid && Math.abs(_target.elevationDeg) > 60;
    }
    readonly property bool _present: _available && _target.present === true
    readonly property string _stateText: {
        if (!_available)
            return client && client.connected ? qsTr("Target data unavailable") : qsTr("DCLink offline");
        if (!_present)
            return qsTr("No fresh target");
        if (!_azimuthValid && !_elevationValid)
            return qsTr("Angular data unavailable");
        if (!_azimuthValid)
            return qsTr("Azimuth unavailable");
        if (_behind)
            return qsTr("Target behind aircraft");
        if (_outside)
            return qsTr("Target outside view");
        if (!_elevationValid)
            return qsTr("Elevation unavailable");
        return qsTr("Target ahead");
    }
    readonly property var _target: client ? client.targetView : ({})
    readonly property real _vertical: _elevationValid ? Math.max(-1, Math.min(1, -_target.elevationDeg / 60)) : 0
    required property var client
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.5
    property real maximumHeight: parent ? parent.height : implicitHeight
    readonly property real minimumHeight: header.implicitHeight + readout.implicitHeight
        + stateLabel.implicitHeight + margin * 4
    readonly property bool plotVisible: header.checked && maximumHeight >= _expandedHeight

    function angleText(value: real): string {
        return (Math.abs(value) < 0.05 ? 0 : value).toFixed(1) + "°";
    }

    function validAngle(value: var, limit: real): bool {
        return typeof value === "number" && Number.isFinite(value) && Math.abs(value) <= limit;
    }

    height: implicitHeight
    implicitHeight: content.implicitHeight + margin * 2
    width: ScreenTools.defaultFontPixelHeight * 15

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

        QGCButton {
            id: header

            Accessible.description: qsTr("Show or hide aircraft-relative target angles")
            Accessible.name: qsTr("Nose view")
            Layout.fillWidth: true
            checkable: true
            checked: true
            enabled: root.maximumHeight >= root._expandedHeight
            text: root.plotVisible ? qsTr("Nose view ▾") : qsTr("Nose view ▸")
        }

        QGCLabel {
            id: readout

            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("AZ %1   EL %2").arg(root._azimuthValid ? root.angleText(root._target.azimuthDeg) : "—").arg(root._elevationValid ? root.angleText(root._target.elevationDeg) : "—")
        }

        QGCLabel {
            id: stateLabel

            Layout.fillWidth: true
            color: root._behind || root._outside ? palette.warningText : palette.text
            horizontalAlignment: Text.AlignHCenter
            text: root._stateText
            wrapMode: Text.Wrap
        }

        Item {
            id: plot

            readonly property real padding: ScreenTools.defaultFontPixelHeight * 0.9
            readonly property real span: Math.max(0, width / 2 - padding)
            readonly property real targetX: width / 2 + root._horizontal * span
            readonly property real targetY: height / 2 + root._vertical * span

            Layout.fillWidth: true
            implicitHeight: width
            visible: root.plotVisible

            Rectangle {
                anchors.fill: parent
                anchors.margins: plot.padding
                border.color: palette.windowShadeLight
                color: palette.windowShade
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    color: palette.windowShadeLight
                    height: plot.span * 2
                    width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.1)
                    x: plot.padding + index * plot.span
                    y: plot.padding
                }
            }

            Repeater {
                model: 3

                Rectangle {
                    required property int index

                    color: palette.windowShadeLight
                    height: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.1)
                    width: plot.span * 2
                    x: plot.padding
                    y: plot.padding + index * plot.span
                }
            }

            QGCLabel {
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("L")
                x: 0
            }

            QGCLabel {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("R")
            }

            QGCLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("Up")
            }

            QGCLabel {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("Down")
            }

            QGCLabel {
                Accessible.name: qsTr("Aircraft nose")
                anchors.centerIn: parent
                color: palette.text
                text: "+"
            }

            // A missing axis is a band, never an invented centred target point.
            Rectangle {
                color: palette.warningText
                height: plot.span * 2
                visible: root._azimuthValid && !root._elevationValid
                width: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
                x: plot.targetX - width / 2
                y: plot.padding
            }

            Rectangle {
                color: palette.warningText
                height: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.2)
                visible: root._elevationValid && !root._azimuthValid
                width: plot.span * 2
                x: plot.padding
                y: plot.targetY - height / 2
            }

            Rectangle {
                Accessible.name: qsTr("Target direction")
                border.color: palette.text
                color: palette.brandPrimary
                height: width
                radius: width / 2
                visible: root._azimuthValid && root._elevationValid && !root._behind && !root._outside
                width: ScreenTools.defaultFontPixelHeight * 0.6
                x: plot.targetX - width / 2
                y: plot.targetY - height / 2
            }

            QGCLabel {
                Accessible.name: root._stateText
                color: palette.warningText
                text: {
                    if (root._behind || Math.abs(root._horizontal) >= Math.abs(root._vertical))
                        return root._horizontal >= 0 ? "▶" : "◀";
                    return root._vertical >= 0 ? "▼" : "▲";
                }
                visible: root._azimuthValid && root._elevationValid && (root._behind || root._outside)
                x: plot.targetX - width / 2
                y: plot.targetY - height / 2
            }
        }

        QGCLabel {
            id: legend

            Layout.fillWidth: true
            font.pointSize: ScreenTools.smallFontPointSize
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Direction only · ±60°")
            visible: root.plotVisible
        }
    }
}
