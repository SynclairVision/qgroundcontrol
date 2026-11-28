# DCLink mission planning

QGC can edit, save and start named DCLink waypoint routes. The editor reads
shared saved revisions over MAVLink. The running DCLink planner admits and
tracks each execution.

DCLink owns the saved mission library. QGC and Maintenance use the same SQLite
library, so either client can create and edit a mission without the other being
present. QGC uses the running DCLink service over MAVLink; Maintenance invokes
DCLink's installed local filesystem helper without a runtime connection.
Helper edits are visible on QGC's next read/Refresh, with revision conflicts
preserving local work. Committed missions survive runtime shutdown; Maintenance
can still load and save while DCLink is stopped if WebMenu and the computer
remain running. QGC retains its local drafts until remote access returns.
The canonical data model and API belong in the shared
[mission contract](https://github.com/SynclairVision/message-definitions/blob/dev-0.8-qgc-missions/missions/README.md)
and [mission schema][schema].
This page specifies QGC interaction and state handling without redefining those
contracts. QGC and DCLink pin matching MAVLink definitions.

[schema]: https://github.com/SynclairVision/message-definitions/blob/dev-0.8-qgc-missions/missions/mission.schema.json

## Available transport interface

`SVBackend.missions` owns the mission-library client separately from flight
commands. It uses the DigiView MAVLink connection without requiring an autopilot
or an active aircraft. Open that connection to the router carrying DCLink.
The default peer is system/component `252/67`; `setPeer(systemId, componentId)`
selects a different explicit nonzero peer and clears its discovered session.

- `discover()` sends a capabilities query and returns its logical request UUID.
- `request(action, payload, requestId)` sends an advertised library operation;
  omit the optional ID for a new operation. An explicit retry uses its original
  ID and unchanged content after rediscovery confirms the same service boot.
- `finished(requestId, action, result)` supplies the complete verified logical
  response. Its `result` code describes the operation, and `data` carries the
  saved identity/revision, catalog or mission record. Create/update return
  metadata; use `missions.get` for complete content.
- `busy`, `available`, `serviceSession`, `capabilities`, `lastResult`, `error`,
  and `outcomeUnknown` supply asynchronous state. Availability requires verified
  discovery, not an aircraft heartbeat.

Library discovery, listing and loading are explicit. After discovery, QGC polls
execution status when advertised. The [wire profile][wire] defines addressed chunks, integrity,
session binding, stop-and-wait exchange retries and limits. Endpoint, sender or
peer changes cancel the current exchange without sending its content elsewhere.
No mission content is exposed until its complete reply is validated.
`cancelRequest()` abandons the current transfer; it is not a flight command or
an undo of a committed save. Cancellation after a mutation COMMIT preserves the
uncertain outcome until review or reconciliation.

If a mutation COMMIT was attempted and its response is lost, `outcomeUnknown`
stays visible through reads, reconnects and rediscovery. A reported storage error
also requires review. `uncertainOperation` preserves the UUID, action, original
endpoint and service session. A same-session retry can reconcile its retained
outcome; it is bound to the original identity and content. Other changes are blocked
until reconciliation or `acknowledgeOutcome()`, which represents an explicit
operator decision after reviewing fresh library state. It sends no request and
does not cancel or alter a saved mission. After a boot change an old mutation
identity cannot be rebound; a new mutation needs that explicit review.

The route editor uses this API without calling aircraft upload or changing
current flight-command controls or the target overlay.

[wire]: https://github.com/SynclairVision/message-definitions/blob/dev-0.8-qgc-missions/missions/mavlink.md

## Delivered route editor

In **Plan**, select **DCLink mission** beside the QGC logo. **Aircraft plan**
retains the existing aircraft planning controls and its separate plan. The
DCLink editor uses the same map and map navigation; only the destination's
route visuals and editing controls are shown. Receiving an aircraft plan does
not recenter a DCLink draft, and aircraft-change prompts wait until returning
to the aircraft destination.

Connect DigiView to the router carrying DCLink and select **Refresh**. Entering
the editor also attempts discovery when needed; there is no background polling.
The panel identifies the current host, port and DCLink system/component. No
aircraft connection is needed for authoring or library operations.

- The picker combines retained local drafts and the selected source's catalog.
  Confirmed entries show a revision; unsaved entries show their draft state.
  Duplicate names receive a short identity suffix. Drafts for another DCLink
  remain identifiable and cannot be saved to the current source accidentally.
- **New** creates a local draft. Edit the name directly to rename it.
  **More → Duplicate / Save as copy** retains the original and creates a new
  local identity. The first **Save to DCLink** confirms the named destination
  and creates the saved mission; later saves check the base revision first.
- **Add waypoints on map** enables insertion until **Done adding** or point
  selection. Select a numbered point to drag it, edit coordinates, altitude
  above HOME or acceptance radius, move it **Up/Down**, or remove it. A selected
  point shows its acceptance circle. New points initially use a 30 m radius
  and the previous point's altitude, or 30 m for the first point. **Fit map**
  frames this mission's route and areas.
- **More → Import QGC plan…** validates the complete supported waypoint subset
  using QGC's `MissionItem` loader, then shows a conversion confirmation.
  Aircraft-default radii require confirmation of the editable positive radius.
  Unsupported content is listed and leaves the current draft intact. Import
  produces a new local draft and never saves or uploads automatically.
- **More → Delete saved mission…** confirms the name and deletes only that
  saved record/revision. Its local route is retained as an unsaved draft.
  **Remove local draft…** separately removes local work.

The implementation is localized in `DCLinkMissionEditor`,
`DCLinkMissionPlanImport`, the `UI/Plan/SVDCLinkMission*` components and the
Plan view/toolbar destination switch. `SVBackend.missionEditor` owns the model
for the application lifetime, independent of view creation and active aircraft.
The existing transport's strict JSON key check is shared with file import.

Drafts are atomically stored in `dclink-mission-drafts.json` in QGC's application
data directory, with mission content, saved base, destination and revision.
There is room for 50 local drafts; explicitly remove unused drafts when full.
A failed/corrupt local store is reported and prevents remote changes rather
than overwriting the original file. The catalog is refreshed in consistent
pages, restarting once if another client changes it during paging. At most
10,000 catalog entries are assembled; an oversized or malformed listing is
reported without replacing the previous catalog.

Before a mutation is sent, its identity, complete payload, destination and
service boot are retained locally. Uncertain saves keep these details through
reconnects and restart, and block other mutations. **Retry original change** is
offered only in the same QGC process, source and DCLink boot. After either
application restarts, refresh and inspect the relevant saved library; explicitly
acknowledge that review before resuming edits. Discovery or a library read never
replays the mutation. A revision conflict offers **Reload** with confirmation or
**Save as copy**. Unsupported documents remain read only with their complete
content retained.

### Run a saved mission

Use an armed, airborne PX4 multicopter in Hold or Position mode, with fresh
HOME and global position. Save the route in **Plan → DCLink mission**. In the
map-side **Fly View** command panel, open **DCLink Mission**, choose a saved
route, and select **Start mission…**. Review the name, revision, and aircraft
before selecting **Confirm Start**. The picker uses the current DCLink library
and leaves the Plan draft alone. If the saved revision changes or is deleted,
refresh and choose a saved revision again.

The Fly View mission panel and status strip show the running mission, revision and waypoint
progress separately from the draft. New saves affect the shared library, and an
admitted run continues with the original route. **Cancel run** requests Hold
for that run. The existing flight controls also remain available. The panel
reports completion after the final waypoint and confirmed Hold.

If status is lost, the strip shows that the run state is unknown. Refresh to
rediscover the runtime and review current status. For a lost Start or Cancel
reply, QGC reconciles a matching run report from the original service session,
or asks the operator to verify the aircraft and confirm review. A new Start
requires a new operator confirmation.

### Manual verification

1. Open both destinations without an autopilot. Confirm the original aircraft
   plan remains intact while DCLink points use the same underlying map.
2. Create a named route, add and drag points, change altitude/radius and reorder
   them. Save, refresh and reload; confirm values and point order remain.
3. Duplicate, rename and save a copy. Delete its saved record, then separately
   remove its retained local draft; confirm the original remains unchanged.
4. Keep a dirty draft while switching entries/destinations, restart QGC and
   reopen it. Reconnect and verify it against the latest saved base before save.
5. Edit the same saved revision from Maintenance. Saving the older QGC draft
   must preserve local work and offer Reload/Save as copy without overwriting.
6. Interrupt a save around its final commit. Refresh/retry the original change
   within the same session, or inspect the library after restart. Confirm no
   automatic replay, duplicate creation or unqualified Saved state.
7. Import a simple relative-HOME `.plan`, including zero/default radii. Confirm
   the radius preview. Reject explicit yaw, holds, complex patterns, terrain or
   absolute altitude, nonempty fence/rally content and duplicate JSON keys.
8. Disconnect or change the DigiView destination. Local edits must remain;
   remote actions must require discovery and a matching draft destination.
9. Add a waypoint rule with message, wait, and message steps and a leg rule with
   one message. Save in QGC, open the same revision in Maintenance, and verify
   the steps and trigger references. In PX4 SITL, start the saved route in Fly
   View and verify the countdown, message order and continuation. Cancel
   during a wait and verify that the route does not advance.
10. Add an **Anywhere on the leg** rule with a camera condition and an
    **At a percentage** rule at 50%. Save, then reopen both rules and verify
    their timing and class settings. In SITL, the Anywhere rule runs once when
    the camera reports a match on that leg. For a conditioned 50% rule, check
    that a match at the crossing runs the steps and a later match leaves
    them skipped.
11. Reorder or remove a waypoint used by the leg. Confirm QGC flags the
    rule and blocks Save until the reference is fixed.

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

The editor keeps the route visible and groups the optional **Rules** and
**Areas** in expandable sections. Each added feature requires a matching
DCLink capability before save. A mission containing unsupported features
opens read-only with its content intact.

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
| Saved, no edits | Show the confirmed revision. Fly View can start it when the matching aircraft and runtime are ready. |
| Local edits | Show **Unsaved changes**; save needs a valid draft and fresh compatible service. |
| Stale/disconnected source | Show last-known library/revision; retain edits; disable remote save/start with a reason. |
| Leaving a dirty draft | Offer **Keep draft**, **Discard** or **Cancel** before switching context. |
| Base revision changed | Preserve draft; offer **Reload** or **Save as copy**. |
| Save reply lost | Show outcome unknown; retain draft and request identity while reconciling. |
| Unsupported import | List rejected items/frames; preserve the existing draft and saved mission. |
| Editing while running | Show saved/draft and running revisions separately; save never changes the run. |
| Start unavailable | Keep authoring available and show the missing prerequisite in the Fly View mission panel. |

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

## Waypoint and leg rules

In the DCLink Plan panel, expand **Rules** and choose **Add rule**. Select
**Reach waypoint** or choose **On a leg** and select two consecutive waypoints.
On a leg, choose **Start of leg**, **Anywhere on the leg**, or **At a percentage**.
A whole percentage marks progress along the route from the first waypoint to
the second. For example, 50% is halfway, 0% is the start of the leg, and 100%
is when DCLink accepts arrival at the second waypoint. The percentage rule
checks once when that point is reached or crossed.
The editor displays waypoint numbers and saves stable waypoint IDs. Removing
a referenced point or changing a leg's order requires fixing the affected
rule before saving.

Set **Condition** to **Camera sees an object** to require a fresh active
classified detection. Leave **Camera class ID (optional)** blank for any class
or enter the numeric ID from your camera model. Waypoint, leg-start, and
percentage rules check the condition once at the selected event. **Anywhere
on the leg** runs once on the first match while that leg is active. With
no camera or area condition, it runs at the beginning of the leg. A missing,
stale, or nonmatching detection leaves the mission on its planned route.

Add **Show message**, **Wait at waypoint**, or a compatible command. The
steps run in the displayed order, then the route continues. For example, at
waypoint 2, show “Starting observation”, wait 10 seconds, then show
“Observation complete”.
Wait steps are available at waypoint arrivals. Use the arrow buttons to
reorder steps or to choose the order of rules that share a trigger.

Refresh the DCLink mission library to load the compatible command choices.
**Face north** is available at waypoint and leg triggers. DCLink pauses at
the current position, turns to north, then continues the steps and the route.
Fly View shows the command and its remaining time budget of up to 20 seconds.
**Cancel run** ends the mission and requests Hold while a command is active.
A command timeout or failure also requests Hold. If a saved step is unavailable
on the connected DCLink, the editor keeps it visible for replacement and
blocks uploading the draft until its command steps are compatible.

**Apply** updates the draft. **Save to DCLink** commits the mission and its rules
together. Every run uses its selected saved revision. The Fly View status strip
shows the wait countdown and new messages. Open **DCLink Mission** to see the
latest message for that run or use the existing Cancel controls. New drafts
and imported routes use mission format 2. Mission format 1 can be opened read
only and is unavailable for Start.

Rules with a camera condition or along-leg timing require a DCLink build
that supports those features. Existing waypoint and leg-start rules continue
to work on mission-format 2 builds.

In PX4 SITL, add **Face north** followed by **Show message** at a waypoint
and at 50% of a leg. Start from Fly View with the aircraft facing away from
north. Verify that the route pauses and the command countdown appears, then
the next step and route resume after the turn. On a second run, cancel during
the turn and verify that Hold is confirmed and the remaining steps are skipped.

## Behavior areas

In **Areas**, choose **Draw area on map**. Tap at least three corners in order,
name the area, and select **Finish area**. QGC closes the outline when you
finish. Use **Undo corner** to remove the last tap, or use **Edit → Redraw** to
replace an existing outline while keeping its rule assignments. **Save to
DCLink** saves the route, rules, and areas together.

An optional advisory appears in the Fly View **DCLink Mission** popup while
the aircraft is inside the area on an active run. To scope an action to that
area, pick its name under **Area** in the rule editor. When the rule also has a
camera condition, both have to match. Removing an area prompts you to repair
its rule references before the draft can be saved.

Try a route that crosses an area from outside to inside and back out. Add an
**Anywhere on the leg** rule scoped to the area, then save and reload the
mission. In PX4 SITL, use the **DCLink Mission** popup to verify that the
advisory appears while inside and the scoped action runs on its first match.

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
- Rule and area data is preserved when a document opens read-only. Saves check
  the required capabilities before uploading the draft.
- In the execution phase, save and start are visibly distinct, prerequisites show
  clear reasons, and edits never mutate a running revision.
- Existing map interaction, aircraft planning, flight controls and the DCLink
  target overlay retain their current behavior.
