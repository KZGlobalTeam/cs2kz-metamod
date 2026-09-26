# Course progress from reference replays

Progress is a CS2KZ player service. It projects the current player position onto a
completed reference replay route for the active map, course and mode, and displays
the fraction of route length in the existing timer HUD. It is an estimate of
position along that reference, rather than elapsed time or a checkpoint counter.

## Player controls

- Use `!progress` to toggle the display, or change Show Progress in HUD preferences.
- Enable the Timer HUD field to see progress there.
- A valid start displays 0%; a valid finish displays 100%.
- Spectators see the observed player's progress using their own HUD preference.
- Normal backwards movement and checkpoint teleports can decrease progress.
- Missing or unsuitable references cause progress to be hidden.

## Reference selection

The existing ReplayWatcher supplies candidate metadata. References must match the
map name and MD5, course and mode, and be complete, valid runs without checkpoint
teleports or styles. The automatic selection chooses a suitable fast replay.
The selected route remains stable during a run; administrator reloads deliberately
invalidate the cached route. Only local replays are used.

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
`auto` removes the override. Restart a run or reload progress to adopt a change.
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

`replay_route.cpp` validates and decodes existing replay files. `routes.cpp` owns
map-level caches and publishes immutable routes on the main thread after background
processing. `kz_progress.cpp` connects player state, timer events, checkpoint
anchors, HUD text and preferences. Background work does not retain player objects.

When briefly off the reference, the HUD can display a `~` estimate or retain the
last value for the configured hold time. Ambiguous overlapping geometry or a
long deviation can still hide progress. One reference cannot describe every
possible route through a branching course.

## Build and testing

Build normally with the repository's documented AMBuild workflow. For isolated
tests only, configure a separate directory with `--progress-tests`. This builds
`progress-tests`, `progress-replay-tests` and a plugin containing runtime test
commands. Do not deploy that test plugin to a public server; use a normal build
directory for release binaries.

The route test covers projection, backwards movement, overlaps, falls, teleports
and search bounds. The replay test uses the production codec and rejects truncated
or unsuitable references. `--audit-falls <replay-path> <courseID>` additionally
audits synthetic drops against the supplied real replay geometry.

The earlier Windows R2 implementation passed 128,160 route checks, 59 codec checks
and 367 simulated drops on a local Garden reference from kz_grotto. These are
historical R2 results, not a fresh build or gameplay validation of this rebased
PR candidate. Rebuild this candidate and run the tests before requesting review;
Linux compilation and runtime verification must be reported separately.

With CS2's September 23, 2026 update, use MultiAddonManager 1.6.1 or a later
compatible version for Workshop connections. Its upstream offset fix is an
external dependency update, not part of the Progress implementation.
