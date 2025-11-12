# Replay File Format
   
## File Extensions

- Filename format: `{UUID}.replay` (UUID parsed from filename)
- Stored in `csgo/kzreplays/` directory

## File Structure

### High-Level Layout

```
[General Header]
[Type-Specific Header]
[Compressed Tick Data Section]
[Compressed Weapon Changes Section]
[Compressed Jump Stats Section]
[Compressed Events Section]
[Compressed Command Data Section]
```

### 1. General Header (`GeneralReplayHeader`)

Located at the start of every replay file.

```cpp
struct GeneralReplayHeader {
    u32 magicNumber;        // 'S2KZ' - file identifier
    u32 version;            // Current version
    ReplayType type;        // RP_RUN, RP_JUMPSTATS, RP_CHEATER, RP_MANUAL
    
    struct {
        char name[128];
        u64 steamid64;
    } player;
    
    struct {
        char name[64];
        char md5[33];
    } map;
    
    EconInfo firstWeapon;   // Initial weapon
    EconInfo gloves;        // Player gloves
    char modelName[256];    // Player model
    
    u64 timestamp;          // Unix timestamp
    char pluginVersion[32]; // CS2KZ plugin version
    u32 serverVersion;      // Server version
    u32 serverIP;           // Server IP address
    f32 sensitivity;        // Mouse sensitivity
    f32 yaw;                // m_yaw value
    f32 pitch;              // m_pitch value
    
    u64 archivedTimestamp;  // When archived (0 = not archived). Only used locally by servers.
};
```

**Magic Number:** `KZ_REPLAY_MAGIC` validates file integrity.  
**Version:** Incremented when format changes incompatibly.

### 2. Type-Specific Headers

Following the general header, one of these headers is written based on `type`:

#### Run Replay Header (`RunReplayHeader`)
```cpp
struct RunReplayHeader {
    char courseName[256];
    RpModeStyleInfo mode;
    i32 styleCount;
    f32 time;
    i32 numTeleports;
};
```

Following the header, an array of `styleCount` style infos is written:
```cpp
struct RpModeStyleInfo {
    char name[64];
    char shortName[64];
    char md5[33];
};
```

#### Jumpstats Replay Header (`JumpReplayHeader`)
```cpp
struct JumpReplayHeader {
    RpModeStyleInfo mode;
    u8 jumpType;
    f32 distance;
    i32 blockDistance;
    u32 numStrafes;
    f32 sync;
    f32 pre;
    f32 max;
    f32 airTime;
};
```

#### Cheater Replay Header (`CheaterReplayHeader`)
```cpp
struct CheaterReplayHeader {
    char reason[512];
    
    struct {
        char name[128];
        u64 steamid64;
    } reporter;  // Empty if automated detection
};
```

#### Manual Replay Header (`ManualReplayHeader`)
```cpp
struct ManualReplayHeader {
    struct {
        char name[128];
        u64 steamid64;
    } savedBy;
};
```

## Compressed Data Sections

All data sections follow the same compression structure:

```
[CompressedSectionHeader]
[zstd compressed payload]
```

### Compressed Section Header
```cpp
struct CompressedSectionHeader
{
	u32 compressedSize;   // Size of compressed data
	u32 uncompressedSize; // Original size of data
	u32 elementCount;     // Number of elements (e.g., tick count)
};
```

See `kz_replay.h` for definitions of each section's data structures.

### Tick Data Compression

Each tick is compared against the previous tick. For each field that changed, a bit flag is set in a 64-bit change mask.

**Change Flags:**
- Three separate flag sets: `topFlags` (top-level), `preFlags` (pre-movement), `postFlags` (post-movement)
- Each flag corresponds to a field (e.g., `CHANGED_SERVER_TICK`, `CHANGED_ORIGIN`, `CHANGED_VELOCITY`)

**Special Optimizations:**

1. **Server Tick:** Only written when ≠ previous tick + 1
   - Most ticks increment by 1, saving 4 bytes/tick
   
2. **Cross-Comparison:** 
   - `pre[N]` compared against `post[N-1]` (catches deltas across tick boundary)
   - `post[N]` compared against `pre[N]` (catches deltas within tick)

**Encoding Process:**
```
For each tick:
  1. Build change flags by comparing fields
  2. Write topFlags, preFlags, postFlags (24 bytes/tick overhead)
  3. Write only changed fields from current tick
  4. Write subtick data
  
After all ticks encoded:
  5. Compress entire buffer with zstd level 3
  6. Write CompressedSectionHeader + compressed data
```

### Command Data Compression

Similar to TickData, uses field-based delta encoding with change flags:

**Change Flags:** Single 64-bit mask for all CmdData fields

**Special Optimizations:**
1. **Server Tick:** Only written when ≠ previous tick + 1
2. **Game Time:** Only written when ≠ previous tick + tick interval
