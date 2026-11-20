pragma ComponentBehavior: Bound

import QtQuick
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.SynclairVisionUI

Item {
    id: root

    property var parentToolInsets
    property var totalToolInsets: toolInsets
    property var mapControl

    readonly property var flyView: parent ? parent.parent : null
    readonly property var videoControl: flyView ? flyView.customLayerVideoControl : null
    readonly property var pipView: flyView ? flyView.customLayerPipView : null
    readonly property var widgetLayer: flyView ? flyView.customLayerWidgetLayer : null
    readonly property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    readonly property var _target: SVBackend.dclink.targetView
    readonly property bool _mainMap: !!flyView && flyView.customLayerMainWindowIsMap
    readonly property bool _overlayVisible: visible && SVState.synclairOverlay && !SVState.cursorTrackingSessionActive
    readonly property bool _targetMapAvailable: !!mapControl && typeof mapControl.addMapItemGroup === "function"
    readonly property var _aircraftCoordinate: _activeVehicle && _target.available
        && _activeVehicle.id === _target.vehicleSystemId && _activeVehicle.vehicleLinkManager
        && !_activeVehicle.vehicleLinkManager.communicationLost && _activeVehicle.coordinate.isValid
        ? _activeVehicle.coordinate : QtPositioning.coordinate()
    readonly property var toolbar: flyView ? flyView.customLayerToolbar : null
    readonly property bool _separateColumns: width - parentToolInsets.leftEdgeTopInset
        - parentToolInsets.rightEdgeTopInset >= dclinkPanel.width * 2 + SVUnits.margin * 3
    readonly property rect _videoToolsRect: {
        if (_mainMap)
            return Qt.rect(0, 0, 0, 0)
        const tools = synclairFlyView.commandToolsRect
        const origin = root.mapFromItem(synclairFlyView, tools.x, tools.y)
        return Qt.rect(origin.x, origin.y, tools.width, tools.height)
    }
    readonly property bool statusStripBelowRightTools: width - parentToolInsets.leftEdgeTopInset
        - parentToolInsets.rightEdgeTopInset - SVUnits.margin * 2 < ScreenTools.defaultFontPixelWidth * 32
    readonly property bool welcomePromptAcknowledged: {
        const shownIds = QGroundControl.settingsManager.appSettings.firstRunPromptIdsShown.rawValue
        return shownIds.includes(QGroundControl.corePlugin.svInitialWelcomePromptId)
    }

    QGCToolInsets {
        id: toolInsets

        leftEdgeTopInset: dclinkTargetViews.visible && root._separateColumns
            ? Math.max(root.parentToolInsets.leftEdgeTopInset, dclinkTargetViews.x + dclinkTargetViews.width)
            : root.parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset: parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset: parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset: dclinkPanel.visible
            ? Math.max(root.parentToolInsets.rightEdgeTopInset, root.width - dclinkPanel.x)
            : root.parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset: parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset: parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset: dclinkTargetViews.visible && root._separateColumns
            ? Math.max(root.parentToolInsets.topEdgeLeftInset, dclinkTargetViews.y + dclinkTargetViews.height)
            : root.parentToolInsets.topEdgeLeftInset
        topEdgeCenterInset: dclinkStatusStrip.visible
            ? Math.max(root.parentToolInsets.topEdgeCenterInset,
                       dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin)
            : root.parentToolInsets.topEdgeCenterInset
        topEdgeRightInset: dclinkPanel.visible
            ? Math.max(root.parentToolInsets.topEdgeRightInset,
                       dclinkTargetViews.visible && !root._separateColumns
                           ? dclinkTargetViews.y + dclinkTargetViews.height : dclinkPanel.y + dclinkPanel.height)
            : root.parentToolInsets.topEdgeRightInset
        bottomEdgeLeftInset: parentToolInsets.bottomEdgeLeftInset
        bottomEdgeCenterInset: parentToolInsets.bottomEdgeCenterInset
        bottomEdgeRightInset: parentToolInsets.bottomEdgeRightInset
    }

    Binding {
        target: toolbar
        property: "visible"
        value: SVState.toolbar && !QGroundControl.videoManager.fullScreen && !SVState.cursorTrackingSessionActive
    }

    Binding {
        target: widgetLayer ? widgetLayer.customLayerTopRightColumn : null
        property: "visible"
        value: widgetLayer && !widgetLayer.customLayerTopRightPanel.visible && !SVState.synclairOverlay
    }

    Binding {
        target: widgetLayer ? widgetLayer.customLayerBottomRightRow : null
        property: "visible"
        value: !SVState.synclairOverlay || (flyView && flyView.customLayerMainWindowIsMap)
    }

    SVFlyView {
        id: synclairFlyView

        parent: videoControl ? videoControl : root
        anchors.fill: parent
        _widgetMargin: flyView ? flyView.customLayerWidgetMargin : 0
        _toolBarHeight: SVState.toolbar && toolbar ? toolbar.height : 0
        pipViewWidth: pipView && pipView._isExpanded ? pipView.width : ScreenTools.defaultFontPixelHeight * 2
        leftToolStripBottom: widgetLayer ? widgetLayer.customLayerToolStrip.topEdgeLeftInset : 0
        previewMode: videoControl && videoControl.pipState.state === videoControl.pipState.pipState
        statusStripInset: dclinkStatusStrip.visible
            ? dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin : 0
        visible: SVState.synclairOverlay
            && videoControl
            && videoControl.pipState.state !== videoControl.pipState.windowState
        z: 1
    }

    SVDCLinkStatusStrip {
        id: dclinkStatusStrip

        x: root.parentToolInsets.leftEdgeTopInset + SVUnits.margin
        y: root.statusStripBelowRightTools
            ? Math.max(root.parentToolInsets.topEdgeCenterInset, root.parentToolInsets.topEdgeRightInset)
            : root.parentToolInsets.topEdgeCenterInset
        readonly property real availableWidth: Math.max(0, root.width - x - SVUnits.margin
            - (root.statusStripBelowRightTools ? 0 : root.parentToolInsets.rightEdgeTopInset))
        width: Math.min(availableWidth, Math.max(availableWidth / 2, implicitWidth))
        client: SVBackend.dclink
        visible: root._overlayVisible && width > 0
        z: 2
    }

    SVDCLinkPanel {
        id: dclinkPanel

        x: root._mainMap
            ? Math.max(0, root.width - width - root.parentToolInsets.rightEdgeTopInset - SVUnits.margin)
            : Math.max(0, root._videoToolsRect.x + root._videoToolsRect.width - width)
        y: root._mainMap
            ? (dclinkStatusStrip.visible ? dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin : 0)
            : root._videoToolsRect.y + SVUnits.objectWidth + SVUnits.margin
        width: Math.min(ScreenTools.defaultFontPixelHeight * 15,
                        Math.max(0, root.width - root.parentToolInsets.leftEdgeTopInset
                                 - root.parentToolInsets.rightEdgeTopInset - SVUnits.margin * 2))
        maximumHeight: root._mainMap
            ? Math.max(0, root.height - y - root.parentToolInsets.bottomEdgeRightInset - SVUnits.margin)
            : Math.max(0, root._videoToolsRect.y + root._videoToolsRect.height - y
                       - SVUnits.objectWidth - SVUnits.margin)
        visible: root._overlayVisible && root.flyView && width > 0
        onFavoriteCommandsChangedByUser: (commands) => SVSettings.dclinkFavoriteCommands = commands
        onOptionsRequested: displayOptions.open()
        z: 2
    }

    SVDCLinkTargetViews {
        id: dclinkTargetViews

        x: root._separateColumns ? root.parentToolInsets.leftEdgeTopInset + SVUnits.margin : dclinkPanel.x
        y: root._separateColumns
            ? dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin
            : dclinkPanel.y + dclinkPanel.height + SVUnits.margin
        width: dclinkPanel.width
        maximumHeight: Math.max(0, root.height - y - (root._separateColumns
            ? root.parentToolInsets.bottomEdgeLeftInset : root.parentToolInsets.bottomEdgeRightInset) - SVUnits.margin)
        client: SVBackend.dclink
        rangeRings: SVSettings.dclinkMapRangeRings
        mapAvailable: root._targetMapAvailable
        headingUp: SVSettings.dclinkRadarHeadingUp
        aircraftHeadingDeg: SVBackend.dclink.aircraftHeadingDeg
        onViewPreferenceChanged: (radarSelected, expanded) => {
            SVSettings.dclinkTargetRadarSelected = radarSelected
            SVSettings.dclinkTargetPlotExpanded = expanded
        }
        onRangeRingsRequested: (enabled) => SVSettings.dclinkMapRangeRings = enabled
        onOptionsRequested: displayOptions.open()
        visible: root._overlayVisible && root._mainMap && root.mapControl && root.mapControl.visible
            && width >= ScreenTools.defaultFontPixelWidth * 18
            && maximumHeight >= minimumHeight
        z: 2
    }

    Binding {
        target: dclinkPanel
        property: "favoriteCommands"
        value: SVSettings.dclinkFavoriteCommands
    }

    SVDCLinkOptions {
        id: displayOptions

        headingUp: SVSettings.dclinkRadarHeadingUp
        rangeRings: SVSettings.dclinkMapRangeRings
        mapAvailable: root._targetMapAvailable
        onHeadingUpRequested: (enabled) => SVSettings.dclinkRadarHeadingUp = enabled
        onRangeRingsRequested: (enabled) => SVSettings.dclinkMapRangeRings = enabled
    }

    Binding {
        target: dclinkTargetViews
        property: "radarSelected"
        value: SVSettings.dclinkTargetRadarSelected
    }

    Binding {
        target: dclinkTargetViews
        property: "expanded"
        value: SVSettings.dclinkTargetPlotExpanded
    }

    Loader {
        active: root._targetMapAvailable && root._overlayVisible && root.mapControl.visible
        sourceComponent: Component {
            SVDCLinkTargetMapItems {
                client: SVBackend.dclink
                mapControl: root.mapControl
                aircraftCoordinate: root._aircraftCoordinate
                rangeRings: SVSettings.dclinkMapRangeRings
                compact: !root._mainMap
                onDetailsRequested: dclinkTargetViews.openDetails()
            }
        }
    }

    on_OverlayVisibleChanged: {
        if (!_overlayVisible) displayOptions.close()
    }

    SVShortcutHandler {
        parent: flyView ? flyView : root
        anchors.fill: parent
        flyView: synclairFlyView
        toolbarVisible: toolbar ? toolbar.visible : false
        z: QGroundControl.zOrderTopMost
    }

    Connections {
        target: SVState

        function onSynclairOverlayChanged() {
            if (SVState.synclairOverlay && !root.welcomePromptAcknowledged) {
                welcomePromptLoader.active = true
            }
        }
    }

    Loader {
        id: welcomePromptLoader

        active: false
        source: "qrc:/qml/QGroundControl/SynclairVisionUI/Flyview/SVWelcomePrompt.qml"
        onLoaded: item.open()
    }
}
