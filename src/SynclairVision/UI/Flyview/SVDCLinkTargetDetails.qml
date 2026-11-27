import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    readonly property bool _present: !!_target && _target.available === true && _target.present === true
    readonly property var _target: client ? client.targetView : ({})
    required property var client
    property bool mapAvailable: false
    property bool rangeRings: false

    signal rangeRingsRequested(bool enabled)

    function valueText(value: var, digits: int, suffix: string): string {
        return typeof value === "number" && Number.isFinite(value) ? value.toFixed(digits) + suffix : "—";
    }

    spacing: ScreenTools.defaultFontPixelHeight * 0.5

    function fieldText(name: string, digits: int, suffix: string): string {
        return valueText(_present ? _target[name] : null, digits, suffix)
    }

    function ageText(value: var): string {
        return _present && typeof value === "number" ? valueText(value / 1000, 1, qsTr(" s")) : "—"
    }

    function metadata(name: string): string {
        return _present && typeof _target[name] === "string" && _target[name].length > 0 ? _target[name] : "—"
    }

    QGCLabel {
        Layout.fillWidth: true
        text: root._present ? qsTr("Selected runtime target") : qsTr("No fresh target")
        wrapMode: Text.Wrap
    }
    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Track: %1 · Source: %2").arg(root.metadata("targetId")).arg(root.metadata("source"))
        textFormat: Text.PlainText
        wrapMode: Text.WrapAnywhere
    }
    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Age: %1 · Measurement age: %2")
            .arg(root.ageText(root._target ? root._target.ageMs : null))
            .arg(root.ageText(root._target ? root._target.sourceAgeMs : null))
        wrapMode: Text.Wrap
    }
    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Azimuth: %1 · Elevation: %2")
            .arg(root.fieldText("azimuthDeg", 1, "°")).arg(root.fieldText("elevationDeg", 1, "°"))
        wrapMode: Text.Wrap
    }
    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Estimated horizontal range: %1").arg(root._present && root._target.rangeSource === 1
            ? root.fieldText("horizontalRangeM", 1, qsTr(" m")) : "—")
        wrapMode: Text.Wrap
    }
    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Latitude: %1 · Longitude: %2")
            .arg(root.fieldText("latitudeDeg", 6, "°")).arg(root.fieldText("longitudeDeg", 6, "°"))
        wrapMode: Text.Wrap
    }
    QGCCheckBox {
        Layout.fillWidth: true
        text: qsTr("Map range rings")
        checked: root.rangeRings
        enabled: root.mapAvailable
        onClicked: root.rangeRingsRequested(checked)
    }
    QGCLabel {
        Layout.fillWidth: true
        text: root.mapAvailable ? qsTr("Map rings show half and full aircraft-to-target map distance.")
            : qsTr("Geographic target overlays use the standard QGC map.")
        wrapMode: Text.Wrap
        font.pointSize: ScreenTools.smallFontPointSize
    }
}
