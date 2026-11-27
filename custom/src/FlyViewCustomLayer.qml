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
    readonly property var _videoAvoidanceRects: {
        if (_mainMap)
            return []
        const rectangles = synclairFlyView.commandAvoidanceRects.map((rect) => {
            const origin = root.mapFromItem(synclairFlyView, rect.x, rect.y)
            return Qt.rect(origin.x, origin.y, rect.width, rect.height)
        })
        const panels = [widgetLayer ? widgetLayer.customLayerTopRightPanel : null, pipView]
        for (const panel of panels) {
            if (panel && panel.visible && panel.width > 0 && panel.height > 0) {
                const origin = root.mapFromItem(panel.parent, panel.x, panel.y)
                rectangles.push(Qt.rect(origin.x, origin.y, panel.width, panel.height))
            }
        }
        return rectangles
    }
    readonly property var _videoCommandPlacement: placeVideoCommands()
    readonly property bool statusStripBelowRightTools: width - parentToolInsets.leftEdgeTopInset
        - parentToolInsets.rightEdgeTopInset - SVUnits.margin * 2 < ScreenTools.defaultFontPixelWidth * 32
    readonly property bool welcomePromptAcknowledged: {
        const shownIds = QGroundControl.settingsManager.appSettings.firstRunPromptIdsShown.rawValue
        return shownIds.includes(QGroundControl.corePlugin.svInitialWelcomePromptId)
    }

    function placeVideoCommands(): var {
        const empty = { y: 0, height: 0, available: false }
        if (_mainMap || dclinkPanel.implicitHeight <= 0 || dclinkPanel.width < dclinkPanel.minimumWidth)
            return empty
        const top = Math.max(0, _videoToolsRect.y)
        const bottom = Math.min(root.height, _videoToolsRect.y + _videoToolsRect.height)
        const left = dclinkPanel.x
        const right = left + dclinkPanel.width
        if (![top, bottom, left, right, dclinkPanel.implicitHeight].every(Number.isFinite))
            return empty
        const margin = SVUnits.margin
        const blocked = _videoAvoidanceRects.filter((rect) => rect.width > 0 && rect.height > 0
            && rect.x - margin < right && rect.x + rect.width + margin > left)
            .map((rect) => ({ top: Math.max(top, rect.y - margin),
                             bottom: Math.min(bottom, rect.y + rect.height + margin) }))
            .filter((span) => span.bottom > span.top)
            .sort((a, b) => a.top - b.top)
        const gaps = []
        let cursor = top
        for (const span of blocked) {
            if (span.top > cursor)
                gaps.push({ top: cursor, bottom: span.top })
            cursor = Math.max(cursor, span.bottom)
        }
        if (cursor < bottom)
            gaps.push({ top: cursor, bottom: bottom })
        const desiredHeight = dclinkPanel.implicitHeight
        const candidates = []
        for (const gap of gaps) {
            const height = Math.min(desiredHeight, gap.bottom - gap.top)
            if (height < dclinkPanel.minimumHeight)
                continue
            const position = SVSettings.dclinkVideoCommandsPosition
            const desiredY = position === "Bottom-right" ? bottom - height
                : position === "Right-center" ? (top + bottom - height) / 2 : top
            const y = Math.max(gap.top, Math.min(desiredY, gap.bottom - height))
            candidates.push({ y: y, height: height, available: true,
                              clipped: desiredHeight - height, distance: Math.abs(y - desiredY) })
        }
        candidates.sort((a, b) => a.clipped - b.clipped || a.distance - b.distance)
        return candidates.length > 0 ? candidates[0] : empty
    }

    QGCToolInsets {
        id: toolInsets

        leftEdgeTopInset: dclinkTargetViews.visible && root._separateColumns
            ? Math.max(root.parentToolInsets.leftEdgeTopInset, dclinkTargetViews.x + dclinkTargetViews.width)
            : root.parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset: parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset: parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset: dclinkPanel.visible && root._mainMap
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
        topEdgeRightInset: dclinkPanel.visible && root._mainMap
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
        missionProgress: SVBackend.missionEditor.flightRunText
        layoutWarning: !root._mainMap && SVSettings.dclinkVideoCommandsVisible
            && dclinkPanel.visibleFavoriteCount > 0 && !synclairFlyView.commandPlacementBlocked
            && !root._videoCommandPlacement.available ? qsTr("Favorites need more space") : ""
        onOptionsRequested: displayOptions.open()
        visible: root._overlayVisible && width > 0
        z: 2
    }

    SVDCLinkPanel {
        id: dclinkPanel

        x: root._mainMap
            ? Math.max(0, root.width - width - root.parentToolInsets.rightEdgeTopInset - SVUnits.margin)
            : Math.max(0, Math.min(root.width - width, root._videoToolsRect.x + root._videoToolsRect.width - width))
        y: root._mainMap
            ? (dclinkStatusStrip.visible ? dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin : 0)
            : root._videoCommandPlacement.y
        width: root._mainMap
            ? Math.min(ScreenTools.defaultFontPixelHeight * 15,
                       Math.max(0, root.width - root.parentToolInsets.leftEdgeTopInset
                                - root.parentToolInsets.rightEdgeTopInset - SVUnits.margin * 2))
            : Math.min(ScreenTools.defaultFontPixelHeight * 13,
                       Math.max(0, root._videoToolsRect.width), Math.max(0, root.width))
        maximumHeight: root._mainMap
            ? Math.max(0, root.height - y - root.parentToolInsets.bottomEdgeRightInset - SVUnits.margin)
            : root._videoCommandPlacement.height
        favoritesOnly: !root._mainMap
        visible: root._overlayVisible && root.flyView && width > 0 && (root._mainMap
            || (SVSettings.dclinkVideoCommandsVisible && visibleFavoriteCount > 0
                && !synclairFlyView.commandPlacementBlocked && root._videoCommandPlacement.available))
        onFavoriteCommandsChangedByUser: (commands) => SVSettings.dclinkFavoriteCommands = commands
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
        videoCommandsVisible: SVSettings.dclinkVideoCommandsVisible
        videoCommandsPosition: SVSettings.dclinkVideoCommandsPosition
        placementWarning: dclinkStatusStrip.layoutWarning
        onHeadingUpRequested: (enabled) => SVSettings.dclinkRadarHeadingUp = enabled
        onRangeRingsRequested: (enabled) => SVSettings.dclinkMapRangeRings = enabled
        onVideoCommandsVisibleRequested: (enabled) => SVSettings.dclinkVideoCommandsVisible = enabled
        onVideoCommandsPositionRequested: (position) => SVSettings.dclinkVideoCommandsPosition = position
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
