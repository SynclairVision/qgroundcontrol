# DCLink mission planning: Phase 1 design

This document defines the proposed QGC workflow. Phase 1 is a design contract;
the mission library, editor and execution UI described below are not implemented
by this phase.

DCLink owns the saved mission library. QGC and Maintenance are clients of the
same service, so either client can create and edit a mission without the other
being present. The canonical data model and API belong in the shared
[mission contract](https://github.com/SynclairVision/message-definitions/blob/dev-0.8-qgc-missions/missions/README.md)
and [mission schema][schema].
This page specifies QGC interaction and state handling without redefining those
contracts. Phase 1 does not change message-definition pins.

[schema]: https://github.com/SynclairVision/message-definitions/blob/dev-0.8-qgc-missions/missions/mission.schema.json

## Planning destination and ownership

Plan presents an explicit **Aircraft plan / DCLink mission** destination. The
existing aircraft plan, map and aircraft controls retain their current behavior.
The DCLink destination uses a separate mission model and service connection;
**Save to DCLink** must not call the aircraft **Upload** path.

The current `PlanMasterController` follows the active vehicle and uses its
mission, fence and rally managers. Its offline controller vehicle enables local
planning, but it is not a DCLink mission store. Reuse the existing map, waypoint
interaction and polygon/polyline editing components where useful. Keep DCLink
storage and execution separate from aircraft plan transfer.

The DCLink header identifies the selected connection and service endpoint.
The library, revisions and drafts belong to that source as defined by the
shared contract, not to `activeVehicle`. With multiple possible sources, require
an unambiguous selection before reads or writes. Changing the active aircraft
must not silently retarget a draft or save it to another DCLink service.

Authoring, local drafts and mission-library operations do not require an
autopilot. Service freshness and mission capabilities determine whether QGC can
read or save. Execution has separate vehicle and runtime prerequisites, shown as
an inline disabled reason. Mission editing must not inherit the current
vehicle-bound flight-command availability gate.

## Main screen

Keep the map visible, with a mission picker and a compact editor for the selected
item. The picker offers **New**, **Duplicate**, **Rename** and **Delete**.
Names are labels; distinguish duplicate names with a short mission ID only when
needed.

- **New** opens a named local draft; the first **Save to DCLink** creates it in
  the shared library.
- **Duplicate** opens a separate local draft. Its first save creates a new
  mission identity and preserves the original mission.
- **Rename** edits the draft name and uses the same revision-aware save as route
  changes.
- **Delete** names the mission in its confirmation and follows the service's
  validation. A denied deletion shows the returned reason. Deleting a saved
  mission is not a flight cancellation action.

**Save to DCLink** and **Start** are separate actions. Save changes a stored
revision; it never starts or changes a running mission. Show a short state label
such as **Local draft**, **Unsaved changes** or **Saved revision 4** beside the
mission name. Display the running revision separately when execution support is
available.

The route-management MVP has only the **Route** editor. Later phases introduce
**Rules** and **Areas** when both the client editor and selected service support
them. Hide unavailable future tabs instead of filling the screen with disabled
controls. A mission containing unsupported features can be viewed read-only;
QGC must not save a reduced document that drops those features.

## Route MVP

The first editor supports ordered waypoint routes: add points on the map, drag
them, reorder them in the list and edit the supported route values. Show waypoint
numbers for orientation, while preserving stable waypoint IDs across reorder.
References must not acquire a different meaning when displayed numbers change.
Expose each waypoint's acceptance radius, initially 30 m. The future execution
adapter must honor supported radii and preserve waypoint IDs, or block Start
with a reason. The existing executor does not yet preserve these values.
A route with no waypoints can be saved as a draft, but cannot execute.

Use the label **Altitude above HOME (m)**. The MVP stores explicitly
HOME-relative altitude; this is not height above terrain or AGL. A draft can be
authored without a current HOME observation. Execution readiness must verify the
required HOME reference through DCLink; QGC must not substitute its map elevation
or planned-home point for the vehicle's execution reference.

Import of a QGC `.plan` file is an explicit conversion into a local draft, not an
aircraft upload. Accept only the documented simple waypoint subset and supported
HOME-relative altitude representation. Before replacing the draft or saving
anything, list unsupported items or frames and reject the import. This includes
complex survey/landing patterns and mission commands or nonempty fence/rally
content for which the MVP has no mapping. Do not silently strip items or convert
AMSL/terrain altitudes. Full `.plan` roundtrip is a separate scope.

The initial conversion accepts only `SimpleItem` records with
`MAV_CMD_NAV_WAYPOINT` (16), `autoContinue: true`, and a HOME-relative frame
(`MAV_FRAME_GLOBAL_RELATIVE_ALT` (3) or its INT form (6)). Use QGC's existing
loader to obtain degree-valued coordinates and reconcile its altitude fields;
reject conflicting or non-relative altitude metadata.

| Waypoint input | Conversion rule |
| --- | --- |
| Param 1, hold time | Must be zero; timed holds require later support. |
| Param 2, acceptance radius | Positive metres map directly; zero needs an explicitly confirmed positive radius. |
| Param 3, pass radius | Must be zero; fly-through/turn-radius instructions are unsupported. |
| Param 4, yaw | Must be unspecified (`null` in the plan JSON). An explicit yaw, including zero, is unsupported. |
| Params 5–7 | Latitude/longitude in degrees and altitude above HOME; validate against the shared schema. |

For aircraft-default radii, offer 30 m as an editable suggestion and require
confirmation before completing import. Generate waypoint IDs for the new draft.
The conversion preview identifies the
result as a DCLink waypoint route and requires confirmation before replacing a
draft. It explicitly states that aircraft firmware/type, planned HOME and
aircraft cruise/hover settings are not applied; DCLink uses its admitted HOME
and configured execution profile. No aircraft-default radius or behavioral
command is silently replaced by a convenient value.

## Interaction and state

| Situation | Visible state and available interaction |
| --- | --- |
| No source selected | Edit a local draft; explain that read/save require a selected service. |
| Connected, empty library | Show **New**; the first save creates the first mission. |
| Loading | Show loading beside the picker/editor; preserve the draft until a complete result arrives. |
| Saved, no edits | Show confirmed revision; Save is inactive. Start appears in the later execution phase. |
| Local edits | Show **Unsaved changes**; save needs a valid draft and fresh compatible service. |
| Stale/disconnected source | Show last-known library/revision; retain edits; disable remote save/start with a reason. |
| Leaving a dirty draft | Offer **Keep draft**, **Discard** or **Cancel** before switching context. |
| Base revision changed | Preserve draft; offer **Reload** or **Save as copy**. |
| Save reply lost | Show outcome unknown; retain draft and request identity while reconciling. |
| Unsupported import | List rejected items/frames; preserve the existing draft and saved mission. |
| Editing while running | Show saved/draft and running revisions separately; save never changes the run. |
| Start unavailable | Keep authoring available; show the missing prerequisite in the later execution UI. |

Local drafts must survive application restart and remain distinct from confirmed
DCLink revisions. Reopening a draft requires a fresh comparison with its service
revision before saving. A draft restored for one endpoint must not be silently
applied to another endpoint with a similarly named mission.
New drafts can start unbound; selecting a destination explicitly binds them.
Loaded drafts retain service, mission ID and base revision. A duplicate has no
saved identity or base revision until its first save.

After reconnect, refresh service state before deciding whether a save is safe.
Conflicts never overwrite newer content automatically. **Reload** confirms
discarding local changes; **Save as copy** preserves them in a separate mission.
An uncertain save uses the shared contract's reconciliation and retry rules.
Do not allocate another copy or start a mission to resolve uncertainty. A service
restart requires fresh reads and an operator decision before a new mutation.

## Later rules, areas and execution

Rules will use a small guided card: **Where**, **When**, **Do**, and **After** for
the supported completion/resume behavior. Scope choices can reference a waypoint area, route
leg or named area using stable IDs. Available classes and actions must come from
the shared capabilities, not an unrestricted expression editor or free-text
command payload. A class-triggered action and its return to the route are runtime
features, not a map-editor simulation.

Areas will reuse geographic drawing controls with a name and supported behavior
preset. A behavior area is distinct from an aircraft geofence. Drawing an area
does not implement obstacle sensing or avoidance; the service must advertise and
validate that behavior before the editor offers it.

The later execution phase will start an explicit saved mission ID and revision,
then display the service-confirmed run identity and state. An immutable running
revision separates execution from QGC or Maintenance edits. Start requires a
saved, current revision and the service's readiness checks; unsaved edits must
be saved or discarded first. Acceptance of a request is not completion of a
mission.

Compact Fly View progress belongs to that later execution phase. It should show
the running mission/revision, route progress and a concise interruption reason
without replacing the map or existing aircraft controls. Phase 1 leaves the
current Fly View controls and overlay unchanged.

## Review checklist for future implementation

- Switching destinations never reads, uploads or removes the aircraft plan as a
  side effect of a DCLink mission operation.
- Without an autopilot, a valid local route can be authored and saved to a fresh
  mission-capable DCLink service.
- New, duplicate, rename and delete affect the intended mission and service;
  source changes and duplicate names do not change mission identity.
- Reordering points preserves waypoint IDs and any supported references.
- HOME-relative altitude is labelled consistently; unsupported altitude frames
  and `.plan` items are reported before any draft replacement or remote save.
- Disconnect, restart and source changes preserve local drafts without presenting
  cached server data as current.
- Concurrent Maintenance and QGC edits produce a conflict with **Reload** and
  **Save as copy**, preserving both the local draft and the confirmed server state.
- A lost save reply is reconciled without a duplicate mission, implicit start or
  unconditional overwrite.
- Unsupported Rules/Areas tabs remain hidden, and documents with unsupported
  features cannot be saved after losing those features.
- In the execution phase, save and start are visibly distinct, prerequisites show
  clear reasons, and edits never mutate a running revision.
- Existing map interaction, aircraft planning, flight controls and the DCLink
  target overlay retain their current behavior.
