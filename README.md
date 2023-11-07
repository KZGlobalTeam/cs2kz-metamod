WIP, project is **not ready** for release

Most utils stuff is based on [CS2Fixes](https://github.com/Source2ZE/CS2Fixes/)

TODO list: (italic means I'm working on it)

Utilities:
- [x] Print functions (chat, alert, center)
- [x] Add functionalities to MovementPlayer class
- [x] Add chat listener
	- [ ] *Add commands to chat listener*

KZ-specific:
- [ ] General
	- [x] Extends MovementPlayer to KZPlayer
	- [x] Add MovementAPI stuff with callback registering for KZ modules
	- [x] Disable player collisions
	- [ ] Player transparency (not implemented, looks ugly)
	- [x] Make players invincible
- [ ] Gameplay
	- [ ] Add MOD (requires finished CVar RE)
		- [ ] Buff jump height to 128t values
		- [ ] Interpolate 64t AA to 128
		- [ ] Disable subticks (maybe)
	- [ ] Add mode toggling
- [ ] !ssp
- [x] Checkpoints
	- [x] Add basic checkpoints
	- [x] Add ladder checkpoints
- [x] Timers
	- [x] Trigger touching logic
- [x] *Jumpstats*
	- [x] Distbug
	- [x] Split jump types
 	- [x] *Console print*
  	- [ ] Invalidate various scenarios
- [ ] HUD
	- [x] Speed panel
 		- [x] Add prespeed and keys
 		- [x] Add print to the top center bar (with game events)
	- [ ] TP Menu (impossible...?)
- [ ] Quiet
	- [ ] !hide
 		- [x] Hide player models
   		- [ ] Hide player sounds
	- [ ] Sound volumes
- [ ] !goto
- [ ] !measure
	- [ ] Trace functionalities
	- [ ] Draw beams for clients
- [ ] Paint
- [ ] Add welcome message and help commands
- [ ] Saveloc
- [ ] Local DB/Ranks
- [ ] Global integration (if API exists)
	- [ ] HTTP Requests
 	- [ ] UDP listener (partially done)
- [x] Add linux sigs
- [ ] Docs
	- [ ] Code style (different naming schemes for RE structs, sdk, third party libs, project code)
Requirements: Metamod plugin

Compilation:
- Remember to *recursively* clone the plugin!
- For each platform:
  
Windows (VS): 
```
mkdir build
cd build
py ../configure.py --hl2sdk-root "../" --gen=vs --vs-version 17
``` 

Linux (ambuild/clang):
```
mkdir build
cd build
py ../configure.py --hl2sdk-root "../"
ambuild
``` 

Note: does not work with gcc!
