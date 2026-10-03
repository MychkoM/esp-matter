# Matter Controller: Wi-Fi, Telnet and USB Console

This project is based on the [Espressif ESP-Matter](https://github.com/espressif/esp-matter)
SDK and extends its [controller example](examples/controller/README.md).
The ESP32 board acts as a Matter controller: it adds devices to its fabric,
sends commands, and reads and writes attributes.

Enter commands in `esp_console` over USB/UART or Telnet over Wi-Fi at the
`matter>` prompt. Matter commands themselves reach devices over the IP network.
Sharing a Wi-Fi network is not enough: the device must join the controller's
fabric. Telnet is intended only for testing on a trusted local network;
there is no TLS or authentication.

## Changes from Upstream

| Change | Purpose |
| --- | --- |
| Optional `esp_console` backend | ESP-IDF REPL instead of CHIP Shell, preserving existing `matter esp ...` commands. |
| `CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE` | Selects the new backend; other examples still default to the original CHIP Shell. |
| UART, USB CDC and USB Serial/JTAG | ESP-IDF configuration selects the REPL transport. |
| Telnet for agents | TCP port `23`, the same `matter esp ...` commands, logs and execution status; no TLS or authentication. |
| Clean Telnet output | ESP-IDF level, timestamp and tag prefixes are removed for all modules; background `ROUTE_HOOK` messages are excluded. USB logs are preserved. |
| Network diagnostics | `up-time` and `mem-dump` use the shared logging path and return data over both USB and Telnet. |
| Controller startup order | The console starts after Matter and commissioner initialization; command registration results are checked. |
| Controller configuration | Enables the new REPL and Wi-Fi station, reusing existing Wi-Fi and Matter handlers. |
| Wi-Fi in the new REPL | Connects through Matter's standard driver without reinitializing Wi-Fi; adds `wifi status` and `wifi scan`. |
| Controller diagnostic logs | Reports the Wi-Fi disconnect reason, assigned IPv4 address and gateway. |
| Separate ESP32-C6 profile | 4 MB flash, internal RAM, USB Serial/JTAG, no BLE commissioning. |
| GitHub Actions | Manual remote ESP32-S3 builds with selectable ESP-IDF version and PSRAM mode. |
| Local file exclusions | Adds `build-podman/` and `.environment/`; existing rules exclude normal build output. |

Main changed files:

- [components/esp_matter_console/esp_matter_console.cpp](components/esp_matter_console/esp_matter_console.cpp), [Kconfig](components/esp_matter_console/Kconfig), [CMakeLists.txt](components/esp_matter_console/CMakeLists.txt) and the [console header](components/esp_matter_console/esp_matter_console.h).
- [Telnet server](components/esp_matter_console/esp_matter_console_network.cpp).
- [Diagnostics handlers](components/esp_matter_console/esp_matter_console_diagnostics.cpp).
- [examples/controller/main/app_main.cpp](examples/controller/main/app_main.cpp).
- [ESP32-S3 profile](examples/controller/sdkconfig.defaults) and [ESP32-C6 profile](examples/controller/sdkconfig.defaults.esp32c6).
- [ESP32-C6 partition table](examples/controller/partitions.esp32c6.csv).
- [Build Controller workflow](.github/workflows/build-controller.yml).
- [.gitignore](.gitignore) and the [controller guide](examples/controller/README.md).

## Platforms and Limitations

| Parameter | Tested ESP32-C6 | Main ESP32-S3 Profile |
| --- | --- | --- |
| Flash | 4 MB | 8 MB |
| PSRAM | Not required | Required; octal by default, quad configurable |
| Console | USB Serial/JTAG and Telnet | UART/USB and Telnet |
| Commissioning | `onnetwork` over IP | `onnetwork`; also BLE when Bluetooth is enabled |
| GitHub workflow | Not supported yet | Supported |

The C6 profile has a single 3 MB application slot. Dual-slot OTA is not
provided. Do not flash an S3 image onto a C6 or use S3 PSRAM settings for C6.

A Wi-Fi Matter device needs a shared, reachable IP network. A
Matter-over-Thread device also needs a Thread Border Router; the C6 profile
described here does not configure one automatically.

Recommended ESP-IDF version: `v6.0.2`. The Matter submodule is pinned to
`539342f32d5f4dc93761c2f9325afe29270068f1`; do not replace it with an arbitrary HEAD.

## First Run on ESP32-C6

The board used here is a Super Mini ESP32-C6 with an ESP32-C6FH4 chip and
4 MB flash. Its owner reported that it previously connected to Wi-Fi with
OTBR firmware. Antenna GPIO settings for other boards, such as XIAO, do not
automatically apply to this board.

You need a C6 board with 4 MB flash, a data-capable USB cable, a Wi-Fi network,
and a Matter device with a known setup PIN. The examples use the macOS port
`/dev/cu.usbmodem101`; replace it with your port on another machine.

### 1. Flash an Existing Local Build

If the previous build files are still available, recompilation is not needed.
Run the following commands from the repository root. This option requires
[uv](https://docs.astral.sh/uv/getting-started/installation/).

```sh
cd examples/controller/build-podman/esp32c6
uv tool run --from esptool==5.4.0 esptool \
    --chip esp32c6 --port /dev/cu.usbmodem101 --baud 460800 \
    write-flash @flash_args
```

The `build-podman/` directory is excluded from Git and is not present in a
fresh clone. If it is missing, complete the local build described below first.

`flash_args` specifies all required files and offsets:

| Address | File |
| --- | --- |
| `0x0` | `bootloader/bootloader.bin` |
| `0x8000` | `partition_table/partition-table.bin` |
| `0x1d000` | `ota_data_initial.bin` |
| `0x20000` | `controller.bin` |

Do not write `controller.bin` at `0x0`: that address belongs to the bootloader.
This command does not fully erase flash or write the NVS region. Existing
data is not guaranteed to survive a change from a different partition table.
Close applications using the serial port before flashing.

### 2. Open the Console

From the same build directory, open a simple serial terminal without
installing ESP-IDF:

```sh
uv run --no-project --with pyserial python -m serial.tools.miniterm \
    /dev/cu.usbmodem101 115200 --raw --dtr 0 --rts 0
```

Exit `miniterm` with `Ctrl+]`. If the prompt is not visible, press Enter.
Opening the port may reset the board; wait for boot to finish and for
`matter>` to appear before entering commands.

Enter the commands below in the board's console, not in the macOS shell.
Do not type the `matter>` prompt itself.

```text
help
matter esp help
matter esp wifi
matter esp controller help
```

The root help should list `wifi`, `controller`, `diagnostics`, `factoryreset`,
`help`, and `terminal` when Telnet is enabled.

### 3. Connect the Controller to Wi-Fi

Replace the SSID and password with your values. Quote arguments containing
spaces. Do not publish commands containing real passwords in logs or issues.

```text
matter esp wifi connect "My Wi-Fi" "password"
```

Wait for the IP address message. The controller and device must be reachable
from each other; guest networks, client isolation and blocked mDNS may prevent
discovery and commissioning.

Check settings and available networks in the new REPL:

```text
matter esp wifi status
matter esp wifi scan
```

`status` shows the saved SSID, country, allowed channels, DHCP client state,
IPv4 address and gateway, but not the password. Before association with an
access point, `DHCP client: not started` and `0.0.0.0` are normal; they do not
indicate a static IP configuration. The STA interface uses DHCP by default.

`scan` performs a passive network scan and reports SSID, channel, RSSI and
authentication mode. It briefly disconnects Wi-Fi and pauses automatic
reconnection, then restores the previous mode. Do not run it during
commissioning or active device control.

### 4. Connect over Telnet

Once the board obtains an IP address, connect from a computer or agent:

```sh
telnet 192.168.1.194 23
```

DHCP assigns the address, so it may change. Check it and the port over USB
with `matter esp terminal info`. For example, these commands work over Telnet:

```text
matter esp help
matter esp wifi status
matter esp controller help
matter esp terminal info
matter esp diagnostics up-time
matter esp diagnostics mem-dump
```

The server negotiates Telnet echo and displays input; the client should not
add duplicate local echo. Enter starts the response on a new line, and
Backspace erases the previous character. Prefixes such as
`I (426254) console_network:` are removed from the network copy of logs for
every module. For example:

```text
matter> matter esp terminal info
Telnet console: 192.168.1.194:23

RESULT ESP_OK
matter>
```

USB retains the original log prefixes. `ROUTE_HOOK` messages about received
IPv6 routes remain on USB but are excluded from Telnet.

To exit a standard Telnet client, press `Ctrl+]`, then enter `quit`.
`Ctrl+C` and `Ctrl+D` close the network session. Telnet Interrupt Process
also closes it. The client message `Connection closed by foreign host`
after Ctrl+C is expected. This does not cancel a Matter operation already
started. The USB console remains available.
`CONFIG_ESP_MATTER_CONSOLE_NETWORK=y` is enabled in the controller profiles;
`CONFIG_ESP_MATTER_CONSOLE_NETWORK_PORT` sets the port (default: `23`).

For agents: the server handles one network session at a time, accepts lines
terminated by LF, CR/LF or CR/NUL, and returns `RESULT ESP_OK` or
`RESULT ESP_ERR_...` followed by `matter>` for each nonempty command.
An empty line only redraws the prompt; it does not execute a command or
produce a `RESULT`. Clients must handle Telnet negotiation and command echo
before the response. The current profile accepts up to 511 bytes per line;
longer lines are rejected in full. Sessions close after 15 minutes without
input. Matter logs may arrive after `RESULT`: success means the handler
accepted the command, not that pairing or communication with the device
has completed successfully. Do not run `wifi scan` over Telnet: it disconnects
Wi-Fi and may terminate the session.

This is access to Matter commands, not SSH or an operating system shell.
Any client on a reachable network can execute commands, including factory
reset. Do not expose the port to the internet; use only a test LAN.

### 5. Add a Matter Device

The device must already be connected to the network with its commissioning
window open. For a device enrolled in another ecosystem, open a multi-admin
window in that ecosystem's app and use the PIN issued for that window.

```text
matter esp controller pairing onnetwork 1234 20202021
```

`1234` is an unused node ID you choose within this controller's fabric.
`20202021` is an example PIN, not a universal code; replace it with the
device's actual PIN. Wait for commissioning to complete successfully before
sending device commands.

BLE is disabled in the C6 profile. The `ble-wifi` command cannot be used with
this build, even if it appears in the handler's general help.

### 6. Control the Device

For an On/Off device with endpoint `1` and cluster `6`:

| Action | Command |
| --- | --- |
| Turn on | `matter esp controller invoke-cmd 1234 1 6 1` |
| Turn off | `matter esp controller invoke-cmd 1234 1 6 0` |
| Toggle | `matter esp controller invoke-cmd 1234 1 6 2` |
| Read OnOff | `matter esp controller read-attr 1234 1 6 0` |
| Subscribe to OnOff | `matter esp controller subs-attr 1234 1 6 0 1 60` |
| Remove pairing | `matter esp controller pairing unpair 1234` |

Replace the node ID and endpoint with those of your device. Not all devices
support endpoint `1` or the On/Off cluster. The example subscription intervals
are 1 and 60 seconds.

To discover endpoints, read `PartsList` from the root Descriptor cluster:
endpoint `0`, cluster `29`, attribute `3`.

```text
matter esp controller read-attr 1234 0 29 3
```

For the full syntax of `write-attr`, `read-event`, subscriptions and JSON
command arguments, use:

```text
matter esp controller help
matter esp diagnostics
```

## Local Rebuild

The previous local build used Podman with
`docker.io/espressif/esp-matter:latest_idf_v6.0.2`. The commands below describe
rebuilding in a configured local ESP-IDF/ESP-Matter environment.

Before building, install ESP-IDF `v6.0.2`, Matter tools and dependencies using
the [official instructions](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html).
The path `$HOME/esp/esp-idf` below assumes ESP-IDF is installed there.
Both `export.sh` scripts activate existing tools; they do not install them.

From the repository root:

```sh
source "$HOME/esp/esp-idf/export.sh"
source ./export.sh
cd examples/controller
idf.py -B build/esp32c6 -D SDKCONFIG="$PWD/build/esp32c6/sdkconfig" \
    -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32c6 set-target esp32c6 build
idf.py -B build/esp32c6 -p /dev/cu.usbmodem101 flash monitor
```

Exit ESP-IDF monitor with `Ctrl+]`. New output is placed in
`examples/controller/build/esp32c6/`, not the previous `build-podman/esp32c6/`.
For subsequent builds in the same environment and directory, use:

```sh
idf.py -B build/esp32c6 build
idf.py -B build/esp32c6 -p /dev/cu.usbmodem101 flash monitor
```

## Remote Builds with GitHub Actions

1. Push the changes to your GitHub repository. The workflow must be on the
  default branch for the first manual run. Enable Actions in a fork.
2. Open **Actions > Build Controller > Run workflow**.
3. Select the branch, ESP-IDF `v6.0.2` or `v5.5.5`, and PSRAM `octal` or `quad`
  to match your ESP32-S3 module.
4. After a successful build, download the `controller-esp32s3-...` artifact.
  Logs are available in `controller-build-log-...`; artifacts are kept for 14 days.

This workflow requires no repository secrets. It builds only the ESP32-S3
Wi-Fi controller, not C6 or the OTBR variant.

After extracting the artifact, enter the `build` directory containing
`flash_args`. In an activated ESP-IDF environment, replace `<PORT>` with your
port and run:

```sh
esptool --chip esp32s3 --port <PORT> write-flash @flash_args
python -m esp_idf_monitor --port <PORT> --baud 115200 controller.elf
```

Do not use this artifact for the ESP32-C6 board on `/dev/cu.usbmodem101`.

## Diagnostics and Reset

These commands are available over USB or Telnet:

```text
matter esp diagnostics
matter esp diagnostics up-time
matter esp diagnostics mem-dump
```

The first command lists the diagnostic subcommands. `up-time` reports
milliseconds since the last boot, for example
`Uptime of the device: 10994 milliseconds`.

`mem-dump` returns a table with `Internal` and `SPIRAM` columns in bytes.
The rows `Current Free Memory`, `Largest Free Block` and `Min. Ever Free Size`
report current free memory, the largest free block and minimum free memory
since boot. On the Super Mini C6 without PSRAM, `SPIRAM` values are `0`;
this is normal. Telnet returns the data before `RESULT ESP_OK`, not instead
of it. The handlers use `ESP_LOGI`: direct `printf` output does not reach the
network buffer, so new console responses should use the shared logging path.
INFO logging is enabled in the standard profile.

- No serial port: check the USB cable, board connector and device list with
  `ls /dev/cu.usbmodem*` on macOS.
- Port busy: close the serial terminal before starting esptool or another
  monitor. For Web Serial, click Disconnect in Chrome; an open tab may hold
  the port.
- No response to the first command: wait for `matter>` after opening USB;
  a command sent during boot may be lost.
- PSRAM error during S3 startup: check octal/quad mode and whether the module
  has PSRAM. For C6, use the separate profile without PSRAM.
- Pairing fails: check the PIN, commissioning window, network and mDNS.
  The example defaults to a test PAA; commercial devices may require trusted
  PAA configuration as described in the
  [controller documentation](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html).
- Wi-Fi does not connect: check `wifi status`, `wifi scan` and
  `Wi-Fi disconnected, reason=...` messages. Code `201` means
  `WIFI_REASON_NO_AP_FOUND`, `202` means authentication failure, and `204`
  means handshake timeout. With `201`, DHCP is not yet the cause of failure.
  C6 supports only 2.4 GHz Wi-Fi.
- Scanning finds no networks while another nearby device sees 2.4 GHz networks:
  check the board model, antenna presence and connection, power supply and
  RF switch control against the manufacturer's documentation. Antenna GPIO
  settings depend on the board, not just the ESP32-C6 chip model.
- A device command fails: check successful pairing, node ID, endpoint and
  supported cluster. After a device reboot, the old CASE session may time out
  first; retrying the command will establish a new session.

Factory reset removes saved data, including Wi-Fi settings and the fabric.
Use it only intentionally; afterward, reconnect and pair devices again:

```text
matter esp factoryreset
```

## Verification Status

As of October 3, 2026, S3 and C6 variants have been built successfully in
Podman with ESP-IDF `v6.0.2`. The C6 image was flashed onto an ESP32-C6FH4
with 4 MB flash through `/dev/cu.usbmodem101`; hashes of flashed files were
verified. Application startup, the `matter>` prompt, `matter esp help` and
`matter esp controller help` responses were confirmed.

A subsequent C6 diagnostic build with `wifi status` and `wifi scan` was also
built and flashed. Initially, after a full flash erase and reconfiguration,
Wi-Fi did not connect: the board reported `reason=201`, and active and passive
scans found no access points, although the nearby Mac saw several 2.4 GHz
networks. Temporarily disabling Wi-Fi 6 and power saving during diagnostic
scans made no difference; that workaround was removed.

After identifying the Super Mini board, scanning before Matter startup and
the unmodified ESP-IDF Wi-Fi Scan example were tested separately on ESP-IDF
`v6.0.2` and `v5.5.4`. These initial tests found no networks. The `v5.5.4`
tests also used its own bootloader and full RF calibration. After the
experiments, the controller and `v6.0.2` bootloader were restored; the
temporary startup scan was removed. These results do not establish a
controller-only defect, a `v6.0.2`-only regression or a faulty antenna.

The supplied ESP-Thread-BR project was inspected at commit `2fd696a`.
Its main example targets ESP32-S3 with a separate RCP; CI also builds the
ESP-IDF `ot_br` example for C6. No saved C6 binaries or `sdkconfig` were found
in that project. STA connection uses standard `example_wifi_start()` and
`WIFI_PS_MAX_MODEM`. AP+STA mode with `WIFI_PS_NONE` is used only during
web-based network provisioning.

To test this difference, an isolated Wi-Fi Scan build on `v5.5.4` used
AP+STA and OTBR scan settings. After the USB port was released in Chrome,
the flashed probe found nine networks, including the target network on
channel 9 at -50 dBm.

The controller with the `v6.0.2` bootloader was then restored
(ELF SHA256 prefix `8987ebffc`). Without further changes to the controller's
production code, it connected in ordinary STA mode and obtained DHCP address
`192.168.1.194` with gateway `192.168.1.1`. Automatic reconnection and address
assignment were confirmed again after another reset. `matter esp wifi status`
and `matter esp controller help` work. The diagnostic AP is no longer running.
The exact cause of the initial failure remains unknown; recovery cannot be
attributed solely to AP+STA or releasing Chrome's serial connection.

The GitHub workflow has been checked statically, but a remote run has not
been confirmed. The current Telnet build with fixed diagnostics on ESP-IDF
`v6.0.2` is flashed on C6; the application size is `0x1df1a0` (1,962,400 bytes).
Tests covered Telnet negotiation with a standard macOS client, `help`,
`diagnostics`, `wifi status`, `terminal info`, `controller help`, an invalid
command, line overflow and reconnection to `192.168.1.194:23`.
`up-time` returns increasing uptime; `mem-dump` returns all three memory rows
with nonzero Internal values and zero SPIRAM values. Server echo, Enter,
Backspace, empty lines and closure by Ctrl+C, Ctrl+D and Telnet Interrupt
Process were checked. Short idle periods did not disconnect the session;
the full 15-minute timeout was not measured. Prefix removal was checked for
different ESP-IDF tags and levels; USB log preservation and the `ROUTE_HOOK`
filter were also checked.
The network console has no TLS, certificates or password. The USB console
and DHCP continue to work. `factoryreset` was not executed during output
checks, to preserve Wi-Fi settings and the fabric.
Commissioning and control of a specific Matter device have not been tested.
This is a modified SDK example, not a claimed certified production controller.
