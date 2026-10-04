# ESP32-C6 Matter Controller Handover and Technical Reference

This document combines the controller handover and the former `CONTROLLER_HANDOVER.md`
technical reference. The latest verified results appear first; the archived
build and diagnostic material below includes older observations superseded by
those results. Daily operating commands are in
[CONTROLLER_USER_GUIDE.md](CONTROLLER_USER_GUIDE.md). Wi-Fi credentials are
intentionally omitted.

Snapshot: October 4, 2026. Workspace: `esp-matter`.
Communicate with the user in Russian; keep project documentation in English.

## Immediate Goal and Constraints

BILRESA node **2489** and Nordic Pro Micro nRF52840 node **52840** were
commissioned in the controller's own Thread fabric. C6 Wi-Fi/Telnet repair completed. The latest user request changed the Nordic
commissioning method to BLE: node 52840 was unpaired and successfully
recommissioned through explicit BLE-Thread pairing, controlled over C6 Telnet.

- Do NOT use Home Assistant OpenThread Border Router, its dataset, APIs or fabric.
- The C6 must create and own its network; this is already implemented and verified.
- Do NOT factory-reset BILRESA, erase board NVS, or replace credentials without permission.
- Preserve all dirty/staged changes, including nested submodules. Do not reset, checkout,
  commit or create a branch without permission.
- Plain Telnet is explicitly required for an isolated LAN. No TLS, SSH or authentication.
- Ctrl+C must close the Telnet session, not cancel input.
- The user authorizes repairing C6 Wi-Fi over USB, then requires subsequent work
  through Wi-Fi/Telnet. Preserve BILRESA. The latest user request explicitly
  authorizes unpairing/factory-resetting/recommissioning only nRF52840 and allows
  the Wi-Fi UART console bridge at 192.168.1.106:1234 for network provisioning.
- The user explicitly authorized including pairing and Thread secrets in this local
  document. Keep them out of public commits, external reports and routine diagnostic logs.

## Latest Nordic BLE Recommissioning

The latest user request supersedes the previous no-BLE commissioning request:
unpair nRF and register it through BLE. C6 control remains Wi-Fi/Telnet; the
approved network UART bridge is used only to inspect target readiness.

- C6 Telnet `matter esp controller pairing unpair 52840` again succeeded.
- Last-fabric removal automatically reset nRF. Bridge checks confirmed Thread
  `disabled`, active dataset `Error 23: NotFound`, BLE advertising `enabled`.
- No Thread dataset was provisioned through the bridge in this BLE run.
- C6 Telnet launched the explicit forced-BLE path:
  `matter esp controller pairing ble-thread 52840 <C6-dataset> <setup-pin> 3840`.
  This selects a BLE peer address and supplies the controller's own Thread
  dataset for Matter Network Commissioning. It does not use onnetwork or the
  mixed DiscoveryType::kAll path.
- C6 connected to BLE peer E0:34:86:3C:4C:EA. GAP connection and GATT discovery
  succeeded; PBKDF and PASE traffic explicitly used `[BLE]` in the controller log.
- Attestation, new NOC and `ThreadNetworkSetup` completed over BLE. Operational
  CASE then used `fd0f:b86:e144:1:1989:ed45:1e5e:6554` over Thread.
- CommissioningComplete errorCode=0; final success for
  `1A859A87FE21A493-CE68`. Old CASE was evicted and the BLE connection closed
  automatically during successful cleanup. No USB or SWD access was used.
- Post-pair C6 Telnet reads passed: Nordic VendorName, CommissionedFabrics 1,
  endpoint 1 OnOff false. BILRESA still returned `BILRESA dual button`.
  C6 Wi-Fi remains connected at 192.168.1.194; Ctrl+C closed the Telnet session.
  BLE recommissioning and verification are complete.

## Latest Nordic Reset and Telnet-only Recommissioning

The user explicitly requested unpairing nRF, clearing its network, and registering
it again without BLE. They explicitly approved the ESP32-C3 network UART bridge
at 192.168.1.106:1234. All device access in this follow-up is over Wi-Fi: C6 Telnet
192.168.1.194:23 plus the approved bridge. No USB or SWD access was used.

Verified completed sequence:
- Before removal, nRF software version is 2.9.2+0 and CommissionedFabrics is 1.
- C6 Telnet `matter esp controller pairing unpair 52840` completed with
  `Remove Current Fabric succeeded` and removal of node 0xCE68's fabric.
- Bridge `matter device factoryreset` completed and rebooted nRF. After reset,
  `ot state` was `disabled`, and `ot dataset active -x` returned
  `Error 23: NotFound`. This confirms the saved Thread network was erased.
- Stock firmware starts BLE advertising after factory reset; it was explicitly
  stopped through the bridge before network provisioning or pairing. No BLE
  transport/session was used.
- The existing C6 dataset was provisioned via short `ot dataset` field commands,
  committed active and Thread started. nRF attached in C6-Controller/channel 15
  and reported `leader`; BLE advertising state was `disabled`.
- New nRF operational address: `fd0f:b86:e144:1:be1c:3787:e268:78ad`.
  New mesh-local address: `fd42:e2c8:def2:fad1:8c69:6c6e:e229:2767`.
- BILRESA read through C6 Telnet still returned `BILRESA dual button` after
  Nordic reset. C6 Wi-Fi/Thread dataset/fabric were preserved.
- C6 Telnet `matter esp controller pairing onnetwork 52840 <setup-pin>` completed
  PASE, attestation, provisioning of new operational credentials and CASE.
  The old CASE session was explicitly evicted during commissioning.
  Final response: CommissioningComplete errorCode=0;
  `Commissioning success with node 1A859A87FE21A493-CE68`.
- Post-pair Telnet read passed: CommissionedFabrics 1, VendorName
  `Nordic Semiconductor ASA`, endpoint 1 OnOff false. Its operational address
  matches the new nRF address observed through the bridge.
- Final bridge `matter ble adv state` returned `BLE advertising is disabled`.
  Neither USB/SWD nor BLE pairing was used in this reset/recommissioning run.
  BILRESA node 2489 and the C6-owned network remain intact.

## Latest Wi-Fi Repair and Network Access

October 4, 2026: USB diagnosis found repeated Wi-Fi disconnect reason 201
(NO_AP_FOUND), IPv4 0.0.0.0, while C6's saved Thread network remained active.
`examples/controller/main/app_main.cpp` now explicitly calls
`esp_coex_wifi_i154_enable()` after starting native Thread, guarded by the
native border-router and software coexistence configuration. The ESP-IDF
OpenThread border-router example uses this API; it enables Wi-Fi/802.15.4
coexistence independently of BLE advertising. No BLE recovery was used.

The incremental build passed: app size 0x2c71c0 (2,912,704 bytes), 7% app space
free. Only app partition 0x20000 was flashed; NVS, PAA storage, Wi-Fi credentials,
Thread dataset and the existing Matter fabric were preserved. RAM bootloader
started the new image successfully. C6 automatically connected to <SSID> and
obtained **192.168.1.194**. BLE advertising remained disabled.

**Telnet 192.168.1.194:23 is now hardware-verified from the Mac.** `matter version`
and `matter esp terminal info` succeeded; Ctrl+C closed the session. USB was
closed after checking startup. Subsequent diagnostics use Telnet only.
Telnet-only Wi-Fi driver restart was verified: the session disconnected, C6
reacquired 192.168.1.194, and a fresh Telnet connection succeeded. BILRESA
attribute read returned IKEA of Sweden / BILRESA dual button through the
preserved CASE session. Nordic CASE and read also passed after the app update.
Known restart limitation: after the separate Wi-Fi driver restart, Nordic
requests to the old operational address timed out (CHIP:0x32). An additional
Thread stop/start changed SRP host addresses and old BILRESA traffic then
returned route error 3000004. This issue is not fixed by the startup change;
do not use separate Wi-Fi/Thread restarts as the normal operating workflow.

A controlled full C6 restart (RAM bootloader, no flash/NVS write) restored the
working state. Wi-Fi connected automatically again. Final checks were entirely
through Telnet: C6-Controller/router/channel 15, BLE advertising disabled,
new CASE + successful Nordic VendorName/OnOff(false) read, new CASE + successful
BILRESA VendorName/ProductName read, and Ctrl+C session closure. Both nodes
remain commissioned. The controller is currently usable over Telnet; no USB
session remains open.

Temporary sanitized Telnet client: `/private/tmp/controller_telnet_run.py`;
log: `/private/tmp/controller-telnet-sanitized.log`.

Nordic details from the preceding successful commissioning: the user corrected
CMSIS-DAP to 192.168.1.106 (esp32c3-dap, MAC 3C-84-27-AD-F6-50). SWD confirmed
nRF52840, 256 KB RAM, 1024 KB flash. With explicit user permission, its original
ESP-BR-1058/channel-13 dataset was backed up locally and replaced via UART with
the C6-owned C6-Controller/channel-15 dataset. BLE advertising was stopped.
`matter esp controller pairing onnetwork 52840 <setup-pin>` completed PASE,
CASE and CommissioningComplete for `1A859A87FE21A493-CE68`. Safe reads identified
Nordic Semiconductor ASA, VendorID 65521, ProductID 32773, endpoint 1. On and Off
commands passed, with OnOff readbacks true and false; LED was left Off.
BILRESA remains node 2489, IKEA of Sweden / BILRESA dual button.

The older pairing/build sections below are historical; their pending/failure
status does not supersede these successful commissioning and Telnet checks.

## Local Pairing and Network Credentials

Included at the user's explicit request for local handover only:

- BILRESA manual setup code: **2538-372-4524**.
- Normalized setup code for the console: **25383724524**.
- Matter node ID to assign: **2489** (hex `0x9B9`).
- Wi-Fi SSID: **<SSID>**.
- Wi-Fi password: **unknown to this session**; saved on the board in NVS. The
  standard `wifi status` command does not reveal it. No NVS credential extraction
  was performed; preserve the saved configuration rather than inventing a password.
- Thread Network Key: `1293d90f1ea0aecef4f71adbb75528ac`.
- Thread PSKc: `17d0b14ab2c32ef25d63ba18f461d12a`.
- Thread Extended PAN ID: `cb28586b4519b6de`.
- Thread Mesh-Local Prefix: `fd42:e2c8:def2:fad1::/64`.

Active operational dataset, read from this C6 over USB on October 4, 2026 using
`matter esp ot_cli dataset active -x`. Wrapped here for readability; concatenate
the lines before supplying it to any command expecting one hex string:

```text
0e080000000000010000000300000f4a0300001a35060004001fffe00208cb28586b4519b6de0708
fd42e2c8def2fad105101293d90f1ea0aecef4f71adbb75528ac030d43362d436f6e74726f6c6c65
7201028149041017d0b14ab2c32ef25d63ba18f461d12a0c0402a0f7f8
```

This is the C6-created dataset, NOT a Home Assistant dataset. It is a snapshot:
if NVS is erased or the network recreated, read the new active dataset. Normally
use `matter esp thread pair`, which retrieves the dataset internally; do not
replace or recommit the live dataset just to reproduce this snapshot.

Exact pairing command after ensuring BILRESA is in commissioning mode:

```text
matter esp thread pair 2489 25383724524
```

## Hardware and Verified Network

- Super Mini ESP32-C6; ESP32-C6FH4, revision 0.2, 4 MB embedded flash, no PSRAM.
- USB Serial/JTAG: `/dev/cu.usbmodem101`, 115200 baud.
- STA MAC: `58:e6:c5:dd:10:58`.
- Saved Wi-Fi SSID: `<SSID>`; previous DHCP address: `192.168.1.194`.
- Own Thread network: **C6-Controller**, role **leader**, channel **15**, PAN **0x8149**.
- Dataset was generated locally and persisted in NVS. Name/PAN survived app reflash
  and restart. No Home Assistant dataset was fetched or used.
- Wi-Fi is intermittent: repeated reason 201 (`NO_AP_FOUND`), one passive scan saw
  zero APs, but later pairing logs showed Wi-Fi becoming operational. Do not claim
  current DHCP, Telnet or backbone routing works without checking.
- About 74 KB free RAM was observed before increasing the CHIP task stack. Current
  available RAM with the 24 KB stack has not been measured.

## Latest Pairing Evidence

1. Earlier BLE discovery timed out without a matching device.
2. User reenabled discovery. BLE connection and PASE succeeded, but attestation
   failed with `CHIP:0x20`, err 101: PAA not found. C6 used the test-only trust store.
3. Enabled the existing SPIFFS verifier and packaged pinned SDK DER roots, including
   IKEA G1. Flashed app and PAA partition, preserving NVS. Actual attestation passed.
4. With 12 KB CHIP task stack, commissioning panicked with `Stack protection fault`
   at `SendOpCertSigningRequest`. Complete panic/backtrace was not retained.
5. Increased CHIP task stack to **24576** bytes, rebuilt and flashed app only.
   Actual CSR request and `ValidateCSR` then passed without panic.
6. That attempt logged a discovery timeout near 30 seconds, followed by BLE discovery
   near 50 seconds. Commissioning subsequently used node ID **0**, not 2489, and
   failed at `GenerateNOCChain` with **CHIP:0x5C / CHIP_ERROR_WRONG_NODE_ID**.
7. Added a C6-only **90-second** setup-code discovery timeout to the project header.
   This is intended to outlast the 60-second BLE scan. It is a hypothesis-driven
   fix: runtime success is NOT yet verified, and it is NOT yet flashed.

### Latest Build Result

The 90-second timeout rebuild was launched synchronously but the terminal tool moved
it to the background. Terminal execution ID:
`e67e0d9b-3b87-41e3-9fd9-c76419890ee7`.

The completion notification subsequently confirmed successful linking, binary
generation and partition-size validation: **0x2c6f00 / 2,912,000 bytes**, 7% free.
No build remains active. The image with the 90-second timeout is ready to flash;
it has NOT been flashed or runtime-tested. Its size matches the previous image,
so size alone cannot distinguish the two. The terminal execution has been cleaned up.

Last verified/flashed app: **0x2c6f00 / 2,912,000 bytes**, 7% app partition free,
SPIFFS PAA enabled, CHIP stack 24 KB, but OLD discovery timeout. PAA image is
128 KiB; both app and PAA flash hashes were verified.

## Build and Flash

ESP-IDF **v6.0.2**; pinned Espressif CHIP commit
`539342f32d5f4dc93761c2f9325afe29270068f1`.
Podman executable `/opt/podman/bin/podman`; ARM64 VM, 4 CPUs, about 5.7 GiB RAM.
Image: `docker.io/espressif/esp-matter:latest_idf_v6.0.2`.

Run from the workspace root. Stable incremental build:

```sh
setopt pipefail
/opt/podman/bin/podman run --rm -v "$PWD:/project" \
  -w /project/examples/controller --entrypoint /bin/bash \
  docker.io/espressif/esp-matter:latest_idf_v6.0.2 \
  -c '. /opt/espressif/esp-idf/export.sh >/dev/null &&
      . /opt/espressif/esp-matter/export.sh >/dev/null &&
      export ESP_MATTER_PATH=/project &&
      idf.py -B build-podman/esp32c6-thread \
        -D SDKCONFIG=/project/examples/controller/build-podman/esp32c6-thread/sdkconfig-nimble build'
```

Use **sdkconfig-nimble**, not the stale initial `sdkconfig` or old Wi-Fi build.
Fresh builds use `sdkconfig.defaults.esp32c6` and target `esp32c6`.
Old Wi-Fi-only binaries remain under `examples/controller/build-podman/esp32c6/`.

Partition offsets: NVS `0x10000` (0xC000 bytes), app `0x20000` (3 MB),
PAA SPIFFS `0x320000` (128 KiB). Single app slot, no dual-slot OTA.

For the pending timeout-only update, flash app only; the PAA image is already installed:

```sh
uv tool run --from esptool==5.4.0 esptool \
  --chip esp32c6 --port /dev/cu.usbmodem101 --baud 460800 \
  --before default-reset --after no-reset \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 4MB \
  0x20000 examples/controller/build-podman/esp32c6-thread/controller.bin
uv tool run --from esptool==5.4.0 esptool \
  --chip esp32c6 --port /dev/cu.usbmodem101 --baud 115200 \
  --before no-reset --after no-reset \
  load-ram examples/controller/build-podman/esp32c6-thread/bootloader/bootloader.bin
```

RAM bootloader execution is a proven workaround for C6 staying in download mode
after esptool reset. `watchdog-reset` and `soft-reset` are not supported for this C6.
If ROM cannot connect, manual recovery may require BOOT held while pressing/releasing
RESET, then releasing BOOT. Never erase NVS as a recovery shortcut.
To install PAA on another board, add the generated `paa_cert.bin` at `0x320000`;
normal project flash includes it automatically. Do not flash app at address zero.

## USB Access and Pairing Check

Use `uv run --no-project --with pyserial python -c '...'` and:

```python
serial.Serial("/dev/cu.usbmodem101", 115200, timeout=0.2,
              write_timeout=2, rtscts=True, dsrdtr=True)
```

Do not explicitly toggle DTR/RTS. Send blank `\r`, read for 2-10 seconds using a
monotonic deadline, and answer cursor query `ESC[6n` with `ESC[1;80R` before sending
commands. The first unsynchronized command can be lost. Commands end in `\r`.
No sleep/poll loops; use bounded serial reads. Close other serial clients first.

```text
matter esp thread info
matter esp thread pair 2489 <setup-code>
matter esp wifi status
matter esp diagnostics mem-dump
```

Wait for actual `Commissioning success with node ...-9B9`, not command acceptance
or PASE success. `pairing_code_thread()` currently logs immediate PairDevice errors
but returns ESP_OK, so acceptance alone is especially unreliable.
Retain sanitized commissioning logs and, on panic, keep reading in the SAME serial
session for several seconds to capture task name/register addresses/backtrace.
Do not output raw stack dumps, setup PIN, dataset or symmetric keys.

After success, verify a safe attribute read from node 2489, endpoint 0:
BasicInformation cluster `0x28`, product/vendor information; Descriptor cluster
`0x1D`, PartsList attribute `3`. Inspect controller help for exact read syntax.
Do not write arbitrary attributes or reset the device.

## Relevant Changes and Ownership

- `examples/controller/main/app_main.cpp`: creates/persists own Thread dataset;
  adds `thread info` and `thread pair`; uses internal dataset with CHIP/OT locking.
  Border-router startup checks its return value before marking initialization complete.
- `examples/controller/sdkconfig.defaults.esp32c6`: native Thread FTD/BR, NimBLE
  Central, Wi-Fi coexistence, USB/Telnet, SPIFFS PAA, CHIP task stack 24 KB.
- `examples/controller/main/matter_project_config.h`: pending C6 discovery timeout 90 s.
- `examples/controller/main/CMakeLists.txt`: C6 SPIFFS image contains only DER roots
  from pinned SDK `credentials/development/paa-root-certs`; filenames use 24 hex
  SHA256 characters plus `.der` to fit SPIFFS. All **114** packaged roots were
  byte-compared to SDK, IKEA G1 presence and 128 KiB image size verified.
  This SDK snapshot includes test/development roots too; not a production-only store.
- `components/esp_matter_controller/core/esp_matter_controller_console.cpp`:
  ESP-console controller dispatch holds ScopedChipStackLock, fixing the previous abort.
- Console files: ESP-IDF REPL plus native CHIP Shell; custom streamer mirrors USB/Telnet;
  shared command mutex; complete long logs; corrected CRLF across chunks; diagnostics
  log through ESP_LOG. Native `matter exit` blocked because upstream calls exit(0).
- Telnet: TCP23, single client, ECHO/SGA, CRLF/CRNUL/LF, 511-byte command limit,
  RESULT framing, prefix stripping, ROUTE_HOOK suppressed on network only,
  Ctrl+C/Ctrl+D/IAC Interrupt Process close session. Older Wi-Fi-only image was
  hardware-tested; do not transfer that validation automatically to current firmware.

### Dirty CHIP Submodule: Do Not Lose

`connectedhomeip/connectedhomeip` has local edits:

- `src/platform/ESP32/ThreadStackManagerImpl.cpp`: removed outer OT lock around
  generic DoInit; generic ConfigureThreadStack already locks the nonrecursive mutex.
  This fixed a real startup deadlock; build and actual Thread Leader verified.
- `src/platform/ESP32/OpenthreadLauncher.cpp`: eventfd limit 3 -> 4 to allow
  OTBR discovery delegate. Build verified; successful backbone routing not verified.
- Nested `third_party/ot-commissioner/repo` is also dirty. Its changes were not
  inspected or attributed in this handover; preserve them.

The two ESP32 fixes are preserved in
`examples/controller/patches/esp32-native-thread.patch`. Reverse applicability was
checked against the working SDK. On a CLEAN pinned SDK only, apply from root:

```sh
git -C connectedhomeip/connectedhomeip apply \
  "$PWD/examples/controller/patches/esp32-native-thread.patch"
```

A parent commit does not preserve dirty submodule contents. The patch does not
capture nested ot-commissioner changes. Do not apply it twice.

## Remaining Work and Known Caveats

BILRESA and Nordic commissioning, automatic Wi-Fi startup and Telnet device
reads are complete. Remaining checks and limitations:

1. Verify external computer-to-Thread routing through the border router.
2. Investigate stale addresses/routing after separate Wi-Fi and Thread restarts.
3. Improve visibility of raw OpenThread CLI output over Telnet.
4. Remote GitHub workflow execution remains unverified; the workflow targets S3.

Other known issue: native `matter dns browse` can return incorrect state on a second
browse after stop because upstream ResolverProxy lifecycle is not reset. No durable
fix was made. Do not use repeated browse failures as proof a device is absent.
The earlier transient Wi-Fi recovery was unexplained; the later startup repair
explicitly enabled Wi-Fi/802.15.4 coexistence, as recorded above. Remote GitHub workflow execution remains unverified; workflow
currently targets S3, not C6.

Repository memory: `/memories/repo/build.md` contains earlier verified history but
does not yet include this session's PAA, CSR/stack and pending timeout work.

## Archived Technical Reference

The following material preserves the former C6 controller document, including
build instructions and diagnostic history. Its initial status and earlier
verification sections describe earlier firmware: pending commissioning,
NO_AP_FOUND and old binary sizes do not describe the current installation.
Use the latest verified sections above and the user guide for current operation.


This project is based on the [Espressif ESP-Matter](https://github.com/espressif/esp-matter)
SDK and extends its [controller example](examples/controller/README.md).
The ESP32 board acts as a Matter controller: it adds devices to its fabric,
sends commands, and reads and writes attributes.

Enter commands in `esp_console` over USB/UART or Telnet over Wi-Fi at the
`matter>` prompt. Matter commands themselves reach devices over the IP network.
Sharing a Wi-Fi network is not enough: the device must join the controller's
fabric. Telnet is intended only for testing on a trusted local network;
there is no TLS or authentication.

### Standalone Thread Status

The current C6 defaults now enable native-radio OpenThread FTD/Border Router
and NimBLE commissioning, alongside Wi-Fi and Telnet. This profile creates
its own `C6-Controller` Thread network on first boot and persists its
random operational dataset in NVS. It does not fetch or use a Home Assistant
OTBR dataset. The older verified Wi-Fi-only build described below remains in
`examples/controller/build-podman/esp32c6/`.

The new commands are:

```text
matter esp thread info
matter esp thread pair 2489 <manual-setup-code-or-MT-payload>
```

The pairing handler reads the controller's own dataset internally, without
printing its keys or requiring them in the command line. Controller commands
now acquire the CHIP stack lock, fixing the abort observed when pairing was
invoked through Telnet.

As of October 4, 2026, the standalone profile builds successfully in
`examples/controller/build-podman/esp32c6-thread/`, using generated
`sdkconfig-nimble`. The flashed application is 2,882,784 bytes and fits the
3 MB application partition with 8% free. USB confirms `C6-Controller` is a
Thread Leader on channel 15, PAN ID `0x8149`; the same network survives an
application reflash and restart without erasing NVS. USB diagnostics reported
about 74 KB free RAM before the latest reflash.

Startup originally deadlocked because ESP32 Thread initialization acquired the
nonrecursive OpenThread lock before calling generic initialization, which
acquires that lock itself. The SDK fix removes the outer lock. A second SDK
fix increases the eventfd limit from 3 to 4 for OTBR discovery; the application
now checks the border-router initialization result before marking it ready.
Both fixes are preserved in
[the SDK patch](examples/controller/patches/esp32-native-thread.patch).
The submodule remains locally modified: a parent repository commit alone does
not preserve its dirty contents. Apply the patch once to a clean pinned SDK
from the repository root before building:

```sh
git -C connectedhomeip/connectedhomeip apply \
  "$PWD/examples/controller/patches/esp32-native-thread.patch"
```

BILRESA commissioning has not completed: BLE discovery ran without a panic,
but timed out without finding a matching device. Reopen the device's
commissioning window before retrying `matter esp thread pair`. Current Wi-Fi
reports reason 201 (`NO_AP_FOUND`), and a passive scan found 0 APs. Telnet and
backbone border-router routing are therefore not currently verified; the
eventfd fix is build-verified but still needs a successful Wi-Fi IP event.

### Changes from Upstream

| Change | Purpose |
| --- | --- |
| Optional `esp_console` backend | ESP-IDF REPL with native CHIP Shell commands under `matter ...`, preserving existing `matter esp ...` commands. |
| `CONFIG_ESP_MATTER_CONSOLE_USE_ESP_CONSOLE` | Selects the new backend; other examples still default to the original CHIP Shell. |
| UART, USB CDC and USB Serial/JTAG | ESP-IDF configuration selects the REPL transport. |
| Telnet for agents | TCP port `23`, CHIP and `matter esp ...` commands, logs and execution status; no TLS or authentication. |
| Clean Telnet output | ESP-IDF level, timestamp and tag prefixes are removed for all modules; background `ROUTE_HOOK` messages are excluded. USB logs are preserved. |
| Complete command output | Long ESP Matter help messages are no longer truncated at 511 bytes; native CHIP Shell output reaches both USB and Telnet. CRLF is preserved across log chunks. |
| Network diagnostics | `up-time` and `mem-dump` use the shared logging path and return data over both USB and Telnet. |
| Controller startup order | The console starts after Matter and commissioner initialization; command registration results are checked. |
| Controller configuration | Enables the new REPL and Wi-Fi station, reusing existing Wi-Fi and Matter handlers. |
| Wi-Fi in the new REPL | Connects through Matter's standard driver without reinitializing Wi-Fi; adds `wifi status` and `wifi scan`. |
| Controller diagnostic logs | Reports the Wi-Fi disconnect reason, assigned IPv4 address and gateway. |
| Separate ESP32-C6 profile | 4 MB flash, internal RAM, USB Serial/JTAG, native Thread network and NimBLE commissioning. |
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

### Platforms and Limitations

| Parameter | Tested ESP32-C6 | Main ESP32-S3 Profile |
| --- | --- | --- |
| Flash | 4 MB | 8 MB |
| PSRAM | Not required | Required; octal by default, quad configurable |
| Console | USB Serial/JTAG and Telnet | UART/USB and Telnet |
| Commissioning | Own-network BLE/Thread enabled; device pairing pending | `onnetwork`; also BLE when Bluetooth is enabled |
| GitHub workflow | Not supported yet | Supported |

The C6 profile has a single 3 MB application slot. Dual-slot OTA is not
provided. Do not flash an S3 image onto a C6 or use S3 PSRAM settings for C6.

A Wi-Fi Matter device needs a shared, reachable IP network. The current C6
profile creates its own Thread network and initializes its border router when
Wi-Fi acquires an IP address. The older Wi-Fi-only image requires a separate
border router for Matter-over-Thread devices.

Recommended ESP-IDF version: `v6.0.2`. The Matter submodule is pinned to
`539342f32d5f4dc93761c2f9325afe29270068f1`; do not replace it with an arbitrary HEAD.

### First Run on ESP32-C6

The board used here is a Super Mini ESP32-C6 with an ESP32-C6FH4 chip and
4 MB flash. Its owner reported that it previously connected to Wi-Fi with
OTBR firmware. Antenna GPIO settings for other boards, such as XIAO, do not
automatically apply to this board.

You need a C6 board with 4 MB flash, a data-capable USB cable, a Wi-Fi network,
and a Matter device with a known setup PIN. The examples use the macOS port
`/dev/cu.usbmodem101`; replace it with your port on another machine.

#### 1. Flash an Existing Local Build

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

#### 2. Open the Console

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
matter help
matter esp help
matter esp wifi
matter esp controller help
```

`matter esp help` lists `wifi`, `controller`, `diagnostics`, `factoryreset`,
`help`, and `terminal` when Telnet is enabled. `matter help` lists native CHIP
commands and the `esp` command group. USB `help` lists ESP-IDF REPL commands;
Telnet `help` is a shortcut for `matter help`.

Native CHIP Shell commands work over both transports:

```text
matter version
matter dns help
matter dns browse help
matter dns browse commissioner
matter dns browse stop
matter device help
matter config help
matter config vendorid
matter config productid
matter stat peak
```

Browsing starts an asynchronous mDNS query; `RESULT ESP_OK` does not mean a
device was found. Stop browsing when finished. Avoid `matter config` without
arguments, `matter config pincode`, and `matter onboardingcodes` in shared
logs: they print commissioning credentials. Factory-reset and configuration
write commands change device state; use them only intentionally.

#### 3. Connect the Controller to Wi-Fi

Replace the SSID and password with your values. Quote arguments containing
spaces. Do not publish commands containing real passwords in logs or issues.

```text
matter esp wifi connect "<SSID>" "<PASSWORD>"
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

#### 4. Connect over Telnet

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
`matter exit` is blocked with `ESP_ERR_NOT_SUPPORTED`: the upstream command
exits the whole application rather than just the Telnet session.
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

#### 5. Add a Matter Device

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

#### 6. Control the Device

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

### Local Rebuild

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

### Remote Builds with GitHub Actions

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

### Diagnostics and Reset

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
network buffer, so new ESP Matter responses should use the shared logging path.
Native CHIP Shell responses use a streamer mirrored to USB and Telnet.
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

### Verification Status

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
been confirmed. As of October 4, 2026, the current Telnet build with native
CHIP commands and fixed diagnostics on ESP-IDF `v6.0.2` is flashed on C6;
the application size is `0x1e25e0` (1,975,776 bytes).
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
Native `matter help`, `version`, `dns help`, `device help`, `config help`,
vendor/product ID reads, `stat peak`, and commissioner mDNS browse/stop were
checked on hardware. CHIP version output sent through USB was also received
by an active Telnet client, which remained responsive afterward. `matter exit`
was rejected without resetting the board. The full controller help, including
the `invoke-cmd` documentation URL, reached Telnet without truncation.
Host checks covered CRLF split across 512-byte log chunks and separate flushes,
LF conversion, and Telnet IAC escaping.
The network console has no TLS, certificates or password. The USB console
and DHCP continue to work. `factoryreset` was not executed during output
checks, to preserve Wi-Fi settings and the fabric.
Commissioning and control of a specific Matter device have not been tested.
This is a modified SDK example, not a claimed certified production controller.

#### October 4, 2026: native Thread coexistence repair

The native Thread controller now explicitly calls `esp_coex_wifi_i154_enable()`
after starting its saved network, matching the ESP-IDF border-router example.
The previous running image lost Wi-Fi and repeatedly returned NO_AP_FOUND
(reason 201). The app-only update preserves NVS and PAA storage and restores
automatic Wi-Fi connection without starting BLE advertising. The new app is
2,912,704 bytes (0x2c71c0), with 7% app space free.

Mac-to-C6 Telnet at 192.168.1.194:23 passed version/address checks and Ctrl+C
session closure. Subsequent control uses Telnet. BILRESA node 2489 and Nordic
nRF52840 LED node 52840 remain in the controller's existing Thread fabric.
Nordic CASE and attribute read succeeded after the controller update.

A Telnet-issued Wi-Fi driver restart dropped the session as expected; the
controller reacquired the same IP and accepted a fresh Telnet connection.
BILRESA attribute reads passed through the retained CASE session afterward.

Final full-restart verification passed again over Telnet: automatic Wi-Fi
connection and new CASE/attribute reads for both nodes 2489 and 52840.
A separate Wi-Fi driver restart followed by Thread stop/start exposed stale
Matter addresses/routing errors; a full controller restart restored both nodes.
That warm-restart routing issue remains unresolved. Leave the running network
intact for normal Telnet operation.
