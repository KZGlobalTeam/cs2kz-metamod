Record submission flow:

1. Player finishes the run → `RunSubmission` is constructed.
   - Snapshots all player/run data (name, steamid, mode, course, styles, time, etc.).
   - If `global`: immediately sends `NewRecord` to the API (`SubmitGlobal`).
   - If `!global && local`: immediately inserts into the local DB with `localUUID` (no UUID
     synchronization needed).

2. Concurrently, the replay recorder continues capturing for a short post-run "breather" period
   before it stops. Once the breather ends, serialization runs on a background thread. When done,
   `OnReplayReady` is invoked on the main thread:
   - If the API already responded, eagerly sets `finalUUID` from the API-assigned UUID.
   - Writes the replay to disk asynchronously (`QueueWriteBuffer`).
   - Notifies the player.
   - Calls `TryFinalize()`.

3. `TryFinalize()` (also called from `OnAPIResponse` and each `CheckAll` frame):
   - For `!local && !global` runs: marks `finalized = true` immediately so announcement fires
     right away, but the submission is still kept alive until `replayReady` so `OnReplayReady`
     can perform the disk write and player notification.
   - For `local && !global` runs: finalizes immediately with `localUUID` (UUID can't change,
     replay disk write is already handled independently in `OnReplayReady`).
   - For `global` runs: waits until `replayReady` (replay buffer needed for upload), then also
     waits for the API response, up to `timeout = 5.0s`. Proceeds early if the submission was
     queued (API was offline) or the timeout expires.
   - Once committed (`finalized = true`):
     a. Inserts into the local DB using `finalUUID` (if `local`).
     b. Queues the replay buffer for API upload (if `global && apiResponseReceived`).

4. `CheckAll()` runs every game frame:
   - Calls `TryFinalize()` to advance the state machine.
   - Once `finalized` and both local and global responses are received, fires all
     announcements as a single burst: `AnnounceRun` + `AnnounceLocal` + `AnnounceGlobal`.
   - If responses don't arrive within `announcementTimeout`, announces what's available and
     garbage-collects the submission.
   - Submissions with `pendingQueuedSubmission = true` (API was offline at submit time) are
     kept alive indefinitely; `Clear()` on map change cleans them up.

---

Scenarios:

1. API not available (non-global style/mode, no API key, player not authenticated):
   - `global = false` at construction. `SubmitLocal(localUUID)` is called immediately.
   - No API UUID synchronization; replay written with `localUUID`.

2. API temporarily down at submit time (`Queued` result from `SubmitRecord`):
   - `pendingQueuedSubmission = true`; local DB insert deferred until API UUID arrives.
   - Once the API reconnects and delivers `NewRecordAck`:
     - If already `finalized` (timeout fired): `DoLateAPIResponse` renames the replay file,
       updates the DB UUID, and queues the replay for upload.
     - If not yet `finalized`: `TryFinalize()` runs normally with the API UUID.

3. API response arrived late (after `TryFinalize` already committed with `localUUID`):
   - `TryFinalize` inserts to DB and names the replay file using `localUUID`.
   - When `NewRecordAck` eventually arrives, `DoLateAPIResponse`:
     - Renames replay on disk (`localUUID → apiUUID`).
     - Updates the DB row (`UPDATE records SET uuid = ?`).
     - Queues replay upload from RAM buffer (if still available) or re-reads from disk.

4. Local database not available:
   - `local = false` at construction; DB insert steps are skipped throughout.
