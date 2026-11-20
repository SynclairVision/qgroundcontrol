# DCLink target views

The SynclairVision overlay adds DCLink status and target information to Fly
View. The QGC map, mission, aircraft marker, map gestures and flight-command
menu remain available.

## Connect and read status

Use matching QGC and DCLink builds with the shared message definitions. Connect
the aircraft to QGC through its normal telemetry link and select it. Enable
the SynclairVision overlay and connect the DigiView MAVLink UDP network profile
to the router carrying that aircraft and DCLink. This supplies the normal QGC
aircraft/mission display alongside the target data. The DCLink connection must
identify one aircraft.
The runtime must advertise the selected-target view for target graphics to
become available. A status-only peer can still supply the existing command menu.

The compact strip shows **DCLink** with a green connection dot or a red
disconnection dot, the current command name, reported control ownership and
target state. Select **DCLink** to open full connection, command and navigation
details. Pending requests, stopping, errors, uncertain command outcomes and
navigation warnings remain visible without opening details.

Control is reported as Manual, Autopilot, DCLink, Unknown or Unavailable.
A running command alone does not establish control ownership. **Tracking**
means a fresh selected target exists; **Guiding** requires an explicit fresh
report that this target is driving DCLink control. **Lost** means the selected
observation expired or target loss was reported. A missing target-data stream
is **Unavailable**, rather than a false indication of a target lock.

If target data is unavailable, check the connection, selected aircraft and
compatible runtime. Target traffic cannot keep a stale DCLink connection online.

## Commands and favorites

The **Commands** panel is on the right in both map and video layouts. **Cancel**
stays visible. Expand **Commands** to see the other existing aircraft commands;
select a star to keep a command visible when the list is collapsed. Select its
star again to remove it from favorites. Changing a favorite never starts a
command. Each command retains its existing connection, aircraft and availability
checks. Cancel is a DCLink command cancellation, not an aircraft emergency-stop
or disarm control.

Favorites start empty and are saved using command identifiers, so a copter's
favorites do not become different plane commands. Unavailable actions remain
disabled, and the panel scrolls when needed. The list starts collapsed. Select
the panel's **Display options** button for radar orientation and map rings.

## Nose and radar card

The card appears on the left when the map is the main view. On narrow screens,
it moves below the commands when enough space remains.
Select **Nose** or **Radar** to switch its display. Select the active view again
to collapse or expand the plot. Your chosen view and manual collapse preference
are saved. When space is limited, compact readouts take priority over the plot;
when space returns, the plot respects your saved preference. Opening the flight
menu retains priority, with scrolling where needed.

- **Nose** shows target direction relative to the aircraft, over ±60° on each
  axis. It works without range or GPS. An edge indication identifies an
  outside or rearward target. A missing axis is a band and a dash in the
  readout, rather than a target point with an invented zero angle.
- **Radar** plots fresh target-minus-aircraft north/east
  offsets. Its scale fits automatically, with the outer ring labelled and an
  inner ring at half that distance. It needs both offsets; range or angles
  alone do not place a dot. Zero offsets are valid. **Display options** offers
  **North up** (the default) or **Aircraft heading up**, and saves the choice.
  The plot labels its current orientation. Heading-up needs fresh matching
  autopilot ATTITUDE or GLOBAL_POSITION_INT heading on the DigiView MAVLink
  bus. After 1.5 seconds without a valid heading, it shows **Heading unavailable**
  and removes the position dot rather than silently switching orientation.
  Heading is aircraft yaw, not course over ground or camera direction.

Lost, stale or disconnected data removes live target graphics. Changing the
selected aircraft clears the previous aircraft's target before new data is
accepted.

## Geographic target on the map

The standard QGC map shows a **Target** marker when the runtime supplies a fresh,
valid target latitude/longitude pair. An angular-only target has no geographic
marker. A context line connects it to the matching active aircraft when that
aircraft has a valid coordinate and its link is available. The line relates
the map positions; it is not a new flight path or flight instruction.

Target updates preserve the current zoom, orientation and follow settings.
The nose inset and geographic marker can be viewed together. The experimental
GeoMap engine retains the nose/radar card; geographic target overlays currently
use the standard map engine.

When video becomes the main view, the same target, context line and enabled
range rings stay on the small map inset. The inset uses a compact target dot.
Select the inset to enlarge the map, then select the marker for details.
Changing radar orientation
does not change the QGC map's orientation.

## Target details

Tap the geographic marker or select **Details** on the nose/radar card to see the selected
track/source, observation age, available measurement age, angles, estimated
horizontal range and coordinates. Missing values remain dashes. Viewing
information does not select a different target or send a flight command.

**Map range rings** is optional and off initially. When a matching aircraft
coordinate is available, rings show half and full aircraft-to-target map
distance. They are geographic context; the runtime's range estimate remains a
separate readout. The ring preference is saved.

Range rings can also be changed in **Display options**. The details popup
scrolls on small screens. Close it with **Close**, Escape,
or a tap outside it. It closes on target loss, unavailable data, aircraft
changes or leaving the target-card view. Switching layouts preserves the chosen map view
and geographic target overlays.

## Local acceptance review

Use a simulator or a controlled development connection. No live flight is
needed for the checks below. Review the existing pilot-command labels and
availability without starting a flight action.

| Scenario | Expected result |
| --- | --- |
| Connect, with the command menu collapsed | Status remains readable; the map stays in its chosen view. |
| Fresh angular data, no range or position | Nose works; no invented radar dot or geographic marker. |
| Fresh north/east offsets | Radar direction and labelled scale follow the relative position. |
| Heading-up at east heading, then stop heading telemetry | An east target is ahead; a north target is left; the dot clears when heading expires. |
| Fresh geographic position | Target marker and context line move correctly during map pan/zoom. |
| Tap a marker, then drag/pan elsewhere | Read-only details open; background map gestures remain available. |
| Loss, stale data or disconnect | Target graphics clear; ownership becomes unavailable when its snapshot expires. |
| Change aircraft or reconnect | No previous-aircraft target, popup or uncertain command is replayed. |
| Switch map/video, then return | Target and enabled rings remain on the small map; no duplicate items; layout choice is retained. |
| Resize; expand the command menu; use picture-in-picture | Plots compact or hide; controls and video retain their space. |
| Star/unstar a disabled command | Visibility changes without sending a command; the command stays disabled. |
| Collapse Commands; restart | Cancel and applicable favorites remain visible; the full list starts collapsed. |
| Restart or reset display settings | Preferences restore or reset to Nose, expanded, north-up, rings off, no favorites. |
| Review copter and plane menus | Cancel, Hold/Loiter, Mission where supported, Follow/Intercept and Target Loiter retain their labels and availability. |

Local builds, static checks and simulated UI review do not establish hardware
or flight readiness. Complete the relevant vehicle and hardware validation
before operational use.
