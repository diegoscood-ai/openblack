# Cut scenes and films

The films (Bink videos) and the scripted cut scenes of the story: the camera moving through shots under cinema bars,
fades, and the advisors and characters acting them out.

**Progress: 8/20 done, 7 partial — 58%**

## Films

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The start-up logos, the pre-intro, the intro film, the loading screen's tip pictures and the creature's fall | partial | the intro film (`SET_AVI_SEQUENCE`, `src/Video/VideoPlayer`, our own Bink decoder) and the creature's fall (`src/Video/FallingSpellVideo`) play; the logos, the pre-intro and the tip pictures are not shown (no front end or loading screen); see ../video/bink_videos.md and [video.md](../../bw1-notes/video.md#startup-and-loading-screen) |

## Scripted cut scenes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene takes the camera and brings the cinema bars in | done | `StartCameraControl`, `SetWidescreen` (`src/CHLApi.cpp`, `ScreenFade`); checked in the Land 1 intro |
| The camera is set to a place and focus at once | done | `SetCameraPosition`, `SetCameraFocus` (`src/CHLApi.cpp`, `Camera/ScriptCamera`) |
| The camera glides to a place and focus over a time | done | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` (`Camera/ScriptCamera`, `test/test_script_camera.cpp`); checked in the Land 1 intro; see ../camera/script_camera.md |
| The camera follows or faces an object | done | `FocusFollow`, `PositionFollow`, `SetCameraToFaceObject`, `MoveCameraToFaceObject` (`src/CHLApi.cpp`); the intro's follow shots checked; see ../camera/follow_cameras.md |
| The camera runs a recorded camera path | partial | `RunCameraPath` is real (`CameraPathSystem`); its scenes (the creatures in the glade) are not checked in game; see ../camera/camera_paths.md |
| The picture fades to a colour and back between shots | done | `SetFade`, `SetFadeIn`, `FadeFinished` (`ScreenFade`); see ../camera/cinematics.md |
| Characters walk to marks, turn and play animations for the scene | partial | `MoveGameThing`, `SetFocus`, `OverrideStateAnimation`, `WalkPath`, `SetScriptState` work for villagers and animals (the intro's family walks, checked) but not for creatures |
| Special effects and sounds are started for the scene | partial | `SpecialEffectPosition`, `SpecialEffectObject`, `PlaySoundEffect` are real (`src/CHLApi.cpp`); which effects match the original is not checked one by one |
| Game time can be set, stopped and moved on for a scene | done | `SetGameTime`, `GameTimeOnOff`, `MoveGameTime` (`src/CHLApi.cpp`) |
| Music for the scene | done | `StartMusic`, `StopMusic`, `AttachMusic` (`Audio/Services/GameMusic`); the intro's music plays; see ../audio/music.md |
| The game's speed can be slowed for a scene | done | `StartGameSpeed`, `SetGamespeed`, `EndGameSpeed` (`Help/ScriptControl`); used by the intro |
| Close camera clipping for close shots | todo | `SetGraphicsClipping` is a stub in `src/CHLApi.cpp` (the camera's own near plane is ported, `test/test_near_clipping.cpp`); see ../camera/cinematics.md |

## The story's set pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The family and the drowning boy at the very start | partial | the intro runs and is checked in game up to the hand-over (the family, the kiss, the son and the sharks, the film, the rescue); the welcome dance (`DanceCreate` stub) and the high-detail family are not done; see [land_1.md](land_1.md) |
| The temple's completion | partial | `CitadelGuide` calls only real natives; not checked in game; see [gold_scrolls/the_temple_is_finished.md](gold_scrolls/the_temple_is_finished.md) |
| The creatures in the glade | todo | the camera part of `CreaturesInGlade` is real, but its creatures are not made or driven (`CreatureDoAction` stub, no script-made creatures) |
| Khazar's arrival and death | todo | never reached (Land 2 needs `LOAD_MAP`), and the computer god natives are stubs; see land_2.md |
| Lethys stealing the creature | todo | never reached (Land 2 needs `LOAD_MAP`), and the creature natives it needs are stubs |
| The vortex opening between lands | partial | a script opens a vortex (`CREATE` of a vortex, `VortexParameters`, `src/ECS/Vortex`); Land 1's exit vortex opens at once in a game that skips the guide; the films and going through (`LOAD_MAP`) are not done; see [portals.md](portals.md) |
| Nemesis's curse, the big fight and the ending | todo | never reached (Land 5 needs `LOAD_MAP`); see ending.md |
