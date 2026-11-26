pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.SynclairVisionUI

Rectangle {
    id: root

    readonly property real _margin: ScreenTools.defaultFontPixelHeight * 0.6
    readonly property bool _numericValid: latitudeField.valid && longitudeField.valid && altitudeField.valid
                                          && radiusField.valid
    readonly property var _point: editor.selectedWaypoint >= 0 ? editor.waypoints[editor.selectedWaypoint] || ({}) : (
                                                                     {})
    property bool addingWaypoints: false
    readonly property var editor: SVBackend.missionEditor
    required property var editorMap

    function confirm(title, message, action) {
        const dialog = confirmFactory.open({
                                               title: title,
                                               message: message
                                           });
        if (dialog) {
            dialog.accepted.connect(action);
        }
    }

    function entryIndex() {
        const entries = editor.entries;
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].key === editor.currentEntry) {
                return i;
            }
        }
        return -1;
    }

    function fitRoute() {
        if (!visible || !editorMap.mapReady || editor.waypoints.length === 0) {
            return
        }
        if (!editor.waypoints.every(point => point && Number.isFinite(point.latitude_deg)
            && Math.abs(point.latitude_deg) <= 90 && Number.isFinite(point.longitude_deg)
            && Math.abs(point.longitude_deg) <= 180)) {
            return
        }
        const first = editor.waypoints[0]
        let minLat = first.latitude_deg
        let maxLat = minLat
        let minLon = first.longitude_deg
        let maxLon = minLon
        for (const point of editor.waypoints) {
            minLat = Math.min(minLat, point.latitude_deg)
            maxLat = Math.max(maxLat, point.latitude_deg)
            minLon = Math.min(minLon, point.longitude_deg)
            maxLon = Math.max(maxLon, point.longitude_deg)
        }
        const latMargin = Math.max((maxLat - minLat) * 0.1, 0.0005)
        const lonMargin = Math.max((maxLon - minLon) * 0.1, 0.0005)
        const topLeft = QtPositioning.coordinate(Math.min(90, maxLat + latMargin),
                                                 Math.max(-180, minLon - lonMargin))
        const bottomRight = QtPositioning.coordinate(Math.max(-90, minLat - latMargin),
                                                     Math.min(180, maxLon + lonMargin))
        const bounds = QtPositioning.rectangle(topLeft, bottomRight)
        editorMap.setVisibleRegion(bounds)
        const viewport = editorMap.centerViewport
        editorMap.alignCoordinateToPoint(bounds.center,
            Qt.point(viewport.x + viewport.width / 2, viewport.y + viewport.height / 2))
    }

    function guardChange(action) {
        if (editor.busy) {
            return;
        }
        addingWaypoints = false;
        if (editor.dirty) {
            const dialog = leaveFactory.open();
            if (dialog) {
                dialog.proceed.connect(action);
            }
        } else {
            action();
        }
    }

    color: palette.window
    radius: ScreenTools.defaultBorderRadius

    onVisibleChanged: {
        addingWaypoints = false;
        if (visible && !editor.fresh && !editor.busy) {
            editor.refresh();
        }
        if (visible) {
            Qt.callLater(fitRoute)
        }
    }

    QGCPalette {
        id: palette

        colorGroupEnabled: true
    }

    QGCFlickable {
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.margins: root._margin
        anchors.right: parent.right
        anchors.top: parent.top
        clip: true
        contentHeight: body.height
        flickableDirection: Flickable.VerticalFlick

        ColumnLayout {
            id: body

            spacing: root._margin
            width: parent.width

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize: ScreenTools.largeFontPointSize
                    text: qsTr("DCLink missions")
                }

                QGCButton {
                    enabled: !root.editor.busy
                    text: qsTr("Refresh")

                    onClicked: root.editor.refresh()
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.editor.source
            }

            QGCLabel {
                Layout.fillWidth: true
                font.pointSize: ScreenTools.smallFontPointSize
                text: qsTr("Offline · drafts are kept locally. Connect DigiView and refresh to read or save.")
                visible: !root.editor.fresh
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true

                QGCComboBox {
                    id: missionPicker

                    Layout.fillWidth: true
                    currentIndex: root.entryIndex()
                    enabled: !root.editor.busy
                    model: root.editor.entries
                    textRole: "label"

                    onActivated: index => {
                        const key = model[index].key;
                        root.guardChange(function () {
                            root.editor.selectEntry(key);
                        });
                        currentIndex = Qt.binding(function () {
                            return root.entryIndex();
                        });
                    }
                }

                QGCButton {
                    enabled: !root.editor.busy && !root.editor.reviewRequired
                    text: qsTr("New")

                    onClicked: root.guardChange(function () {
                        root.editor.newDraft();
                    })
                }

                QGCButton {
                    enabled: !root.editor.busy
                    text: qsTr("More")

                    onClicked: missionMenu.popup()

                    QGCMenu {
                        id: missionMenu

                        QGCMenuItem {
                            enabled: root.editor.hasDraft && !root.editor.readOnly && !root.editor.reviewRequired
                            text: qsTr("Duplicate / Save as copy")

                            onTriggered: root.editor.duplicateDraft()
                        }

                        QGCMenuItem {
                            enabled: root.editor.fresh && root.editor.destinationMatches
                                     && root.editor.missionId.length > 0
                            text: qsTr("Reload saved version")

                            onTriggered: {
                                const message = qsTr("Replace this draft with the latest saved revision? "
                                                     + "Local changes will be discarded.")
                                root.confirm(qsTr("Reload mission"), message, function() { root.editor.reload() })
                            }
                        }

                        QGCMenuItem {
                            enabled: !root.editor.reviewRequired
                            text: qsTr("Import QGC plan…")

                            onTriggered: planFileDialog.openForLoad()
                        }

                        QGCMenuItem {
                            enabled: root.editor.fresh && root.editor.destinationMatches
                                     && root.editor.missionId.length > 0 && !root.editor.reviewRequired
                            text: qsTr("Delete saved mission…")

                            onTriggered: {
                                const message = qsTr("Delete “%1” from this DCLink library? "
                                    + "Your local draft is kept. This does not cancel a flight.")
                                    .arg(root.editor.mission.name)
                                root.confirm(qsTr("Delete saved mission"), message,
                                             function() { root.editor.deleteMission() })
                            }
                        }

                        QGCMenuItem {
                            enabled: root.editor.hasDraft && !root.editor.reviewRequired
                            text: qsTr("Remove local draft…")

                            onTriggered: {
                                const message = qsTr("Remove this local draft? The saved DCLink mission will remain.")
                                root.confirm(qsTr("Remove local draft"), message,
                                             function() { root.editor.discardDraft() })
                            }
                        }
                    }
                }
            }

            QGCTextField {
                Layout.fillWidth: true
                enabled: root.editor.canEdit
                maximumLength: 160
                placeholderText: qsTr("Mission name")
                text: root.editor.mission.name || ""
                visible: root.editor.hasDraft

                onTextEdited: root.editor.setName(text)
            }

            QGCLabel {
                Layout.fillWidth: true
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.editor.status
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.editor.draftSource
                visible: root.editor.hasDraft && root.editor.draftSource !== root.editor.source
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                color: palette.warningText
                text: root.editor.error
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.editor.conflict

                QGCButton {
                    enabled: root.editor.fresh && root.editor.destinationMatches && !root.editor.busy
                    text: qsTr("Reload")

                    onClicked: root.confirm(qsTr("Reload mission"), qsTr(
                                                "Discard local changes and load the latest revision?"), function () {
                                                    root.editor.reload();
                                                })
                }

                QGCButton {
                    enabled: !root.editor.busy && !root.editor.reviewRequired && !root.editor.readOnly
                    text: qsTr("Save as copy")

                    onClicked: root.editor.duplicateDraft()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.editor.reviewRequired

                QGCLabel {
                    Layout.fillWidth: true
                    color: palette.warningText
                    text: root.editor.reviewDescription
                    wrapMode: Text.WordWrap
                }

                QGCButton {
                    text: qsTr("Retry original change")
                    visible: root.editor.canRetry

                    onClicked: root.editor.retryChange()
                }

                QGCButton {
                    Layout.fillWidth: true
                    enabled: root.editor.canAcknowledgeReview
                    text: qsTr("I have reviewed the library…")

                    onClicked: {
                        const message = qsTr("Confirm you checked the refreshed library. "
                            + "The previous change may already exist. Use its saved mission instead of creating "
                            + "another copy. This only resumes editing.")
                        root.confirm(qsTr("Review unconfirmed change"), message,
                                     function() { root.editor.acknowledgeReview() })
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Route · %1 waypoints").arg(root.editor.waypoints.length)
                }

                QGCButton {
                    enabled: root.editor.waypoints.length > 0
                    text: qsTr("Fit")

                    onClicked: root.fitRoute()
                }
            }

            QGCButton {
                Layout.fillWidth: true
                checkable: true
                checked: root.addingWaypoints
                enabled: root.editor.canEdit && root.editor.waypoints.length < 1000 && root._numericValid
                text: root.addingWaypoints ? qsTr("Done adding") : qsTr("Add waypoints on map")

                onClicked: root.addingWaypoints = !root.addingWaypoints
            }

            QGCLabel {
                Layout.fillWidth: true
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.addingWaypoints
                    ? qsTr("Click the map to add points. Select a point to move or edit it.")
                    : qsTr("Add points to draw a route. An empty mission can be saved as a draft.")
                visible: root.editor.hasDraft && (root.addingWaypoints || root.editor.waypoints.length === 0)
                wrapMode: Text.WordWrap
            }

            ListView {
                id: routeList

                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, ScreenTools.defaultFontPixelHeight * 10)
                clip: true
                currentIndex: root.editor.selectedWaypoint
                model: root.editor.waypoints
                spacing: ScreenTools.defaultFontPixelHeight * 0.15

                delegate: QGCButton {
                    required property int index
                    required property var modelData

                    checked: index === root.editor.selectedWaypoint
                    text: qsTr("%1   %2 m · radius %3 m").arg(index + 1)
                        .arg(Number(modelData.altitude_m).toFixed(1))
                        .arg(Number(modelData.acceptance_radius_m).toFixed(1))
                    width: routeList.width

                    onClicked: {
                        root.addingWaypoints = false;
                        root.editor.selectedWaypoint = index;
                    }
                }

                onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            }

            ColumnLayout {
                Layout.fillWidth: true
                enabled: root.editor.canEdit
                visible: root.editor.selectedWaypoint >= 0

                QGCLabel {
                    text: qsTr("Waypoint %1").arg(root.editor.selectedWaypoint + 1)
                }

                SVDCLinkMissionField {
                    id: latitudeField

                    Layout.fillWidth: true
                    decimals: 8
                    label: qsTr("Latitude (°)")
                    maximum: 90
                    minimum: -90
                    value: root._point.latitude_deg || 0

                    onEdited: value => root.editor.updateWaypoint(root.editor.selectedWaypoint, "latitude_deg", value)
                }

                SVDCLinkMissionField {
                    id: longitudeField

                    Layout.fillWidth: true
                    decimals: 8
                    label: qsTr("Longitude (°)")
                    maximum: 180
                    minimum: -180
                    value: root._point.longitude_deg || 0

                    onEdited: value => root.editor.updateWaypoint(root.editor.selectedWaypoint, "longitude_deg", value)
                }

                SVDCLinkMissionField {
                    id: altitudeField

                    Layout.fillWidth: true
                    label: qsTr("Altitude above HOME (m)")
                    value: root._point.altitude_m || 0

                    onEdited: value => root.editor.updateWaypoint(root.editor.selectedWaypoint, "altitude_m", value)
                }

                SVDCLinkMissionField {
                    id: radiusField

                    Layout.fillWidth: true
                    label: qsTr("Acceptance radius (m)")
                    minimum: Number.MIN_VALUE
                    value: root._point.acceptance_radius_m || 30

                    onEdited: value => root.editor.updateWaypoint(root.editor.selectedWaypoint, "acceptance_radius_m",
                                                                  value)
                }

                RowLayout {
                    Layout.fillWidth: true

                    QGCButton {
                        enabled: root.editor.selectedWaypoint > 0
                        text: qsTr("Up")

                        onClicked: root.editor.moveWaypoint(root.editor.selectedWaypoint, root.editor.selectedWaypoint
                                                            - 1)
                    }

                    QGCButton {
                        enabled: root.editor.selectedWaypoint + 1 < root.editor.waypoints.length
                        text: qsTr("Down")

                        onClicked: root.editor.moveWaypoint(root.editor.selectedWaypoint, root.editor.selectedWaypoint
                                                            + 1)
                    }

                    QGCButton {
                        text: qsTr("Remove")

                        onClicked: root.editor.removeWaypoint(root.editor.selectedWaypoint)
                    }
                }
            }
        }
    }

    ColumnLayout {
        id: footer

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: root._margin
        anchors.right: parent.right
        spacing: root._margin * 0.5

        QGCLabel {
            Layout.fillWidth: true
            font.pointSize: ScreenTools.smallFontPointSize
            text: root.editor.saveReason
            visible: !root.editor.canSave && (root.editor.dirty || root.editor.busy)
            wrapMode: Text.WordWrap
        }

        QGCButton {
            Layout.fillWidth: true
            enabled: root.editor.canSave && root._numericValid
            text: root.editor.busy ? qsTr("Working…") : qsTr("Save to DCLink")
            visible: root.editor.hasDraft

            onClicked: {
                root.addingWaypoints = false;
                if (root.editor.missionId.length === 0) {
                    const message = qsTr("Save “%1” to %2? Saving does not start a mission.")
                        .arg(root.editor.mission.name).arg(root.editor.source)
                    root.confirm(qsTr("Save new mission"), message, function() { root.editor.save() })
                } else {
                    root.editor.save();
                }
            }
        }
    }

    QGCFileDialog {
        id: planFileDialog

        folder: QGroundControl.settingsManager.appSettings.missionSavePath
        nameFilters: [qsTr("QGC plans (*.plan)")]
        title: qsTr("Import QGC plan as DCLink route")

        onAcceptedForLoad: file => {
            close();
            importFactory.open({
                                   filePath: file
                               });
        }
    }

    QGCPopupDialogFactory {
        id: confirmFactory

        dialogComponent: confirmComponent
    }

    Component {
        id: confirmComponent

        QGCPopupDialog {
            id: confirmDialog

            property string message

            buttons: Dialog.Ok | Dialog.Cancel

            QGCLabel {
                text: confirmDialog.message
                width: Math.min(ScreenTools.defaultFontPixelWidth * 55, confirmDialog.maxContentAvailableWidth)
                wrapMode: Text.WordWrap
            }
        }
    }

    QGCPopupDialogFactory {
        id: leaveFactory

        dialogComponent: leaveComponent
    }

    Component {
        id: leaveComponent

        QGCPopupDialog {
            id: leaveDialog

            signal proceed

            buttons: Dialog.Cancel
            title: qsTr("Unsaved mission draft")

            ColumnLayout {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 45, leaveDialog.maxContentAvailableWidth)

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Keep this draft locally before switching? It will remain in the mission picker.")
                    wrapMode: Text.WordWrap
                }

                QGCButton {
                    Layout.fillWidth: true
                    text: qsTr("Keep draft")

                    onClicked: {
                        leaveDialog.proceed();
                        leaveDialog.close();
                    }
                }

                QGCButton {
                    Layout.fillWidth: true
                    enabled: !root.editor.reviewRequired
                    text: qsTr("Discard draft")

                    onClicked: {
                        root.editor.discardDraft();
                        leaveDialog.proceed();
                        leaveDialog.close();
                    }
                }
            }
        }
    }

    QGCPopupDialogFactory {
        id: importFactory

        dialogComponent: importComponent
    }

    Component {
        id: importComponent

        QGCPopupDialog {
            id: importDialog

            required property string filePath
            property var preview: root.editor.previewPlan(filePath, 30)

            acceptButtonEnabled: preview.errors.length === 0 && importRadius.valid
            buttons: Dialog.Ok | Dialog.Cancel
            title: qsTr("Import waypoint route")

            onAccepted: root.guardChange(function () {
                root.editor.acceptPlanImport();
            })

            ColumnLayout {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 60, importDialog.maxContentAvailableWidth)

                QGCLabel {
                    Layout.fillWidth: true
                    text: importDialog.preview.errors.length > 0 ? importDialog.preview.errors.join("\n")
                        : qsTr("Import “%1” as a local DCLink route with %2 waypoints?")
                          .arg(importDialog.preview.name).arg(importDialog.preview.waypoint_count)
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Only HOME-relative waypoints are converted. Aircraft type, firmware, planned HOME "
                               + "and cruise/hover settings are not applied. Nothing is uploaded or started.")
                    wrapMode: Text.WordWrap
                }

                SVDCLinkMissionField {
                    id: importRadius

                    Layout.fillWidth: true
                    label: qsTr("Replace aircraft-default acceptance radius with (m)")
                    minimum: Number.MIN_VALUE
                    value: 30
                    visible: !!importDialog.preview.needs_radius

                    onEdited: value => {
                        importRadius.value = value;
                        importDialog.preview = root.editor.previewPlan(importDialog.filePath, value);
                    }
                }
            }
        }
    }

    Connections {
        target: root.editorMap

        function onMapReadyChanged() {
            root.fitRoute()
        }
    }

    Connections {
        function onChanged() {
            if (!root.editor.canEdit) {
                root.addingWaypoints = false;
            }
        }

        function onRouteLoaded() {
            root.addingWaypoints = false;
            root.fitRoute();
        }

        target: root.editor
    }
}
