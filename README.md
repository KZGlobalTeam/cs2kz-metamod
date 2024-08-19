WIP, not ready for release 

# Requirements

- [Metamod 2.0.0](https://www.metamodsource.net/downloads.php/?branch=master) build 1284 or later

- Optional: [MultiAddonManager](https://github.com/Source2ZE/MultiAddonManager/releases) for KZ sound effects (eg. jumpstats)

- Optional: [ClientCvarValue](https://github.com/komashchenko/ClientCvarValue/releases) for automatic client language support
  
- Optional: [SQL_MM](https://github.com/zer0k-z/sql_mm/releases) 1.3.3 or later for local database support

# Installation:

- Download the latest version in the release section and extract them to your server's `csgo/` directory.

# Compilation
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

Copy the contents of `build/package/` to your server's `csgo/` directory.
