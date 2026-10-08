# SynclairVision: QGroundControl

**Automate your drone**

Synclair: QGroundControl is SynclairVision's QGroundControl fork for operating DigiView-enabled camera payloads. It keeps the standard QGroundControl flight workflow and adds SynclairVision controls for live video, camera layouts, payload movement and zoom, AI overlays and tracking, recording, network profiles, and operator shortcuts.

## Documentation

- [User guide](user_guide.md) - setup, connection, controls, layouts, tracking, settings, shortcuts, and troubleshooting.
- [Releases](https://github.com/SynclairVision/qgroundcontrol/releases) - release downloads and release notes.
- [QGroundControl documentation](https://docs.qgroundcontrol.com/) - upstream QGroundControl flight-planning and vehicle documentation.

## Quick start

1. Start `SynclairQGC` and open **Fly** view.
2. Enable the SynclairVision overlay if it is hidden. While **Fly** view is active, the default shortcut is **O**.
3. Open **Settings > Network**, select a DigiView profile, and choose **Connect**.
4. Select a camera view before using camera movement, zoom, overlays, or tracking.
5. Use the on-screen controls or the configured shortcuts. Default movement controls are **W/A/S/D** for pitch/yaw and **Q/E** for zoom.

See the [user guide](user_guide.md) for the complete operator workflow.

## Build from source

### Tested development environment

- Ubuntu 24.04
- Qt 6.11.1
- Git
- CMake
- Ninja
- GCC/G++
- Python 3

### Clone

Clone the repository and initialize all submodules:

```bash
git clone git@github.com:SynclairVision/qgroundcontrol.git
cd qgroundcontrol
git submodule update --init --recursive
```

Recursive submodules are required. This fork generates its custom MAVLink dialect headers from the pinned `message-definitions` submodule and its nested `mavlink` submodule.

### Linux setup

Install system dependencies:

```bash
python3 ./tools/setup/install_dependencies --platform debian
```

Install the Python tooling for Qt, activate the generated virtual environment, and install the Qt version and modules configured by the repository:

```bash
python3 ./tools/setup/install_python.py qt
source .venv/bin/activate

QT_VERSION="$(python3 ./tools/setup/read_config.py --get qt.version)"
QT_MODULES="$(python3 ./tools/setup/read_config.py --get qt.modules)"

python3 ./tools/setup/install_qt.py install \
    --version "$QT_VERSION" \
    --host linux \
    --target desktop \
    --arch linux_gcc_64 \
    --modules "$QT_MODULES" \
    --outdir "$HOME/Qt"
```

Use the repository-configured Qt SDK rather than Debian/Ubuntu Qt packages. `linux_gcc_64` is installed on disk as `gcc_64`.

Set and verify the Qt root:

```bash
QT_ROOT="$HOME/Qt/$QT_VERSION/gcc_64"
test -x "$QT_ROOT/bin/qt-cmake"
"$QT_ROOT/bin/qt-cmake" --version
```

### Build and run

Configure a fresh Release build:

```bash
python3 ./tools/configure.py \
    -B build \
    --release \
    --qt-root "$HOME/Qt/$QT_VERSION/gcc_64" \
    -- --fresh
```

Build and launch:

```bash
cmake --build build --parallel
./build/Release/SynclairQGC
```

## Development

Most SynclairVision-specific code lives under `src/SynclairVision`:

- `Digiview/` contains the DigiView communication and protocol integration.
- `UI/` contains the SynclairVision Fly-view overlay, camera controls, settings, resources, and operator workflows.
- `UI/Settings/SVSettings.qml` stores operator settings and state persisted by the SynclairVision UI.
- `UI/Flyview/SVFlyViewMenusList.js` defines the main operator menu models.

When adding SynclairVision UI code, keep custom files under `src/SynclairVision`, use the `SV` file prefix, reuse the shared QGC palette and SynclairVision resources, use `SVUnits.qml` for sizing, and keep layouts responsive across fullscreen and windowed use.

## Known limitations

The current codebase still contains release-relevant limitations that should be validated before publishing a production release:

- Tracking workflows are still marked for rework and need end-to-end validation with the target DigiView release.
- Positioning the control panel at the top can constrain the zoom-button sizing.
- Joystick and zoom sensitivity can differ between pointer interaction and shortcuts.
- Some shortcuts can still respond outside their intended Fly-view context.
- Still-image capture depends on support in the connected DigiView release.

For user-facing behavior and workarounds, see [Troubleshooting and known limitations](user_guide.md#7-troubleshooting-and-known-limitations).
