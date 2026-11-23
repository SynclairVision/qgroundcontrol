# DCLink developer notes

This file describes the QGC data model and Fly View integration. Pilot setup,
controls, display behavior and manual review scenarios belong in the
[operator guide](../../../docs/en/qgc-user-guide/fly_view/dclink.md).

## Shared target model

`SVBackend.dclink.targetView` is the single read-only QML snapshot for target
presentation. The map and current pilot actions retain their existing behavior.
The pinned [wire contract](../../../message-definitions/dclink-mavlink.md)
defines schema 1 and coordinate conventions.

| Property | Meaning |
| --- | --- |
| `supported` | Fresh STATUS advertises the selected-target view capability. |
| `available` | A supported correlated snapshot from DCLink is fresh. |
| `vehicleBound` | Snapshot matches a fresh unambiguous aircraft heartbeat; false for camera-only data. |
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

Read-only polling is independent of the pilot-command aircraft-type gate.
With no selected aircraft or observed autopilot, camera-only snapshots can
carry an unknown or reported aircraft identity. QGC exposes no bound aircraft,
control ownership, driving flag or relative position until identity is matched.
Direction, independently valid target coordinates and range keep their wire
validity. Query context changes clear cached data and in-flight requests.
Source/request/schema/freshness/sequence checks remain mandatory. Selection
or ambiguity never falls back to an unbound target. Radar can show relative
bearing as a labelled edge arrow without constructing position from angles
and range; heading-up needs no compass heading for this direction-only cue.

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

## Fly View integration

Display preferences use the existing `SVSettings` persistence/reset
path: `dclinkTargetRadarSelected`, `dclinkTargetPlotExpanded`,
`dclinkMapRangeRings`, `dclinkRadarHeadingUp`, `dclinkFavoriteCommands`,
`dclinkVideoCommandsVisible` and `dclinkVideoCommandsPosition`.
Favorites contain stable command identifiers. Cancel uses the same command-row
and favorite filtering as every other command. New/reset settings favorite
`cancel`; persisted favorite lists, including empty lists, are retained. The
command list shows the complete advertised DCLink catalog in identifier order,
including without an aircraft. Listing and favoriting are independent of command
execution: aircraft, fresh-status, catalog and transaction checks still apply.
Every advertised command uses the same named START path. The `copter/` and
`plane/` namespaces require the matching aircraft kind; other command-specific
prerequisites remain with DCLink. Requests use the existing runtime configuration
without additional command arguments. ACK reports acceptance, not completion;
mutations are sent once and uncertain outcomes retain the reconciliation gate.
Command identifiers may fill the full 32-byte UTF-8 name field; longer names
are rejected without truncation.
The list is collapsed initially.
Video favorites start hidden and have no dropdown or editing controls.
The only DCLink Options entry is in the status strip.
Changing favorites does not mutate runtime state. Automatic compact layout does not overwrite the user's
manual expansion preference.

Video position presets use `SVFlyView.commandAvoidanceRects`, mapped into the
custom layer, to find clear vertical space along the right side. The exports
include expanded menu bounds, the visible control panel, recording information
and notifications. A full-height fit is preferred; longer lists scroll in the
available interval. Narrow areas also require enough width for usable labels.
With no room for one button, the panel hides with an Options warning badge and
tooltip/detail text. The notice does not change strip height, preventing a
feedback loop through HUD layout. The QGC multi-vehicle panel and map inset
are additional obstacles. The DigiView settings drawer blocks video favorites while open.
Per-camera compass/attitude and tracking-flag bounds are exported through the
camera layer, including combined-compass overflow, and merged while video is
visible. Hover tooltips are excluded so commands do not jump around the pointer.

The standard QtLocation map attaches one `SVDCLinkTargetMapItems` group through
the custom Fly View layer. The group reparents into the map before attachment,
removes itself before destruction, and moves explicitly if the map object
changes. It contains a validated target marker, a matching-aircraft context
line and optional geographic distance rings. Target updates never set map
centre, zoom, bearing or follow policy. The experimental GeoMap engine does
not expose this attachment API; the target card stays available there.

The same map group stays attached when the map becomes the video layout's
small inset. The marker becomes a compact dot; the existing PIP click behavior
still expands the map. No map navigation property is changed by the overlay.

Marker taps open read-only details. The popup clears on loss, aircraft changes
or leaving the card view, and its body scrolls within the current window.
Control ownership comes from the fresh target snapshot, with no inference
from command state. Shared-target-capable peers show observation freshness in
the strip; status-only peers use concise conventional high-level target states.
The compact strip uses `commandNameText`, `commandStateText`,
`commandOutcomeUnknown`, `commandPending`, `targetStateText` and
`navigationHealthy`, keeping errors/uncertain outcomes visible without parsing
translated status strings. Full status/navigation detail is available by
selecting DCLink. The connection dot reflects fresh correlated STATUS only.

Validation must distinguish the linked application, native-map simulated UI
review, backend protocol review and actual vehicle operation. Do not treat a
successful UI build or fake snapshot as hardware/flight validation. QGC's
temporary no-test policy applies; use builds, lint/static checks and the
manual scenarios in the operator guide.

For this feature series, the target stream is supplied by DCLink's
`feat/dclink-shared-target-data` branch (or a branch containing that work),
using message-definitions `5170b62`. The `dclink-0.8-compatible` branch alone
supplies the status/command integration; use the shared-target build to
exercise nose, radar and geographic target data.
