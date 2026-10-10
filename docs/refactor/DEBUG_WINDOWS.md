# Debug windows

Every feature or fix that can be reached in play gets a debug control: a trigger (do it now), a readout (show the
state) or a switch (turn it on or off). The control goes in the debug window listed below for its area, in the same
series of commits as the feature. Windows are closed by default, and a closed window reads and writes nothing, so the
verification checks (state hash, random trace, screenshots, hand demos) stay identical. Changes under `src/Debug` are
reviewed by the owner of the debug tools area.

All windows derive from `debug::gui::Window` (`src/Debug/Window.h`), are created in `DebugGuiInterface::Create`
(`src/Debug/Gui.cpp`) and are listed, in that order, under **Debug > Windows** in the debug menu bar
(`Gui::ShowMenu`). A new window is one more `emplace_back` there; it then appears in the menu by its title.

## The debug menu bar

| Menu | Controls |
|---|---|
| Load Island | Story Islands, Playground Islands: load any level; "Take your creature along" (on by default: as the game's land change, the creature is saved to the profile and the island loads it; off: only the island is loaded) |
| World | Time of Day slider; visual time and sky type readouts; Sky alignment; Detail level (original) |
| Debug > Editor (F2) | Opens the Editor |
| Debug > Windows | Opens any window in the table below |
| Debug > Villager Names | Show, Show States, Debug: the villager overlays drawn over each villager |
| Debug > View | Game Detail Overlay (camera, mouse, hand, buffers, turn); draw Sky, Water, Island, Entities, Sprites; Wireframe, Bounding Boxes, Footpaths, Streams |
| Debug > Hand | Right Handed |
| Debug > Field of View | Aspect ratio readout, Reset, Field of View slider |
| Debug > Game Speed | Turn duration readout; Slow, Normal, Fast; Multiplier |
| Debug > Mods | Only when `ModLoader.dll` is loaded: loader version and the result of reading the mods folder |
| Capture | Capture a screenshot to the given file name |
| Quit (Esc) | Quits; the bar also shows the frame rate |

Keys outside the bar: F1 toggles the renderer's debug text, P pauses the game clock.

## Windows

All open from **Debug > Windows > *title***.

| Window | Files | Controls | Area it serves |
|---|---|---|---|
| Profiler | `Profiler` | CPU and GPU timings per pass, bgfx counts; draw Sky, Water, Island, Entities, Sprites | Rendering, performance |
| MeshPack Viewer | `MeshViewer` | Browse meshes, submeshes, animations and frames; flag filter; bounding box, armature; Spawn at a location | Assets (meshes, animations) |
| Texture Viewer | `TextureViewer` | Browse loaded textures with size and format | Assets (textures) |
| Console | `Console` | Runs `help`, `history`, `clear` and the land script (`.txt`) commands; Copy, Clear | Land scripts |
| Land Island | `LandIsland` | Block and country counts, resolution; Bump, Small Bump; Dump Heightmap, Dump Textures; land textures; Object draw list (closed by default): Draw only what the list draws switch (`EngineConfig::drawFollowsList`), entries against the 2999 cap, full or still pass, last rebuild turn against the turn, rebuilds asked for, objects drawn by the last pass, and the objects left out of the last frame (entity, kind, not in the list or listed and not drawn) | Land, terrain, object draw list |
| LHVM Viewer | `LHVMViewer` | Read-only: scripts (code, locals), variables, data, tasks (locals, stack, exception handlers) | Challenge scripts (VM state) |
| Path Finding | `PathFinding` | Select a villager; Teleport, Move To Point, Move On Footpath with speed; Execute | Villager movement |
| Audio Player | `Audio` | Switches Game sounds, Game music; Sound tab (banks, Play, Stop, Sample volume); Music; Channels (sample, voice, music channels); Atmos (loops, loose samples, climates) | Audio |
| Music | `Music` | Music type; Play, Play (sync, fade), Stop (cut), Stop (fade); Music volume; script and alignment music readouts | Music |
| Audio banks | `AudioBanks` | Every `.sad` bank's samples; play, Stop, Decode all with timings | Audio decoders |
| Citadel | `Temple` | Player; Alignment readout, Evil to good slider; Snap the outside now; Leash posts (closed by default): Draw the posts switch (the drawing only), the player's temple's three posts (point, spin, smoke cell), which show now, which the hand feels now (radius 1 at each post's point now), the leash picked there, Pick Aggression / Learning / Compassion (the temple's pick alone), Tap Aggression / Learning / Compassion (the hand's tap as the local interface: the click, the pick, the leash's packet for the next turn) | Temple, temple leash posts |
| Miracles | `Miracles`, `MiraclesCaster` | Cast any miracle (one-shot orb, in the hand, dispenser; where the camera looks, under the hand, at the next click); seeds, Charge; infinite prayer power; running miracles, dispensers, creatures' spells, villagers' reactions | Magic (miracles, worship) |
| Vortices | `Vortices` | Create a vortex now, under the hand or at the next click; list with Fade out, Delete; the way to the next land | Land-to-land travel |
| Creature Spawner | `CreatureSpawner*` | Spawn (species, owner, size, strength, alignment, fatness); Creatures list; Selected tabs: actions and gestures, body and needs, fight, hands, leash, learning mind, mind files, appearance (tattoos, wounds), sounds, footprints; Pause mind, Leash works, Creatures faint, Mute creatures | Creature, leash |
| Consciences | `Consciences` | Who speaks; Appear, Vanish, Home, Eject; Say a text or voice sample; Clip speed; message sets with Run | Advisor spirits |
| Scripts | `Scripts`, `Editor/Panels/ScriptsPanel` | Challenge script source, disassembly, globals, natives, handlers; Compile; breakpoints, Step, Next, Hold, Continue; start or stop tasks; edit values | Challenge scripts (debugger) |
| Camera | `Camera` | Camera paths: Run, Stop, Pause, Resume; points and duration | Camera |
| Weather | `Weather` | Climates, Climates breed storms, Lightning; made-to-measure storm (cloud, rain, temperature, wind, height, length); Force this storm, Clear every storm, End | Weather |
| Gestures | `Gestures` | Last recognised gesture, matches, what is awaited; Draw it, Forget the path; draw the hand's path | Hand gestures |
| Key Bindings | `KeyBindingsWindow` | Read-only: every action's key, mouse button and hold | Input |
| File Dialog | `FileDialogWindow` | Open or Save; Debug browser, Platform dialog | Tools |
| Magic | `Magic` | Miracle tab: one magic type's `info.dat` values; All miracles table; Particles tab: Spawn any effect at the camera focus or the hand with height, magnitude and draw path; Close all, Delete all | Magic data, particles |
| Editor | `Editor/EditorWindow` (also Debug > Editor, F2) | Toolbar (View, Step, speed, snap); Outliner, Inspector, Palette panels | Level editing, entities |
| Video | `VideoViewer` (only with `OPENBLACK_USE_BINK`) | Play any `.bik`; Restart, Step; frame and timing readouts; switch Game films | Video |

Other ImGui windows: the villager overlays and the Game Details Overlay (`Gui.cpp`, switched from Debug > Villager
Names and Debug > View), and the Creature Cave screen (`src/Gui/CreatureCaveScreen.cpp`), which is a game screen, not
a debug window.

## Where new controls go

- **Camera:** Camera (paths), Debug > Field of View, Game Details Overlay (readouts).
- **Creature:** Creature Spawner, in the Selected tab of its subject (`CreatureSpawner<Subject>.cpp`).
- **Leash:** Creature Spawner, leash section (`CreatureSpawnerLeash.cpp`). Its mind line reads out the desire a leash
  or a village has made dominant and its seconds left; the creature window's mind section sets and clears one
  ("Timed dominant desire", closed by default). "Kept area" (closed by default) reads out the point the creature is
  kept near this turn, the radius, its distance, the walk back's speed above walking, arrival and restart distance, and
  where it is walking back to; "Keep at home" and "Let roam" set and clear a home radius to try it. "Tug", next to
  the pull, tugs the leash held in the hand as the original's tug would (nothing in the game sends one); the line under
  it counts the pulls away from each desire's plan.
  "Tie with the hand" (closed by default) reads out what the hand's double click and its tap with the leash in
  the hand would do (tie, untie, tap or nothing) to the thing last clicked and to the nearest thing. The nearest is
  searched within 400 of the creature and among plain things only, never a leash post, a spell icon, a script
  highlight or a seed: a debug stand-in for the hand's own pick, the nearest thing within 5 of where the hand acts.
  "Tie to nearest (packet)" and "Untie (packet)" send the hand's tie packet, carried out at the next turn as the
  hand's would be (the hand does not send it yet).
- **Hand:** Gestures for gestures; Debug > Hand for hand switches (see gaps).
- **Magic and worship:** Miracles (casting, prayer power, reactions); Magic (`info.dat` values).
- **Particles:** Magic, Particles tab.
- **Weather and sky:** Weather; World menu for time of day and sky.
- **Town and villagers:** Path Finding, Debug > Villager Names (see gaps).
- **Audio:** Audio Player (game sounds), Music (music), Audio banks (decoders).
- **Video:** Video.
- **Temple:** Citadel.
- **Scripts:** Scripts (challenge scripts), Console (land scripts), LHVM Viewer (read-only).
- **Advisors:** Consciences.
- **Land and travel:** Land Island, Vortices, Load Island menu.
- **Rendering:** Debug > View, Profiler; the object draw list in Land Island.
- **Assets:** MeshPack Viewer, Texture Viewer.
- **Game flow:** Debug > Game Speed.

## Gaps

No window exists yet for these areas; the first lane that needs one adds it to `Gui.cpp` and to this page:

- **Hand:** picking up, holding and throwing (only Right Handed and the Game Details hand position exist).
- **Town, villagers and buildings:** jobs, needs, town state, building construction (only the villager overlays and
  Path Finding).
- **Animals.**
- **Objects and physics:** thrown and falling objects.
- **Interface and in-game screens:** menus, help texts, the land's interface states.
- **Players and computer players:** belief, alignment and influence outside the temple.
