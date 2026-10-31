# Vanilla bugs: engine

Bugs in the original game's general engine code: picking under the cursor, land queries, animation clips and the intro
films. Most come from the physics research, read from the executable; where no progress file mentions the bug yet, the
Found in column names the nearest one.

**Bugs: 8 (soft-locks and lost progress: 1)**

## Soft-locks and lost progress

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| A rival god that fetches wood from another of its towns for its own town short of wood gives the wood to the town itself, and the give asks the town whether it stores wood through a function the town does not have | The game stops (a call to address 0) once such a god's hand reaches the town with the wood; reachable by the tree whenever a god's mind is on and holds two towns with stores, one short of wood, the other with wood enough; not seen on Land 2: openblack's traced run from the land's start (4800 turns, 2026-10-28) chose wood from another town 59 times, every one under the building site short of wood, never under the town | The wood action gives the thing above its parent, the town the town-short-of-wood need chose; the held pot's apply test calls the target's store test, past the end of the town's table of functions, where the word is 0 (read through the oracle, 2026-10-28). No land script changes the weights of that branch (every personality call of the story resolved: none names it), and no personality file is read. Detail in [computer-minds.md](../../../hand/computer-minds.md#towninteract) (GetWoodFromOtherTown) | [ai.md](../rival_gods/ai.md) | differs: openblack's give finds the town is no store, and the wood pot stays in the god's hand (the only sensible behaviour; `GameHandRunner::GiveHeldTo`); a traced mind's line names the chosen node and its parent |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Picking a drawn object under the cursor takes the first triangle that contains the cursor, not the nearest one within that object | The cursor can pick a point on the far side of an object, so the hand or tooltip lands slightly wrong on overlapping parts | Read from the executable's draw-time picking (physics research; not yet in a progress file) | nearest: [placement.md](../hand/placement.md) | differs: the hand's pick takes the nearest triangle of each object (`L3DMesh::RayIntersect`) |
| The land point under the cursor always splits each land cell along the same diagonal, whatever the cell's own split | On cells drawn with the other diagonal, the picked point on the land can sit slightly off the drawn ground | The cursor's land test uses a fixed diagonal while drawing and the land height use the cell's split; read from the executable (physics research; not yet in a progress file) | nearest: [look_and_morph.md](../hand/look_and_morph.md) | differs: the cursor's land point comes from the physics ray against the drawn land triangles, which follow each cell's own split |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The land ray walk's "no extension needed" test compares a single-precision value with a double-precision 1e20, which never matches | nothing visible found: the ray is always extended to the map edge (a vertical ray's height is pushed out by 1e20) | Read from the executable (physics research; not yet in a progress file) | nearest: [look_and_morph.md](../hand/look_and_morph.md) | reproduced (`LandIsland::RayCast` always extends the ray) |
| A clip played once that is advanced past its end replays its events from the start | nothing visible: callers stop advancing a finished one-shot clip | Read from the executable's clip event loop (physics research; not yet in a progress file) | nearest: [animation.md](../creature/animation.md) | differs: a clip played once stops its events at its end (`creature_audio::EventsPassed`, the creature's clips) |
| A script asking whether something that is not a creature holds a thing gets its "not a creature" line and false, and then the command goes on to read that creature's hand anyway | nothing seen: the shipped script known to ask it (the Pied Piper's, by its progress page) passes the creature; one passing anything else would crash the game. Whether another shipped script does was not checked | `IN_CREATURE_HAND` gives the line and false but does not stop there; read from the executable ([script-control.md](../../../creature/script-control.md#in-its-hand-for-a-script-in_creature_hand)) | [decision_making.md](../creature/decision_making.md) | Guarded (crash): the line and the false only, always on (`script::property_rules::InCreatureHandNative`) |
| The intro film fades out from 58 s and ends at 60 s | Its last 160 frames are never seen | Read from the intro's playback | [bink_playback.md](../video/bink_playback.md) | reproduced (`VideoPlayer::ScheduleIntro`) |
| A script's DANCE_CREATE takes its dance row with no check, so a row outside the 25 of the table reads the memory after it: rows 25 to 27 empty memory (no file, no start of its own), the rows from 28 on and the negative ones the game's own runtime state | nothing seen: the shipped scripts pass only rows 14 and 6; a script passing another row would make a dance of unknowable data | `DANCE_CREATE` passes the row to the dance with no bound; read from the executable | [dances.md](../../../beings/dances.md#making-a-dance) | fixed (by decision, 2026-10-25): a row outside the table makes no dance, as `DanceSystem::Create` answers none for it |
