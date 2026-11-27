# Replay store

Server owners can optionally configure their servers to use a remote replay store through HTTP instead of depending on local disk for replays.
Every replay recorded from then on is uploaded to the store, and the server's disk only holds the ones that haven't been uploaded yet, plus a cache of downloaded ones.
Replays already on disk before the store is set up are left alone.

The store is required to support `PUT`, `GET` and `HEAD` methods.
A custom store can add a query endpoint, which lets `!replays` and the menus see replays that aren't on the server's disk.

The plugin never deletes anything from the store.
This document only describes the plugin's behavior and its interaction with the store. The store's own logic, such as its retention policy, is outside of the plugin's scope.

## Configuration

The store is set up in the `store` block of the `replays` block in `cfg/cs2kz-server-config.txt`.
A `store` block with an `uploadUrl` turns it on.

```
"replays"
{
	"store"
	{
		"uploadUrl"		"https://replays.example.com/kz/{map}/{id}.replay"
		"downloadUrl"	"https://replays.example.com/kz/{map}/{id}.replay"
		"queryUrl"		""
		"auth"			""
		"includeGlobal"	"true"
	}
}
```

| Option          | Description                                                                                                                                     |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `uploadUrl`     | Where replays are uploaded with `PUT`.                                                                                                          |
| `downloadUrl`   | Where replays are downloaded with `GET` and checked with `HEAD`. Without it the server uploads but never reads from the store.                  |
| `queryUrl`      | The optional query endpoint.                                                                                                                    |
| `auth`          | Sent unchanged as the `Authorization` header of every request, for example `Basic a3o6c2VjcmV0` or `Bearer ...`. Nothing is sent if it's empty. |
| `includeGlobal` | Whether to also upload global runs the API already has. With `false` a global run is only uploaded if its API upload fails.                     |

`{id}` is the replay's UUID and `{map}` the name of the map it was played on.
They are the only placeholders, because a download only ever knows those two.
`{map}` is percent-encoded.

## Replay files

A replay file starts with a little-endian u32 length, followed by that many bytes of the protobuf `ReplayHeader` from `protobuf/kz_replay.proto`, followed by the tick data.
A store that wants metadata such as the player, map, course, mode or time can read it from the header.
The plugin uploads the file exactly as it is on disk.

## Required

### Upload

```
PUT <uploadUrl>
Authorization: <auth>
Content-Type: application/octet-stream

<replay file>
```

Any 2xx means the store has the replay.
So do `409` and `412`, because replay UUIDs are unique: a store that refuses to overwrite an existing file answers a repeated upload with one of them.
Uploading the same URL again is always safe.

Anything else is retried, and uploads never give up:

- After a 5xx, a `408`, a `429` or no response, the plugin waits 2 seconds, 10 seconds, a minute and then 5 minutes between attempts.
- After any other status, the plugin retries every hour and logs a warning. These usually mean a setup problem such as wrong credentials or a size limit, which the owner can fix without losing replays.

At most 2 uploads run at once.

### Download

```
GET <downloadUrl>
Authorization: <auth>
```

A `200` with the replay file as its body means the store has it.
Anything else means it doesn't.

### Availability

```
HEAD <downloadUrl>
Authorization: <auth>
```

`200` means the store has the replay.
Anything else means it isn't there yet.

## Optional: query endpoint

An endpoint dedicated to searching replays can be configured. It needs to support all of the parameters below for `!replays` to work properly.

```
GET <queryUrl>?<parameters>
Authorization: <auth>
```

Parameter values are percent-encoded.
All string matching is case-insensitive.

### Lookups by ID

These cover every replay type.

| Parameter                     | Matches                               |
| ----------------------------- | ------------------------------------- |
| `id`, repeated up to 50 times | replays with exactly these UUIDs      |
| `idsubstring`                 | replays whose UUID contains this text |

### Searches

The parameter names are the arguments `!replays` takes in chat.
`type` is always sent.

| Parameter      | Applies to | Matches                                                                  |
| -------------- | ---------- | ------------------------------------------------------------------------ |
| `type`         | all        | `run`, `jump`, `manual` or `cheater`                                     |
| `map`          | all        | the exact map name, or map names containing it if no map matches exactly |
| `player`       | all        | player names containing it                                               |
| `steamid`      | all        | the player's SteamID64                                                   |
| `course`       | run        | course names containing it                                               |
| `mode`         | run, jump  | mode names or short mode names containing it                             |
| `maxtime`      | run        | a time of at most this many seconds                                      |
| `maxteleports` | run        | at most this many teleports                                              |
| `numstyles`    | run        | exactly this many styles                                                 |
| `jumptype`     | jump       | this jump type number                                                    |
| `mindistance`  | jump       | a distance of at least this                                              |
| `saver`        | manual     | names of the player who saved it containing it                           |
| `saversteamid` | manual     | the SteamID64 of the player who saved it                                 |
| `reason`       | cheater    | reasons containing it                                                    |

A missing parameter doesn't filter.
`offset` and `limit` page through the matches, and the plugin never asks for more than 50.
Runs are sorted fastest first and everything else by UUID, which is oldest first because the UUIDs are v7.

### Response

```json
{
	"total": 132,
	"replays": [{ "id": "019a...", "header": "<base64>" }]
}
```

- `total` counts every match, ignoring `offset` and `limit`.
- `header` is the base64 of the start of the replay file: the u32 length and the header bytes. Both the standard and the URL-safe alphabet work, with or without padding.
- Entries that can't be read are skipped.
- Any status other than `200` counts as a failed search.

The plugin runs every returned header through its own filters and drops what doesn't match, so a store can't show the wrong replays.

### What uses it

| Feature                                            | With a query endpoint                                          | Without                                 |
| -------------------------------------------------- | -------------------------------------------------------------- | --------------------------------------- |
| `!replays` and the replays window                  | the store's replays after the ones on disk                     | only the replays on disk                |
| `!replay <partial UUID>`                           | `idsubstring` if nothing on disk matches                       | only replays on disk                    |
| replay buttons in `!ctop`, `!profile` and `!jstop` | one query per page, and jump top learns the map of each replay | one `HEAD` per row, at most 4 at a time |

## What the server uploads

Every replay recorded after the store is set up is uploaded.
Replays already on disk at that point aren't uploaded or deleted.

| Type                              | Uploaded once                                                                                                       |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Jump, manual and cheater          | it is written to disk                                                                                               |
| Run                               | the run's submission is done (a global run's replay is renamed to the UUID the API gives it so waiting is required) |
| Global run the API never answered | the submission gives up, or at the next start of the server, under the UUID it has                                  |

A replay's file is deleted from the server once every upload it needs has succeeded: the store's, and the API's for a global run.
Pending uploads are saved in `kzreplays/archive_index.txt` and resume after a restart.
Note that API uploads don't resume after a restart, so they count as failed.

## Replay searching

To play a replay, the server looks for it in this order:

- its replay folder and download folder,
- the store, downloading it from there,
- the global API (`replayDownloadUrl`).

Downloads are saved to `kzreplays/downloads` and kept for `downloadRetentionDays`.
`{map}` is the current map when playing, because a replay can only be played on its own map.

The replay buttons of the menus check the disk first and then ask the store.
A replay the store has is remembered for 10 minutes. Missing replays do not get cached the same way (the server will re-query the store every time).

The progress bar in course info UI downloads the server record and world record replays it doesn't have.

## Example scenarios

- A player sets a run on a server with a store. The replay is written to disk, uploaded once the submission is done, and deleted from disk once the store confirms it.
- The store is down for a day. Replays pile up on disk and upload once it is back.
- The store rejects uploads because `auth` is wrong. Replays stay on disk, a warning is logged every hour, and they upload once `auth` is fixed.
- A player opens `!ctop` for a run recorded on another server that shares the database and the store. The row gets a replay button once the store answers.
- A player plays a replay that is only in the store. It is downloaded to `kzreplays/downloads` and played from there.
- A store is added to a server that already has replays on disk. New replays are uploaded and the old ones stay on disk until the owner moves them.
- A player opens `!ctop` while another server's upload of a replay is still running. The row has no button, and it gets one the next time the menu asks once the upload is done.

## Notes

- Without a query endpoint, jump top can't tell which map a stored replay is from and looks it up under the current map. With `{map}` in `downloadUrl`, a replay of another map isn't found and gets no button. Without `{map}`, it gets a button and loading it gives the usual wrong map message.
- A store without a `downloadUrl` is only a backup: the server uploads to it but never reads from it.
- Removing the `store` block stops the uploads that are still pending at the next start of the server. Their replays and any replays the owner didn't move then fall under the server's [database or files mode](replay-retention.md).
