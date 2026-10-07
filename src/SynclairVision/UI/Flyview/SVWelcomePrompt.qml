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

    property int selectedDropdown: -1
    readonly property var getStartedItems: [
        {
            title: qsTr("Connect to DigiView"),
            icon: "/qmlimages/network_connected.svg",
            details: qsTr("Open Settings > Network, select the DigiView profile for your payload, and choose Connect. Wait for control and video to become active before operating the camera.")
        },
        {
            title: qsTr("Select a camera"),
            icon: "/qmlimages/layout_single.svg",
            details: qsTr("Select a camera view before using movement, zoom, per-view overlays, or tracking. Use the layout menu to arrange the available camera feeds.")
        },
        {
            title: qsTr("Control the payload"),
            icon: "/qmlimages/settings_controls.svg",
            details: qsTr("Use the on-screen control panel or keyboard shortcuts to move and zoom the selected camera. Controls can be locked to prevent accidental input.")
        },
        {
            title: qsTr("Use AI and tracking"),
            icon: "/qmlimages/tracking_main.svg",
            details: qsTr("Enable the AI overlay when supported, then use Pixel, GNSS, or Manual tracking from the tracking menu. Validate tracking behavior for your DigiView and aircraft configuration before operational use.")
        }
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

                    RowLayout {
                        id: getStartedRow

                        width: parent.width
                        spacing: SVUnits.bigMargin

                        readonly property real collapsedCardWidth: SVUnits.objectWidth
                        readonly property real defaultCardWidth: (width - (3 * spacing)) / 4
                        readonly property real expandedCardWidth: width - (3 * collapsedCardWidth) - (3 * spacing)

                        Repeater {
                            model: root.getStartedItems.length

                            Item {
                                id: cardItem

                                required property int index
                                readonly property bool isSelected: root.selectedDropdown === index
                                readonly property bool hasSelection: root.selectedDropdown !== -1
                                readonly property bool isCollapsed: hasSelection && !isSelected
                                readonly property var itemData: root.getStartedItems[index]

                                Layout.fillWidth: false
                                Layout.preferredWidth: {
                                    if (!hasSelection) {
                                        return getStartedRow.defaultCardWidth
                                    }
                                    return isSelected ? getStartedRow.expandedCardWidth : getStartedRow.collapsedCardWidth
                                }

                                implicitHeight: SVUnits.objectHeight * 0.5

                                Behavior on Layout.preferredWidth {
                                    NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                                }

                                SVBackground {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    normalColor: qgcPalette.windowShade
                                    hovered: cardMouse.containsMouse
                                    checkable: true
                                    checked: cardItem.isSelected
                                    pressed: cardMouse.pressed
                                    hoverPosition: Qt.point(cardMouse.mouseX, cardMouse.mouseY)
                                    radius: SVUnits.radius
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: cardItem.isCollapsed ? 0 : SVUnits.bigMargin * 1.5
                                    spacing: SVUnits.margin

                                    QGCColoredImage {
                                        Layout.alignment: Qt.AlignVCenter | (cardItem.isCollapsed ? Qt.AlignHCenter : Qt.AlignLeft)
                                        Layout.preferredWidth: SVUnits.width * 2
                                        Layout.preferredHeight: SVUnits.width * 2
                                        source: cardItem.itemData.icon
                                        color: "white"
                                    }

                                    QGCLabel {
                                        Layout.fillWidth: true
                                        text: cardItem.itemData.title
                                        font.pointSize: SVUnits.svText
                                        color: qgcPalette.text
                                        wrapMode: Text.WordWrap
                                        visible: !cardItem.isCollapsed
                                    }
                                }

                                MouseArea {
                                    id: cardMouse

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: root.selectedDropdown = cardItem.isSelected ? -1 : cardItem.index
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: root.selectedDropdown === -1 ? 0 : SVUnits.objectHeight
                        color: qgcPalette.windowShade
                        radius: SVUnits.radius
                        visible: root.selectedDropdown !== -1
                        clip: true

                        Behavior on height {
                            NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
                        }

                        QGCLabel {
                            anchors.fill: parent
                            anchors.margins: SVUnits.bigMargin * 1.5
                            text: root.selectedDropdown === -1 ? "" : root.getStartedItems[root.selectedDropdown].details
                            wrapMode: Text.WordWrap
                            verticalAlignment: Text.AlignVCenter
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
