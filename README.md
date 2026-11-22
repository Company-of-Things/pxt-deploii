# Deploii for micro:bit

MakeCode extension that connects a micro:bit V2 to [Deploii](https://deploii.no) over Bluetooth Low Energy,
using the same protocol as the Deploii Arduino library on the Arduino UNO R4.

## Quick start

```sh
./scripts/setup.sh      # once: toolchain, Node 24, pxt, dependencies, first build
make deploy             # build and flash the test program to a plugged-in micro:bit
```

Then implement `deploii_hash()` in `deploii_hash.cpp` and put your board ID in `test.ts`.

| Command        | What it does |
|----------------|--------------|
| `make build`   | Compile `main.ts`, `test.ts` and the C++ into `built/binary.hex` with the local Arm toolchain |
| `make deploy`  | Build, then copy the hex to the `MICROBIT` drive |
| `make flash`   | Copy the last hex without rebuilding |
| `make monitor` | USB serial console at 115200 baud (exit with Ctrl-A Ctrl-X) |
| `make rebuild` | Force a full C++ rebuild |
| `make cloud`   | Compile with the MakeCode cloud compiler instead (no local toolchain needed) |
| `make shims`   | Regenerate `shims.d.ts` / `enums.d.ts` from the C++ `//%` comments |

The first build clones CODAL (codal-microbit-v2 v0.3.5, the version pxt-microbit 9.1 uses) into
`built/codal` and takes a few minutes. Later builds only recompile what changed.
In VS Code, `Ctrl+Shift+B` runs `make build`; C++ IntelliSense works after the first build.

## Files

| File | Purpose |
|------|---------|
| `pxt.json` | Extension manifest. Enables Bluetooth with "no pairing required", disables the micro:bit V1 build |
| `main.ts` | Blocks and TypeScript API (`connect`, `send`, `onReceive`), MessagePack encoding, simulator stubs |
| `deploii.cpp` | C++ shims: `connect`, `sendRaw`, `onDataReceived`, `receiveMessage` |
| `deploii_service.h/.cpp` | `DeploiiService`, the GATT service (CODAL `MicroBitBLEService`): advertising, packet sending, receive queue, BLE long writes |
| `deploii_decode.h/.cpp` | Decodes received MessagePack into TypeScript values, using CMP |
| `cmp.c`, `cmp.h` | [CMP](https://github.com/camgunz/cmp) MessagePack library (MIT, vendored unchanged) |
| `deploii_hash.h/.cpp` | `deploii_hash()`, **to implement** |
| `deploii_config.h` | UUIDs, sizes, error codes, debug switch |
| `test.ts` | Test program, only built when building the extension itself |
| `scripts/setup.sh` | One-time setup. Also creates `Makefile` (which includes `scripts/dev.mk`) and `.vscode/` from `scripts/vscode/` |
| `scripts/dev.mk`, `package.json` | Dev tooling (make targets, pinned pxt + pxt-microbit), not part of the extension |

## How connect works

`deploii.connect(boardID)` (TypeScript block) calls `deploii::connect(String)` in C++, which:

1. checks that the Bluetooth stack is running,
2. hashes the board ID with `deploii_hash()` into a 16-bit UUID,
3. adds a primary service with that UUID and two characteristics, as the Arduino library does:

   | UUID     | Name    | Properties     | Direction |
   |----------|---------|----------------|-----------|
   | `0x4C1D` | send    | read, notify   | micro:bit → Deploii |
   | `0xA7F3` | receive | write, notify  | Deploii → micro:bit |

   All UUIDs are 16-bit on the Bluetooth SIG base (`0000xxxx-0000-1000-8000-00805f9b34fb`), the same
   as ArduinoBLE's short UUID strings.
4. replaces the default advertising with flags + the service UUID (the device name goes in the scan
   response) and starts advertising. After a disconnect, CODAL restarts advertising with the same data.

It returns `0` on success or a negative code:

| Code | Meaning |
|------|---------|
| -1 | Bluetooth unavailable (built without BLE, or BLE stack not running) |
| -2 | Already connected (`connect` already succeeded) |
| -3 | Empty board ID |
| -4 | Adding the GATT service failed |
| -5 | Adding a characteristic failed |
| -6 | Advertising data too large to encode |
| -7 | Configuring advertising failed |
| -8 | Starting advertising failed (for example, a central is already connected) |

For the raw nRF error code, set `DEPLOII_DEBUG` to `1` in `deploii_config.h` and run `make monitor`.

## Sending

`deploii.send(id, value)` sends any value to a datastream, like the Arduino library's templated
`send()`. `main.ts` encodes it as MessagePack, the same map with one entry the Arduino sends:
`{ "<datastream id>": value }`.

| TypeScript value | MessagePack |
|------------------|-------------|
| whole number (int32 range) | fixint / int 32 |
| other number | float 32 (like an Arduino `float`) |
| text | str |
| true / false | bool |
| null / undefined | nil |
| list (can be nested) | array |
| anything else | its text form, as str |

`sendRaw()` (C++) then splits the bytes into packets in the Arduino format (first byte = packets
remaining after this one). `send` returns `0` or `-9` (not connected/subscribed yet), `-10` (more than
256 packets) or `-11` (sending failed).

## Receiving

`deploii.onReceive((moduleId, data) => ...)` works like the Arduino library's `receive(cb)`: the handler
runs for every message of type `"module_data"`, with its module ID and data.

1. Deploii writes one MessagePack message to the receive characteristic, with no header byte, as on the
   Arduino. Writes longer than 20 bytes arrive as BLE long writes (prepared writes). `DeploiiService`
   handles these by giving the SoftDevice a buffer for them (`BLE_EVT_USER_MEM_REQUEST`).
2. Each complete message goes into a queue (`DEPLOII_RX_QUEUE_SIZE`, default 4), and an event is raised.
3. For each event, TypeScript calls `receiveMessage()`. C++ decodes the oldest message with CMP into
   `[moduleId, data, cardId, type, protocol]` (`deploii.MessageField`).
4. `onReceive` passes messages with `type == "module_data"` to your handler.

Fields are matched by key name (`moduleId`, `data`, `cardId`, `type`, `protocol`). A key with another
name falls back to its position, which is how the Arduino library decodes (it ignores key names).
`data` can be any type: text, number, boolean, `null`, a list, or a Buffer. A nested map becomes a list
`[key, value, key, value, ...]`.

To see every message, including ones that aren't `module_data`, use the advanced
`deploii.onDataReceived(() => { const msg = deploii.receiveMessage() ... })`.

## Things to know

- **The hash must match Deploii.** The Arduino library uses `DEPLOII_fnv1a16()` from `src/deploii_utils.h`.
  Until `deploii_hash()` is implemented, every board advertises UUID `0x0000`.
- **Packet size.** CODAL v2 uses the default BLE MTU of 23 bytes, so one notification holds at most
  20 bytes. `send()` therefore uses 20-byte packets (`DEPLOII_BLE_PACKET_SIZE`) instead of the Arduino's
  128. The header byte lets Deploii reassemble either size, but check that the web client doesn't assume
  128-byte packets. Writes *to* the micro:bit don't need a bigger MTU: longer writes are handled as
  BLE long writes. Raising the MTU would mean overriding `NRF_SDH_BLE_GATT_MAX_MTU_SIZE` (for example in
  `pxt.json` → `yotta.config`) and making sure the SoftDevice still fits in RAM.
- **Received messages are at most 128 bytes** (`DEPLOII_BLE_BUFFER_MAX_SIZE`), as on the Arduino. If
  messages arrive faster than the handler runs, up to 4 are queued and the rest are dropped.
- **Bluetooth and radio can't be used together.** Bluetooth replaces the `radio` extension. MakeCode
  offers to remove radio when you add this extension.
- **No pairing.** `pxt.json` sets `open: 1`, so Deploii (Web Bluetooth) can connect without pairing,
  like the Arduino.
- **Simulator.** `connect` and `send` return 0 in the simulator and nothing is ever received.
  Bluetooth only works on hardware.

## Testing without Deploii

Use the nRF Connect app (Android/iOS) or `chrome://bluetooth-internals` in Chrome. The micro:bit shows up
as `BBC micro:bit [xxxxx]` advertising your service UUID. You can connect to it, read `0x4C1D`, and
write to `0xA7F3`.

## Using it in the MakeCode web editor

Push this folder to a GitHub repository, then in [makecode.microbit.org](https://makecode.microbit.org)
go to **Extensions** and paste the repository URL. The cloud compiler builds the C++ automatically.
