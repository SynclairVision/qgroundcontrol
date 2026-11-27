pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root

    required property var editor
    property bool expanded: false
    readonly property var _rules: editor.readOnly ? [] : (editor.mission.rules || [])
    readonly property var _waypoints: editor.waypoints
    readonly property bool _brokenReference: _rules.some(rule => triggerIndex(rule.trigger) < 0)

    function triggerIndex(trigger) {
        if (!trigger)
            return -1
        const leg = trigger.type === "leg_started"
        return _waypoints.findIndex((point, index) => leg
            ? point.id === trigger.from_waypoint_id && index + 1 < _waypoints.length
                && _waypoints[index + 1].id === trigger.to_waypoint_id
            : point.id === trigger.waypoint_id)
    }

    function triggerText(trigger) {
        const index = triggerIndex(trigger)
        if (index < 0)
            return qsTr("Choose a waypoint or leg again")
        return trigger.type === "leg_started" ? qsTr("Leaving waypoint %1 for %2").arg(index + 1).arg(index + 2)
            : qsTr("On reaching waypoint %1").arg(index + 1)
    }

    function openRule(rule) {
        editFactory.open({ title: rule ? qsTr("Edit rule") : qsTr("Add rule"),
                           entry: editor.currentEntry, savedRule: rule })
    }

    QGCButton {
        Layout.fillWidth: true
        text: (root.expanded ? qsTr("Rules (%1) ▾") : qsTr("Rules (%1) ▸")).arg(root._rules.length)
        onClicked: root.expanded = !root.expanded
    }

    QGCLabel {
        Layout.fillWidth: true
        visible: root._brokenReference
        text: qsTr("A route edit changed a waypoint or leg used by a rule. Update the affected rule before saving.")
        wrapMode: Text.Wrap
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.expanded

        QGCLabel {
            Layout.fillWidth: true
            visible: root._rules.length === 0
            text: root._waypoints.length === 0 ? qsTr("Add a waypoint to create a rule.")
                : qsTr("Add steps that run on reaching a waypoint or starting a leg.")
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root._rules

            ColumnLayout {
                id: row

                required property int index
                required property var modelData

                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("%1. %2").arg(row.index + 1).arg(root.triggerText(row.modelData.trigger))
                    wrapMode: Text.Wrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: (row.modelData.actions || []).map(action => action.type === "wait"
                        ? qsTr("Wait %1 s").arg(action.duration_s)
                        : qsTr("Show message")).join(qsTr(" → "))
                    font.pointSize: ScreenTools.smallFontPointSize
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.editor.canEdit

                    QGCButton {
                        Layout.fillWidth: true
                        text: qsTr("Edit")
                        onClicked: root.openRule(row.modelData)
                    }

                    QGCButton {
                        text: "↑"
                        Accessible.name: qsTr("Move rule up")
                        enabled: row.index > 0
                        onClicked: root.editor.moveRule(row.index, row.index - 1)
                    }

                    QGCButton {
                        text: "↓"
                        Accessible.name: qsTr("Move rule down")
                        enabled: row.index + 1 < root._rules.length
                        onClicked: root.editor.moveRule(row.index, row.index + 1)
                    }

                    QGCButton {
                        text: qsTr("Remove")
                        onClicked: root.editor.removeRule(row.index)
                    }
                }
            }
        }

        QGCButton {
            Layout.fillWidth: true
            visible: root.editor.canEdit
            enabled: root._waypoints.length > 0 && root._rules.length < 16
            text: qsTr("Add rule")
            onClicked: root.openRule(null)
        }
    }

    QGCPopupDialogFactory {
        id: editFactory
        dialogComponent: editComponent
    }

    Component {
        id: editComponent

        QGCPopupDialog {
            id: dialog

            required property string entry
            required property var savedRule
            property bool arrival: !savedRule || savedRule.trigger.type === "waypoint_reached"
            property string first: savedRule
                ? (arrival ? savedRule.trigger.waypoint_id || "" : savedRule.trigger.from_waypoint_id || "")
                : (root._waypoints.length > 0 ? root._waypoints[0].id : "")
            property string last: savedRule && !arrival ? savedRule.trigger.to_waypoint_id || "" : ""
            property string error: ""
            readonly property var choices: root._waypoints.filter((point, index) =>
                arrival || index + 1 < root._waypoints.length).map((point, index) => {
                    return { first: point.id, last: arrival ? "" : root._waypoints[index + 1].id,
                             label: arrival ? qsTr("Waypoint %1").arg(index + 1)
                                            : qsTr("Waypoint %1 to %2").arg(index + 1).arg(index + 2) }
                })
            readonly property int selection: choices.findIndex(choice => choice.first === first && choice.last === last)

            buttons: Dialog.Apply | Dialog.Cancel
            acceptButtonEnabled: root.editor.canEdit && entry === root.editor.currentEntry
                && selection >= 0 && steps.count > 0

            function appendStep() {
                steps.append({ actionType: "notify", message: "", seconds: "5" })
            }

            ListModel { id: steps }

            Component.onCompleted: {
                if (!savedRule) {
                    appendStep()
                    return
                }
                for (const action of savedRule.actions) {
                    steps.append({ actionType: action.type, message: action.text || "",
                                   seconds: String(action.duration_s || 5) })
                }
            }

            onAccepted: {
                if (!acceptButtonEnabled) {
                    preventClose = true
                    return
                }
                const trigger = arrival ? { type: "waypoint_reached", waypoint_id: first }
                    : { type: "leg_started", from_waypoint_id: first, to_waypoint_id: last }
                const actions = []
                for (let i = 0; i < steps.count; ++i) {
                    const step = steps.get(i)
                    actions.push(step.actionType === "wait" ? { type: "wait", duration_s: Number(step.seconds) }
                        : { type: "notify", text: step.message })
                }
                error = root.editor.applyRule(savedRule ? savedRule.id : "", trigger, actions)
                preventClose = error.length > 0
            }

            ColumnLayout {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 50, dialog.maxContentAvailableWidth)

                QGCLabel { text: qsTr("When") }

                QGCComboBox {
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Trigger")
                    model: [qsTr("Reach waypoint"), qsTr("Start leg")]
                    currentIndex: dialog.arrival ? 0 : 1
                    onActivated: index => {
                        dialog.arrival = index === 0
                        dialog.last = ""
                    }
                }

                QGCComboBox {
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Waypoint or leg")
                    model: dialog.choices
                    textRole: "label"
                    currentIndex: dialog.selection
                    displayText: dialog.selection < 0 ? qsTr("Choose waypoint or leg…") : currentText

                    onActivated: index => {
                        const choice = dialog.choices[index]
                        dialog.first = choice.first
                        dialog.last = choice.last
                    }
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Then run these steps in order")
                    font.bold: true
                }

                Repeater {
                    model: steps

                    ColumnLayout {
                        id: stepRow

                        required property int index
                        required property string actionType
                        required property string message
                        required property string seconds

                        Layout.fillWidth: true

                        RowLayout {
                            Layout.fillWidth: true

                            QGCLabel { text: String(stepRow.index + 1) }

                            QGCComboBox {
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Step %1 action").arg(stepRow.index + 1)
                                model: dialog.arrival || stepRow.actionType === "wait"
                                    ? [qsTr("Show message"), qsTr("Wait at waypoint")] : [qsTr("Show message")]
                                currentIndex: stepRow.actionType === "wait" ? 1 : 0
                                onActivated: index => steps.setProperty(stepRow.index, "actionType",
                                                                        index === 1 ? "wait" : "notify")
                            }

                            QGCButton {
                                text: "↑"
                                Accessible.name: qsTr("Move step up")
                                enabled: stepRow.index > 0
                                onClicked: steps.move(stepRow.index, stepRow.index - 1, 1)
                            }

                            QGCButton {
                                text: "↓"
                                Accessible.name: qsTr("Move step down")
                                enabled: stepRow.index + 1 < steps.count
                                onClicked: steps.move(stepRow.index, stepRow.index + 1, 1)
                            }

                            QGCButton {
                                text: qsTr("Remove")
                                onClicked: steps.remove(stepRow.index)
                            }
                        }

                        QGCTextField {
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Step %1 message").arg(stepRow.index + 1)
                            visible: stepRow.actionType === "notify"
                            placeholderText: qsTr("Message to show in Fly View")
                            text: stepRow.message
                            onTextEdited: steps.setProperty(stepRow.index, "message", text)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: stepRow.actionType === "wait"

                            QGCTextField {
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Wait seconds")
                                text: stepRow.seconds
                                validator: IntValidator { bottom: 1; top: 60; locale: "C" }
                                onTextEdited: steps.setProperty(stepRow.index, "seconds", text)
                            }

                            QGCLabel { text: qsTr("seconds (1–60)") }
                        }
                    }
                }

                QGCButton {
                    Layout.fillWidth: true
                    enabled: steps.count < 4
                    text: qsTr("Add step")
                    onClicked: dialog.appendStep()
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: dialog.error.length > 0 ? dialog.error
                        : dialog.arrival ? qsTr("The route continues when the steps finish.")
                                         : qsTr("Leg rules can show messages while the route continues.")
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
