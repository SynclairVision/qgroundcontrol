# Synclair: QGroundControl User Guide

Synclair: QGroundControl adds SynclairVision camera and DigiView controls to QGroundControl. This guide covers the SynclairVision operator workflow. For aircraft setup, mission planning, and standard QGroundControl features, use the upstream QGroundControl documentation.

## 1. Getting started

1. Start **SynclairQGC** and open **Fly** view.
2. Make sure the SynclairVision overlay is visible.
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

The active camera is the target for camera movement, zoom, overlays, and tracking commands.

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

## 3. Recording and photos

### 3.1 Recording

Use **Record** to start or stop recording. The recording destination is selected in **Settings > General**.

The optional recording information box can also be enabled or disabled from settings.

### 3.2 Photos

Use **Photo** to request a still image from DigiView. Currently unsupported.

---

## 4. Tracking

Tracking commands operate on the active camera view.

### 4.1 Pixel tracking

**Pixel** starts tracking from a point selected in the video.

1. Select the camera view.
2. Start Pixel tracking.
3. Select the target point in the video.

### 4.2 GNSS tracking

**GNSS** creates a tracking target from a selected position in the video when the required navigation data is available.

### 4.3 Manual tracking

**Manual** switches to coordinate-based tracking using the configured longitude, latitude, and altitude values.

### 4.4 Lock target

**Lock target** locks the currently selected detection target when supported by the active tracking workflow.

### 4.5 Stop tracking

Use **Deselect tracking** to clear the current tracking operation.

Tracking behavior is still being refined in this release. Validate tracking on the target DigiView and aircraft configuration before operational use.

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

Tracking is still being refined in this release and must be validated against the target DigiView build and navigation source before operational use.

### Control panel sizing

Placing the control panel at the top can constrain zoom-button sizing in some window sizes.

### Pointer and shortcut sensitivity

Joystick and zoom behavior can differ between pointer interaction and keyboard shortcuts.

### Shortcuts outside the intended view

Some shortcuts can still respond outside their intended Fly-view context. Use the control lock and review shortcut assignments when operating near other QGroundControl views.

---

## 8. Additional documentation

* [Synclair: QGroundControl repository](https://github.com/SynclairVision/qgroundcontrol)
* [Release notes](https://github.com/SynclairVision/qgroundcontrol/releases)
* [QGroundControl user guide](https://docs.qgroundcontrol.com/)
