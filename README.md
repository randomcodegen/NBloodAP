## APWorld Setup:
- Put duke3d.apworld in C:\\ProgramData\\Archipelago\\custom_worlds\\
- Start the Launcher and click "Generate Template Options" to create the Duke Nukem 3D.yaml file
- Modify it and put the .yaml file in C:\\ProgramData\\Archipelago\\players\\
- Run ArchipelagoGenerate.exe and then ArchipelagoServer.exe

## Client Setup:
- Extract NBloodAP.zip into a folder (e.g. .\\NBloodAP\\ )
- Launch the .exe and enter your connection details

## Adaptations for Archipelago Support

* Only supports Windows x64 builds right now
* Building with Visual Studio:
  * Clone APCpp lib and dependencies: git submodule update --init --recursive
  * Generate Visual Studio solution: cmake APCpp -B platform/Windows/APCpp -G "Visual Studio 17 2022" -A x64
  * Build rednukem project as Debug/Release x64 variant
* Game specific APWorld implementations:
  * Duke Nukem 3D 1.5 Atomic Edition:
    * https://github.com/LLCoolDave/Duke3DAP
   

# Original Project Readme:

# NBlood / Rednukem / PCExhumed
Reverse-engineered ports of Build games using EDuke32 engine technology and development principles

## NBlood
Blood port based on EDuke32

### Installing
1. Extract NBlood to a new directory
2. Copy the following files from Blood 1.21 to NBlood folder:
   * BLOOD.INI
   * BLOOD.RFF
   * BLOOD000.DEM, ..., BLOOD003.DEM (optional)
   * CP01.MAP, ..., CP09.MAP (optional, Cryptic Passage)
   * CPART07.AR_ (optional, Cryptic Passage)
   * CPART15.AR_ (optional, Cryptic Passage)
   * CPBB01.MAP, ..., CPBB04.MAP (optional, Cryptic Passage)
   * CPSL.MAP (optional, Cryptic Passage)
   * CRYPTIC.INI (optional, Cryptic Passage)
   * CRYPTIC.SMK (optional, Cryptic Passage)
   * CRYPTIC.WAV (optional, Cryptic Passage)
   * GUI.RFF
   * SOUNDS.RFF
   * SURFACE.DAT
   * TILES000.ART, ..., TILES017.ART
   * VOXEL.DAT

3. Optionally, if you want to use CD audio tracks instead of MIDI, provide FLAC/OGG recordings in following format: bloodXX.flac/ogg, where XX is track number. Make sure to enable Redbook audio option in sound menu.
4. Optionally, if you want cutscenes and you have the original CD, copy the `movie` folder into NBlood's folder (the folder itself too).
If you have the GOG version of the game, do the following:
   * make a copy of `game.ins` (or `game.inst`) named `game.cue`
   * mount the `.cue` as a virtual CD (for example with `WinCDEmu`)
   * copy the `movie` folder from the mounted CD into NBlood's folder
5. Launch NBlood (on Linux, to play Cryptic Passage, launch with the `-ini CRYPTIC.INI` parameter)

### Notes
NBlood now uses nblood_cvars.cfg instead of settings.cfg. Please rename your settings.cfg if you need to retain settings from a previous version.

## PCExhumed
A port of the PC version of Exhumed based on EDuke32

### Installing
1. Extract PCExhumed to a new directory.
2. Copy the following files from the PC retail version of Exhumed or Powerslave (Exhumed preferred), or the Powerslave demo available at http://www.jonof.id.au/build.games/ps. Beta, pre-release or other demo versions not supported.

   STUFF.DAT
   DEMO.VCR
   BOOK.MOV

3. Recommended (but optional) - Add the games CD audio tracks as OGG files in the format exhumedXX.ogg or trackXX.ogg (where XX is the track number) to the same folder as
   pcexhumed.exe. The game includes tracks 02 to 19.
   These will provide the game with its music soundtrack and add storyline narration by the King Ramses NPC.

4. Launch PCExhumed.

### Notes
Demo playback is not yet working.

### Adjusting settings
We are currently working on fancy new menus for the game. In the meantime, you can edit the settings.cfg file in the game directory that's created on first run.

To invert the mouse, add the line 'in_mouseflip 0' to settings.cfg.
To change the FOV, add a new line to settings.cfg, e.g. 'fov "120"' where 120 is the desired FOV value between 60 and 140.

## Rednukem
A port of BUILD engine games based on Duke Nukem 3D codebase using EDuke32 engine

### Supported games

* Duke Nukem 3D: Atomic Edition
* Redneck Rampage
* Redneck Rampage: Rides Again
* Duke Nukem 64
* NAM/NAPALM
* World War II GI

### Installing
1. Extract Rednukem to a new directory.
2. Copy game data files:
    * Duke Nukem 3D: DUKE3D.GRP, DUKE.RTS
    * Redneck Rampage: REDNECK.GRP, REDNECK.RTS, optionally CD audio tracks as OGG file in the format trackXX.ogg (where XX is the track number)
    * Duke Nukem 64: Duke 64 cartridge ROM dump, optionally MIDI tracks extracted from DUKE3D.GRP
    * NAM: NAM.GRP, NAM.RTS, CON files
    * World War II GI: WW2GI.GRP, WW2GI.RTS, CON files
3. Launch Rednukem

## Building from source
See: https://wiki.eduke32.com/wiki/Main_Page

## Acknowledgments
  See AUTHORS.md
