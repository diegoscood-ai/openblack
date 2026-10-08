# Debug windows

openblack's own debug interface: the menu bar along the top of the screen and the windows it opens, for looking inside
the game while it runs. None of this is in the original game; it is tracked because it is part of openblack's progress.
The creature tools, the testbed, the editor and the diagnostics have their own files.

**Progress: 22/31 done, 8 partial — 84%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## Menu bar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The debug interface is drawn with Dear ImGui over the game, in its own render view | done | `src/Debug/Gui.cpp` |
| Load a story land, a playground land or the creature testbed from the menu, with each land's description | partial | "Load Island" with "Story Islands" and "Playground Islands" (`src/Debug/Gui.cpp`); no creature testbed in our tree |
| Set the time of day and the player's alignment, and read the sky's and camera's alignment | partial | "World" menu: time of day, visual time, sky type, a sky alignment slider and the detail level (`src/Debug/Gui.cpp`); a player's alignment is set in the Temple window (`src/Debug/Temple.cpp`); the camera's alignment is not shown |
| Open the editor (F2) and each debug window | done | "Debug" menu: "Editor" (F2) and "Windows" |
| Show villagers' names, details and states over them, with a debug view | done | "Villager Names" menu (`config.showVillagerNames`, `debugVillagerStates`, `debugVillagerNames`) |
| Turn the sky, water, land, objects and sprites on and off; wireframe, bounding boxes, footpaths and streams; an overlay of the game's detail settings | done | "View" menu, with the game detail overlay |
| Switch the hand between right and left handed | todo | no hand menu in our debug bar |
| Change and reset the field of view | done | "Field of View" menu |
| Slow down or speed up the game, by presets or a multiplier | done | "Game Speed" menu (presets and a multiplier) |
| Take a screenshot to a file | done | "Capture" menu |
| Quit, and the frame rate shown on the bar | done | "Quit" and the frame rate on the bar (`src/Debug/Gui.cpp`) |
| The system cursor stays hidden; the game's hand or pointer shows over the debug windows | done | the system cursor is hidden (`src/Windowing/Sdl2WindowingSystem.cpp`); the game's hand or pointer shows (`src/Gui/GameInterface.cpp`) |

## Windows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Profiler: the frame's CPU and GPU stages, with the draw toggles | done | `src/Debug/Profiler.cpp` |
| Mesh pack viewer: every mesh with its bounding box, skeleton and animations, and spawning it | done | `src/Debug/MeshViewer.cpp` |
| Texture viewer: every texture of the packs | done | `src/Debug/TextureViewer.cpp` |
| Console: the log, and land script commands typed in with completion and history; what is under the mouse | done | `src/Debug/Console.cpp` (completion and history) |
| Land: block and country counts, the small bump strength, and dumping the land's textures and height map | done | `src/Debug/LandIsland.cpp` |
| Path finding: send a picked villager somewhere by teleport, straight walk or footpath | done | `src/Debug/PathFinding.cpp` |
| Audio: every sound and music pack to play, the emitters, the voice banks, and the ambient loops, one-shots, climates and storms | done | the Audio Player (`src/Debug/Audio.cpp`: Sound, Music and Channels tabs, the game sound and music switches), Audio banks (`src/Debug/AudioBanks.cpp`); the ambience's climates and storms in the Weather window |
| Temple: go to each room of the temple | done | `src/Debug/Temple.cpp` |
| Camera: run the game's camera paths | done | `src/Debug/Camera.cpp` |
| Weather: force rain, snow and storms over the island from presets or by hand | done | `src/Debug/Weather.cpp` (presets, "Made to measure", "Storms") |
| Magic: running miracles, any miracle cast or powered up, prayer power, dispensers and bubbles, creatures' spells, particles | done | `src/Debug/Magic.cpp` ("Miracle", "All miracles", "Particles"), `src/Debug/Miracles.cpp`, `MiraclesCaster.cpp`, `src/Debug/Vortices.cpp` |
| Gestures: the path drawn, its best template and score, drawing or sending gestures | partial | `src/Debug/Gestures.cpp`: the path over the screen, what is looked for, the templates that match now and the last one recognised, and "Draw it" plays a template's stroke through the recogniser; no score is shown |
| Key bindings: every action, its binding and whether it is built yet; rebinding and pressing actions | partial | `src/Debug/KeyBindingsWindow.cpp` lists every action with its key and mouse binding, whether it is held, and bindings two actions share; it does not rebind or press actions, nor say whether one is built (the action map can be rebound in memory, `src/Input/GameActionMap.cpp`) |
| A music window showing the land's music, its tracks and what plays next | done | the Music window (`src/Debug/Music.cpp`): the main volume, the six channels, a player for any music type and the game music's state |
| A window of a land's game state hash, to check two runs stay the same | partial | the per-turn state hash is written to a file with `OPENBLACK_STATE_HASH` (`src/Debug/StateHash.cpp`); no window shows it |
| A fixed clock for running the game turn by turn at a set rate | done | `OPENBLACK_FIXED_FRAME_MS` (`src/Debug/FixedClock.cpp`) |
| Memory use by system | partial | the process and graphics memory are logged with `OPENBLACK_PROFILE` (`src/Debug/MemoryStats.cpp`); not by system and not in a window |
| A window for the help system, the advisors and their messages | partial | the Consciences window (`src/Debug/Consciences.cpp`): the good and evil advisors' state, clips, voices and the help's message sets; no window for the rest of the help system |
| A window for the land scripts' challenges, their state and their scrolls | partial | the Scripts window (`src/Debug/Scripts.cpp`, the editor's script panel) shows the running tasks ([script_debugger.md](script_debugger.md)), not challenges as such |
