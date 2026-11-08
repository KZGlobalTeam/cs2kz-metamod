# Persistence

A player who loses their connection, or goes to spectate, keeps a persistent pawn and all of their KZ state.

The pawn is uncontrolable and stays in the world with no input.
It keeps falling, touches triggers, and the timer keeps running until it can be paused.
Coming back restores the pawn's control to the player.

## Away

A player goes "away" when their client dropped or quit on its own with a timer running, or in the middle of a race.

This away state stays until one of the following happens:
- The player reconnects
- The server fills up and the player has been away the longest, so one slot always stays open
- Someone else connects into the held slot
- The same SteamID joins another slot
- The map changes
- Nobody is connected and every away player has been gone for an hour, which makes the server reload the map

When the state is let go, the plugin runs the normal leave and frees the slot.
Kicks and bans never stay.

A player who reconnects from the same address before the engine noticed the drop gets the old connection kicked, which counts as reconnecting.

Normally when the player leaves, CS2 would kill the pawn, move the controller to team 0 (which respawns it) and strip its weapons.
The pawn is protected from the kill, the team change is blocked, and the weapons are given back afterwards by the plugin.
The pawn persists, the engine holds the slot, skipping it for everyone else and handing it back to the same SteamID or IP.
API is notified of the leave, since a leave carries their preferences.
A race keeps them as a participant until the state is let go.

While the player is away, CS2 runs the pawn with an empty command every tick.
The plugin fills in the last known angles, weapon and hand, and runs its per-tick logic around that command.
The run pauses once the pawn stands completely still.

A returning player gets the held slot and controller back.
Their team is hidden while the client activates, which would otherwise respawn the pawn.
The view snaps to the pawn's angles, and only the mode and style convars are resent.
The run is paused if the rules allow it, and the player is told their run was kept.

The engine only treats a client as authenticated once Steam's backend confirms its ticket, and kicks it when Steam rejects the ticket (`sv_steamauth_enforce`).
While Steam is down, a returning player gets the pawn and run back but stays unauthenticated, so a finished run is not saved.
They are warned, and `!gc` shows them as not global.

## Spectate

A player goes to spectate with `jointeam 1` or `!spec`.
The controller views through its observer pawn and joins the spectator team's player list, while the pawn keeps its team.
Every client rebuilds its scoreboard at that point, which would otherwise hide the first spectator it sees.
The plugin keeps the pawn alive and simulates it by hand, the way CS2 runs an away player's pawn.
The run pauses once the pawn stands completely still, unless the player turned off the "Spectate Pause" preference.
The HUD shows the free camera's speed and keys.

Commands behave differently while spectating:
- Teleports (checkpoints, start, end, `!goto`) give the pawn back first.
- Commands that need control (`!cp`, `!setstartpos`, `!noclip`, `!pause`) refuse.
- `!specs` lists only in-eye and chase spectators.

Joining a team hands the pawn back as it is.
Joining the other team switches the pawn's team in place without a respawn, so nothing the map did to the pawn is undone.
The model and weapons of the old team stay until the next spawn.
A paused run resumes, unless the player turned off the "Spectate Resume" preference.

If the pawn dies, the player becomes an ordinary spectator.
A spectator who disconnects gets the pawn back first, then goes away like any other player.

## Visibility

A persistent pawn and its weapons are hidden from every player, its owner included, and only CSTV receives them.
A client cannot draw its own pawn correctly while spectating.
It hides a pawn whose controller it does not know, and freezes one linked to its own controller.
A stand-in controller would break game code that maps controllers to player slots.

`!spec` refuses a persistent pawn, and the game's own observer commands skip it.
`!goto` can still target one, but prefers players in control.

## Replays

Control events (`RPEVENT_CONTROL`, replay version 7) mark when the player stops controlling the pawn and when they return.
Ticks keep recording in between.
The skip button in the replay controls jumps over such a stretch, the same way it jumps over a timer pause.

## Scenarios

- A player whose game crashes and who reconnects a minute later resumes where the pawn is, paused if allowed.
- A player whose game restarts before the server timed out the old connection resumes in the same slot, after the engine kicks the old connection.
- A player who reconnects from a different address while the old connection is still alive gets another slot and loses the run, since the SteamID is playing elsewhere.
- A player who reconnects an hour later on the same map resumes like after a minute, as long as someone else stayed connected.
- When the server fills up, the player away the longest is let go so the next connection succeeds.
- When someone else on the same IP connects, the engine hands them the held slot, and the away state is let go because the SteamID does not match.
- A player who spectates mid-air leaves the pawn falling and touching triggers, and gets it back mid-air.

## Notes

- While spectating, the free camera's commands are still recorded as command data.
- A reconnect from a different address while the old connection is still alive loses the run. Once the old connection has timed out, any address works.
- Anyone else on the away player's IP takes the held slot, even with other slots free.
- A race only ends for an away participant at its time limit, or when the state is let go.
