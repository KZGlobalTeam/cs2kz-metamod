# Course progress from reference replays

Progress belongs to the existing `KZHUDService`. It projects the current player position onto a
completed reference replay route for the active map, course and mode, and displays
the fraction of route length in the existing timer HUD. It is an estimate of
position along that reference, rather than elapsed time or a checkpoint counter.

## Player controls

- Use `!progress` to toggle the display, or change Show Progress in HUD preferences.
- Enable the Timer HUD field to see progress there.
- A valid start displays 0%; a valid finish displays 100%.
- Spectators see the observed player's progress. Show Progress follows the HUD's
  existing Mimic Spectated Player preference, just like the other HUD settings.
- Normal backwards movement and checkpoint teleports can decrease progress.
- Missing or unsuitable references cause progress to be hidden.

## Reference selection

Progress consumes the existing map-level record caches. The normal SR database
query and WR API response retain the record's replay UUID and notify the existing
HUD timer listener after publishing their results. No additional record query is
issued for progress. Automatic selection prefers the cached no-TP WR when its
replay is available locally, then the cached no-TP SR. If neither is usable,
progress remains hidden unless an administrator selects a local reference.

ReplayWatcher's existing index supplies read-only headers and a change revision.
A record-cache event, map initialization, index change or explicit reload queues
route decoding. Progress does not rescan directories, download replays, create
records, or start/seek a replay bot. Files must match the map name and MD5, course
and mode and contain a complete, valid, unstyled no-TP run.

HUD publishes validated immutable routes on the main thread. Active players pick
up the replacement without restarting the map or timer; the new route generation
invalidates old checkpoint anchors and forces a position rematch. An invalid
replacement leaves the previous valid route active. Stale asynchronous results
from an earlier map or superseded request are discarded.

These commands are restricted to the server console/RCON:

```text
kz_progress_routes
kz_progress_replays [courseID] [mode]
kz_progress_reference <courseID> <mode> <UUID|auto>
kz_progress_reload
```

Use the course ID and exact mode name printed by the listing. A fixed reference
is validated before replacing the previous selection. Selections are stored in
`addons/cs2kz/data/progress-references.cfg`, scoped by map MD5, course and mode.
`auto` removes the override. Validated changes are applied to active HUDs automatically.
Selecting a replay that follows the intended shortcut can improve coverage.

## Server settings

```text
kz_progress_enable 1
kz_progress_hud_enable 1
kz_progress_update_interval 0.05
kz_progress_max_distance 384
kz_progress_hold_time 1.5
```

Place console settings in the existing CS2KZ configuration. The reference-selection
file is KeyValues data and must not be executed as console commands.

## Implementation and limits

`route.cpp` contains polyline projection, a spatial grid and bounded candidate
search. Local matching uses route history to avoid switching between overlapping
sections. Global matching permits reacquisition after shortcuts and drops when
the nearby geometry provides a sufficiently clear match. Genuine map teleports
are excluded from route distance using recorded replay events.

`replay_route.cpp` validates and reads existing replay files; it and `route.cpp`
are independent, read-only helpers in `src/kz/hud/progress/`. HUD integration and
reference selection live in `src/kz/hud/progress_routes.cpp`, which consumes the
existing record cache and publishes immutable routes after background decoding. `KZHUDService` owns the per-player estimate, resets it with the HUD lifecycle,
handles timer events through its existing listener, and formats localized progress
for both HUD styles. Checkpoints save and restore an anchor through HUDService.
There is no separate player service or second timer listener. The algorithm/decoder
helpers do not write files, records, timer state or replay playback state. Only an
explicit administrator selection persists the HUD reference preference. Background work does not retain player objects.

When briefly off the reference, the HUD can display a `~` estimate or retain the
last value for the configured hold time. Ambiguous overlapping geometry or a
long deviation can still hide progress. One reference cannot describe every
possible route through a branching course.

## Build and testing

Build normally with the repository's documented AMBuild workflow. To also build
the terminal regression executables, configure a separate build directory:

```text
python ../configure.py --enable-optimize --progress-tests
ambuild
```

Run `python tests/progress/record_cache_test.py` from the repository root to test
the production SR query on an in-memory SQLite database (UUID/metadata pairing,
TP/style/ban filtering, ties, course/mode isolation and refreshed records).

Run `progress-tests` and `progress-replay-tests` from their AMBuild target
directories (on Windows, use the `.exe` files and put the server's `game/bin/win64`
directory on `PATH` for `tier0.dll`). Both exit nonzero on failure.
`--progress-tests` adds only those executables; it does not add test commands,
hooks or synthetic bots to the plugin.

The route test covers projection, backwards movement, overlaps, falls, teleports
and bounded search. The replay test exercises the production codec and rejects
truncated or unsuitable references. For a locally available replay:

```text
progress-replay-tests --audit-falls <replay-path> <courseID>
```

This additional audit simulates drops against the supplied replay geometry. It
does not validate client rendering or replace real gameplay checks. Before
requesting review, verify both HUD styles, falling back to an earlier section,
checkpoint/undo teleports, shortcuts, spectator preferences, reference selection,
map changes and reconnects on a compatible server. Report the tested platforms
and remaining gaps in the PR description.
