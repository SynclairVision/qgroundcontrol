import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.SynclairVisionUI

AbstractButton {
    id: control

    property url iconSource
    property string description: ""
    property real pointSize: ScreenTools.defaultFontPointSize
    property real fontWeight: Font.Normal
    property real heightFactor: 0.25
    property real backRadius: ScreenTools.defaultBorderRadius
    readonly property real _iconHeight: ScreenTools.defaultFontPixelHeight * 1.1
    readonly property color _foreground: checked ? qgcPal.buttonHighlightText : qgcPal.text

    implicitWidth: Math.max(contentItem.implicitWidth + leftPadding + rightPadding,
                            ScreenTools.defaultFontPixelWidth * 4)
    implicitHeight: Math.max(contentItem.implicitHeight + topPadding + bottomPadding,
                             ScreenTools.defaultFontPixelHeight * 1.75)
    topPadding: ScreenTools.defaultFontPixelHeight * heightFactor
    bottomPadding: topPadding
    leftPadding: ScreenTools.defaultFontPixelWidth * 0.8
    rightPadding: leftPadding
    focusPolicy: Qt.StrongFocus
    hoverEnabled: !ScreenTools.isMobile
    Accessible.name: text.length > 0 ? text : description
    Accessible.description: description
    ToolTip.visible: hovered && description.length > 0
    ToolTip.text: description

    QGCPalette { id: qgcPal }

    HoverHandler { id: pointer }

    background: SVBackground {
        enabled: control.enabled
        transparentBackground: true
        normalColor: qgcPal.windowTransparent
        hoverEnabled: control.hoverEnabled
        hovered: control.hovered
        checked: control.checked
        checkable: control.checkable
        pressed: control.down
        hoverPosition: pointer.point.position
        radius: control.backRadius
        borderWidth: Math.max(1, ScreenTools.defaultFontPixelWidth * 0.1)
        borderColor: control.activeFocus ? qgcPal.brandPrimary : qgcPal.windowShadeLight
    }

    contentItem: RowLayout {
        spacing: ScreenTools.defaultFontPixelWidth * 0.5
        opacity: control.enabled ? 1 : 0.55

        QGCColoredImage {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: control._iconHeight
            Layout.preferredWidth: control._iconHeight
            color: control._foreground
            source: control.iconSource
            visible: control.iconSource.toString().length > 0
            fillMode: Image.PreserveAspectFit
            sourceSize.height: control._iconHeight
        }

        QGCLabel {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: control._foreground
            text: control.text
            textFormat: Text.PlainText
            font.pointSize: control.pointSize
            font.weight: control.fontWeight
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            visible: control.text.length > 0
        }
    }
}
