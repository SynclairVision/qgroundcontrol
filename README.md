# SynclairVision: QGroundControl

SynclairQGC is SynclairVision's QGroundControl fork for DigiView-enabled camera payloads.
It adds camera controls, layouts, AI overlays and tracking to the standard flight workflow.

## Documentation

- [Downloads](https://github.com/SynclairVision/qgroundcontrol/releases) - SynclairVision release packages for Linux and Windows.
- [User guide](user_guide.md) - installation, connection, controls and troubleshooting.
- [Release notes](CHANGELOG.md) - changes in this fork.
- [QGroundControl user documentation](https://docs.qgroundcontrol.com/) - aircraft setup and mission planning.
- [License](.github/COPYING.md) - upstream QGroundControl licensing.

Use SynclairVision packages for the DigiView integration. Upstream QGroundControl downloads do not include it.
The rest of this README is for developers.

## Build from source

Use Git, CMake, Ninja, Python 3 and the Qt version/modules in
[build-config.json](.github/build-config.json). Linux instructions target Ubuntu 24.04 with GCC/G++.
Windows requires Visual Studio 2022's **Desktop development with C++** workload and the matching MSVC Qt kit.

### Clone

```bash
git clone --recurse-submodules git@github.com:SynclairVision/qgroundcontrol.git
cd qgroundcontrol
```

Recursive submodules are required: this fork generates MAVLink headers from the pinned
`message-definitions` submodule and its nested `mavlink` submodule.

### Linux setup

Install dependencies and the repository-configured Qt SDK:

```bash
python3 ./tools/setup/install_dependencies --platform debian
python3 ./tools/setup/install_python.py qt,scripts
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

Configure, build and run. The `linux_gcc_64` SDK is installed as `gcc_64`:

```bash
python3 ./tools/configure.py \
    -B build \
    --release \
    --qt-root "$HOME/Qt/$QT_VERSION/gcc_64" \
    -- --fresh
cmake --build build --parallel
./build/Release/SynclairQGC
```

### Windows setup and build

Install the repository-configured Qt version and modules using the
[upstream build instructions](https://dev.qgroundcontrol.com/en/getting_started/).
Run these commands from an **x64 Native Tools Command Prompt for VS 2022** in the repository root:

```bat
python tools/setup/install_dependencies --platform windows --nsis
python tools/setup/install_python.py qt,scripts
.venv\Scripts\activate.bat
python tools/configure.py -B build --release --qt-root "C:/Qt/<version>/msvc2022_64" -- --fresh
cmake --build build --parallel
build\Release\SynclairQGC.exe
```

Replace `<version>` with the configured Qt version. NSIS is needed for the Windows installer.

## Package a release manually

Build from a clean checkout of the intended version tag and initialize its recursive submodules before configuring.
The application and Windows installer derive their version from Git tags; an untagged build can retain the previous version.
Use the default `custom/` overlay, which names the application `SynclairQGC`.

After the Release build above, package on each platform:

| Platform | Command | Output in `build/` |
| --- | --- | --- |
| Linux | `cmake --install build --config Release` | `SynclairQGC-<architecture>.AppImage` |
| Windows | `cmake --build build --config Release --target qgc-package` | `SynclairQGC-installer-AMD64.exe` and its SHA-256 file |

On Linux, create the checksum file with `(cd build && sha256sum SynclairQGC-*.AppImage > SHA256SUMS)`.
Check installation, video, camera controls and tracking against the matching DigiView release before publishing.
Upload the Linux/Windows packages, checksums and [release notes](CHANGELOG.md) to a draft GitHub release for review.
The upstream **Release** workflow targets `master` and additional platforms; use this manual path for SynclairVision releases.

## Development

Most SynclairVision-specific code lives under `src/SynclairVision`:

- `Digiview/` contains the DigiView communication and protocol integration.
- `UI/` contains the SynclairVision Fly-view overlay, camera controls, settings, resources, and operator workflows.
- `UI/Settings/SVSettings.qml` stores persistent operator settings.
- `UI/Flyview/SVFlyViewMenusList.js` defines operator menus.

Keep product-specific changes in `src/SynclairVision` and the `custom/` overlay to ease upstream rebases.
Use the `SV` file prefix, the shared QGC palette, SynclairVision resources and `SVUnits.qml` sizing.
See [CODING_STYLE.md](CODING_STYLE.md) for conventions and [tools/README.md](tools/README.md) for development tools.

Native model-tool instructions live in [DigiView](https://github.com/SynclairVision/DigiView/blob/dev-0.7.1/README.md).
Device configuration and WebMenu instructions live in
[Maintenance](https://github.com/SynclairVision/Maintenance/blob/dev-0.7.1-cleanup/docs/external/webmenu.md).
For current limitations, see [Troubleshooting](user_guide.md#7-troubleshooting-and-known-limitations).
