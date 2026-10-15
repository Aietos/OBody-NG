# OBody

OBody is a mod for The Elder Scrolls V: Skyrim Special Edition which distributes all installed bodyslide presets randomly to NPCs for greater body variety. It also lets you manually select which bodyslide preset you want for a given NPC or your player character.

## Table of Contents

- [Building](#building)
- [Versioning and Git builds](#versioning-and-git-builds)
- [AI Usage Policy](#ai-usage-policy)

## Building

### Requirements
- [xmake](https://xmake.io/)
- [vcpkg](https://github.com/microsoft/vcpkg) (don't forget to set the Windows environment variable `VCPKG_ROOT` pointing to the folder where vcpkg is installed)
- [Pyro](https://github.com/fireundubh/pyro/releases) (download a release and unzip it anywhere you want on your computer)
- [Skyrim SE Creation Kit](https://store.steampowered.com/app/1946180/Skyrim_Special_Edition_Creation_Kit/) so you have the Papyrus Compiler. Don't forget to unzip the `Scripts.zip` folder it comes with, which is put inside your Skyrim Data folder

### Setup
- Clone this repository
- Open the terminal, switch into the repository's folder and fetch submodules: `git submodule update --init --recursive`
- In the `pyro.config.json` file of this repository, set the `pyro_path` to where `pyro.exe` that you downloaded above resides. `game_path` is optional, you should only need to set it if Pyro has trouble finding the game folder. Don't commit changes to this file. Example of how `pyro.config.json` may look like:
```json
{
  "pyro_path": "C:\\Skyrim Projects\\pyro-master-1656807840\\pyro.exe",
  "game_path": ""
}
```
- Generate a Github personal access token with `public_repo` access scope and create a `.secrets` file in the same folder where `pyro.exe` resides. This is needed for Pyro to fetch the OBody Papyrus dependencies that reside in Github repositories. For more information, see [Pyro Remotes](https://wiki.fireundubh.com/pyro/remotes). The `.secrets` file should look something like this:
```ini
[github.com]
access_token = your_access_token
```

From here you have several options depending on what you're looking to do:
- `xmake papyrus` - compiles only the Papyrus scripts
- `xmake build` - builds only the DLL and PDB files
- `xmake dist --skip-dll` - compile only the Papyrus scripts and make `.zip` package to install with mod manager. It will reuse an already existing DLL file if there is any
- `xmake dist --skip-papyrus` - compile only the DLL, skipping Papyrus scripts compilation, and make `.zip` package to install with mod manager. It will reuse the already existing compiled `.pex` scripts for the `.zip` package
- `xmake dist` - builds the entire project (compile Papyrus scripts + compile DLL + make .zip package to install with mod manager)

The `.zip` package created by `xmake dist` commands will be created in `contrib\Artifact`.

For most cases or if you are unsure, `xmake dist` is what you should be using, since it does all the work and necessary packaging for you.

You can compile the Papyrus scripts in the Creation Kit if you want to. However, Pyro is preferred since it anonymizes scripts properly (Papyrus compiler includes metadata in the compiled scripts that may have information about your computer and Windows username). Besides that, Pyro is also much faster and lightweight.

If you add new files or headers to the project, don't forget to run `xmake project -k cmakelists`.

## Versioning and Git builds

OBody uses semantic versioning. When making a PR, upgrade the version in `xmake.lua`, `cmakelists.txt` and `obody.ppj` taking into account whether it should be a patch, minor or major version upgrade.

Failing to upgrade version means that the Git CI build automatic release will fail, because there was already a version built with the current version number.

## AI Usage Policy

OBody is an open source project and a community effort. You are free to do whatever you want with this mod as long as you follow the terms of its license (GPL-3), and contributions are very welcome!

However, you should keep in mind that this project follows certain policies regarding the usage of AI when you make a PR and commits to this project.

- Any and all usage of AI must be disclosed. You must state the tool you used (e.g. Claude, Codex, etc) and to what extent your code was AI assisted or generated. Be as detailed as possible in the usage of AI.

- You must understand the code that you are submitting. This also means that you must be able to explain the code. If you have your AI tool explain the code on top of generating it, your contribution will be rejected. AI is a powerful tool when used correctly, but if you have it blindly generate the code without understanding it, this will result in code that is difficult/impossible to maintain, and which will make bugs surface in the project that no one will know how to fix.

- The above point means that PR descriptions must be written by yourself. PR descriptions written by AI will result in the contribution being immediately rejected.

- You must test the code that you submit and make sure that it's working. Even if you had AI write a specific part of the code, you must also test any and all scenarios that may run into those code sections.

- If you submit a PR or commits that are hundreds/thousands of lines of code fully generated by AI, it will most likely get rejected. Reviewing such large contributions meaningfully is straight up impossible and puts a lot of burden into the people doing reviews. Anyone working on this project is doing it on their free time, which is precious and hard to come by. Please be mindful of this.

- Ultimately, it is up to the person reviewing the code if the contribution will be accepted, AI assisted/generated or not.

In short, this project isn't an anti-AI zealot. Usage of AI is inevitable, and can be beneficial if used cautiously and properly. This project is not against AI assistance within reason, and as long as the human(s) involved know what they are doing and delegating to AI. But blind usage of AI isn't welcome here. While this is a mod for Skyrim and not critical software, we want to maintain some semblance of code quality and a maintaneable code base for contributors to come in the future.
