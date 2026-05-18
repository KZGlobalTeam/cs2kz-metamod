# Contributing

This guide assumes you have a working local CS2KZ server installed (see https://docs.cs2kz.org/servers/lan).

## Windows

### Recommended setup

- Install VS Code with the `C/C++` extension
- Install [Visual Studio 2022](https://aka.ms/vs/17/release/vs_community.exe) with atleast these invidual components (or just the whole editor):
    - C++ core features
    - MSVC v143 x64/86
    - Windows 11 SDK
- Install Python 3.3+ and [AMBuild](https://github.com/alliedmodders/ambuild#installation).
- Clone the repo with submodules and symlinks enabled:

```bash
git clone -c core.symlinks=true --recursive https://github.com/KZGlobalTeam/cs2kz-metamod.git
```

### Build

From the repository root:

```bash
mkdir build
cd build
python ../configure.py
ambuild
```

### Dev workflow

- Re-run `ambuild` after code changes.
- Copy files from `build/package/` to your server's `game/csgo/` folder.
- For debugging in VS Code, create a `.vscode/launch.json` file with a launch configuration like the following (alteast replace paths to match your setup):
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(Windows) Launch",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "C:\\cs2-ds\\game\\bin\\win64\\cs2.exe",
            "console": "externalTerminal",
            "args": [
                "-dedicated",
                "-console",
                "+map de_dust2",
            ],
            "logging": {
                "moduleLoad": false,
                "trace": true
            },
            "stopAtEntry": false,
            "cwd": "${fileDirname}",
            "environment": [],
        },
    ]
}
```
- Before making a PR, run formatting (use Git Bash on Windows):

```bash
./scripts/format.sh
```