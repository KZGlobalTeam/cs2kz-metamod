# Replay format: shared layouts

These are the building blocks of every replay version. The [version pages](README.md) say which variant of each block a
version uses.

## Conventions

- All integers and floats are **little-endian**.
- `u8/u16/u32/u64` are unsigned, `i32` and `i64` are signed two's complement, `f32` is IEEE 754 single precision.
- `bool` is one byte. Any non-zero value is true.
- `vec3` is three consecutive `f32` (x, y, z). Angles use the same layout (pitch, yaw, roll).
- `char[N]` is a fixed-size, NUL-terminated string buffer. Bytes after the first NUL are unspecified.
- Several structs are stored as raw x86-64 memory images; the offsets below are part of the format.
  **Padding bytes and unused union bytes are unspecified.**
- Compression is [zstd](https://github.com/facebook/zstd). Every frame records its content size in the frame header.

## File layout

```
u32            headerSize
u8[headerSize] replay header
section 1      ticks              (tick encoding)
section 2      tick subticks      (subtick entries, one per tick)
section 3      weapons            (weapon table)
section 4      jumps              (jump records)
section 5      events             (events)
section 6      commands           (command encoding)
section 7      command subticks   (subtick entries, one per command)
```

Sections follow each other with no padding, and the file ends after section 7. Sections 6 and 7 hold the raw user commands
the server received; playback only needs sections 1 to 5.

## Replay header

`headerSize` bytes of a protobuf (proto2) `cs2kz.replay.ReplayHeader` message, defined in
[`protobuf/kz_replay.proto`](../../protobuf/kz_replay.proto). The header is the same in every version.

| Field | Number | Meaning |
|---|---|---|
| `version` | 1 | Format version |
| `type` | 2 | `RP_MANUAL` 0, `RP_CHEATER` 1, `RP_RUN` 2, `RP_JUMPSTATS` 3 |
| `player` | 3 | Recorded player name and SteamID64 |
| `map` | 4 | Map name and MD5 |
| `gloves`, `model_name` | 5, 6 | Player appearance |
| `timestamp` | 7 | Unix time the replay was saved |
| `plugin_version`, `server_version`, `server_ip` | 8-10 | Provenance |
| `sensitivity`, `yaw`, `pitch` | 11-13 | Mouse settings of the player |
| `manual` / `cheater` / `run` / `jump` | 14-17 | Type-specific data (one of) |
| `viewmodel_offset_*`, `viewmodel_fov` | 18-21 | Viewmodel settings |

The plugin always writes `version` as the first field, so a header begins with the bytes `08 <version>`.

## Section containers

Every section is a header followed by a zstd-compressed payload. `elementCount` is the number of elements in the section;
each section's page below says what an element is.

### 12-byte container (versions 1-6)

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | `compressedSize`: bytes of zstd data after the header |
| 4 | u32 | `uncompressedSize`: bytes after decompression |
| 8 | u32 | `elementCount` |

The payload is exactly one zstd frame.

### 24-byte container (version 7)

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | `compressedSize` |
| 8 | u64 | `uncompressedSize` |
| 16 | u64 | `elementCount` |

The payload is **one or more** zstd frames back to back. Long recordings are written one 15-minute chunk at a time, and
each chunk becomes its own frame. The section's data is the concatenation of all decompressed frames, and the delta
encodings run continuously across frame boundaries. The frames' content sizes add up to `uncompressedSize`. A section
without elements still holds one frame, with content size 0.

## Tick encoding

Section 1. One element is one server tick (64 Hz) of the recorded player. Each tick is a `u64` change mask followed by the
fields whose bit is set, in a fixed order. The decoder keeps one tick state and updates it in place.

### Tick state

| Field | Type | Notes |
|---|---|---|
| `serverTick` | u32 | Server tick the tick was recorded on |
| `gameTime` | f32 | Server game time |
| `realTime` | f32 | |
| `unixTime` | u64 | |
| `cmdNumber` | u32 | Number of the command this tick ran |
| `clientTick` | u32 | |
| `forward`, `left`, `up` | f32 | Movement input of the command |
| `leftHanded` | bool | |
| `weapon` | i32 | Weapon ID, see [weapon table](#weapon-table). -1 means no weapon. |
| `pre` | Movement | Player state at the start of the tick's physics |
| `post` | Movement | Player state at the end of the tick's physics |
| `checkpoint.index` | i32 | Current checkpoint index |
| `checkpoint.checkpointCount` | i32 | Checkpoints made |
| `checkpoint.teleportCount` | i32 | Teleports used |
| `modernJump` | ModernJump | Versions 2+ only |

**Movement**

| Field | Type | Notes |
|---|---|---|
| `origin` | vec3 | |
| `velocity` | vec3 | |
| `angles` | vec3 | Eye angles |
| `buttons[0..2]` | u32 each | The three button state words |
| `jumpPressedTime` | f32 | |
| `duckSpeed` | f32 | |
| `duckAmount` | f32 | |
| `duckOffset` | f32 | |
| `lastDuckTime` | f32 | |
| `replayFlags` | u8 | Bit 0 ducking, bit 1 ducked, bit 2 desiresDuck. **Bits 3-7 are unspecified.** |
| `entityFlags` | u32 | Entity flags (`m_fFlags`) |
| `moveType` | u8 | Engine `MoveType_t`; `MOVETYPE_WALK` is 2 |

**ModernJump**

| Field | Type |
|---|---|
| `lastActualJumpPressTick` | i32 |
| `lastActualJumpPressFrac` | f32 |
| `lastUsableJumpPressTick` | i32 |
| `lastUsableJumpPressFrac` | f32 |
| `lastLandedTick` | i32 |
| `lastLandedFrac` | f32 |
| `lastLandedVelocity` | vec3 |

### Initial state

Before the first tick every field of the state is zero, except that the first tick's `pre` starts from a Movement whose
`moveType` is `MOVETYPE_WALK` (2) with every other field zero.

### Change mask

Bits 0-38 are the same in every version:

| Bit | Field | Payload |
|---|---|---|
| 0 | `serverTick` | u32 |
| 1 | `gameTime` | f32 |
| 2 | `realTime` | f32 |
| 3 | `unixTime` | u64 |
| 4 | `cmdNumber` | u32 |
| 5 | `clientTick` | u32 |
| 6 | `forward` | f32 |
| 7 | `left` | f32 |
| 8 | `up` | f32 |
| 9 | `leftHanded` | bool (1 byte) |
| 10 | `pre.origin` | vec3 |
| 11 | `pre.velocity` | vec3 |
| 12 | `pre.angles` | vec3 |
| 13 | `pre.buttons[0]` | u32 |
| 14 | `pre.buttons[1]` | u32 |
| 15 | `pre.buttons[2]` | u32 |
| 16 | `pre.jumpPressedTime` | f32 |
| 17 | `pre.duckSpeed` | f32 |
| 18 | `pre.duckAmount` | f32 |
| 19 | `pre.duckOffset` | f32 |
| 20 | `pre.lastDuckTime` | f32 |
| 21 | `pre.replayFlags` | u8 |
| 22 | `pre.entityFlags` | u32 |
| 23 | `pre.moveType` | u8 |
| 24-37 | `post.*` | Same fields and payloads as bits 10-23, in the same order |
| 38 | checkpoint | i32 `index`, i32 `checkpointCount`, i32 `teleportCount` |

The weapon flag and the three modern jump flags depend on the version; each version page lists them. Their payloads are:

| Flag | Payload |
|---|---|
| Weapon | i32 `weapon` |
| Actual press | i32 `lastActualJumpPressTick`, f32 `lastActualJumpPressFrac` |
| Usable press | i32 `lastUsableJumpPressTick`, f32 `lastUsableJumpPressFrac` |
| Landed | i32 `lastLandedTick`, f32 `lastLandedFrac`, vec3 `lastLandedVelocity` |

Bits not listed for a version are never set.

### Decoding one tick

Fields are read in exactly this order. Each read is skipped when its bit is clear, and the field keeps its value.

```
flags = read u64

if this is not the first tick and bit 0 is clear:
    serverTick += 1                      # the only implicit change
read serverTick, gameTime, realTime, unixTime, cmdNumber, clientTick,
     forward, left, up, leftHanded       # bits 0-9, in bit order
read weapon                              # weapon flag

pre = post of the previous tick          # the initial pre for the first tick
read pre fields                          # bits 10-23, in bit order

post = pre
read post fields                         # bits 24-37, in bit order

read checkpoint                          # bit 38

if the version has modern jump state:
    read actual press, usable press, landed   # modern jump flags, in this order
```

`gameTime` does not advance implicitly; it is written whenever it changes. The tick section holds exactly
`elementCount` ticks and nothing after them.

## Subtick entries

Sections 2 and 7. One entry holds the subtick moves (`CSubtickMoveStep`) of one user command: in section 2 the command each
tick ran (entry `i` belongs to tick `i`), in section 7 each command of section 6. In section 2 the entry count normally
equals the tick count, but some recordings hold one more entry than ticks; that entry belongs to no tick.

### Subtick move (24 bytes)

| Offset | Type | Field |
|---|---|---|
| 0 | f32 | `when`: fraction of the tick at which the step happens |
| 4 | u32 | `button`: the button bit this step presses or releases, 0 for an analog step |
| 8 | bool | `pressed` (button steps) |
| 8 | f32 | `analogForwardDelta` (analog steps) |
| 12 | f32 | `analogLeftDelta` (analog steps) |
| 16 | f32 | `pitchDelta` |
| 20 | f32 | `yawDelta` |

Offsets 8-15 are a union: button steps (`button != 0`) use only offset 8, analog steps (`button == 0`) use offsets 8 and 12.
`pitchDelta` and `yawDelta` apply to every step, but versions 1-6 never wrote them for analog steps, so in those versions
they are garbage there and stand for 0.

### Fixed entries (versions 1-6)

Every entry has the same size, so `uncompressedSize == elementCount * entrySize`.

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | `numSubtickMoves` |
| 4 | move[slots] | Move slots, 24 bytes each |

| Versions | Slots | Entry size |
|---|---|---|
| 1-3 | 64 | 1540 |
| 4-6 | 36 | 868 |

The entry holds `min(numSubtickMoves, 36)` moves in its first slots; the recorder never kept more than 36.
`numSubtickMoves` itself can be larger, when the client sent more steps than were kept. Unused slots are unspecified.

### Packed entries (version 7)

```
u8      count            # 0-36
move    moves[count]
```

Entries follow each other with no padding, and the section holds exactly `elementCount` of them. Most ticks have no moves,
so most entries are a single zero byte.

## Weapon table

Section 3. One element per weapon the replay uses:

```
i32       weaponID
MainInfo  mainInfo                              # 360 bytes, raw
Attribute attributes[mainInfo.numAttributes]    # 8 bytes each
```

`weaponID` is the value ticks store in `weapon`. It numbers the weapons within the recording and is not an item
definition index. A tick's weapon is the entry with a matching `weaponID`; no match (including -1) means no weapon.

**MainInfo (360 bytes)**

| Offset | Type | Field |
|---|---|---|
| 0 | i32 | `itemDef`: item definition index |
| 4 | i32 | `quality` |
| 8 | i32 | `level` |
| 12 | i32 | `accountID` |
| 16 | i64 | `itemID` |
| 24 | i32 | `inventoryPosition` |
| 28 | char[161] | `customName` |
| 189 | char[161] | `customNameOverride` |
| 350 | | padding |
| 352 | i32 | `numAttributes`, at most 32 |
| 356 | | padding |

**Attribute (8 bytes)**

| Offset | Type | Field |
|---|---|---|
| 0 | i32 | `defIndex` |
| 4 | f32 | `value` |

## Jump records

Section 4. One element per jumpstat recorded during the replay.

```
i32 numJumps                          # equals elementCount
repeat numJumps times:
    GeneralData overall               # 364 bytes, raw
    i32         numStrafes
    StrafeData  strafes[numStrafes]   # 72 bytes each
    i32         numAACalls
    AAData      aaCalls[numAACalls]   # 92 or 84 bytes, see the version page
```

**GeneralData (364 bytes)**

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | `serverTick` |
| 4 | vec3 | `takeoffOrigin` |
| 16 | vec3 | `adjustedTakeoffOrigin` |
| 28 | vec3 | `takeoffVelocity` |
| 40 | vec3 | `landingOrigin` |
| 52 | vec3 | `adjustedLandingOrigin` |
| 64 | u8 | `jumpType` |
| 65 | u8 | `distanceTier`: 0 none, 1 meh, 2 impressive, 3 perfect, 4 godlike, 5 ownage, 6 wrecker |
| 66 | | padding |
| 68 | f32 | `totalDistance` |
| 72 | f32 | `maxSpeed` |
| 76 | f32 | `maxHeight` |
| 80 | f32 | `airtime` |
| 84 | f32 | `duckDuration` |
| 88 | f32 | `duckEndDuration` |
| 92 | f32 | `release` |
| 96 | f32 | `block` |
| 100 | f32 | `edge` |
| 104 | f32 | `landingEdge` |
| 108 | char[256] | `invalidateReason` |

`jumpType`: 0 long jump, 1 bhop, 2 multi bhop, 3 weird jump, 4 ladder jump, 5 ladderhop, 6 jumpbug, 7 fall, 8 other,
9 invalid, 255 fully invalid.

**StrafeData (72 bytes)**

| Offset | Type | Field |
|---|---|---|
| 0 | f32 | `duration` |
| 4 | f32 | `badAngles` |
| 8 | f32 | `overlap` |
| 12 | f32 | `deadAir` |
| 16 | f32 | `syncDuration` |
| 20 | f32 | `width` |
| 24 | f32 | `airGain` |
| 28 | f32 | `maxGain` |
| 32 | f32 | `airLoss` |
| 36 | f32 | `collisionGain` |
| 40 | f32 | `collisionLoss` |
| 44 | f32 | `externalGain` |
| 48 | f32 | `externalLoss` |
| 52 | f32 | `strafeMaxSpeed` |
| 56 | bool | `hasArStats` |
| 57 | | padding |
| 60 | f32 | `arMax` |
| 64 | f32 | `arMedian` |
| 68 | f32 | `arAverage` |

Strafe fields can be NaN.

**AAData, 92 bytes (versions 5-7)**

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | `strafeIndex` |
| 4 | f32 | `externalSpeedDiff` |
| 8 | f32 | `prevYaw` |
| 12 | f32 | `currentYaw` |
| 16 | f32 | `startFraction` |
| 20 | f32 | `endFraction` |
| 24 | vec3 | `wishDir` |
| 36 | f32 | `wishSpeed` |
| 40 | f32 | `accel` |
| 44 | f32 | `surfaceFriction` |
| 48 | f32 | `duration` |
| 52 | u32[3] | `buttons` |
| 64 | vec3 | `velocityPre` |
| 76 | vec3 | `velocityPost` |
| 88 | bool | `ducking` |
| 89 | | padding |

**AAData, 84 bytes (versions 1-4)**

The same fields without `startFraction` and `endFraction`, which stand for 0 and 1.

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | `strafeIndex` |
| 4 | f32 | `externalSpeedDiff` |
| 8 | f32 | `prevYaw` |
| 12 | f32 | `currentYaw` |
| 16 | vec3 | `wishDir` |
| 28 | f32 | `wishSpeed` |
| 32 | f32 | `accel` |
| 36 | f32 | `surfaceFriction` |
| 40 | f32 | `duration` |
| 44 | u32[3] | `buttons` |
| 56 | vec3 | `velocityPre` |
| 68 | vec3 | `velocityPost` |
| 80 | bool | `ducking` |
| 81 | | padding |

## Events

Section 5. One element per timer, mode, style, teleport or checkpoint event.

Event types: 0 timer, 1 mode change, 2 style change, 3 teleport, 4 checkpoint.
Timer event types: 0 start, 1 end, 2 stop, 3 pause, 4 resume, 5 split, 6 checkpoint zone, 7 stage.

### Raw events (versions 1-5)

`uncompressedSize == elementCount * 172`. Each event is 172 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | i32 | `type`: 0-3; checkpoint events do not exist in these versions |
| 4 | u32 | `serverTick` |
| 8 | union | `data`, 164 bytes, by `type` |

| Type | Offset | Type | Field |
|---|---|---|---|
| 0 timer | 8 | i32 | `timerType` |
| | 12 | i32 | `index`: course ID for start and end, otherwise the split, checkpoint or stage number |
| | 16 | f32 | `time` |
| 1 mode | 8 | char[64] | `name` |
| | 72 | char[64] | `shortName` |
| | 136 | char[33] | `md5` |
| 2 style | 8-168 | | same as mode |
| | 169 | bool | `clearStyles` |
| 3 teleport | 8 | bool | `hasOrigin` |
| | 9 | bool | `hasAngles` |
| | 10 | bool | `hasVelocity` |
| | 12 | vec3 | `origin` |
| | 24 | vec3 | `angles` |
| | 36 | vec3 | `velocity` |

These teleports carry no reason, checkpoint index or previous origin.

Raw events are stored in recording order, which is not always server tick order. Mode and style names are byte strings
and are not always valid UTF-8.

### Protobuf events (versions 6-7)

The section is `elementCount` records back to back, and nothing after them:

```
u32 length
u8  message[length]      # protobuf cs2kz.replay.ReplayEvent
```

`ReplayEvent` is defined in [`protobuf/kz_replay.proto`](../../protobuf/kz_replay.proto):

| Field | Meaning |
|---|---|
| `type` | Event type |
| `server_tick` | Server tick of the event |
| `phase` | 0 before physics, 1 during physics, 2 after physics |
| `timer` | `type` (timer event type), `index`, `time`, `origin` |
| `mode` | `name`, `short_name`, `md5` |
| `style` | `style` (name, short name, md5), `clear_styles` |
| `teleport` | `reason` (-1 unknown, 0 checkpoint, 1 checkpoint undo, 2 map trigger, 3 bhop fail), `checkpoint_index`, `previous_origin`, and `origin`, `angles`, `velocity` when the teleport set them |
| `checkpoint` | `type` (0 save, 1 reset), `index` (1-based for saves, 0 for reset), `origin` (saves only) |

`type`, `server_tick` and `phase` are always present, and so is the sub-message for the type. Each sub-message carries all
of its fields except those marked optional above. Every vector carries x, y and z. Events are in non-decreasing
`server_tick` order.

A record whose `type` is not listed here can be skipped by its `length`.

## Command encoding

Section 6. One element per user command the server received from the player, whether or not a tick was recorded for it.
Like ticks, each command is a `u64` change mask followed by the present fields in bit order.

| Bit | Field | Payload |
|---|---|---|
| 0 | `serverTick` | u32 |
| 1 | `gameTime` | f32 |
| 2 | `realTime` | f32 |
| 3 | `unixTime` | u64 |
| 4 | `framerate` | f32 |
| 5 | `latency` | f32 |
| 6 | `avgLoss` | f32 |
| 7 | `cmdNumber` | u32 |
| 8 | `clientTick` | u32 |
| 9 | `forward` | f32 |
| 10 | `left` | f32 |
| 11 | `up` | f32 |
| 12 | `buttons[0]` | u64 |
| 13 | `buttons[1]` | u64 |
| 14 | `buttons[2]` | u64 |
| 15 | `angles` | vec3 |
| 16 | `mousedx` | i32 |
| 17 | `mousedy` | i32 |
| 18 | `sensitivity` | f32 |
| 19 | `m_yaw` | f32 |
| 20 | `m_pitch` | f32 |

```
state starts with every field zero
for each command:
    flags = read u64
    if this is not the first command:
        if bit 0 is clear: serverTick = previous serverTick + 1
        if bit 1 is clear: gameTime = previous gameTime + 0.015625   # in f32 arithmetic
        every other field with a clear bit keeps its previous value
    read the fields whose bits are set, in bit order
```

The `gameTime` addition is done in single precision, which reproduces the recorded values exactly.

The command encoding is the same in every version.
