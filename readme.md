# openblack

[![VCPKG CI](https://github.com/openblack/openblack/actions/workflows/ci-vcpkg.yml/badge.svg?branch=master&event=push)](https://github.com/openblack/openblack/actions/workflows/ci-vcpkg.yml?query=branch%3Amaster+event%3Apush)
[![Packaging CI](https://github.com/openblack/openblack/actions/workflows/packaging.yml/badge.svg?branch=master&event=push)](https://github.com/openblack/openblack/actions/workflows/packaging.yml?query=branch%3Amaster+event%3Apush)
[![GitHub Stars](https://img.shields.io/github/stars/openblack/openblack?logo=github)](https://github.com/openblack/openblack/stargazers)
[![Discord chat](https://img.shields.io/discord/608729286513262622?logo=discord&logoColor=white)](https://discord.gg/5QTexBU)
[![License](https://img.shields.io/github/license/openblack/openblack)](license)

> **This fork: a 1:1 reconstruction of Black & White.** Progress and the research wiki: [docs](docs/README.md) (live progress bars in the viewer, `docs/open_wiki.bat`).

<!-- progress:start -->

### 📊 Progress of the reconstruction

**🎮 Features of the original**

![🟩🟩🟩🟩🟨🟪⬜⬜⬜⬜ 55%](docs/wiki/project/bars/features-all.svg?v=74a05c4d)

6279 of the original game's 13975 behaviours are done and 5 done as the user chose; 2504 partial (10% of the rows' worth, 237 not measured yet); 240 inferred, not yet checked against the original.

**🔀 raffclar's work in our tree**

![🟩🟩⬜⬜⬜⬜⬜⬜⬜⬜ 21%](docs/wiki/project/bars/raffclar-all.svg?v=5dfc5007)

132 of his 783 pull requests are in our tree, 60 more in part.

**🧹 Code clean-up**

![🟩🟩🟩🟩🟩🟩🟩🟧🟧🟧 7 of 10](docs/wiki/project/bars/refactor-all.svg?v=2010eee3)

7 of the 10 code goals are reached; in 3 more, what is left stays on purpose.

**🏁 Latest milestones**

- **2026-10-22** · [The checks read the original's meshes](docs/wiki/project/milestones.md#the-checks-read-the-originals-meshes)
- **2026-10-22** · [Mods: the SDK's version, the Mods page and data mods](docs/wiki/project/milestones.md#mods-the-sdks-version-the-mods-page-and-data-mods)
- **2026-10-21** · [Faster builds](docs/wiki/project/milestones.md#faster-builds)
- **2026-10-21** · [The leashes a creature knows in the state hash](docs/wiki/project/milestones.md#the-leashes-a-creature-knows-in-the-state-hash)
- **2026-10-21** · [The top detail level draws the high meshes, chosen from Maximum Detail](docs/wiki/project/milestones.md#the-top-detail-level-and-maximum-detail)

The detail is on the [progress dashboard](docs/wiki/project/dashboard.md). *This block is written by `docs/tools/progress_stats.py`.*

<!-- progress:end -->

openblack is an open source reimplementation of [Black & White (2001)](https://en.wikipedia.org/wiki/Black_&_White_(video_game)) written in modern C++ and modern rendering engines (OpenGL, Vulkan).

You still need to have the original game assets in order to use this. See further below for an explanation on how to extract them.

---

<a href="https://user-images.githubusercontent.com/32263167/184559293-56cfc6a7-a7da-4876-8fce-434ba8827eae.png">
    <img src="https://user-images.githubusercontent.com/32263167/184559293-56cfc6a7-a7da-4876-8fce-434ba8827eae.png" width="45%">
</a>
<a href="https://user-images.githubusercontent.com/32263167/184559291-f37e44b3-c738-4c0f-82eb-28b7c84a3919.png">
    <img src="https://user-images.githubusercontent.com/32263167/184559291-f37e44b3-c738-4c0f-82eb-28b7c84a3919.png" width="45%">
</a>

# Pre-built Binaries

For the latest build of openblack without having to build it yourself, we offer pre-built binaries from our test service.
These links are upstream openblack's builds of its main branch and do not include this fork's changes; to run this
fork, build it from source (see [Building](#building)).

[![Linux (amd64) Build](https://img.shields.io/badge/Build-Linux%20(amd64)-333333)](https://nightly.link/openblack/openblack/workflows/ci-vcpkg/master/openblack-ubuntu-24.04-master.zip)
[![OSX (amd64) Build](https://img.shields.io/badge/Build-OSX%20(amd64)-eeeeee)](https://nightly.link/openblack/openblack/workflows/ci-vcpkg/master/openblack-macos-13-master.zip)
[![Windows (amd64) Build](https://img.shields.io/badge/Build-Windows%20(amd64)-0078d4)](https://nightly.link/openblack/openblack/workflows/ci-vcpkg/master/openblack-windows-latest-master.zip)

[![Arch Linux](https://img.shields.io/badge/Package-Arch%20Linux-1793d0)](https://nightly.link/openblack/openblack/workflows/packaging/master/openblack-arch-linux-pkg-master.zip)

## Experimental

The following are qualified as experimental. They have base minimum setup such as compilation and rendering system.
Don't expect to be able to launch without some effort and your own patches.

[![Windows (x86) Build](https://img.shields.io/badge/Build-Windows%20(x86)-0078d4)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-x86-windows-master.zip)
[![Windows (arm64) Build](https://img.shields.io/badge/Build-Windows%20(arm64)-0078d4)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-arm64-windows-master.zip) 

[![Android](https://img.shields.io/badge/Package-Android-3ddc84)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-android-apk-master.zip)
[![iOS](https://img.shields.io/badge/Package-iOS-eeeeee)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-ios-arm64-vcpkg-master.zip)

[![Linux (x86) Build](https://img.shields.io/badge/Build-Linux%20(x86)-333333)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-x86-linux-master.zip)
[![Linux (arm64) Build](https://img.shields.io/badge/Build-Linux%20(arm64)-333333)](https://nightly.link/openblack/openblack/workflows/ci-cross-compile/master/openblack-arm64-linux-master.zip)
[![Flatpak](https://img.shields.io/badge/Package-Flatpak-79ADE3)](https://nightly.link/openblack/openblack/workflows/packaging/master/openblack-master.flatpak.zip)
[![AppImage](https://img.shields.io/badge/Package-AppImage-00BBFF)](https://nightly.link/openblack/openblack/workflows/packaging/master/openblack-master.AppImage.zip)
[![Snap](https://img.shields.io/badge/Package-Snap-E95420)](https://nightly.link/openblack/openblack/workflows/packaging/master/openblack-master.snap.zip)


# Building

Clone this fork's code using: `git clone --recursive --branch AI-Decompiled https://github.com/diegoscood-ai/openblack.git`
(upstream openblack is `https://github.com/openblack/openblack.git`).

If you forgot to add `--recursive`, you will have to also run `git submodule update --init` from the `openblack/` directory.

## Configuration for using vcpkg (Recommended for new users)

The simplest way to obtain all the required dependencies is through [vcpkg](https://github.com/Microsoft/vcpkg) which is included with a [manifest file](https://github.com/microsoft/vcpkg/blob/master/docs/users/manifests.md).

The easiest way to get started on any platform is to allow CMake and vcpkg to handle all dependencies and configuration
for you. To do so, you will be selecting the `"ninja-multi-vcpkg"   - Ninja Multi-Config (vcpkg)` preset. Other presets are available for more advanced users.

### Visual Studio Code
 * Install [Visual Studio Code](https://code.visualstudio.com/Download)
 * Install the [C++ Extension](vscode:extension/ms-vscode.cpptools)
    * The extension might not be required on Linux
    * Install a compiler depending on your platform
        * GNU or Clang compilers on Linux
        * Visual Studio's MSVC on Windows (See Visual Studio)
        * Clang compiler on Mac
 * Install the [CMake Tools Extension](vscode:extension/ms-vscode.cmake-tools)
     * Install [CMake version 3.20+](https://cmake.org/download/)
 * You can simply [open the `openblack` folder directly in Visual Studio Code and select a preset](https://devblogs.microsoft.com/cppblog/cmake-presets-integration-in-visual-studio-and-visual-studio-code/).

### Visual Studio 2022
* Install [Visual Studio](https://visualstudio.microsoft.com/downloads/)
    * Select an appropriate MSVC C++ Component for your version
    * Select an appropriate Windows SDK Component for your version
    * Select the C++ CMake tools for Windows Component
* You can simply [open the `openblack` folder directly in Visual Studio and select a preset](https://devblogs.microsoft.com/cppblog/cmake-presets-integration-in-visual-studio-and-visual-studio-code/).

### CLion
* Install [CLion](https://www.jetbrains.com/clion/download/)
* You can simply [open the `openblack` folder directly in CLion and select a preset](https://www.jetbrains.com/help/clion/cmake-presets.html).

### XCode
* Install [XCode](https://apps.apple.com/ca/app/xcode)
* Install the homebrew dependencies using [homebrew](https://brew.sh/):  `brew install cmake ninja pkg-config`
* Run `sudo xcode-select --reset` after installing cmake
* You need to configure using the cmake preset `xcode-vcpkg` using cmake in either the GUI or using the command line argument `--preset xcode-vcpkg`

### Android Studio
* Install [Android Studio](https://developer.android.com/studio)
* Install a recent SDK and an NDK of at least 23
* Open openblack from the `android` directory
* Set the SDK and NDK in the project settings
* Do gradle configure which will run cmake configuration and compile the vcpkg dependencies
* Build the project for either a virtual device or a connected device
* Currently requires user to upload game assets to `/data/local/tmp/bw` and `chmod -R a+wrx /data/local/tmp/bw` with adb

### Web Assembly

* Get [emsdk](https://github.com/emscripten-core/emsdk)
* Add the directory of emsdk as the `EMSDK` environment variable.
* Run `emsdk install latest`
* Run `emsdk activate latest`
* Build the `emscripten-vcpkg` preset

### Command Line
* Your usual build tool-chain.
    * Ubuntu / Debian: `# apt install build-essential cmake ninja-build`
    * Arch Linux / Manjaro: `# pacman -S base-devel cmake ninja`
* You can generate the cmake build preset using `cmake --preset` and you can list the presets using `cmake --list-presets`

## Configuration for using System Dependencies (Recommended for packagers)

If you don't want to use vcpkg; CMake will use system libraries, or manually specified package directories.

CMake will find the required libraries as long as they provide a [proper config file](https://cmake.org/cmake/help/latest/guide/using-dependencies/index.html#libraries-providing-config-file-packages).

### Arch Linux

Install [openblack-git](https://aur.archlinux.org/packages/openblack-git/) from the AUR which builds and installs directly for all Arch Linux derived distros such as Manjaro. This will require pacman to install all required dependencies.

The preset to use is `"linux-system-deps"   - Linux (System Deps)`


## Contributing
Contributions are always welcome, whether it's modifying source code to add new
features or bug fixes, documenting new file formats or simply editing some
grammar.

You can also join the [Discord for development discussion](https://discord.gg/5QTexBU)
if you are unsure of anything.

This fork's code follows the conventions in [docs/wiki/project/refactor-conventions.md](docs/wiki/project/refactor-conventions.md): state lives in ECS
components or Locator services, assets are loaded through the resource caches, and comments describe behaviour in plain
English. [docs/wiki/project/testing.md](docs/wiki/project/testing.md) describes how a change is verified against the previous
build, and [CHANGELOG.md](CHANGELOG.md) lists what changed. What is known about the original game is in the research
wiki, [docs/wiki](docs/wiki/project/reading-the-wiki.md), one page per game system. To read it with its contents list,
search and progress dashboard, open the viewer page `docs/index.html` through `docs/open_wiki.bat` (or
`python docs/serve.py`); on GitHub the pages read as plain Markdown.

# Extracting asset files from original game

As mentioned before, the original game assets are required in order to run
openblack. Since these data are copyrighted, **we won't provide them**, so please
don't ask. Here is a guide for extracting them from the original game.

## Prerequisites

You will need the following:

- The original installer (usually on a CD)
- [Official patch 1.10](https://www.bwgame.net/downloads/v1-1-patch.129/)
- [Official patch 1.2](https://www.bwgame.net/downloads/v1-2-patch.130/)
- Not needed (but will not compromise the extraction): [Unofficial patch 1.42](https://www.bwgame.net/downloads/black-white-unofficial-patch-v1-42.1418/)

**Do not install 1.3, it is a patch to add P5 Power Glove support and is generally unstable**

## Installation

### On Windows

1. Install the game from the installer, remember the installation directory
(`C:\Program Files (x86)\Lionhead Studios\Black & White\` is the default)
2. Install the official patch 1.10 (in the same directory)
3. Install the official patch 1.2 (in the same directory)

### On Linux and macOS

If you run on Linux or macOS, you can install the game with Wine
([App Page on WineHQ](https://appdb.winehq.org/objectManager.php?sClass=application&iId=156)).

1. Install `wine`
2. Run `wine Setup.exe` from the directory containing the installer, don't
   change the install path
3. Run `wine Black_White_Patch_v1.100.exe` from the directory containing the
   first patch (adapt the command in case the filename isn't exactly that)
4. Run `wine black_white_patch_v1_20.exe` from the directory containing the
   second patch (adapt too)
5. In case you would like to install the unofficial patch, it requires .NET 2.0,
   that can be installed with `winetricks dotnet20sp2` (you should install
   `winetricks` first)

All the files should be installed in the following directory:
`~/.wine/drive_c/Program Files (x86)/Lionhead Studios Ltd/Black & White`

## Getting the assets

The folders you're interested in are the `Audio`, `Data` and `Scripts` folders in the
installation directory. You can use them from there or make a copy to the
location of your choice.

### Using them in place

If you want to use them from there, remember the installation directory chosen
in the installation step, unless you changed it, it should be either:

- either `C:\Program Files (x86)\Lionhead Studios\Black & White\`
- or `~/.wine/drive_c/Program Files (x86)/Lionhead Studios Ltd/Black & White`

This is the path you will give to openblack (using the `-g` flag).

If you prefer to copy the data (e.g. if you want to uninstall the original
game), just copy the `Audio`, `Data` and `Scripts` folders to the desired new location,
and give the path to this location to openblack (using the `-g` flag).

# License

openblack is released as open source software under the [GPL v3](https://opensource.org/licenses/gpl-3.0.html)
license, see the [license](./license) file in the project root for the full license text.
