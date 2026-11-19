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
