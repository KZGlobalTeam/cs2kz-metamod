# Course progress

Progress estimates position along a completed reference route, not elapsed time or
the proportion of jumps completed. `KZProgressService` owns the estimate; the
Course HUD element reads and formats it as a row inside its progress block.
A running timer is not required. The timer's remembered
course selects the reference, falling back to the first course before selection.
Successful completion hides progress for that course, including after CP/undo or
death. The service listens to the timer's existing post-start/post-end events:
starting another run resets that suppression; selecting another course permits
its progress. Stopping an unfinished timer still allows positional progress.

## Reference and lifetime

The existing SR/WR queries retain their record UUIDs in their existing cache.
Progress reads those UUIDs; it does not query records again or scan ReplayWatcher.
It prefers a locally available pro WR, overall WR, pro SR, then overall SR. Overall
references may contain checkpoint teleports. Missing files are skipped; progress
does not initiate downloads or replay playback.

Once per second per active course/mode, the service checks the cached UUIDs and
their exact file paths. Failed references are retried after 30 seconds, so a file
observed during a write/download is not excluded until the next map change.
One worker uses the replay system's movement reader to obtain
private data, verifies map name/MD5, course, mode and absence of styles, and builds
the route. It never accesses players or the playing replay. The main thread joins
the worker before moving the result into its cache. A new generation discards
players' old matching history. Map change/unload cancels and joins the worker.
There are no Progress-specific file-size or tick-count limits. The movement reader
shares the existing header and delta decoder, retaining only positions, CP counters
and events. It skips subticks, weapons, jumpstats and command data. Playback keeps
its original full-data path. The compressed file and decompressed tick block still
occupy memory; this is not a streaming decoder. Only one reference is decoded at a
time. Temporary movement data is freed after construction. Format limits and
available memory remain practical constraints, including for runs over an hour.

## Route construction

1. Find the course's completed start/end interval among recorded timer events. The
   recorder's earlier buffer and post-run breather are not part of the route.
2. Keep movement positions in that interval. Teleport events split the polyline;
   the displacement across a teleport contributes no route length.
3. Observe new CP indices and retain their sampled pre/post positions. On a single
   CP teleport, resolve its selected CP index or the preceding teleport's source
   against the exact destination. Only an unambiguous recorded endpoint restores
   that visit. Points form a parent chain: restoring a saved visit omits the failed
   attempt from the final route while preserving branches for next-CP or undo.
   Unknown or multiple teleports create a break; they do not erase earlier points.
   No spatial-radius search guesses which visit a CP belongs to.
4. Store each segment's cumulative distance. Split segments longer than 128 units
   and index their midpoints once in a 128-unit spatial grid. A midpoint is within
   64 units of its segment. This avoids duplicate segment projections and stores
   an amount of data proportional to the trajectory, not its number of CP returns.

## Matching

Every 0.1 seconds, project the player's position onto nearby segments:
`t = Clamp(dot(position - start, end - start) / lengthSquared, 0, 1)`.
The candidate's distance along the route is `segmentDistance + t * segmentLength`.
The percentage is `100 * distanceAlongRoute / totalRouteLength`.

The geometric search radius is 256 units. The cell search extends three cells in
each direction to include the segment half-length. It considers every route stage
in that neighborhood, regardless of the previous percentage. A large shortcut or a
fall can therefore move the estimate immediately forwards or backwards. The
nearest geometry wins when there is no ambiguity; there is no monotonic clamp or
delayed rollback. Beyond 64 units from the reference, `~` marks an approximation.

Two passes first find the nearest error, then compare all candidates within eight
units of that error. If their full minimum-to-maximum span exceeds 256 route units,
consider only candidates within `Clamp(travel * 3 + 64, 128, 1024)` of the previous
distance. Use that history only if the remaining full span is at most 256 units.
This continuity allowance covers bends between samples; it does not constrain
unambiguous skips or falls. CP/undo teleports and route changes discard history.
If history cannot distinguish overlapping visits, hide the estimate until the
player reaches distinguishable geometry. Off-route positions also hide it.

Each query allows 4096 projections across its two passes (at most 2048 nearby
segments). This bounds geometry work per player at 10 Hz. If the neighborhood is
denser, the query hides the estimate rather than publishing a partial search.
This is a per-query work bound, not a cap on replay length. Dense repeated routes
can therefore hide progress even close to the reference. The geometric thresholds
are explicit heuristics, not measurements establishing accuracy on every map.

## Display and limits

The route percentage belongs to the Course element and follows its visibility,
position, font, scale and spectator preferences. Show Route Progress and Show
Progress Label are options on the Course settings page, separate from the existing
zone-count toggle. The label has its own localization phrase. Edit mode uses the
Course panel's regular preview, including a sample percentage. Completion hides
only the route row; course information and zone counts remain available.

The Course panel reads the service's cached estimate on its normal update path,
independently of the slower record-row refresh. The Timer element is unchanged.
Legacy HTML HUD has no Course element and does not render route progress.

One reference cannot identify the intended branch of every non-linear map. Old
replays do not record exact saved CP positions or identify undo operations. CPs
created before the completed interval, between sampled positions, or together
with another operation may be unidentifiable. The algorithm then keeps the failed
attempt rather than deleting a guessed branch; this affects the denominator even
where the route is locally unambiguous. An
identical shared segment may remain ambiguous after teleporting there. A shortcut
farther than the search radius hides progress until it rejoins known geometry.
These are limits of the estimate, not claims of complete course coverage.

Build using the normal project workflow. No test targets, test commands or replay
fixtures are added. Runtime verification should cover stopped timers, large skips,
falls, overlapping routes, long/TP references, Course visibility/editing, record updates and
map changes. Record actual observations separately from compilation results.
