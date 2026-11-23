# Shared DCLink target model

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

## Nose view

With the SynclairVision overlay enabled and the map as the main view, the nose
card appears on the left, with the command panel on the right. The map remains visible and
interactive. Select Nose to open the nose view, or select it again to collapse
the plot. Select Radar to switch the same card to target radar. Opening the
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

## Target radar

Select Radar on the target card to view the shared target's horizontal
position relative to the aircraft. Nose and Radar share one card, so only one
plot occupies map space. Select the active view again to collapse or expand
its plot. The command menu retains priority; compact readouts remain when the
plot cannot fit, and the card hides if even those readouts cannot fit. Space
reserved for the video inset and bottom controls is unchanged.

The radar defaults to north-up: north at the top, east to the right, with the
aircraft at the centre. It uses the shared `relativeNorthM`/`relativeEastM` pair;
both must be fresh and finite. North-up needs no GPS coordinates or heading. It
does not combine nose azimuth with range or independently timed aircraft
position to invent a relative offset from angular-only observations. Zero offsets
are valid and put the target at the centre.

Display options also offers heading-up. `aircraftHeadingDeg` accepts only
ATTITUDE yaw or GLOBAL_POSITION_INT heading from the autopilot component of
the single matching aircraft on the operator bus. It is unavailable after
1.5 seconds without a valid message, or on session/aircraft ambiguity or loss.
This is local receipt age, not a guarantee against delayed router replay.
Heading-up rotates the existing north/east offsets; it does not reconstruct
target position from angles/range. Invalid heading hides the dot and compass
and explicitly reports heading unavailable, preserving the requested orientation.

Offset is the horizontal distance calculated from the same north/east pair
used for plotting. The scale fits automatically using 1/2/5 distance steps,
with a minimum outer radius of 50 metres. It changes when the target moves
beyond 90% or inside 20% of the current radius, keeping small movements from
constantly changing scale. The outer radius is labelled; the inner ring is
half that radius. Position loss resets the scale. Distances are shown in
metres or kilometres.

With range alone, the card shows an estimated-range readout and no target dot.
With angles alone, it directs the operator to Nose. A missing coordinate,
invalid value, lost target or stale/disconnected snapshot removes the dot.
Reported range and positions remain runtime observations/estimates, not a
claim of direct range measurement. Radar viewing never selects a target,
starts a command, changes map/video mode or changes flight control authority.

For manual visual review, cover all cardinal directions, zero offsets,
metre/kilometre scaling, small movements near scale thresholds, partial or
nonfinite coordinates, angular-only and range-only data, target loss and
reconnect, view switching and collapse/expand, constrained height, and both
colour themes. Verify that the existing map gestures and pilot menu remain
available. Live telemetry and integrated map interaction require validation
in the complete application.

## Integration and operator review

The [operator guide](../../../docs/en/qgc-user-guide/fly_view/dclink.md)
contains connection prerequisites, display semantics and a manual acceptance
matrix. Display preferences use the existing `SVSettings` persistence/reset
path: `dclinkTargetRadarSelected`, `dclinkTargetPlotExpanded`,
`dclinkMapRangeRings`, `dclinkRadarHeadingUp`, `dclinkFavoriteCommands`,
`dclinkVideoCommandsVisible` and `dclinkVideoCommandsPosition`.
Favorites contain stable command identifiers; Cancel remains fixed and only
applicable aircraft actions are shown. The command list is collapsed initially.
Map Cancel stays fixed and can be starred for the optional favorites-only video
panel. Video favorites start hidden and have no dropdown or editing controls.
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
