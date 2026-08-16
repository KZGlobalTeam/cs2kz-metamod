Record submission flow:

1. Player finishes the run → `RunSubmission` is constructed.
   - Snapshots all player/run data (name, steamid, mode, course, styles, time, map name, etc.).
     Everything needed to submit is captured by value here; nothing is re-read from live game or
     map state afterwards.
   - `local = KZDatabaseService::IsReady() && IsMapSetUp()`.
   - Decodes `apiMode` from the run's mode. If the mode isn't a valid global mode, the run can
     never be global.
   - Determines global status from `KZGlobalService::WithCurrentMapState()`:
     - Map **confirmed** and approved → resolves `globalFilterID` by matching the run's course
       name against the API's course list. `global = true` if a course matches.
     - Map **confirmed** and not approved → `global = false`.
     - Map **not yet confirmed** (API reply in flight, or connection down/reconnecting) →
       `mapResolutionPending = true`; the global decision is deferred to `TryFinalize()`.
   - Snapshots previous global PBs (`oldGPB`) unconditionally: it must be read from the live
     course here, since a deferred run may still turn out to be global later, by which point the
     live course may belong to a different map.
   - Kicks off submission:
     - If `mapResolutionPending`: inserts into the local DB immediately with `localUUID`. Only the
       *global* decision waits; the local insert is independent.
     - Else if `global`: immediately sends `NewRecord` to the API (`SubmitGlobal`).
     - Else if `local`: immediately inserts into the local DB with `localUUID` (no UUID
       synchronization needed).

2. Concurrently, the replay recorder continues capturing for a short post-run "breather" period
   before it stops. Once the breather ends, serialization runs on a background thread. When done,
   `OnReplayReady` is invoked on the main thread:
   - If the API already responded, eagerly sets `finalUUID` from the API-assigned UUID.
   - Writes the replay to disk asynchronously (`QueueWriteBuffer`).
   - Notifies the player.
   - Calls `TryFinalize()`.

3. `TryFinalize()` (also called from `OnAPIResponse` and each `CheckAll` frame):
   - **Deferred map resolution** (only while `mapResolutionPending`): re-reads
     `WithCurrentMapState()` every frame. The run is treated as global only if the map is now
     confirmed, approved, **and** its name matches the map this run was played on; a confirmation
     naming any other map never resolves it global. Gives up, downgrading to local-only, once
     `timeout = 5.0s` elapses or the global service becomes permanently unavailable
     (`!MayBecomeAvailable()`). On resolution, calls `SubmitGlobal()` if global; the local row was
     already committed at construction.
   - Note that if the map changes while a run is still pending resolution, the name check 
     takes care of the scenario where the API confirms a different map than the one the run was played on.
   - For `!local && !global` runs: marks `finalized = true` immediately so announcement fires
     right away, but the submission is still kept alive until `replayReady` so `OnReplayReady`
     can perform the disk write and player notification.
   - For `local && !global` runs: finalizes immediately with `localUUID` (UUID can't change,
     replay disk write is already handled independently in `OnReplayReady`).
   - For `global` runs: waits for the API response, up to `timeout = 5.0s`. Proceeds early if the
     submission was queued (API was offline) or the timeout expires.
   - Once committed (`finalized = true`):
     a. Inserts into the local DB using `finalUUID` (if `local` and not already inserted).
     b. Queues the replay buffer for API upload (if `global && apiResponseReceived`).

4. `CheckAll()` runs every game frame:
   - Calls `TryFinalize()` to advance the state machine.
   - Once both sides are ready, fires all announcements as a single burst: `AnnounceRun` +
     `AnnounceLocal` + `AnnounceGlobal`.
   - If responses don't arrive within `announcementTimeout`, announces what's available and
     garbage-collects the submission. This is still a single burst gated on `runAnnounced` — a
     response that lands after the forced announcement does not produce a second, detached
     message, so `runAnnounced` is a true "all announcements are done" latch.
   - A `pendingQueuedSubmission = true` run counts as `globalReady`: its message has not even been
     sent yet and the ack may be minutes away or never arrive, so the run is announced immediately.
     If the ack does arrive later it silently reconciles with the DB and uploads the replay.
   - `OnMapChange()` (called from `Hook_ActivateServer`) bumps `currentMapGeneration`, which
     retroactively mutes every live submission so announcements do not bleed into the next map.

---

Scenarios:

1. Run can never be global (non-global style/mode, no API key, player not authenticated, or the
   map is confirmed not approved):
   - `global = false` at construction. `SubmitLocal(localUUID)` is called immediately.
   - No API UUID synchronization; replay written with `localUUID`.

2. API temporarily down at submit time, but the map is already known to be global (`Queued` result
   from `SubmitRecord`):
   - `pendingQueuedSubmission = true`. `TryFinalize()` treats this as a commit trigger, so the
     local DB row is written right away with `localUUID` rather than waiting for the API.
   - Once the API reconnects and delivers `NewRecordAck`:
     - If already `finalized`: `DoLateAPIResponse` renames the replay file, updates the DB UUID,
       and queues the replay for upload.
     - If not yet `finalized`: `TryFinalize()` runs normally with the API UUID.
   - The run is announced immediately without a global line, so the player is not left waiting on
     a reconnect. If the ack never arrives (queue wiped by a later disconnect) the submission stays
     parked — see the known gap above. Its local row is already committed with `localUUID`; only the
     UUID reconciliation and replay upload are outstanding.

3. API down and the map's global status is unknown: eg. the connection dropped and the map
   changed during the outage, so no `map_change` reply ever arrived for the current map:
   - `mapResolutionPending = true` at construction; the local row is committed immediately with
     `localUUID`, so nothing is lost if the run turns out non-global.
   - `TryFinalize()` polls each frame. If the API reconnects within `timeout` and confirms *this
     run's own map* as approved, the run is submitted globally with the correct filter ID.
   - If the API confirms this map as unapproved, confirms a different map, goes permanently
     unavailable, or simply doesn't answer in time, the run stays local-only.

4. API response arrived late (after `TryFinalize` already committed with `localUUID`):
   - `TryFinalize` inserted to the DB and named the replay file using `localUUID`.
   - When `NewRecordAck` eventually arrives, `DoLateAPIResponse`:
     - Renames replay on disk (`localUUID → apiUUID`).
     - Updates the DB row (`UPDATE records SET uuid = ?`).
     - Queues replay upload from RAM buffer (if still available) or re-reads from disk.

5. Local database not available:
   - `local = false` at construction; DB insert steps are skipped throughout.

6. Map changes while a submission is still in flight:
   - The submission is not destroyed. Its snapshotted data stays valid, `CheckAll()` keeps driving
     it, and a late ack still reconciles the DB row and uploads the replay.
   - `IsFromPreviousMap()` becomes true, so it prints nothing: no run/rank announcement and no
     replay-saved notification. Everything else (DB row, UUID reconciliation, replay disk write
     and upload) still completes normally.
