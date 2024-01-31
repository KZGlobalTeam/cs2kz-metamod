WIP

Requirements: Metamod

Compilation:
- Remember to *recursively* clone the plugin, and symlink needs to be enabled as well!
- Latest [AMBuild](https://github.com/alliedmodders/ambuild/) needs to be installed for compilation.

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

Usage:

Copy the contents of `build/package/` to your server's `csgo/` directory.

For CS# compatibility: Add `addons/cs2kz/bin/win64/cs2kz`/`addons/cs2kz/bin/linuxsteamrt64/cs2kz` into Metamod's metaplugins.ini.
