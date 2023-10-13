WIP

Most utils stuff is based on [CS2Fixes](https://github.com/Source2ZE/CS2Fixes/)

Doesn't work on linux (no sigs)


TODO list: (italic means I'm working on it)

Utilities:
- [x] Print functions (chat, alert, center)
- [ ] *Add functionalities to MovementPlayer class*
- [x] Add chat listener
	- [ ] Add commands to chat listener

KZ-specific:
- [ ] General
	- [x] Extends MovementPlayer to KZPlayer
	- [x] Add MovementAPI stuff with callback registering for KZ modules
	- [x] Disable player collisions
	- [ ] Player transparency
	- [x] Make players invincible
- [ ] Gameplay
	- [ ] Add MOD (requires finished CVar RE)
		- [ ] Buff jump height to 128t values
		- [ ] Interpolate 64t AA to 128
		- [ ] Disable subticks (maybe)
	- [ ] Add mode toggling
- [ ] Checkpoints
- [ ] Timers
	- [x] Trigger touching logic
- [ ] Jumpstats
	- [ ] *Distbug*
	- [ ] Split jump types
- [ ] HUD
	- [ ] *Speed panel*
	- [ ] TP Menu (impossible...?)
- [ ] Quiet
	- [ ] !hide
	- [ ] Sound volumes
- [ ] !goto
- [ ] !measure
	- [ ] Trace functionalities
	- [ ] Draw beams for clients
- [ ] Paint
- [ ] Saveloc
- [ ] Local DB/Ranks
- [ ] Global integration (if API exists)
- [x] Add linux sigs


VS: 
```
mkdir build
cd build
py ../configure.py --hl2sdk-root "../" --gen=vs --vs-version 17
``` 

ambuild:
```
mkdir build
cd build
py ../configure.py --hl2sdk-root "../"
ambuild
``` 
