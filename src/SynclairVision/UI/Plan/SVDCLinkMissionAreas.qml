pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root

    required property var editor
    property bool expanded: false
    readonly property bool editing: _editingEntry.length > 0
    readonly property bool drawing: editing && _drawing
    readonly property string editingId: _editingId
    readonly property var outline: _vertices
    readonly property var _areas: editor.readOnly ? [] : editor.areas
    property string _editingEntry: ""
    property string _editingId: ""
    property bool _drawing: false
    property var _vertices: []
    property string _error: ""

    function start(area) {
        _editingEntry = editor.currentEntry
        _editingId = area?.id || ""
        _drawing = !area
        _vertices = area ? Array.from(area.vertices) : []
        nameField.text = area?.name || qsTr("Area %1").arg(_areas.length + 1)
        advisoryField.text = area?.advisory || ""
        _error = ""
    }

    function cancel() {
        _editingEntry = ""
        _editingId = ""
        _drawing = false
        _vertices = []
        _error = ""
    }

    function addCorner(latitude, longitude) {
        if (!drawing || !editor.canEdit || _editingEntry !== editor.currentEntry || _vertices.length >= 32
            || !Number.isFinite(latitude) || !Number.isFinite(longitude))
            return
        if (_vertices.some(point => point.latitude_deg === latitude && point.longitude_deg === longitude)) {
            _error = qsTr("Choose a different position for each corner.")
            return
        }
        _vertices = _vertices.concat([{ latitude_deg: latitude, longitude_deg: longitude }])
        _error = ""
    }

    function finish() {
        if (!editing || _editingEntry !== editor.currentEntry)
            return
        _error = editor.applyArea(_editingId, nameField.text.trim(), _vertices, advisoryField.text.trim())
        if (!_error)
            cancel()
    }

    QGCPalette { id: palette }

    QGCButton {
        Layout.fillWidth: true
        enabled: !root.editing
        text: (root.expanded ? qsTr("Areas (%1) ▾") : qsTr("Areas (%1) ▸")).arg(root._areas.length)
        onClicked: root.expanded = !root.expanded
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.expanded

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.editing

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Tap the map to place at least three corners in order around the area.")
                visible: root.drawing
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("Corners · %1 of %2").arg(root._vertices.length).arg(32)
                visible: root.drawing
            }

            QGCTextField {
                id: nameField

                Layout.fillWidth: true
                Accessible.name: qsTr("Area name")
                enabled: root.editor.canEdit
                maximumLength: 160
                placeholderText: qsTr("Area name")
            }

            QGCTextField {
                id: advisoryField

                Layout.fillWidth: true
                Accessible.name: qsTr("Advisory shown inside the area")
                enabled: root.editor.canEdit
                maximumLength: 200
                placeholderText: qsTr("Advisory while inside (optional)")
            }

            RowLayout {
                Layout.fillWidth: true

                QGCButton {
                    enabled: root.editor.canEdit && (!root.drawing || root._vertices.length > 0)
                    text: root.drawing ? qsTr("Undo corner") : qsTr("Redraw")
                    onClicked: {
                        root._vertices = root.drawing ? root._vertices.slice(0, -1) : []
                        root._drawing = true
                        root._error = ""
                    }
                }

                QGCButton {
                    enabled: root.editor.canEdit && root._vertices.length >= 3 && nameField.text.trim().length > 0
                    text: root.drawing ? qsTr("Finish area") : qsTr("Apply")
                    onClicked: root.finish()
                }

                QGCButton {
                    text: qsTr("Cancel")
                    onClicked: root.cancel()
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                color: palette.warningText
                text: root._error
                textFormat: Text.PlainText
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }

        Repeater {
            model: root.editing ? [] : root._areas

            ColumnLayout {
                id: row

                required property var modelData
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    text: row.modelData.name
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: row.modelData.advisory || ""
                    textFormat: Text.PlainText
                    visible: text.length > 0
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.editor.canEdit

                    QGCButton {
                        Layout.fillWidth: true
                        text: qsTr("Edit")
                        onClicked: root.start(row.modelData)
                    }

                    QGCButton {
                        text: qsTr("Remove")
                        onClicked: root.editor.removeArea(row.modelData.id)
                    }
                }
            }
        }

        QGCButton {
            Layout.fillWidth: true
            enabled: root.editor.canEdit && root._areas.length < 8
            text: qsTr("Draw area on map")
            visible: !root.editing
            onClicked: root.start(null)
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root._areas.length === 0 ? qsTr("Use an area to show an advisory or limit when a rule applies.")
                : qsTr("Choose an area under Area in a rule to apply its steps only inside that outline.")
            font.pointSize: ScreenTools.smallFontPointSize
            visible: !root.editing
            wrapMode: Text.WordWrap
        }
    }
}
