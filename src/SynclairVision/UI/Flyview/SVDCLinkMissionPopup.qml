pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.SynclairVisionUI

Popup {
    id: root

    readonly property real _margin: ScreenTools.defaultFontPixelHeight * 0.5
    readonly property var _editor: SVBackend.missionEditor
    readonly property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    readonly property string _session: SVBackend.missions.serviceSession
    readonly property var _entries: visible ? _editor.launchEntries : []
    property var _selection: null
    property var _confirmation: null
    readonly property int _selectedIndex: {
        if (!_selection || _selection.session !== _session)
            return -1
        return _entries.findIndex(entry => entry.mission_id === _selection.mission_id
            && entry.revision === _selection.revision)
    }
    readonly property string _startReason: {
        if (_editor.startReason.length > 0)
            return _editor.startReason
        if (_selectedIndex < 0)
            return qsTr("Choose a saved mission.")
        if (!_activeVehicle || _activeVehicle.id !== _editor.executionVehicleId)
            return qsTr("Select the aircraft connected to this DCLink.")
        return _activeVehicle.armed && _activeVehicle.flying ? "" : qsTr("The aircraft must already be airborne.")
    }

    parent: Overlay.overlay
    width: Math.max(0, Math.min(ScreenTools.defaultFontPixelHeight * 23, parent ? parent.width - _margin * 2 : 0))
    height: Math.max(0, Math.min(contents.implicitHeight + padding * 2, parent ? parent.height - _margin * 2 : 0))
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: _margin
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function refresh() {
        _selection = null
        _confirmation = null
        _editor.refresh()
    }

    function prepareStart() {
        if (_startReason.length > 0 || !_activeVehicle)
            return
        const route = _entries[_selectedIndex]
        if (!route)
            return
        _confirmation = {missionId: route.mission_id, revision: route.revision, name: route.name,
                         vehicleId: _activeVehicle.id, session: _session}
    }

    function confirmStart() {
        const request = _confirmation
        if (!request || !_selection || _startReason.length > 0 || !_activeVehicle ||
                _activeVehicle.id !== request.vehicleId || _session !== request.session ||
                _selection.mission_id !== request.missionId || _selection.revision !== request.revision)
            return
        _confirmation = null
        _editor.startMission(request.missionId, request.revision, request.vehicleId, request.session)
    }

    onOpened: refresh()
    onClosed: {
        _selection = null
        _confirmation = null
    }
    on_ActiveVehicleChanged: _confirmation = null
    on_SessionChanged: _confirmation = null

    QGCPalette { id: palette }

    background: Rectangle {
        color: palette.window
        border.color: palette.windowShadeLight
        radius: ScreenTools.defaultBorderRadius
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: contents.implicitHeight

        ColumnLayout {
            id: contents

            width: scroll.availableWidth
            spacing: root._margin

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    font.bold: true
                    text: qsTr("DCLink Mission")
                }

                SVDCLinkButton {
                    enabled: !root._editor.busy
                    text: qsTr("Refresh")
                    onClicked: root.refresh()
                }

                SVDCLinkButton {
                    text: qsTr("Close")
                    onClicked: root.close()
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root._editor.source
                textFormat: Text.PlainText
                elide: Text.ElideMiddle
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root._editor.runText
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                visible: text.length > 0
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root._editor.ruleNotification
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                visible: text.length > 0
            }

            SVDCLinkButton {
                Layout.fillWidth: true
                text: qsTr("Cancel run")
                visible: root._editor.canCancelRun
                onClicked: root._editor.cancelRun(root._editor.runId)
            }

            QGCLabel {
                Layout.fillWidth: true
                color: palette.warningText
                text: root._editor.executionReviewRequired
                    ? (root._editor.busy ? root._editor.flightRunText : root._editor.reviewDescription)
                    : root._editor.error
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                visible: text.length > 0
            }

            SVDCLinkButton {
                Layout.fillWidth: true
                enabled: root._editor.canAcknowledgeReview
                text: qsTr("I checked the aircraft and run status")
                visible: root._editor.executionReviewRequired && !root._editor.busy
                onClicked: root._editor.acknowledgeReview()
            }

            QGCComboBox {
                Layout.fillWidth: true
                Accessible.name: qsTr("Saved mission")
                enabled: !root._editor.busy
                model: root._entries
                textRole: "label"
                currentIndex: root._selectedIndex
                displayText: currentIndex < 0 ? qsTr("Choose a saved mission…") : currentText
                visible: root._entries.length > 0 && !root._confirmation && root._editor.flightRunText.length === 0

                onActivated: index => {
                    const route = root._entries[index]
                    if (!route)
                        return
                    root._selection = {mission_id: route.mission_id, revision: route.revision, session: root._session}
                    currentIndex = Qt.binding(() => root._selectedIndex)
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                text: {
                    if (root._editor.catalogReady && root._editor.executionSupported && root._entries.length === 0)
                        return qsTr("Save a waypoint route in Plan View.")
                    return root._startReason
                }
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                visible: text.length > 0 && !root._editor.executionReviewRequired &&
                    (root._confirmation || root._editor.flightRunText.length === 0)
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root._confirmation
                    ? qsTr("Fly “%1” (revision %2) on aircraft %3?")
                        .arg(root._confirmation.name).arg(root._confirmation.revision).arg(root._confirmation.vehicleId)
                    : ""
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                visible: !!root._confirmation
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !!root._confirmation || (root._entries.length > 0 && root._editor.flightRunText.length === 0)

                SVDCLinkButton {
                    Layout.fillWidth: true
                    text: qsTr("Back")
                    visible: !!root._confirmation
                    onClicked: root._confirmation = null
                }

                SVDCLinkButton {
                    Layout.fillWidth: true
                    enabled: root._startReason.length === 0
                    text: root._confirmation ? qsTr("Confirm Start") : qsTr("Start mission…")
                    onClicked: root._confirmation ? root.confirmStart() : root.prepareStart()
                }
            }
        }
    }
}
