# P5 CLI

`p5-cli` is a small C++ command-line tool for controlling a Phase One camera over IP using the Phase One Camera SDK.

It currently supports:

- opening a camera by IP address
- printing camera status and properties
- triggering captures to host storage
- listing and downloading stored images
- inspecting and setting camera properties

## Requirements

- CMake 3.20 or newer
- A C++17 compiler
- The Phase One Camera SDK unpacked at `SDK/p1camerasdk-linux/dest`

The build is configured to find the SDK here:

```text
SDK/p1camerasdk-linux/dest/
├── include
├── lib
└── lib/cmake/CameraSDK
```

## Build

Configure and build with CMake:

```bash
cmake -S . -B build
cmake --build build
```

Run the help output:

```bash
./build/p5-cli --help
```

On Linux, the target sets an `RPATH` to the bundled SDK library directory so the executable can find the Camera SDK shared libraries from the repo checkout.

## Usage

```text
./build/p5-cli <camera-ip> status [password]
./build/p5-cli <camera-ip> properties [password]
./build/p5-cli <camera-ip> capture [output.iiq] [password]
./build/p5-cli <camera-ip> live-view [output.jpg|output.ppm] [password]
./build/p5-cli <camera-ip> images [password]
./build/p5-cli <camera-ip> download-image <image-id|filename> [output.iiq] [password]
./build/p5-cli <camera-ip> property-info <property-id> [password]
./build/p5-cli <camera-ip> set-property <property-id> <value> [password]
./build/p5-cli <camera-ip> set-ip <new-ip> [netmask] [gateway] [password]
./build/p5-cli <camera-ip> set-iso <value> [password]
./build/p5-cli <camera-ip> set-aperture <value> [password]
./build/p5-cli <camera-ip> set-shutter-us <microseconds> [password]
./build/p5-cli <camera-ip> set-shutter <display-value> [password]
./build/p5-cli <camera-ip> set-exposure-program <value> [password]
./build/p5-cli <camera-ip> set-host-storage <value> [password]
./build/p5-cli <camera-ip> set-local-storage <value> [password]
```

## Common commands

Show camera status:

```bash
./build/p5-cli 192.168.1.241 status
```

Dump all visible camera properties:

```bash
./build/p5-cli 192.168.1.241 properties
```

Trigger a capture and save the returned IIQ file:

```bash
./build/p5-cli 192.168.1.241 capture shot.iiq
```

List images already on camera storage:

```bash
./build/p5-cli 192.168.1.241 images
```

Download a stored image by numeric ID:

```bash
./build/p5-cli 192.168.1.241 download-image 42 latest.iiq
```

Inspect a property before changing it:

```bash
./build/p5-cli 192.168.1.241 property-info 120
```

Set ISO:

```bash
./build/p5-cli 192.168.1.241 set-iso 400
```

Set only the camera IP address and apply it immediately:

```bash
./build/p5-cli 192.168.1.241 set-ip 192.168.1.250
```

Set the IP and also update netmask and gateway:

```bash
./build/p5-cli 192.168.1.241 set-ip 192.168.1.250 255.255.255.0 192.168.1.1
```

Set shutter speed from the camera display string:

```bash
./build/p5-cli 192.168.1.241 set-shutter 1/1000
```

Set a raw property directly:

```bash
./build/p5-cli 192.168.1.241 set-property 1204 On
```

If the camera requires a password, append it as the last argument.

## Notes

- Property values are camera-defined. Invalid values are rejected by the SDK, and some properties may clamp values based on the camera's property specification.
- `set-property` requires a numeric property ID.
- `download-image` accepts either a numeric image ID or the exact filename reported by the camera.
- `live-view` writes JPEG directly when the SDK returns JPEG frames. RGB24 frames are written as binary PPM (`P6`).
- The code explicitly rejects `live-view` on P5 cameras because Phase One CameraSDK 3.1.8 does not provide live view support for them.

## Output behavior

The CLI prints the Camera SDK version at startup, then opens the camera and executes the requested command. SDK log messages are forwarded to `stderr`.
