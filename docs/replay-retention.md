# Replay retention

The plugin decides which replays on the server's disk are kept or deleted.

Replays live in `kzreplays`, and replays downloaded from a store or the API in `kzreplays/downloads`.
The replay watcher scans `kzreplays` every 5 seconds and deletes replay files if necessary.

## Modes

Each server runs in one of three modes:

| Mode                       | When                                                        | The disk keeps                                                                        |
| -------------------------- | ----------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| [Store](#store-mode)       | the `store` block has an `uploadUrl`                        | replays still waiting for an upload, and replays from before the store was configured |
| [Database](#database-mode) | no store, and `db` is configured with the SQL plugin loaded | replays the database references                                                       |
| [Files](#files-mode)       | neither store or database was configured                    | replays the header rules keep                                                         |

The configuration file decides which mode the server will be using.

Every mode keeps downloaded replays for `downloadRetentionDays`.

## Store mode

Every replay recorded while the store is configured is uploaded to the store, then deleted from disk if the upload succeeds.

Replays that were on disk before the store was set up are left alone.

## Database mode

A replay is kept if the database references it.

| Type    | Retention rules                                                                                                                                     |
| ------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| Run     | The run's `Times` row has no styles, is among the player's N fastest runs (or PRO runs) on the course in that mode (N being `disk/maxRunsPerGroup`) |
| Jump    | It has a `Jumpstats` row.                                                                                                                           |
| Manual  | It has a `ManualReplays` row                                                                                                                        |
| Cheater | A `Bans` row has it as `ReplayUUID`                                                                                                                 |

Everything else is marked for eventual deletion.
That covers:

- jumps that weren't saved to the jump top or whose row was pruned or removed, including jumps below `jumpstatsMinSaveTier` recorded for anticheat or debugging,
- runs with styles and runs outside the top `disk/maxRunsPerGroup`,
- runs and manual saves made while the database was unavailable, and cheater replays whose ban couldn't be recorded,
- replays from before the database recorded UUIDs, and replays recorded while the server ran in files mode.

Course top and the profile show each player's fastest run and fastest run without teleports, so they are always kept because `disk/maxRunsPerGroup` is at least 1.

`ManualReplays` holds the newest `maxManualSaves` manual saves of each player.
A manual save adds its row and removes the player's rows beyond that in the same step.
The rows are written whenever a database is configured, also with a store.

The watcher asks the database about the replays on disk once the database is set up and again at every map start, 100 replays per query.
If the database stays unavailable, replays will pile up on disk, nothing will be marked for deletion.

## Files mode

These rules only read replay headers.
Without a database there are no local leaderboards, since jumps aren't saved and runs aren't recorded locally, so the rules only manage disk space.

| Type    | Retention rules                                                                                                                      |
| ------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Run     | It has no styles and is among the player's N fastest runs (or PRO runs) on the map, course and mode (N being `disk/maxRunsPerGroup`) |
| Jump    | It is among the player's N longest jumps or N best block jumps of its jump type and mode (N being `disk/maxJumpsPerCategory`)        |
| Manual  | It is among the player's `maxManualSaves` newest manual saves                                                                        |
| Cheater | Always kept                                                                                                                          |

Everything else is marked for eventual deletion.

## Deletion

A replay marked for deletion is deleted `disk/archiveRetentionMinutes` later if its mode still doesn't keep it by then.
A replay its mode keeps again before that is no longer marked.
With `disk/archiveRetentionMinutes 0` it is deleted at the next scan.

A replay file is also deleted:

- once every upload it needs has succeeded, in store mode,
- once the API has a global run's replay, if `disk/deleteGlobalRunsAfterUpload` is on,
- through `kz_deletejump` or `kz_deletealljumps`.

A file waiting for an upload is never deleted, except when a server command explicitly removes it (eg. `kz_deletejump`).

## Switching modes

| Change                      | What happens                                                                                                                                                                                                                                             |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A store is added            | New replays go to the store. Files already on disk stay as they are until the owner moves them.                                                                                                                                                          |
| Files mode to database mode | The replays recorded in files mode have no rows, so they're marked for deletion and deleted `disk/archiveRetentionMinutes` later. **NOTE**: If server owners wish to preserve these replays, they will need to copy them out _before_ changing the mode. |
| The store is removed        | Pending store uploads are dropped at the next start, and the files left on disk fall under database or files mode.                                                                                                                                       |
| Database mode to files mode | The header rules take over.                                                                                                                                                                                                                              |

## Archive index

`kzreplays/archive_index.txt` records what is left to do for each replay: when it was marked for deletion, which uploads are pending, and whether a confirmed upload means the file can go.
An entry is dropped once there is nothing left to track.

## Options

The options are in the `replays` block of `cfg/cs2kz-server-config.txt`.

| Option                             | Default |                                                                                                                    |
| ---------------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------ |
| `maxManualSaves`                   | 2       | manual saves kept per player                                                                                       |
| `downloadRetentionDays`            | 7       | days to keep downloaded replays (0 = forever)                                                                      |
| `disk/maxRunsPerGroup`             | 3       | database and files mode, minimum 1                                                                                 |
| `disk/maxJumpsPerCategory`         | 3       | files mode only                                                                                                    |
| `disk/archiveRetentionMinutes`     | 2880    | How long a replay marked for deletion will stay until it's actually deleted. This affects database and files mode. |
| `disk/deleteGlobalRunsAfterUpload` | false   | Whether global replays will be deleted from disk after global upload. In store mode this is always on.             |

## Example scenarios

- A player sets a fourth jump in their top 3. The jump top prunes the row of the shortest one, the next map start finds its replay without a row, and it is deleted two days later.
- A player sets 20 runs on a course. The 3 fastest and the 3 fastest without teleports are kept and the others are deleted two days later.
- A faster run on another server that shares the database pushes a run out of a player's top 3. It is marked for deletion on the server that recorded it at its next map start.
- A player makes a third manual save. The oldest one loses its row and is deleted two days later. With a shared database this counts the player's saves on every server.
- MySQL is down while the server starts. Replays stay on disk until the database answers.
- A server has no database. Jumps aren't saved, and their replays are kept by distance and block per player, jump type and mode.

## Notes

- The database is only asked at startup and at each map start, so a replay recorded during a map is never marked for deletion before the next map has started.
- A jump whose database insert failed has no row, so its replay is deleted `disk/archiveRetentionMinutes` after the next map start.
- Removing a jump that another server recorded can't reach that server's disk. Its replay loses its row and is marked for deletion there at the next map start.
