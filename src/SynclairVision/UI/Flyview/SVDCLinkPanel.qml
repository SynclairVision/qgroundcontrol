pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.SynclairVisionUI

Item {
    id: root

    width: ScreenTools.defaultFontPixelHeight * 15
    height: Math.min(maximumHeight, implicitHeight)
    clip: true
    implicitHeight: fixedContent.implicitHeight + margin * 2
        + (commandList.contentHeight > 0 ? commandList.contentHeight + margin : 0)

    property real maximumHeight: parent ? parent.height : implicitHeight
    property var client: SVBackend.dclink
    property var favoriteCommands: []
    property bool expanded: false
    readonly property real margin: ScreenTools.defaultFontPixelHeight * 0.25
    readonly property real minimumHeight: fixedContent.implicitHeight + margin * 2
    // Polls notify unchanged action lists; keep delegates and scroll position stable between real changes.
    readonly property string _actionSnapshot: JSON.stringify(client && client.actions ? client.actions : [])
    readonly property var _actions: JSON.parse(_actionSnapshot)
    readonly property var _favoriteIds: normalizedFavorites(favoriteCommands)
    readonly property var _cancel: _actions.find(action => action.command === "cancel") || null
    readonly property var _favorites: _actions.filter(action => action.command !== "cancel"
        && isFavorite(action.command))
    readonly property var _otherActions: _actions.filter(action => action.command !== "cancel"
        && !isFavorite(action.command))

    signal favoriteCommandsChangedByUser(var commands)
    signal optionsRequested()

    function normalizedFavorites(values: var): var {
        const commands = []
        // Qt Settings restores string lists as QML sequences rather than JavaScript arrays.
        if (!values || typeof values !== "object" || !Number.isInteger(values.length) || values.length < 0)
            return commands
        for (let index = 0; index < values.length; ++index) {
            const command = values[index]
            if (typeof command === "string" && command.length > 0 && commands.indexOf(command) === -1)
                commands.push(command)
        }
        return commands
    }

    function isFavorite(command: string): bool {
        return _favoriteIds.indexOf(command) !== -1
    }

    function toggleFavorite(command: string) {
        const commands = _favoriteIds.slice()
        const index = commands.indexOf(command)
        if (index < 0)
            commands.push(command)
        else
            commands.splice(index, 1)
        favoriteCommandsChangedByUser(commands)
    }

    onVisibleChanged: {
        if (!visible)
            expanded = false
    }

    SVBackground {
        anchors.fill: parent
        radius: ScreenTools.defaultBorderRadius
    }

    ColumnLayout {
        id: fixedContent

        x: root.margin
        y: root.margin
        width: Math.max(0, root.width - root.margin * 2)
        spacing: root.margin

        RowLayout {
            Layout.fillWidth: true
            spacing: root.margin

            QGCButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Accessible.name: qsTr("Show or hide other commands")
                checkable: true
                checked: root.expanded
                text: root.expanded ? qsTr("Commands ▾") : qsTr("Commands ▸")
                heightFactor: 0.25
                onClicked: root.expanded = !root.expanded
            }

            QGCButton {
                Layout.preferredWidth: height
                Accessible.name: qsTr("Display options")
                text: "⋯"
                heightFactor: 0.25
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Display options")
                onClicked: root.optionsRequested()
            }
        }

        QGCButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Accessible.description: qsTr("Cancel the current DCLink command")
            text: root._cancel ? root._cancel.label : qsTr("Cancel")
            enabled: !!root._cancel && root._cancel.enabled === true
            heightFactor: 0.25
            ToolTip.visible: hovered && !!root._cancel && root._cancel.reason.length > 0
            ToolTip.text: root._cancel ? root._cancel.reason : ""
            onClicked: { if (root.client) root.client.startAction("cancel") }
        }
    }

    Flickable {
        id: commandList

        x: root.margin
        y: fixedContent.y + fixedContent.height + root.margin
        width: Math.max(0, root.width - root.margin * 2)
        height: Math.max(0, root.height - y - root.margin)
        clip: true
        contentWidth: width
        contentHeight: listContent.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: listContent

            width: commandList.width
            spacing: root.margin

            Repeater {
                model: root._favorites

                delegate: CommandRow {
                    required property var modelData

                    Layout.fillWidth: true
                    action: modelData
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.client ? root.client.availabilityReason : ""
                visible: !!root.client && root.client.connected && text.length > 0
                font.pointSize: ScreenTools.smallFontPointSize
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.client ? root.client.feedback : ""
                visible: text.length > 0
                font.pointSize: ScreenTools.smallFontPointSize
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: root.expanded ? root._otherActions : []

                delegate: CommandRow {
                    required property var modelData

                    Layout.fillWidth: true
                    action: modelData
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root._otherActions.length > 0 ? qsTr("Star commands to keep them visible.")
                    : qsTr("No other commands available.")
                visible: root.expanded
                font.pointSize: ScreenTools.smallFontPointSize
                wrapMode: Text.WordWrap
            }
        }
    }

    component CommandRow: ColumnLayout {
        id: commandRow

        required property var action
        readonly property bool favorite: root.isFavorite(action.command)
        spacing: root.margin

        RowLayout {
            Layout.fillWidth: true
            spacing: root.margin

            QGCButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: commandRow.action.label
                enabled: commandRow.action.enabled === true
                heightFactor: 0.25
                ToolTip.visible: hovered && commandRow.action.reason.length > 0
                ToolTip.text: commandRow.action.reason
                onClicked: { if (root.client) root.client.startAction(commandRow.action.command) }
            }

            QGCButton {
                Layout.preferredWidth: height
                Accessible.name: commandRow.favorite ? qsTr("Remove %1 from favorites").arg(commandRow.action.label)
                    : qsTr("Favorite %1").arg(commandRow.action.label)
                checkable: true
                checked: commandRow.favorite
                text: commandRow.favorite ? "★" : "☆"
                heightFactor: 0.25
                ToolTip.visible: hovered
                ToolTip.text: commandRow.favorite ? qsTr("Remove favorite") : qsTr("Keep visible")
                onClicked: root.toggleFavorite(commandRow.action.command)
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: commandRow.action.reason
            visible: text.length > 0 && root.client && text !== root.client.availabilityReason
            font.pointSize: ScreenTools.smallFontPointSize
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }
    }
}
