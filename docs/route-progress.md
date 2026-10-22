Route progress flow:

1. Picking a reference (`kz_progress.cpp`), once per mode and course (`PBDataKey`):
   - Candidates are the pro and overall WR (`wrCache`) and the pro and overall SR (`srCache`). Each cached record
     carries its `replayUUID`: the API record ID for WRs, `Times.ID` for SRs.
   - Only replays already on disk are used (`FindReplayPath`: `kzreplays/`, then `kzreplays/downloads/`). Progress never
     downloads anything.
   - Files are read on one background thread (`ReadShortestReference`). Anything that is not a v6 or newer run of this map
     (name and md5), course and mode without styles is skipped.
   - A route is built for every candidate and the shortest one is kept. Leftover fails and walk backs only make a route
     longer, so the shortest is usually the cleanest.
   - A reference is only reloaded when something changes: the record cache gets a different replay UUID
     (`OnRecordCacheUpdated`, `OnRecordCacheCleared`), or a candidate's replay file finishes writing or renaming
     (`NotifyReplayFileChanged`). A failed read is not retried until then. A map change cancels and joins the reader.

2. Building the route (`Route::Build`):
   - Find the run: the header's `start_event_index` and `end_event_index` must point at this course's `TIMER_START` and
     `TIMER_END`. The pre-run buffer can contain other completions of a short course, so the indices are used instead of
     searching for them. Unknown events, events out of order and any other start/stop/end inside the run reject it.
   - Walk the ticks of the run. For each tick: events `BEFORE_PHYSICS`, the pre origin, events `DURING_PHYSICS`, the post
     origin, events `AFTER_PHYSICS`. Noclip or several samples for one tick reject the run.
   - Positions go into a checkpoint tree. Every new position is attached to the current end of the tree:
     - A CP save (`CHECKPOINT_SAVE`) remembers the current node under its checkpoint index.
     - A CP teleport (`TELEPORT_CHECKPOINT` with its index) moves the end of the tree back to that saved node. Whatever
       the player did since the save becomes a dead branch. Scrolling through several CPs in one tick is just several
       moves.
     - Undo (`TELEPORT_CHECKPOINT_UNDO`) moves back to where the last CP teleport came from.
     - Any other teleport (map trigger, bhop fail, unknown) disconnects: the route continues at the destination and the
       teleport itself adds no distance.
   - The final path is the branch from the end of the run back to the start. Failed TP attempts are not on it.
   - Pro fails: a `TELEPORT_BHOP_FAIL` whose destination lies exactly on the earlier path, within the same
     split/checkpoint/stage zone interval, cuts the path back to that point. The player has only been exactly at the
     destination after the first fail there, so the first failed attempt of a section stays. `TELEPORT_MAP_TRIGGER`
     teleports are never treated as fails.
   - Consecutive path points become segments, each with its distance along the route. Segments are indexed in a
     128-unit grid by their midpoint; the few longer than 256 units are kept in a separate list.
   - About a third of map trigger teleports during physics are recorded with the destination as the previous origin.
     The real source is unknown, so no segment is made to it.

3. Following the player (`KZProgressService::OnPhysicsSimulatePost`, every 0.1s, staggered by player slot):
   - On timer start, `Tracker::Start` places the player at distance 0.
   - Follow (`Route::Follow`): only the part of the route around the last known distance is looked at, from the
     distance travelled + 64 behind to 3 × the distance travelled + 128 ahead. The closest point within 256 units of the
     player wins. Moving ahead is allowed more because the reference can take a longer line than the player.
     - Where the route crosses itself, the player stays on the pass they are on.
     - A player on a slightly different line than the reference keeps being followed.
   - Locate (`Route::Locate`): only used after a teleport, or when nothing on the route around the player is within 256
     units. Every segment in the grid around the player is checked. Points within 32 units of the nearest one are
     candidates. If they are spread over more than 256 units of route, the route overlaps itself there, and the one
     closest to the last value is taken. Without a last value, nothing is displayed.
   - When the route is lost, the last value stays for 1 second, then it is hidden.

4. What gets displayed (`Tracker::Update`):
   - The tracker keeps a target, where the player is on the route, and the displayed value, which moves towards it.
   - Following continuously moves the target.
   - A jump, meaning a new position further from the target than the distance travelled + 128, is held. If it is still
     there after 1.5 seconds, it becomes the target. If the player comes back first, it is dropped.
   - The displayed value slides towards the target by at most 2 × the distance travelled + 64 per update. Catching up
     after a lag or after an accepted jump is a slide over a few updates, not a jump.
   - A teleport that sets an origin (`OnTeleport`) shows the next position as is. A CP teleport shows the
     progress the CP was saved with.
   - The value is marked approximate (`~`) when the player is more than 64 units from the route.
   - Finishing hides it until the next timer start. Dead players and replay bots are reset.

---

Scenarios:

1. TP run, the player misses a jump and falls:
   - A fall of less than 256 units keeps being followed at the spot above.
   - A deeper fall lands near an earlier part of the route. That position is held. The player teleports back to the CP
     within 1.5 seconds and the CP's progress is shown. Nothing moved on the HUD.
2. Pro run, the player falls onto a lower floor and keeps going from there:
   - Held for 1.5 seconds, then the value slides down to where the player is.
3. The route goes through the same hub several times:
   - Follow keeps the player on the current pass. Locate is only needed after a teleport, and then picks the pass
     closest to the last value.
4. The player uses a skip the reference didn't:
   - Follow loses the route, Locate finds it further ahead. Held for 1.5 seconds, then the value slides forward.
5. The player takes a completely different line, more than 256 units from the reference:
   - Nothing on the route is close enough. The last value stays for a second, then progress is hidden until the player
     is back near the route.
6. Short course (a few thousand units):
   - One wrong match is a big part of the course, but the slide keeps each update to a small step.

---

Validation:

Tested against 11,527 v6 run replays from the API (222 map/course/mode combinations), with the shortest of the fastest
pro and fastest overall run as the reference. Every run was replayed through the tracker at 10 Hz.

| | Pro runs | TP runs |
|---|---|---|
| No step of 5+ points without a teleport | 99.0% | 99.5% |
| No decline of 5+ points without a teleport | 84% | 89% |
| 95%+ at the finish | 97% | 99% |
| Visible | 96.5% of the time | 98.2% of the time |

CP teleports land within 5 points of the progress the CP was saved with 99.5% of the time.

Not handled:

- One reference line. Players more than 256 units off it get no value there.
- Most pro fails stay in the route: the first bhop fail of a section and every map trigger teleport.
- Only v6 or newer replays already on the server can be references.
