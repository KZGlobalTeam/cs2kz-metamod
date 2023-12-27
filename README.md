WIP

Protobuf compiler and most utils stuff is based on [CS2Fixes](https://github.com/Source2ZE/CS2Fixes/)

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
	- [ ] Player transparency
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
- [ ] Timers
	- [x] Trigger touching logic
- [ ] *Jumpstats*
	- [x] Distbug
	- [x] Split jump types
 	- [ ] *Console print*
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
- [AMBuild](https://github.com/alliedmodders/ambuild/) needs to be installed for compilation.

- For each platform:
  
Windows (ambuild/msvc): 
```
mkdir build
cd build
python3 ../configure.py 
ambuild
``` 

For windows debugging with VS, build the project then add the following command at the end:
```
python3 ../configure.py --gen=vs --vs-version 17
``` 

Linux (ambuild/clang):
```
mkdir build
cd build
python3 ../configure.py 
ambuild
``` 

Linux (Docker w/ Valve SDK Image):
```
mkdir build
docker build -t cs2kz-linux-builder .
docker run --rm -v ./build:/app/build cs2kz-linux-builder
```

Note: does not work with gcc!

Copy the contents of `build/package/` to your server's `csgo/` directory.
