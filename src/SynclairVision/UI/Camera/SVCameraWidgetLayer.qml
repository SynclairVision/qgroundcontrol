import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models
import QtQuick.Shapes 2.15

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap

Item {
    id: root

    property var  parentToolInsets
    property real _heading:  NaN
    property real _pitch:    NaN
    property real _roll:     NaN
    property bool _inverted: false
    property var vehicle: globals.activeVehicle
    property real _rollAngle: (Number.isFinite(_roll) ? _roll : vehicle ? vehicle.roll.rawValue : 0)
        + (_inverted ? 180 : 0)
    property int  cameraSlot
    property int index
    readonly property real compassScale: SVSettings.compassSize / 100

    QGCPalette { id: qgcPalette }

    QtObject {
        id: cameraVehicle

        readonly property bool armed: root.vehicle ? root.vehicle.armed : false
        readonly property QtObject roll: QtObject {
            readonly property real rawValue: root._rollAngle
        }
        readonly property QtObject pitch: QtObject {
            readonly property real rawValue: Number.isFinite(root._pitch)
                ? root._pitch
                : root.vehicle ? root.vehicle.pitch.rawValue : 0
        }
        readonly property QtObject heading: QtObject {
            readonly property real rawValue: Number.isFinite(root._heading)
                ? root._heading
                : root.vehicle ? root.vehicle.heading.rawValue : 0
        }
        readonly property QtObject headingToHome: QtObject {
            readonly property real rawValue: NaN
        }
        readonly property QtObject groundSpeed: QtObject {
            readonly property real rawValue: 0
        }
        readonly property QtObject headingToNextWP: QtObject {
            readonly property real rawValue: NaN
        }
        readonly property QtObject gps: QtObject {
            readonly property QtObject courseOverGround: QtObject {
                readonly property real rawValue: 0
            }
        }
    }

    function wrapPitchFull(pitch) {
        var normalizedPitch = Math.abs(pitch % 360)          // 0..360
        var segment = Math.floor(normalizedPitch / 90)   // segment: 0,1,2,3
        var remainder = normalizedPitch - segment * 90           // remainder within segment: 0..90

        var newPitch, inverted
        switch (segment) {
        case 0: newPitch = remainder;         inverted = false; break  // 0   -> 90
        case 1: newPitch = 90 - remainder;    inverted = true;  break  // 90  -> 180 (back to 0, upside down)
        case 2: newPitch = -remainder;        inverted = true;  break  // 180 -> 270 (0 -> -90, still upside down)
        case 3: newPitch = -(90 - remainder); inverted = false; break  // 270 -> 360 (-90 -> 0, flips back normal)
        default: newPitch = 0;        inverted = false; break
        }

        return {
            pitch:    (pitch % 360) < 0 ? -newPitch : newPitch,
            inverted: inverted
        }
    }

    function resetCameraOverrides() {
        _heading = NaN
        _pitch = NaN
        _roll = NaN
        _inverted = false
    }

    onCameraSlotChanged: resetCameraOverrides()

    Loader {
        anchors.right:  parent.right
        anchors.bottom: parent.bottom

        sourceComponent: {
            switch (SVSettings.compassType) {
            case "vertical":
                return verticalCompassComponent

            case "combined":
                return combinedCompassComponent

            default:
                return horizontalCompassComponent
            }
        }
    }

    Component {
        id: horizontalCompassComponent

        SVBackground {
            width:  attitude.width + compass.width + SVUnits.bigMargin * 3
            height: Math.max(attitude.height, compass.height) + SVUnits.bigMargin * 2

            enabled:        true
            borderColor:    qgcPalette.windowShade
            hoverEnabled:   false
            checkable:      false
            checked:        false
            hovered:        false
            pressed:        false
            borderWidth:    1
            radius:         height / 2

            QGCAttitudeWidget {
                id:                     attitude
                size:                   SVUnits.objectWidth * 1.4 * root.compassScale
                vehicle:                cameraVehicle
                anchors.left:           parent.left
                anchors.leftMargin:     SVUnits.bigMargin
                anchors.verticalCenter: parent.verticalCenter
            }

            SVCameraCompass {
                id:                     compass
                anchors.left:           attitude.right
                anchors.leftMargin:     SVUnits.bigMargin
                size:                   SVUnits.objectWidth * 1.4 * root.compassScale
                heading:                cameraVehicle.heading.rawValue
                showBorder:             !SVSettings.simplifiedUserInterface
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Component {
        id: verticalCompassComponent

        SVBackground {
            width:  Math.max(attitude.width, compass.width) + SVUnits.bigMargin * 2
            height: attitude.height + compass.height + SVUnits.bigMargin * 3

            enabled:      true
            borderColor:  qgcPalette.windowShade
            hoverEnabled: false
            checkable:    false
            checked:      false
            hovered:      false
            pressed:      false
            borderWidth:  1
            radius:       height / 2

            QGCAttitudeWidget {
                id:                       attitude
                size:                     SVUnits.objectWidth * 1.4 * root.compassScale
                vehicle:                  cameraVehicle
                anchors.top:              parent.top
                anchors.topMargin:        SVUnits.bigMargin
                anchors.horizontalCenter: parent.horizontalCenter
            }

            SVCameraCompass {
                id:                       compass
                anchors.top:              attitude.bottom
                anchors.topMargin:        SVUnits.bigMargin
                size:                     SVUnits.objectWidth * 1.4 * root.compassScale
                heading:                  cameraVehicle.heading.rawValue
                showBorder:               !SVSettings.simplifiedUserInterface
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    Component {
        id: combinedCompassComponent

        Item {
            // Container bound matching the horizontal/vertical backgrounds
            width:  SVUnits.objectWidth * root.compassScale
            height: SVUnits.objectWidth * root.compassScale

            Rectangle {
                id: border
                anchors.left: combinedWidget.left
                anchors.bottom: combinedWidget.bottom
                anchors.leftMargin: -SVUnits.lineWidth
                anchors.bottomMargin: -SVUnits.lineWidth
                width: combinedWidget.width + SVUnits.lineWidth * 2
                height: combinedWidget.height + SVUnits.lineWidth * 2
                color: "white"
                radius: height / 2
                visible: !SVSettings.simplifiedUserInterface
            }

            Item {
                id: combinedWidget

                readonly property real compassRadius: SVUnits.objectWidth * root.compassScale
                readonly property real attitudeSize: SVUnits.objectWidth * 0.20 * root.compassScale
                readonly property real attitudeSpacing: SVUnits.margin * root.compassScale
                readonly property real totalAttitudeSize: attitudeSize + attitudeSpacing

                width: compassRadius * 2
                height: width
                anchors.right: parent.right
                anchors.bottom: parent.bottom

                IntegratedAttitudeIndicator {
                    x: -combinedWidget.totalAttitudeSize
                    attitudeAngleDegrees: cameraVehicle.roll.rawValue
                    compassRadius: combinedWidget.compassRadius
                    attitudeSize: combinedWidget.attitudeSize
                    attitudeSpacing: combinedWidget.attitudeSpacing
                }

                IntegratedAttitudeIndicator {
                    x: -combinedWidget.totalAttitudeSize
                    attitudeAngleDegrees: -cameraVehicle.pitch.rawValue
                    compassRadius: combinedWidget.compassRadius
                    attitudeSize: combinedWidget.attitudeSize
                    attitudeSpacing: combinedWidget.attitudeSpacing
                    transformOrigin: Item.Center
                    rotation: 90
                }

                SVCameraCompass {
                    anchors.fill: parent
                    size: combinedWidget.width
                    heading: cameraVehicle.heading.rawValue
                    showBorder: !SVSettings.simplifiedUserInterface
                }
            }
        }
    }

    function sensorState(cameraId) {
        if (!SVState.digiview || !SVState.digiview.sensorStates || cameraId <= 0) {
            return null
        }

        const index = cameraId - 1
        if (index < 0 || index >= SVState.digiview.sensorStates.length) {
            return null
        }

        const state = SVState.digiview.sensorStates[index]
        return state && state.valid ? state : null
    }

    function defaultBrightnessCameraId() {
        if (!SVState.digiview || SVState.digiview.videoOutputNumCameras <= 0) {
            return 0
        }

        const sourceCamera = root.cameraSlot >= 0 && root.cameraSlot < SVState.viewSourceCameras.length
            ? SVState.viewSourceCameras[root.cameraSlot] : 0
        if (sourceCamera > 0 && sourceCamera <= SVState.digiview.videoOutputNumCameras) {
            return sourceCamera
        }

        return Math.min(root.cameraSlot + 1, SVState.digiview.videoOutputNumCameras)
    }

    ToolButton {
        id: brightnessButton
        anchors.left: parent.left
        anchors.top: detectionFlag.visible ? detectionFlag.bottom : parent.top
        anchors.topMargin: detectionFlag.visible ? SVUnits.margin / 2 : 0
        width: Math.max(ScreenTools.defaultFontPixelHeight * 1.65, 28)
        height: width
        visible: SVState.cameraSelected === root.cameraSlot
            && SVState.uiInteractionEnabled
            && SVState.digiview
            && SVState.digiview.videoOutputNumCameras > 0
        text: "\u2600"
        font.pixelSize: ScreenTools.defaultFontPixelHeight
        padding: 0
        z: 20

        background: Rectangle {
            radius: width / 2
            color: qgcPalette.window
            opacity: brightnessButton.hovered || brightnessPopup.opened ? 0.9 : 0.65
            border.width: 1
            border.color: qgcPalette.windowShadeLight
        }

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Target brightness")

        onClicked: {
            if (brightnessPopup.opened) {
                brightnessPopup.close()
            } else {
                brightnessPopup.open()
            }
        }
    }

    Popup {
        id: brightnessPopup
        x: brightnessButton.x
        y: brightnessButton.y + brightnessButton.height + SVUnits.margin / 2
        width: Math.max(ScreenTools.defaultFontPixelWidth * 24, 210)
        padding: SVUnits.margin
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int selectedCameraId: 0
        property real draftBrightness: 0

        function syncFromBackend() {
            const state = root.sensorState(selectedCameraId)
            if (state && !brightnessSlider.pressed) {
                draftBrightness = state.targetBrightness
            }
        }

        onOpened: {
            selectedCameraId = root.defaultBrightnessCameraId()
            if (selectedCameraId > 0) {
                SVState.digiview.requestSensorParameters(selectedCameraId)
            }
            syncFromBackend()
        }

        background: Rectangle {
            color: qgcPalette.window
            border.width: 1
            border.color: qgcPalette.windowShade
            radius: SVUnits.radius
        }

        contentItem: ColumnLayout {
            spacing: SVUnits.margin

            RowLayout {
                Layout.fillWidth: true
                spacing: SVUnits.margin

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Brightness")
                    font.bold: true
                }

                QGCComboBox {
                    id: brightnessCamera
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 11
                    model: {
                        const cameras = []
                        const count = SVState.digiview ? SVState.digiview.videoOutputNumCameras : 0
                        for (let cameraId = 1; cameraId <= count; ++cameraId) {
                            cameras.push(qsTr("Camera %1").arg(cameraId))
                        }
                        return cameras
                    }
                    currentIndex: Math.max(0, brightnessPopup.selectedCameraId - 1)

                    onActivated: {
                        brightnessPopup.selectedCameraId = currentIndex + 1
                        SVState.digiview.requestSensorParameters(brightnessPopup.selectedCameraId)
                        brightnessPopup.syncFromBackend()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: SVUnits.margin

                QGCSlider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: -3
                    to: 3
                    stepSize: 0.1
                    value: brightnessPopup.draftBrightness

                    onMoved: brightnessPopup.draftBrightness = value
                    onPressedChanged: {
                        if (!pressed && brightnessPopup.selectedCameraId > 0) {
                            SVState.digiview.setSensorTargetBrightness(
                                brightnessPopup.selectedCameraId,
                                brightnessPopup.draftBrightness)
                        }
                    }
                }

                QGCLabel {
                    Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 4
                    horizontalAlignment: Text.AlignRight
                    text: Number(brightnessPopup.draftBrightness).toFixed(1)
                }
            }
        }
    }

    Connections {
        target: SVState.digiview

        function onSensorStatesChanged() {
            brightnessPopup.syncFromBackend()
        }
    }

    SVCameraDetectionFlag {
        id: detectionFlag
        anchors.left: parent.left
        anchors.top: parent.top
        width: SVUnits.objectWidth
        height: SVUnits.objectWidth
        radius: SVUnits.radius
        index: root.cameraSlot

        // 1. Safely grab the state for this specific camera slot
        property var camState: (SVState.digiview &&
                                SVState.digiview.cameraStates &&
                                root.cameraSlot >= 0 &&
                                root.cameraSlot < SVState.digiview.cameraStates.length)
                               ? SVState.digiview.cameraStates[root.cameraSlot]
                               : null

        // 2. Make visible for backend or local tracking/selection state.
        visible: (camState ? camState.hasActiveTarget : false)
            || (root.cameraSlot >= 0 && root.cameraSlot < SVState.cameraTrackingIds.length
                && SVState.cameraTrackingIds[root.cameraSlot] !== "")
            || (SVState.cursorTrackingSessionActive
                && SVState.cursorTrackingSessionSlot === root.cameraSlot)

        // 3. Dynamically set the color based on the camera slot index
        // 3. Color reflects actual tracking state, not the slot index
        normalColor: qgcPalette.windowTransparent
        hoverColor: qgcPalette.windowShadeLight
        borderColor: qgcPalette.windowShadeLight
        borderHoverColor: "white"
    }

    Connections {
        target: SVState.digiview

        function onStreamNameChanged() {
            root.resetCameraOverrides()
        }

        function onCamTargetingParametersReceived(streamName, camId, _targetingMode, eulerDelta, yaw, pitch, roll) {
            if (streamName !== SVState.digiview.streamName || camId !== root.cameraSlot) {
                return
            }

            if (eulerDelta !== 0 || !Number.isFinite(yaw) || !Number.isFinite(pitch)
                    || !Number.isFinite(roll)) {
                root.resetCameraOverrides()
                return
            }

            root._heading = (yaw % 360 + 360) % 360
            root._roll = roll

            var result = root.wrapPitchFull(pitch)
            root._pitch    = result.pitch
            root._inverted = result.inverted
        }
    }
}
