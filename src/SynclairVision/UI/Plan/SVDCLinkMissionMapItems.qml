pragma ComponentBehavior: Bound

import QtLocation
import QtPositioning
import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.PlanView
import QGroundControl.SynclairVisionUI

Item {
    id: root

    readonly property var _editor: SVBackend.missionEditor
    property var _line: null
    property var _preview: null
    property var _previewLine: null
    readonly property var _points: active && _editor.waypoints.every(point =>
        point && typeof point.latitude_deg === "number" && Number.isFinite(point.latitude_deg)
        && Math.abs(point.latitude_deg) <= 90 && typeof point.longitude_deg === "number"
        && Number.isFinite(point.longitude_deg) && Math.abs(point.longitude_deg) <= 180) ? _editor.waypoints : []
    property var _radius: null
    readonly property var _selected: _points[_editor.selectedWaypoint] || ({})
    readonly property var _areas: active && !_editor.readOnly ? _editor.areas.filter(area =>
        area.id !== editingAreaId && area.vertices.length >= 3) : []
    readonly property var _corners: active ? areaOutline : []
    required property bool active
    required property bool drawingArea
    required property string editingAreaId
    required property var areaOutline
    required property var mapControl

    signal pointSelected

    function coordinates(vertices) {
        return vertices.map(point => QtPositioning.coordinate(point.latitude_deg, point.longitude_deg))
    }

    Component.onCompleted: {
        _line = lineComponent.createObject(mapControl);
        _radius = radiusComponent.createObject(mapControl);
        _preview = polygonComponent.createObject(mapControl, {
            draft: true, path: Qt.binding(() => root.coordinates(root._corners)),
            visible: Qt.binding(() => root._corners.length >= 3)
        });
        _previewLine = previewLineComponent.createObject(mapControl);
        mapControl.addMapItem(_line);
        mapControl.addMapItem(_radius);
        mapControl.addMapItem(_preview);
        mapControl.addMapItem(_previewLine);
    }
    Component.onDestruction: {
        if (_line) {
            mapControl.removeMapItem(_line);
            _line.destroy();
        }
        if (_radius) {
            mapControl.removeMapItem(_radius);
            _radius.destroy();
        }
        if (_preview) {
            mapControl.removeMapItem(_preview);
            _preview.destroy();
        }
        if (_previewLine) {
            mapControl.removeMapItem(_previewLine);
            _previewLine.destroy();
        }
    }

    QGCPalette {
        id: palette
    }

    Component {
        id: lineComponent

        MapPolyline {
            line.color: palette.mapMissionTrajectory
            line.width: ScreenTools.defaultFontPixelWidth * 0.4
            path: root._points.map(point => QtPositioning.coordinate(point.latitude_deg, point.longitude_deg))
            z: QGroundControl.zOrderWaypointLines
        }
    }

    Component {
        id: radiusComponent

        MapCircle {
            border.color: palette.brandPrimary
            border.width: ScreenTools.defaultFontPixelWidth * 0.2
            center: QtPositioning.coordinate(root._selected.latitude_deg || 0, root._selected.longitude_deg || 0)
            color: Qt.rgba(palette.brandPrimary.r, palette.brandPrimary.g, palette.brandPrimary.b, 0.1)
            radius: root._selected.acceptance_radius_m || 0
            visible: root.active && root._editor.selectedWaypoint >= 0
            z: QGroundControl.zOrderWaypointLines
        }
    }

    Component {
        id: polygonComponent

        MapPolygon {
            required property bool draft

            border.color: palette.brandPrimary
            border.width: ScreenTools.defaultFontPixelWidth * (draft ? 0.35 : 0.2)
            color: Qt.rgba(palette.brandPrimary.r, palette.brandPrimary.g, palette.brandPrimary.b, draft ? 0.25 : 0.14)
            z: QGroundControl.zOrderWaypointLines - 1
        }
    }

    Component {
        id: previewLineComponent

        MapPolyline {
            line.color: palette.brandPrimary
            line.width: ScreenTools.defaultFontPixelWidth * 0.35
            path: root.coordinates(root._corners)
            visible: root._corners.length === 2
            z: QGroundControl.zOrderWaypointLines - 1
        }
    }

    Repeater {
        model: root._areas

        delegate: Item {
            id: areaDelegate

            required property var modelData
            property var polygon: null

            Component.onCompleted: {
                polygon = polygonComponent.createObject(root.mapControl, {
                    draft: false, path: Qt.binding(() => root.coordinates(areaDelegate.modelData.vertices))
                })
                root.mapControl.addMapItem(polygon)
            }
            Component.onDestruction: {
                if (polygon) {
                    root.mapControl.removeMapItem(polygon)
                    polygon.destroy()
                }
            }
        }
    }

    Repeater {
        model: root.drawingArea ? root._corners : []

        delegate: Item {
            id: cornerDelegate

            required property int index
            required property var modelData
            property var marker: null

            Component.onCompleted: {
                marker = cornerComponent.createObject(root.mapControl)
                root.mapControl.addMapItem(marker)
            }
            Component.onDestruction: {
                if (marker) {
                    root.mapControl.removeMapItem(marker)
                    marker.destroy()
                }
            }

            Component {
                id: cornerComponent

                MapQuickItem {
                    anchorPoint.x: sourceItem.width / 2
                    anchorPoint.y: sourceItem.height / 2
                    autoFadeIn: false
                    coordinate: QtPositioning.coordinate(cornerDelegate.modelData.latitude_deg,
                                                         cornerDelegate.modelData.longitude_deg)
                    z: QGroundControl.zOrderMapItems + 1

                    sourceItem: Rectangle {
                        color: palette.window
                        border.color: palette.brandPrimary
                        border.width: ScreenTools.defaultFontPixelWidth * 0.2
                        width: ScreenTools.defaultFontPixelHeight * 1.25
                        height: width
                        radius: width / 2

                        QGCLabel {
                            anchors.centerIn: parent
                            font.pointSize: ScreenTools.smallFontPointSize
                            text: String(cornerDelegate.index + 1)
                        }
                    }
                }
            }
        }
    }

    Repeater {
        model: root._points

        delegate: Item {
            id: pointDelegate

            property var dragger: null
            required property int index
            property var marker: null
            required property var modelData

            Component.onCompleted: {
                marker = markerComponent.createObject(root.mapControl);
                root.mapControl.addMapItem(marker);
                dragger = dragComponent.createObject(root.mapControl);
            }
            Component.onDestruction: {
                if (dragger) {
                    dragger.destroy();
                }
                if (marker) {
                    root.mapControl.removeMapItem(marker);
                    marker.destroy();
                }
            }

            Component {
                id: markerComponent

                MapQuickItem {
                    anchorPoint.x: sourceItem.anchorPointX
                    anchorPoint.y: sourceItem.anchorPointY
                    autoFadeIn: false
                    coordinate: QtPositioning.coordinate(pointDelegate.modelData.latitude_deg,
                                                         pointDelegate.modelData.longitude_deg)
                    z: QGroundControl.zOrderMapItems

                    sourceItem: MissionItemIndexLabel {
                        checked: root._editor.selectedWaypoint === pointDelegate.index
                        color: checked ? palette.brandPrimary : palette.mapIndicator
                        enabled: !root.drawingArea
                        index: pointDelegate.index + 1
                        label: ""

                        onClicked: {
                            root.pointSelected();
                            root._editor.selectedWaypoint = pointDelegate.index;
                        }
                    }
                }
            }

            Component {
                id: dragComponent

                MissionItemIndicatorDrag {
                    itemCoordinate: pointDelegate.marker.coordinate
                    itemIndicator: pointDelegate.marker
                    mapControl: root.mapControl
                    visible: root.active && !root.drawingArea && root._editor.canEdit && root._editor.selectedWaypoint
                             === pointDelegate.index

                    onClicked: root.pointSelected()
                    onDragStop: root._editor.setWaypointCoordinate(pointDelegate.index, itemCoordinate.latitude,
                                                                   itemCoordinate.longitude)
                    onItemCoordinateChanged: {
                        if (pointDelegate.marker) {
                            pointDelegate.marker.coordinate = itemCoordinate;
                        }
                    }
                }
            }
        }
    }
}
