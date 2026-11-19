# Synclair: QGroundControl User Guide

SynclairQGC adds camera and DigiView controls to QGroundControl. This guide covers installation and operation.
For aircraft setup and mission planning, use the [QGroundControl documentation](https://docs.qgroundcontrol.com/).

## 1. Getting started

### Install

Download a matching package from [SynclairVision releases](https://github.com/SynclairVision/qgroundcontrol/releases):

- **Windows:** run the `SynclairQGC-installer-AMD64.exe` installer, then open **SynclairQGC** from the Start menu.
- **Linux:** make the downloaded AppImage executable in its file properties, then open it.
  In a terminal, use `chmod +x SynclairQGC-*.AppImage` and `./SynclairQGC-<architecture>.AppImage`,
  replacing `<architecture>` with the downloaded filename's architecture.

Use a SynclairVision build compatible with the DigiView version installed on your device.
These packages target Linux and Windows; upstream QGroundControl packages do not include the DigiView controls.

### Connect

1. Start **SynclairQGC** and open **Fly** view.
2. Make sure the SynclairVision overlay is visible; the default shortcut is **O** while in **Fly** view.
3. Open **Settings > Network**.
4. Select or create a DigiView network profile and choose **Connect**.
5. Wait for video and control communication to become active.
6. Select a camera view before using movement, zoom, per-view overlays, or tracking.

The default profiles target DigiView hosts at `192.168.4.60` and `192.168.4.126`. Your system may use different addresses.

---

## 2. Main Fly-view controls

### 2.1 HUD and toolbar

* **HUD**
  * Shows or hides the SynclairVision heads-up display.
* **Toolbar**
  * Shows or hides the top toolbar.
* **SynclairVision overlay**
  * Enables or disables the SynclairVision operator overlay.
* **Lock controls**
  * Prevents accidental movement and zoom commands while the lock is active.

### 2.2 Camera selection

The selected camera receives movement, zoom, overlay and tracking commands.
The **T/Y** tracking shortcuts use the view under the pointer instead.

* Select a camera by clicking its view or by using a configured camera shortcut.
* **Next camera** cycles forward through available views.
* **Previous camera** cycles backward if a shortcut is configured.
* **Deselect camera** clears the active camera.

### 2.3 Camera movement and zoom

The control panel provides yaw, pitch, and zoom controls for the selected camera.

* Pitch moves the camera view up or down.
* Yaw moves the camera view left or right.
* Zoom changes the active camera zoom level.
* Holding the configured small-movement shortcut reduces movement increments for fine control.

The control panel can be shown or hidden and moved through **Settings > Controls**.

### 2.4 Layouts

The layout menu changes how camera feeds are arranged.

Available layouts include:

* Single Camera
* Two Columns
* Two Rows
* Top 2 / Bottom 1
* Top 2 / Bottom 2
* Top 3 / Bottom 1
* Source Frame

Use the **Next layout** shortcut to cycle through layouts.

### 2.5 View overlays

Per-view overlays are available for the active camera.

* **Grid**
  * Shows or hides a thirds grid.
* **Crosshair**
  * Shows or hides a center crosshair.
* **AI detection**
  * Shows or hides the AI detection overlay when supported by the connected DigiView system.

---

### 2.6 DCLink pilot commands

With the SynclairVision overlay enabled, a compact status strip stays visible
above the command panel on the map and over the main video view. It shows the
DCLink connection, current command, target state, and navigation source and health.
It wraps to additional rows on smaller displays. Status remains visible when
the command panel is collapsed; the strip itself does not issue commands.
Missing or stale DCLink status shows unavailable values. Navigation health
reports DCLink's status and does not indicate who controls the aircraft.
Enabling the overlay preserves the current map/video layout, including when
the welcome dialog is shown for the first time.

With the SynclairVision overlay enabled, switch to the map and open **DCLink**.
Opening or closing this menu only changes its visibility. Select an action explicitly to send a command.

| Action | Copter | Plane |
| --- | --- | --- |
| Cancel | Request safe cancellation | Request safe cancellation |
| Hold | Hold | Loiter |
| Mission | Run DCLink's stored route | Unavailable |
| Follow Target / Intercept | Follow Target | Intercept |
| Target Loiter | Target Loiter | Target Loiter |

Follow, Intercept and Target Loiter use the target currently observed by DCLink.
Mission uses DCLink's stored route and its onboard target-follow handoff behavior.
The menu does not select map coordinates or change command parameters.

DCLink shares the selected DigiView profile's MAVLink UDP connection and works without video decoding.
The Maintenance router exposes this connection on port **14570**; DCLink uses its onboard endpoint on port **14880**.
The legacy TCP control fallback does not carry DCLink commands.
Aircraft telemetry must arrive on the shared connection. If QGC has an active aircraft, its system ID must match.
Commands are unavailable when multiple aircraft appear on the same bus, since the DCLink messages have no destination.
Use one operator client on this bus; do not run a second command client alongside QGC.

The menu shows the aircraft type, current command, target status and request feedback.
Only commands returned by DCLink and included in this menu are available.
An accepted request is not confirmation of flight completion.
Requests are sent once. If an acknowledgement is missing, QGC reports an unknown outcome and refreshes current status;
it never resends a command after a timeout or reconnect. Check the reported state before selecting another action.

QGC and DCLink must use matching `DCLINK_STATUS`, `DCLINK_COMMAND` and
`DCLINK_PARAMETER` definitions from the same pinned message-definitions revision.
Update both applications together. The shared wire contract is documented in
`message-definitions/dclink-mavlink.md` in the source checkout.

---

## 3. Recording and photos

### 3.1 Recording

Use **Record** to start or stop recording. The recording destination is selected in **Settings > General**.

The optional recording information box can also be enabled or disabled from settings.

### 3.2 Photos

Use **Photo** to request a still image from DigiView. Currently unsupported.

---

## 4. Tracking

Pixel and GNSS shortcuts use the camera view under the pointer.
Other tracking controls use the selected camera view.

### 4.1 Pixel tracking

**Pixel** starts tracking from a point selected in the video.

1. Select the camera view.
2. Start Pixel tracking.
3. Select the target point in the video.

To start immediately, move the pointer over the desired camera view and press **T** (the default Pixel shortcut).
The view under the pointer is selected when tracking starts, even if another view or no view was selected.
Starting Pixel tracking at a new point replaces the target without first clearing the displayed view.

### 4.2 GNSS tracking

**GNSS** creates a tracking target from a selected position in the video when the required navigation data is available.

Press **Y** (the default GNSS shortcut) to use the pointer position in any visible camera view and select that view.
Tracking shortcuts require an active connection, visible video and unlocked controls.
If the pointer is outside all camera views, no tracking request is sent and the selected view stays unchanged.

### 4.3 Manual tracking

**Manual** switches to coordinate-based tracking using the configured longitude, latitude, and altitude values.

### 4.4 Lock target

**Lock target** (**J**) locks the current target when supported by the active tracking workflow.
Locking a target preserves the saved manual view and stabilization settings.

### 4.5 Stop tracking

Use **Deselect tracking** (**I**) to stop tracking, including while the last view is held after target loss.
DigiView restores the manual view without changing its saved stabilization mode.

Check tracking with your DigiView and aircraft configuration before operational use.

---

## 5. Settings

Open the SynclairVision settings drawer from the Fly view.

### 5.1 General

#### Video

* **Resolution** [`dropdown`]
  * Selects the DigiView output resolution used by the application.
* **Target brightness** [`slider`]
  * Adjusts the target brightness used by the video/tracking workflow.

#### User Interface

* **Simplified User Interface** [`bool`]
  * Reduces the visual complexity of buttons and menus.
* **Align HUD** [`bool`]
  * Aligns HUD elements to the video area.
* **Compass type** [`dropdown`]
  * Changes the compass presentation.

#### AI

* **Enable AI** [`bool`]
  * Stages AI enablement for the next DigiView restart.
* **Scan model** [`dropdown`]
  * Selects the discovered scan model for the next DigiView restart.
* **Detection overlay position** [`dropdown`]
  * Changes the AI overlay position immediately.

AI enablement and scan-model changes are staged. Use **Restart DigiView** to apply them.

#### Record

* **Record destination** [`dropdown`]
  * Selects where recordings are stored.
* **Record information box** [`bool`]
  * Shows or hides recording status information.

#### Other

* **Restart DigiView** [`button`]
  * Applies staged resolution and AI settings, then restarts DigiView.
* **Reset settings** [`button`]
  * Restores saved SynclairVision settings to defaults.

### 5.2 Network

Network profiles store the connection details required for DigiView control, telemetry, and video.

#### Profiles

* **Profile** [`dropdown`]
  * Selects the active DigiView profile.
* **Connect / Disconnect** [`button`]
  * Starts or stops the selected connection.
* **Edit** [`button`]
  * Edits the selected profile.
* **New** [`button`]
  * Creates a new profile.

A profile contains:

* **Profile name** [`string`]
* **Stream name** [`string`]
* **IP address** [`ip`]
* **MAVLink router UDP port** [`int`]
* **Legacy TCP control port** [`int`]
* **Video port** [`int`]
* **Listen port** [`int`]

#### Other

* **Autoconnect on start** [`bool`]
  * Reconnects to the previously selected profile when SynclairQGC starts.
* **Force RTSP video over TCP** [`bool`]
  * Forces RTSP transport over TCP and uses the control fallback.

### 5.3 Controls

#### Control Panel

* **Show control panel** [`bool`]
  * Shows or hides the on-screen movement controls.
* **Control panel position** [`dropdown`]
  * Changes the placement of the control panel.
* **Interaction mode** [`dropdown`]
  * Chooses how click and press behavior is handled.
* **Passive opacity** [`bool`]
  * Makes the control panel partially transparent while idle.
* **Passive opacity value** [`slider`]
  * Controls idle transparency.

#### Joystick

* **Joystick type** [`dropdown`]
* **Joystick size** [`slider`]
* **Joystick ratio** [`slider`]
* **Joystick knob size** [`slider`]
* **Joystick sensitivity** [`slider`]
* **Joystick deadzone** [`slider`]
* **Invert horizontal** [`bool`]
* **Invert vertical** [`bool`]

#### Zoom

* **Zoom size** [`slider`]
* **Zoom sensitivity** [`slider`]

### 5.4 Calibration

The Calibration section exposes SynclairVision calibration controls for supported DigiView releases. Only use calibration procedures that match the installed payload and DigiView version.

### 5.5 Shortcuts

All shortcuts can be changed from the Shortcuts section. Unassigned shortcuts appear with no default key.

### 5.6 Developer

Developer settings expose lower-level AI, camera, and tracking parameters. These values are intended for integration, tuning, and diagnostics rather than normal operation.

---

## 6. Default shortcuts

| Action | Default |
| --- | --- |
| Pitch up | W |
| Pitch down | S |
| Yaw left | A |
| Yaw right | D |
| Zoom in | Q |
| Zoom out | E |
| Fine movement/camera modifier | Shift |
| Disable SynclairVision overlay | O |
| HUD | H |
| Toolbar | B |
| AI detection overlay | F |
| Next layout | L |
| Grid | G |
| Photo | P |
| Record | R |
| View 1 | 1 |
| View 2 | 2 |
| View 3 | 3 |
| View 4 | 4 |
| Next View | V |
| Deselect View | C |
| Pixel tracking | T |
| GNSS tracking | Y |
| Manual tracking | U |
| Deselect tracking | I |
| Lock target | J |

Shortcuts are ignored while a text input or modal overlay has focus. Some movement and zoom shortcuts also require an active View selection.

---

## 7. Troubleshooting and known limitations

### No video or controls after connecting

1. Confirm that the computer can reach the DigiView IP address.
2. Verify the selected network profile, including video and control ports.
3. Disconnect and reconnect the profile.
4. If required by the network, enable **Force RTSP video over TCP**.
5. Confirm that DigiView and its MAVLink routing services are running.

### Camera movement or zoom does not respond

* Select a camera view first.
* Make sure controls are not locked.
* Check that the SynclairVision overlay is enabled.
* Verify that DigiView control communication is connected.
* Check shortcut assignments for conflicts.

### AI options do not take effect

AI enablement, scan-model selection, and some resolution changes are staged. Use **Restart DigiView** after changing them.

### Tracking behaves unexpectedly

Check that QGC and DigiView are compatible, video is visible and controls are unlocked.
GNSS tracking also needs navigation data. Use **Deselect tracking** to leave a held or unwanted target.

### Control panel sizing

Placing the control panel at the top can constrain zoom-button sizing in some window sizes.

### Pointer and shortcut sensitivity

Joystick and zoom behavior can differ between pointer interaction and keyboard shortcuts.

### Shortcuts outside the intended view

Some shortcuts can still respond outside their intended Fly-view context. Use the control lock and review shortcut assignments when operating near other QGroundControl views.

---

## 8. Additional documentation

- [Synclair: QGroundControl repository](https://github.com/SynclairVision/qgroundcontrol)
- [Release notes](https://github.com/SynclairVision/qgroundcontrol/releases)
- [QGroundControl user guide](https://docs.qgroundcontrol.com/)
- [WebMenu and device configuration](https://github.com/SynclairVision/Maintenance/blob/dev-0.7.1-cleanup/docs/external/webmenu.md)
