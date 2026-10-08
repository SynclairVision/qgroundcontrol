import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QGroundControl.FirstRunPromptDialogs

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

FirstRunPrompt {
    id: root

    title: qsTr("Welcome to Synclair: QGroundControl")
    promptId: QGroundControl.corePlugin.svInitialWelcomePromptId

    readonly property string userGuideUrl: "https://github.com/SynclairVision/qgroundcontrol/blob/master/user_guide.md"
    readonly property string releaseNotesUrl: "https://github.com/SynclairVision/qgroundcontrol/releases"

    readonly property var getStartedSteps: [
        qsTr("Start SynclairQGC and open Fly view."),
        qsTr("Make sure the SynclairVision overlay is visible. While Fly view is active, the default shortcut is O."),
        qsTr("Open Settings > Network."),
        qsTr("Select or create a DigiView network profile and choose Connect."),
        qsTr("Wait for video and control communication to become active."),
        qsTr("Select a camera view before using movement, zoom, per-view overlays, or tracking.")
    ]

    onClosed: {
        var appSettings = QGroundControl.settingsManager.appSettings
        var shownIds = appSettings.firstRunPromptIdsShown.rawValue

        if (!shownIds.includes(promptId)) {
            shownIds.push(promptId)
            appSettings.firstRunPromptIdsShown.rawValue = shownIds
        }
    }

    QGCPalette { id: qgcPalette }

    Flickable {
        id: contentFlickable

        width: SVUnits.objectWidth * 13
        height: SVUnits.objectHeight * 4.5
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentWidth: width
        contentHeight: contentColumn.height

        ScrollBar.vertical: ScrollBar { }

        Column {
            id: contentColumn

            width: parent.width
            spacing: SVUnits.bigMargin * 2

            Rectangle {
                width: parent.width
                height: SVUnits.objectHeight * 2
                color: "transparent"
                radius: SVUnits.radius

                Image {
                    anchors.fill: parent
                    source: Qt.resolvedUrl("../Resources/Images/no_video_background.png")
                    fillMode: Image.PreserveAspectCrop
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: SVUnits.objectWidth * 4

                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: qgcPalette.window }
                    }
                }

                Row {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.leftMargin: SVUnits.bigMargin * 2
                    anchors.bottomMargin: SVUnits.bigMargin * 2
                    spacing: SVUnits.bigMargin

                    Image {
                        width: SVUnits.objectWidth
                        height: SVUnits.objectWidth
                        source: "/res/resources/svlogo.png"
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        antialiasing: true
                        asynchronous: true
                    }

                    Column {
                        anchors.top: parent.top
                        anchors.topMargin: -SVUnits.margin
                        spacing: 0

                        QGCLabel {
                            text: qsTr("Welcome to SynclairVision's QGroundControl")
                            font.pointSize: SVUnits.largeText
                            color: qgcPalette.text
                        }

                        QGCLabel {
                            text: qsTr("Automate your drone")
                            font.pointSize: SVUnits.svText
                            color: qgcPalette.text
                        }
                    }
                }

                SVBorder {
                    anchors.fill: parent
                    borderVisible: true
                    radius: SVUnits.radius
                }
            }

            Column {
                width: parent.width
                spacing: SVUnits.bigMargin * 4

                Column {
                    width: parent.width
                    spacing: SVUnits.margin

                    QGCLabel {
                        text: qsTr("About")
                        font.pointSize: SVUnits.svText
                        color: qgcPalette.buttonHighlight
                    }

                    QGCLabel {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Synclair: QGroundControl combines the familiar QGroundControl flight workflow with SynclairVision tools for DigiView camera payloads. Connect to DigiView to manage live video, camera layouts, movement and zoom, AI overlays and tracking, recording, network profiles, and operator shortcuts from the Fly view.")
                        font.pointSize: SVUnits.svText
                        color: qgcPalette.text
                    }

                    Rectangle {
                        width: SVUnits.objectWidth
                        height: SVUnits.margin - SVUnits.lineWidth * 2
                        color: "transparent"
                    }

                    Rectangle {
                        width: parent.width
                        height: SVUnits.lineWidth
                        color: qgcPalette.windowShade
                    }
                }

                Column {
                    id: getStarted

                    width: parent.width
                    spacing: SVUnits.margin

                    QGCLabel {
                        text: qsTr("Get Started")
                        font.pointSize: SVUnits.svText
                        color: qgcPalette.buttonHighlight
                    }

                    Column {
                        width: parent.width
                        spacing: SVUnits.margin

                        Repeater {
                            model: root.getStartedSteps.length

                            Row {
                                required property int index

                                width: parent.width
                                spacing: SVUnits.bigMargin

                                QGCLabel {
                                    width: SVUnits.objectWidth * 0.35
                                    text: (index + 1) + "."
                                    font.pointSize: SVUnits.svText
                                    font.bold: true
                                    color: qgcPalette.buttonHighlight
                                    horizontalAlignment: Text.AlignRight
                                }

                                QGCLabel {
                                    width: parent.width - x
                                    text: root.getStartedSteps[index]
                                    wrapMode: Text.WordWrap
                                    font.pointSize: SVUnits.svText
                                    color: qgcPalette.text
                                }
                            }
                        }

                        QGCLabel {
                            width: parent.width
                            text: qsTr("The default profiles target DigiView hosts at 192.168.4.60 and 192.168.4.126. Your system may use different addresses.")
                            wrapMode: Text.WordWrap
                            font.pointSize: SVUnits.svText
                            color: qgcPalette.text
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: SVUnits.lineWidth
                        color: qgcPalette.windowShade
                    }
                }

                Column {
                    width: parent.width
                    spacing: SVUnits.margin

                    QGCLabel {
                        text: qsTr("Learn More")
                        font.pointSize: SVUnits.svText
                        color: qgcPalette.buttonHighlight
                    }

                    Item {
                        width: parent.width
                        height: SVUnits.objectWidth / 1.5

                        SVBackground {
                            anchors.fill: parent
                            hoverEnabled: true
                            transparentBackground: true
                            hovered: documentationMouse.containsMouse
                            checkable: true
                            pressed: documentationMouse.pressed
                            hoverPosition: Qt.point(documentationMouse.mouseX, documentationMouse.mouseY)
                            radius: SVUnits.radius
                        }

                        QGCLabel {
                            anchors.left: parent.left
                            anchors.leftMargin: SVUnits.bigMargin
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("User Documentation")
                            font.pointSize: SVUnits.svText
                            color: qgcPalette.text
                        }

                        QGCColoredImage {
                            anchors.right: parent.right
                            anchors.rightMargin: SVUnits.bigMargin
                            anchors.verticalCenter: parent.verticalCenter
                            width: SVUnits.width * 2
                            height: SVUnits.width * 2
                            source: "/qmlimages/external_link.svg"
                            color: "white"
                        }

                        MouseArea {
                            id: documentationMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(root.userGuideUrl)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: SVUnits.lineWidth
                        color: qgcPalette.windowShade
                    }

                    Item {
                        width: parent.width
                        height: SVUnits.objectWidth / 1.5

                        SVBackground {
                            anchors.fill: parent
                            hoverEnabled: true
                            transparentBackground: true
                            hovered: releaseNotesMouse.containsMouse
                            checkable: true
                            pressed: releaseNotesMouse.pressed
                            hoverPosition: Qt.point(releaseNotesMouse.mouseX, releaseNotesMouse.mouseY)
                            radius: SVUnits.radius
                        }

                        QGCLabel {
                            anchors.left: parent.left
                            anchors.leftMargin: SVUnits.bigMargin
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Release Notes")
                            font.pointSize: SVUnits.svText
                            color: qgcPalette.text
                        }

                        QGCColoredImage {
                            anchors.right: parent.right
                            anchors.rightMargin: SVUnits.bigMargin
                            anchors.verticalCenter: parent.verticalCenter
                            width: SVUnits.width * 2
                            height: SVUnits.width * 2
                            source: "/qmlimages/external_link.svg"
                            color: "white"
                        }

                        MouseArea {
                            id: releaseNotesMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally(root.releaseNotesUrl)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: SVUnits.lineWidth
                        color: qgcPalette.windowShade
                    }
                }
            }
        }
    }
}
