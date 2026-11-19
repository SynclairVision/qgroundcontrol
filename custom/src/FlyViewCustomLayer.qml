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
    readonly property bool _targetMapAvailable: !!mapControl && typeof mapControl.addMapItemGroup === "function"
    readonly property var _aircraftCoordinate: _activeVehicle && _target.available
        && _activeVehicle.id === _target.vehicleSystemId && _activeVehicle.vehicleLinkManager
        && !_activeVehicle.vehicleLinkManager.communicationLost && _activeVehicle.coordinate.isValid
        ? _activeVehicle.coordinate : QtPositioning.coordinate()
    readonly property var toolbar: flyView ? flyView.customLayerToolbar : null
    readonly property bool statusStripBelowRightTools: width - parentToolInsets.leftEdgeTopInset
        - parentToolInsets.rightEdgeTopInset - SVUnits.margin * 2 < ScreenTools.defaultFontPixelWidth * 32
    readonly property bool welcomePromptAcknowledged: {
        const shownIds = QGroundControl.settingsManager.appSettings.firstRunPromptIdsShown.rawValue
        return shownIds.includes(QGroundControl.corePlugin.svInitialWelcomePromptId)
    }

    QGCToolInsets {
        id: toolInsets

        leftEdgeTopInset: dclinkPanel.visible
            ? Math.max(parentToolInsets.leftEdgeTopInset, dclinkPanel.x + dclinkPanel.width)
            : parentToolInsets.leftEdgeTopInset
        leftEdgeCenterInset: parentToolInsets.leftEdgeCenterInset
        leftEdgeBottomInset: parentToolInsets.leftEdgeBottomInset
        rightEdgeTopInset: parentToolInsets.rightEdgeTopInset
        rightEdgeCenterInset: parentToolInsets.rightEdgeCenterInset
        rightEdgeBottomInset: parentToolInsets.rightEdgeBottomInset
        topEdgeLeftInset: dclinkTargetViews.visible
            ? Math.max(root.parentToolInsets.topEdgeLeftInset, dclinkTargetViews.y + dclinkTargetViews.height)
            : (dclinkPanel.visible
                ? Math.max(root.parentToolInsets.topEdgeLeftInset, dclinkPanel.y + dclinkPanel.height)
                : root.parentToolInsets.topEdgeLeftInset)
        topEdgeCenterInset: dclinkStatusStrip.visible
            ? Math.max(root.parentToolInsets.topEdgeCenterInset,
                       dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin)
            : root.parentToolInsets.topEdgeCenterInset
        topEdgeRightInset: parentToolInsets.topEdgeRightInset
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
        width: Math.max(0, root.width - x - SVUnits.margin
                        - (root.statusStripBelowRightTools ? 0 : root.parentToolInsets.rightEdgeTopInset))
        client: SVBackend.dclink
        visible: SVState.synclairOverlay && !SVState.cursorTrackingSessionActive && width > 0
        z: 2
    }

    SVDCLinkPanel {
        id: dclinkPanel

        x: root.parentToolInsets.leftEdgeTopInset + SVUnits.margin
        y: dclinkStatusStrip.visible ? dclinkStatusStrip.y + dclinkStatusStrip.height + SVUnits.margin : 0
        width: Math.min(ScreenTools.defaultFontPixelHeight * 15, Math.max(0, root.width - x))
        maximumHeight: Math.max(0, root.height - y - root.parentToolInsets.bottomEdgeLeftInset - SVUnits.margin)
        visible: SVState.synclairOverlay && root.flyView && root.flyView.customLayerMainWindowIsMap
            && root.mapControl && root.mapControl.visible && !SVState.cursorTrackingSessionActive
        z: 2
    }

    SVDCLinkTargetViews {
        id: dclinkTargetViews

        x: dclinkPanel.x
        y: dclinkPanel.y + dclinkPanel.height + SVUnits.margin
        width: dclinkPanel.width
        maximumHeight: Math.max(0, root.height - y - root.parentToolInsets.bottomEdgeLeftInset - SVUnits.margin)
        client: SVBackend.dclink
        rangeRings: SVSettings.dclinkMapRangeRings
        mapAvailable: root._targetMapAvailable
        onViewPreferenceChanged: (radarSelected, expanded) => {
            SVSettings.dclinkTargetRadarSelected = radarSelected
            SVSettings.dclinkTargetPlotExpanded = expanded
        }
        onRangeRingsRequested: (enabled) => SVSettings.dclinkMapRangeRings = enabled
        visible: dclinkPanel.visible && width >= ScreenTools.defaultFontPixelWidth * 18
            && maximumHeight >= minimumHeight
        z: 2
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
        active: root._targetMapAvailable && dclinkPanel.visible
        sourceComponent: Component {
            SVDCLinkTargetMapItems {
                client: SVBackend.dclink
                mapControl: root.mapControl
                aircraftCoordinate: root._aircraftCoordinate
                rangeRings: SVSettings.dclinkMapRangeRings
                onDetailsRequested: dclinkTargetViews.openDetails()
            }
        }
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
