# Shared DCLink target model

`SVBackend.dclink.targetView` is the single read-only QML snapshot for target
presentation. The map and current pilot actions retain their existing behavior.
The pinned [wire contract](../../../message-definitions/dclink-mavlink.md)
defines schema 1 and coordinate conventions.

| Property | Meaning |
| --- | --- |
| `available` | A supported correlated snapshot from the aircraft on this connection is fresh. |
| `present` | The selected target has remaining validity. |
| `targetId`, `source` | Optional selected track/source metadata; null when no fresh target. |
| `vehicleSystemId`, `vehicleComponentId` | Aircraft owning the snapshot; null when unavailable. |
| `controlOwner` | 0 unknown, 1 manual, 2 autopilot, 3 DCLink. Independent of active command. |
| `driving` | A fresh target is actively supplying DCLink control. |
| `azimuthDeg`, `elevationDeg` | Nose azimuth positive right; body elevation positive up. Independently nullable. |
| `behind` | Rear hemisphere, qualified by available azimuth. |
| `horizontalRangeM`, `rangeSource` | Nullable horizontal range; source 0 unknown, 1 runtime estimate. |
| `latitudeDeg`, `longitudeDeg` | Nullable target WGS84 coordinates. |
| `relativeNorthM`, `relativeEastM` | Nullable target-minus-aircraft local NED offsets. |
| `ageMs`, `sourceAgeMs` | Observation and optional measurement ages, advancing locally. |

A null geometry field means unavailable. Zero is a valid value. Consumers must
check `available`, `present` and each required geometry field. An angular-only
target can populate the nose display but cannot create a map marker or a radar
point requiring range. Displaying a target never starts a flight action.

Polling uses its own non-reused 32-bit read IDs at up to 4 Hz. Only a matching
STATUS establishes service liveness. Target responses require matching DCLink
source, request, aircraft, schema and increasing sequence within a boot.
Transport/aircraft changes, ambiguous aircraft, unsupported capabilities and
stale status invalidate the snapshot. The snapshot expires after one second;
target geometry also expires at its reported remaining lifetime after subtracting
the whole request round trip. Repeated polls cannot freshen an old observation.

For manual review with a simulated aircraft: confirm that an angular-only
target has null range/coordinates, lost detections clear geometry, stale STATUS
clears availability despite other traffic, and switching aircraft or disconnecting
clears the snapshot. Confirm that the existing pilot menu and map interaction
remain unchanged. No automated QGC test targets are part of this workflow.

## Nose view

With the SynclairVision overlay enabled and the map as the main view, the nose
card appears below the existing DCLink menu. The map remains visible and
interactive. Open or close the plot with the Nose view button. Opening the
command menu takes priority: when remaining space is limited, the card keeps
its angle readout and status in compact form; it hides if even that cannot fit.
The video inset and bottom controls retain their reserved space.

The plot shows aircraft-relative direction over ±60 degrees on each axis.
Right is positive azimuth; up is positive elevation. The centre cross marks
the aircraft nose. A dot marks a fresh target inside the displayed span.
An arrow at the edge means outside the displayed span or behind the aircraft,
with an explicit status label distinguishing those conditions. Numerical
readouts retain the full angles. Direction alone does not establish range.

Each axis is independent. With only azimuth, a vertical band shows the known
left/right direction; with only elevation, a horizontal band shows the known
up/down direction. The missing readout is a dash. The view never substitutes
zero for missing data or places a rear target at the centre. Missing, expired
or disconnected target data removes the target marker and shows its current
availability state. GPS, position and range are not required.

The card consumes only `targetView`; it does not select targets, send commands
or change map/video modes. Existing flight controls remain in the DCLink menu.

For manual visual review, cover centred and off-centre targets without range
or GPS, each missing axis, behind/outside directions, loss of target data,
light/dark themes, collapse/expand, menu expansion, and constrained viewport
height. Verify that map gestures and the existing flight controls still work.
