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

The strip shows connection, current command, reported control ownership,
target state and navigation health. Control is reported as Manual, Autopilot,
DCLink, Unknown or Unavailable. A running command alone does not establish
control ownership. **Target observed** means a fresh target exists;
**Target driving control** requires an explicit fresh runtime report.

If target data is unavailable, check the connection, selected aircraft and
compatible runtime. Target traffic cannot keep a stale DCLink connection online.

## Nose and radar card

The card appears below the DCLink menu when the map is the main view.
Select **Nose** or **Radar** to switch its display. Select the active view again
to collapse or expand the plot. Your chosen view and manual collapse preference
are saved. When space is limited, compact readouts take priority over the plot;
when space returns, the plot respects your saved preference. Opening the flight
menu retains priority, with scrolling above the video inset where needed.

- **Nose** shows target direction relative to the aircraft, over ±60° on each
  axis. It works without range or GPS. An edge indication identifies an
  outside or rearward target. A missing axis is a band and a dash in the
  readout, rather than a target point with an invented zero angle.
- **Radar** is north-up and plots fresh target-minus-aircraft north/east
  offsets. Its scale fits automatically, with the outer ring labelled and an
  inner ring at half that distance. It needs both offsets; range or angles
  alone do not place a dot. Zero offsets are valid.

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

## Target details

Tap the geographic marker or select **Target details** to see the selected
track/source, observation age, available measurement age, angles, estimated
horizontal range and coordinates. Missing values remain dashes. Viewing
information does not select a different target or send a flight command.

**Map range rings** is optional and off initially. When a matching aircraft
coordinate is available, rings show half and full aircraft-to-target map
distance. They are geographic context; the runtime's range estimate remains a
separate readout. The ring preference is saved.

The details popup scrolls on small screens. Close it with **Close**, Escape,
or a tap outside it. It closes on target loss, unavailable data, aircraft
changes or leaving the target-card view. Switching map/video layouts removes
the map overlays as appropriate and preserves the chosen layout.

## Local acceptance review

Use a simulator or a controlled development connection. No live flight is
needed for the checks below. Review the existing pilot-command labels and
availability without starting a flight action.

| Scenario | Expected result |
| --- | --- |
| Connect, with the command menu collapsed | Status remains readable; the map stays in its chosen view. |
| Fresh angular data, no range or position | Nose works; no invented radar dot or geographic marker. |
| Fresh north/east offsets | Radar direction and labelled scale follow the relative position. |
| Fresh geographic position | Target marker and context line move correctly during map pan/zoom. |
| Tap a marker, then drag/pan elsewhere | Read-only details open; background map gestures remain available. |
| Loss, stale data or disconnect | Target graphics clear; ownership becomes unavailable when its snapshot expires. |
| Change aircraft or reconnect | No previous-aircraft target, popup or uncertain command is replayed. |
| Switch map/video, then return | No leftover map items; map/video choice and target-view preference are retained. |
| Resize; expand the command menu; use picture-in-picture | Plots compact or hide; controls and video retain their space. |
| Restart or reset display settings | View/ring preferences restore or reset to Nose, expanded, rings off. |
| Review copter and plane menus | Cancel, Hold/Loiter, Mission where supported, Follow/Intercept and Target Loiter retain their labels and availability. |

Local builds, static checks and simulated UI review do not establish hardware
or flight readiness. Complete the relevant vehicle and hardware validation
before operational use.
