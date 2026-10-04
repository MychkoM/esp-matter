# ESP32 C6 Matter Controller User Guide

This guide explains how to operate the current ESP32-C6 controller through
Telnet: connect Wi-Fi, inspect its own Thread network, register devices through
BLE or IP, and control registered devices. Commands are based on this
repository's console implementation and hardware checks on October 4, 2026.

The normal operating interface is Telnet. USB is reserved for initial setup or
explicitly authorized firmware recovery. This guide contains placeholders for
credentials; replace them before entering a command. Angle brackets are not
part of the values.

## Current installation

| Item | Current value |
| --- | --- |
| Controller | ESP32-C6, 4 MB flash, native Thread radio |
| Controller Telnet | `192.168.1.194:23` |
| Controller Wi-Fi MAC | `58:E6:C5:DD:10:58` |
| Thread network | `C6-Controller`, channel 15, PAN ID `0x8149` |
| IKEA BILRESA | Matter node `2489`, product BILRESA dual button |
| Nordic Pro Micro | nRF52840, Matter node `52840`, LED endpoint `1` |
| Nordic network console bridge | ESP32-C3 at `192.168.1.106:1234`, UART 115200 |

These IP addresses are DHCP assignments, not permanent addresses. The bridge
is a separate device; connecting to it opens the Nordic console, not the C6
controller console. BILRESA and Nordic are already registered. Use an unused
node ID for a new device; the pairing examples below use `53000`.

The C6 owns the Thread dataset and Matter fabric. No Home Assistant OTBR
credentials or fabric are used. A Thread Leader is elected by the network:
the C6 may report `router` while Nordic reports `leader`. This does not change
who originally created the network or owns its Matter fabric.

## Console transport and device transport

| Connection | Purpose |
| --- | --- |
| Computer to C6 over Wi-Fi and Telnet | Enter commands and receive logs |
| C6 to an uncommissioned device over BLE | Establish PASE and provision Wi-Fi or Thread credentials |
| C6 to a commissionable device already reachable over IP | Establish PASE without BLE |
| C6 to a registered device over Thread or Wi-Fi | Establish CASE and read, write or invoke Matter operations |

PASE is the initial secure session established with a setup PIN. CASE is the
operational secure session using the device's Matter fabric credentials.
BLE commissioning does not make BLE the permanent control transport: after
commissioning a Thread device, normal Matter control uses Thread.

Joining the same Thread or Wi-Fi network does not register a device in the
controller's Matter fabric. Conversely, deleting a device's last fabric can
also erase its network credentials, depending on its firmware.

## Connect and inspect the controller

Run this on the computer:

```sh
telnet 192.168.1.194 23
```

At the `matter>` prompt, enter:

```text
matter version
matter help
matter esp controller help
matter esp wifi status
matter esp terminal info
matter esp thread info
matter esp diagnostics up-time
matter esp diagnostics mem-dump
```

Expected working state: a nonzero Wi-Fi IPv4 address, the expected Thread name
and channel, and Thread role `child`, `router` or `leader`. `disabled` and
`detached` mean Thread is not ready for normal device communication.

Telnet serves one client at a time. Commands have a 511-byte input limit in
the current C6 profile. Ctrl+C or Ctrl+D closes the session; Ctrl+], then
`quit`, exits the Telnet client. Closing Telnet does not cancel an asynchronous
Matter operation. An input-idle session closes after 15 minutes.

This Telnet server has no authentication or encryption. It is configured for
an isolated test LAN. Commands and console echo may expose credentials, so
keep credential-bearing output out of shared logs.

### Recognize command completion

`RESULT ESP_OK` means the command handler accepted the request. Pairing,
reads and commands can complete after the prompt returns.

For pairing, wait for all of the following:

```text
PASE session establishment success
Received CommissioningComplete response, errorCode=0
Commissioning success with node <fabric>-<node>
```

PASE success alone is insufficient: attestation, network setup or operational
CASE can still fail. For reads, check the attribute values and `read done`.
For cluster commands, check response `Status=0x0` and `Send command success`.
Start one commissioning operation at a time.

## Wi-Fi operations

### Inspect or configure Wi-Fi

On the C6 console:

```text
matter esp wifi status
matter esp terminal info
matter esp wifi connect "<SSID>" "<PASSWORD>"
```

C6 uses 2.4 GHz Wi-Fi and obtains its IPv4 address through DHCP. `wifi status`
shows the configured SSID, country/channel range, DHCP state, address and gateway.
The `connect` command configures the C6's Wi-Fi; it does not configure a remote
Matter device. Changing networks can drop Telnet, so determine the new DHCP
address in the router and reconnect. Before the first Wi-Fi connection, this
command needs an already available local console.

### Scan or restart Wi-Fi

```text
matter esp wifi scan
matter esp wifi restart
```

These are disruptive diagnostic commands. Scan disconnects the station and
then restores automatic reconnect. Restart stops and starts the driver with
its existing configuration. Both can interrupt Telnet and active operations.

A separate Wi-Fi restart followed by Thread stop/start exposed stale Matter
addresses and routing errors in this installation. Telnet recovered, but some
device requests failed. A full C6 restart restored both devices. This limitation
remains unresolved; leave the running network intact during normal operation.

### Wi-Fi and Thread coexistence

The current application calls `esp_coex_wifi_i154_enable()` after starting
native Thread. Before this repair, C6 repeatedly reported Wi-Fi reason 201
(`NO_AP_FOUND`) and IPv4 `0.0.0.0`. With the repair, automatic Wi-Fi connection
was verified after two controller starts, with BLE advertising disabled.
Starting BLE advertising is not the normal Wi-Fi recovery procedure.

## Thread operations

### Inspect the existing network

The reliable Telnet summary is:

```text
matter esp thread info
```

Additional OpenThread commands are passed through `ot_cli`:

```text
matter esp ot_cli state
matter esp ot_cli networkname
matter esp ot_cli channel
matter esp ot_cli panid
matter esp ot_cli ipaddr
matter esp ot_cli parent
matter esp ot_cli child table
matter esp ot_cli router table
matter esp ot_cli br state
```

`parent` applies to a child; `child table` lists children of this router, not
all devices in the network. Border routing should report `running` when its
backbone is operational. C6-to-device Matter reads prove Thread communication;
they do not, by themselves, prove that a computer can route into Thread.

**Output limitation:** the OpenThread CLI callback writes directly to `vprintf`.
Some CLI responses appear on USB but are absent from Telnet, which may show
only `RESULT ESP_OK`. Do not treat missing output as a negative result. Use
`matter esp thread info` for the Telnet summary; a local console may be needed
to inspect raw OpenThread output. The CLI command can still execute even when
its response is not visible in Telnet.

### Read the dataset

```text
matter esp ot_cli dataset active
matter esp ot_cli dataset active -x
```

The second command returns the active operational dataset as hexadecimal TLVs.
The first can also show keys. Both outputs contain network credentials. Read
from an interface that actually displays the OpenThread response and keep the
result private. Remove display line breaks when using the hexadecimal dataset
as one argument. A local saved snapshot is usable only while the C6's saved
network remains unchanged.

The application starts its saved network automatically and creates
`C6-Controller` only when no commissioned dataset exists. Normal operation does
not require `dataset init new`, a dataset commit, or erasing NVS. Creating a new
dataset changes the network and can strand existing devices.

### Start or stop an existing Thread network

```text
matter esp ot_cli thread stop
matter esp ot_cli ifconfig down
matter esp ot_cli ifconfig up
matter esp ot_cli thread start
```

These commands interrupt Thread service and are for deliberate maintenance,
not routine health checks. They do not intentionally replace the dataset.
See the warm-restart routing limitation above before using them.

## Register a Thread device through BLE

Prepare the device with an open commissioning window and BLE advertising.
Obtain its actual setup PIN and discriminator from its label, onboarding data
or device console. The PIN is not the complete printed manual pairing code.
The current Nordic example uses discriminator `3840`; do not assume this for
other devices.

### Force BLE commissioning

On C6:

```text
matter esp controller pairing ble-thread 53000 <DATASET_HEX> <SETUP_PIN> <DISCRIMINATOR>
```

Use the C6's own current dataset. This path explicitly selects a BLE peer,
performs PASE over BLE and supplies the dataset through Matter Network
Commissioning. After the device joins Thread, the controller establishes CASE
and sends CommissioningComplete over IP.

Expected evidence includes:

```text
BLE GAP connection established
GATT discovery complete status:0
... [BLE] ... PBKDFParamRequest
PASE session establishment success
Successfully finished commissioning step 'ThreadNetworkSetup'
Received CommissioningComplete response, errorCode=0
Commissioning success with node ...
```

This exact path was hardware-verified with Nordic node `52840`. Its BLE
connection closed during successful cleanup. Verify the newly registered
device with a Matter read before considering the operation complete.

### Use the internal dataset helper

```text
matter esp thread pair 53000 <SETUP_CODE_OR_QR_PAYLOAD>
```

This helper reads the C6 dataset internally, so no dataset argument is echoed.
It accepts a supported manual setup code or QR payload such as `MT:...`.
It uses `DiscoveryType::kAll`, so BLE and IP discovery are allowed. Choose
`pairing ble-thread` when BLE must be forced, or `pairing onnetwork` when BLE
must be excluded. This helper was verified with BILRESA.

## Register a Wi-Fi device through BLE

On C6:

```text
matter esp controller pairing ble-wifi 53000 "<SSID>" "<PASSWORD>" <SETUP_PIN> <DISCRIMINATOR>
```

This provisions the remote device's Wi-Fi credentials over BLE. The target
must have an open commissioning window and subsequently be reachable over IP
from C6. Wait for CommissioningComplete and then verify a read.

This command is present in the current implementation. The recent hardware
checks covered BLE-to-Thread commissioning, not BLE-to-Wi-Fi commissioning.

## Register a device without BLE

On C6:

```text
matter esp controller pairing onnetwork 53000 <SETUP_PIN>
```

This discovers commissionable devices over IP and establishes PASE without
BLE. The target must already be reachable over IP and have an open Matter
commissioning window. For Thread in this installation, first provision the
target with the C6 dataset and let it attach. The command itself does not
bootstrap an offline Thread device into the network.

Discovery currently uses no device filter. Open the intended target's window
and avoid other simultaneously commissionable devices with matching setup
credentials. Confirm the peer address and identity in the subsequent read.

### Prepare Nordic through its network console

The ESP32-C3 bridge gives network access to the Nordic UART console:

```sh
telnet 192.168.1.106 1234
```

The bridge uses UART at 115200 baud in this installation. Its first packet can
select a baud rate; the tested automated client sends `115200` before console
commands. The prompt is `uart:~$` and these are **Nordic commands**, not C6
commands:

```text
matter ble adv stop
ot state
ot dataset active -x
```

If a full reset left no network, the dataset query returns `Error 23: NotFound`.
For an intentionally authorized network replacement, populate the working
dataset using short commands, then commit it:

```text
ot thread stop
ot ifconfig down
ot dataset clear
ot dataset activetimestamp <ACTIVE_TIMESTAMP>
ot dataset channel <CHANNEL>
ot dataset channelmask <CHANNEL_MASK>
ot dataset extpanid <EXTENDED_PAN_ID_HEX>
ot dataset meshlocalprefix <MESH_LOCAL_PREFIX_IPV6>
ot dataset networkkey <NETWORK_KEY_HEX>
ot dataset networkname <NETWORK_NAME>
ot dataset panid <PAN_ID>
ot dataset pskc <PSKC_HEX>
ot dataset securitypolicy <ROTATION_HOURS> <POLICY_FLAGS> <VERSION_THRESHOLD>
ot dataset commit active
ot ifconfig up
ot thread start
ot state
ot ipaddr
matter ble adv state
```

Use values from the actual C6 dataset. For the verified installation, channel
is 15, PAN ID is `0x8149`, name is `C6-Controller`, and the security-policy
command was `ot dataset securitypolicy 672 onrc 0`. Other fields remain private.
Wait until `child`, `router` or `leader`, confirm an address, and then run
`pairing onnetwork` on C6. Nordic must show BLE advertising disabled if BLE
commissioning is excluded.

A long `ot dataset set active <DATASET_HEX>` command was rejected by this
Nordic console; short field commands worked. The device-side input limit is
separate from C6's 511-byte Telnet limit. Generic devices without a usable
console or another bootstrap mechanism cannot be provisioned this way.

## BLE advertising commands

These commands affect the device whose console you are connected to:

```text
matter ble adv state
matter ble adv start
matter ble adv stop
```

On C6, they control C6's own advertising. They do not enable Nordic advertising
and are not the switch for C6's central scanner: the forced BLE pairing command
starts target discovery itself. On the Nordic bridge, the same commands affect
Nordic advertising.

Advertising alone is not proof of an open Matter commissioning window.
For a device already in a fabric, open a window through an authorized existing
controller or the device's documented local procedure. Stock Nordic firmware
starts BLE advertising after factory reset; in the no-BLE test it was stopped
before provisioning and pairing.

## Read and control registered devices

Read one attribute with:

```text
matter esp controller read-attr <NODE_ID> <ENDPOINT_ID> <CLUSTER_ID> <ATTRIBUTE_ID>
```

Numeric IDs can be decimal or hexadecimal with a lowercase `0x` prefix.
The following examples use the
registered Nordic LED device:

```text
matter esp controller read-attr 52840 0,0,0,0,0 40,40,40,40,29 1,2,3,4,3
matter esp controller read-attr 52840 0 62 3
matter esp controller read-attr 52840 1 6 0
matter esp controller invoke-cmd 52840 1 6 1
matter esp controller invoke-cmd 52840 1 6 0
matter esp controller invoke-cmd 52840 1 6 2
```

The reads request Basic Information and root PartsList, CommissionedFabrics,
and OnOff. The invokes turn the LED on, off and toggle it. Nordic On/Off
commands and true/false readbacks were hardware-verified; Toggle is available
from the cluster command interface but was not part of those checks.

| Endpoint | Cluster | Attribute | Meaning |
| --- | --- | --- | --- |
| 0 | 40 (`0x28`) | 1 / 2 / 3 / 4 | VendorName / VendorID / ProductName / ProductID |
| 0 | 29 (`0x1D`) | 3 | PartsList, used to find device endpoints |
| 0 | 62 (`0x3E`) | 3 | CommissionedFabrics |
| 1 on Nordic | 6 (`0x06`) | 0 | OnOff |

Comma-separated endpoint, cluster and attribute lists must have the same
length. They form corresponding paths, not a Cartesian product. Inspect the
actual device's endpoints before using endpoint `1` for another product.

For BILRESA, a safe identity read is:

```text
matter esp controller read-attr 2489 0,0 40,40 1,3
```

Battery devices can require longer response windows than an always-on LED
example. Allow the request to complete before repeating it.

### Subscribe or write

Available syntax, not covered by the recent hardware verification:

```text
matter esp controller subs-attr <NODE_ID> <ENDPOINT_IDS> <CLUSTER_IDS> <ATTRIBUTE_IDS> <MIN_SECONDS> <MAX_SECONDS> [keep-subscription] [auto-resubscribe]
matter esp controller shutdown-subs <NODE_ID> <SUBSCRIPTION_ID>
matter esp controller shutdown-subss <NODE_ID>
matter esp controller write-attr <NODE_ID> <ENDPOINT_IDS> <CLUSTER_IDS> <ATTRIBUTE_IDS> <ATTRIBUTE_VALUE_JSON> [TIMED_WRITE_TIMEOUT_MS]
matter esp controller invoke-cmd <NODE_ID> <ENDPOINT_ID> <CLUSTER_ID> <COMMAND_ID> [COMMAND_DATA_JSON] [TIMED_INVOKE_TIMEOUT_MS]
```

For JSON-bearing operations, use the format required by the particular cluster
and attribute. Check `matter esp controller help`; do not assume every attribute
is writable. Telnet session closure does not automatically terminate a device
subscription.

## Unpair and register again

On C6:

```text
matter esp controller pairing unpair <NODE_ID>
```

Wait for `Remove Current Fabric succeeded` and
`Succeeded to remove fabric for remote node ...`. This removes C6's fabric
from the target; it is different from erasing the controller's NVS.

In the tested Nordic firmware, removing its last fabric automatically reset
the device and cleared Thread credentials. Before unpairing, choose how the
device will receive network credentials again: BLE commissioning or its
independent network console. Keep BILRESA's existing registration intact
unless its removal is explicitly intended.

For an explicit Nordic factory reset, use its bridge console:

```text
matter device factoryreset
```

This is destructive to the Nordic's settings. Confirm the target console
before entering it. The same command on C6 resets the controller instead.
After Nordic reset, `ot state` returned `disabled` and the active dataset query
returned `NotFound`; its stock firmware restarted BLE advertising.

Then register with an open window and the correct credentials using either
`pairing ble-thread` or the prepared-device `pairing onnetwork` flow. Node
`52840` was successfully reused after removal in both verified flows.

### Open a window for multi admin

For a registered device accessible through C6, the controller exposes:

```text
matter esp controller open-commissioning-window <NODE_ID> 1 300 1000 3840
```

This requests an enhanced 300-second window with PBKDF iteration count 1000
and discriminator 3840. Use the newly generated onboarding code printed by
the command when adding another controller. It is a different workflow from
unpairing, and preserves the existing fabric. This command's syntax was
checked in source; this multi-admin workflow was not hardware-tested here.

## Troubleshooting

| Symptom | Check or action |
| --- | --- |
| Telnet times out | Verify the current C6 DHCP address by MAC in the router. A missing ARP response is not evidence of a Telnet parser problem. |
| Wi-Fi IP is `0.0.0.0` | Check association logs and DHCP. Reason 201 was observed with missing Wi-Fi/Thread coexistence initialization; firmware repair restored startup. |
| Only `RESULT ESP_OK` appears | Await asynchronous logs. For raw `ot_cli` output, account for the USB-only output limitation. |
| BLE target is not found | Check the target's commissioning window, advertising and actual discriminator. C6's own advertising is a different setting. |
| PASE succeeds but pairing fails | Inspect the failing attestation, CSR/NOC, network setup or CASE stage. Do not assume successful PASE means registration completed. |
| CASE/read times out | Check current target reachability and address. Separate Wi-Fi/Thread restarts exposed stale addresses and routing failures; full C6 restart restored operation in that test. |
| No-BLE pairing fails after factory reset | The target may have no Thread dataset. Provision it through an independent supported interface before onnetwork pairing. |
| Attestation fails | This C6 build uses a SPIFFS PAA trust store containing the roots used by the verified devices; inspect the actual trust/validation error. |
| A second mDNS browse reports incorrect state | The upstream ResolverProxy lifecycle issue remains unresolved. Do not conclude that the target is absent from this result alone. |

Keep the saved C6 network and fabric intact during troubleshooting. App-only
firmware updates preserve them when NVS is not erased. There is no dual-slot
OTA partition in this 4 MB C6 profile; do not assume a firmware update can be
performed through Telnet. Build and recovery details are in
[controller technical reference](CONTROLLER_HANDOVER.md).

## Implementation references

- [Controller commands](components/esp_matter_controller/core/esp_matter_controller_console.cpp)
- [Pairing transports](components/esp_matter_controller/commands/esp_matter_controller_pairing_command.cpp)
- [Wi-Fi commands](components/esp_matter_console/esp_matter_console_wifi.cpp)
- [Telnet server](components/esp_matter_console/esp_matter_console_network.cpp)
- [Native Thread startup and helper](examples/controller/main/app_main.cpp)
- [OpenThread console output](connectedhomeip/connectedhomeip/src/platform/ESP32/OpenthreadLauncher.cpp)

The `CONTROLLER_HANDOVER.md` contains detailed hardware evidence, build history
and local Matter/Thread commissioning data. Wi-Fi credentials are omitted;
review other commissioning secrets before sharing that document.
