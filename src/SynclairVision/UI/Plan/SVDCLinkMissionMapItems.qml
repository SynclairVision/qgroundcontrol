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
    readonly property var _points: active && _editor.waypoints.every(point =>
        point && typeof point.latitude_deg === "number" && Number.isFinite(point.latitude_deg)
        && Math.abs(point.latitude_deg) <= 90 && typeof point.longitude_deg === "number"
        && Number.isFinite(point.longitude_deg) && Math.abs(point.longitude_deg) <= 180) ? _editor.waypoints : []
    property var _radius: null
    readonly property var _selected: _points[_editor.selectedWaypoint] || ({})
    required property bool active
    required property var mapControl

    signal pointSelected

    Component.onCompleted: {
        _line = lineComponent.createObject(mapControl);
        _radius = radiusComponent.createObject(mapControl);
        mapControl.addMapItem(_line);
        mapControl.addMapItem(_radius);
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
                    visible: root.active && root._editor.canEdit && root._editor.selectedWaypoint
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
