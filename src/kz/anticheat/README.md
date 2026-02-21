# Anticheat

Each detection method is implemented separately in its own file under the `detectors` folder.

## Post-detection flow

After a player is marked as cheating via `KZAnticheatService::MarkInfraction(...)`, the flow is:

1. **Create pending infraction**
	- Ignore duplicate processing if the player is already marked banned.
	- Create a pending infraction entry with type, details, and SteamID.
	- Mark the player as banned for the current session (`isBanned = true`).

2. **Attach replay evidence**
	- If the infraction type requires replay evidence, create a `CheaterRecorder` and attach it to the pending infraction.

3. **Submit global infraction** (WIP)
	- Attempt global submission first.
	- The API is responsible for the ban duration along with the ban ID + replay ID associated with this ban.
	- If global submission fails/unavailable, use fallback behavior and continue local processing.

4. **Submit local ban**
	- Apply type-based ban duration policy.
	- If local DB is available and duration applies, write the local ban record.

5. **Save replay (if any)**
	- Queue replay write and keep replay UUID metadata for tracking.

6. **Finalize enforcement**
	- Finalization marks the infraction as submitted.
	- If the player is still online, they are kicked using the infraction-mapped disconnect reason/internal label.

## Auth and backend availability scenarios

The anticheat services behaves differently depending on different availability scenarios of the local DB and the global API.

### If marked before Steam auth

- `MarkInfraction(...)` immediately kicks with `Invalid Steam Logon` and returns.
- No pending infraction is created.
- No replay, API submission, or local DB ban write is attempted.

### If marked after Steam auth, before global API auth is finished

- A pending infraction is created immediately (`isBanned = true`), with replay attached when required by type.
- Submission starts right away through `SubmitGlobalInfraction()`.
- Later, when global auth finishes:
	- If global says the player is already banned, the pending infraction is canceled (replay cleared, pending marked submitted), and autokick/message behavior follows config.
	- If global says not banned, the existing pending infraction continues through submission/finalization.

### If marked after both auths are already finished

- Flow is the same as normal post-detection.

### Backend availability when player is marked

- **Only local DB available**
	- Global submission falls back to local policy/duration.
	- Local DB ban write is attempted.
	- Finalization kicks the player.

- **Only API available**
	- On API success, API-provided ban metadata is applied.
	- Local write is skipped if DB is unavailable.
	- Finalization still kicks the player.

- **Both API and local DB available**
	- API submission is attempted first.
	- API response (ban id/duration/replay id) is applied.
	- Local DB ban write is then attempted using that infraction data.
	- Finalization kicks the player.

> Note: In the current code, global submission is stubbed and routes to failure fallback (`OnGlobalSubmitFailure()`), so runtime behavior currently follows the local-fallback path.

