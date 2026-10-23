# Portals between lands

At the end of each land a swirling vortex opens in the ground. Whatever it pulls in or is thrown into it (villagers,
animals, food and wood, one-shot miracles, rocks, toys, trees, scaffolds) is written to a crossing file, the player's
creature is carried over in its own files, and clicking the vortex's scroll flies the camera down into it and loads the
next land, where an outgoing vortex throws everything back out beside the new home town and tops the newcomers up to
30 villagers. The same kind of object makes the glowing crater of Land 5's volcano. Each land's own vortices (where,
when, what is said, what differs) are in [portals_per_land.md](./portals_per_land.md); the quests that open them are in
[gold_scrolls/](./gold_scrolls/); the lands in [land_1.md](./land_1.md) to [land_5.md](./land_5.md). Script commands in
general are in [../scripts/](../scripts/).

**Progress: 76/137 done, 0 chosen, 31 partial, 0 inferred — 71%**

How the original does it, in our wiki: [Vortexes and the tornado: objects swallowed, carried and flung](../../../world/vortex.md).

Sources: the executable (the vortex objects, the crossing file, the land change, the creature's files), the shipped
challenge scripts' source text and the game's tables. openblack is judged on this tree: it has the vortex object
(`src/ECS/Vortex.cpp`) and writes what a vortex takes in for the next land (`src/ECS/VortexSave.cpp`), and the next land is
loaded (`LOAD_MAP`, `Game::ChangeLand`); the next land's outgoing vortex reads the crossing file back from the save folder, but a full crossing has not been checked in play against the original, so much here is still to
do.

## The three kinds of vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An incoming vortex (the exit from a land) pulls things in and writes each one to the crossing file for the next land | partial 80% | the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) and writes each thing's line (`ecs::vortex::HandOff`, `src/ECS/VortexSaveLines.cpp`) to `SaveGame/vortex.txt` under the game's folder, never in a run of the checks (unverified against v1.42); scaffolds and pieces knocked off buildings have their lines too (`VortexSaveLines`), while toys and reward chests have none, as their line in the original is undefined; see [vortex.md](../../../world/vortex.md#taking-objects-in-the-in-vortex); re-measured 2026-10-21: it pulls in and writes each thing's line; never in a run of the checks |
| An outgoing vortex (the arrival in a land) reads the crossing file back, throws each thing out, then makes new villagers | partial 60% | the Out's emission, reader and hand-over are ported (`ProcessOut`, `HandOver`, `vortex_save::OpenReader`); the file is read from `SaveGame` under the game's folder (`vortex_save::SaveFolderUnder`), set as each land starts (unverified against v1.42); the land change is `LOAD_MAP` (`Game::ChangeLand`); a whole crossing in play is not checked against the original; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| A volcano vortex is the glowing mouth of Land 5's volcano; it pulls nothing in and throws nothing out, and flying things pass straight through it | partial 60% | the Volcano is made and has no contents and leaves the land alone (`ecs::vortex`); its four effects and its rumble (`audio::vortex_sound::CreationTag`) are made by `ecs::vortex::Create`, and its box and ground decal are drawn (`Graphics/RendererVortex.cpp`); flying things passing through it is not checked; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Every vortex is made with the same pull radius, 50 (five land cells), whatever its kind | done | `ecs::vortex::Create` gives every kind a radius of 50; `test/test_vortex.cpp` |
| A vortex is an object the creature cannot pick up, throw, eat, stomp on, set fire to, fight, examine, destroy by stoning or put in a store, and it cannot act as a container | pending | the creature's own rules for a vortex were not found in our creature code |
| A vortex is not touched by miracles or other effects | partial 60% | a vortex is not an effect receiver in our tree either (no effect component); not checked against every miracle; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The hand never takes a vortex and does not rest on its model; a vortex can never be put in the hand | partial 60% | the hand's cursor ignores a vortex (`HandSystem.cpp`); whether every pick-up path refuses it was not checked; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| An incoming vortex is solid to flying things: a thrown object hits it rather than passing through, and is not lifted up over it | done | a vortex is filed in the map cells; the In's unseen cylinder becomes a resting body (`DynamicsSystem`: proxies, constants 13, the statics' vortex row), a raise never makes it one but goes over it once it is in the physics, as the original's raising loop does with every listed body, and what strikes it is taken (`vortex::ReactToPhysicsImpact`) (unverified against v1.42) |
| The creature keeps away from an incoming vortex unless it may go in (see [The creature](#the-creature)) | partial 70% | `route_plan_world::CreatureMustAvoid` asks `ecs::vortex::CreatureMustAvoid` (unless property 36 and the leash, or a script's control); the routes' own obstacle planner (raffclar #303/#330) is not ported; re-measured 2026-10-21: kept away by the rule; the routes' own obstacle planner is not ported |
| Scripts see a vortex as a vortex object and can find one at or near a place | partial 90% | `CREATE` returns the vortex; it is type 23 VORTEX with its kind as its sub-type (`script_type::TypeOf`, `SubtypeOf`, GAME_SUB_TYPE); searching by place uses the general `CallNear`; re-measured 2026-10-21: created and typed as the original; searched by the general call |
| The vortex is saved and loaded with a saved game, with its state, its fade, its counts and what it is in the middle of throwing | pending | openblack has no saved games |

## The vortex table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack reads the vortex table (particles, starting state, fade, scale, limits) from the game's info file | done | `InfoConstants::vortex` (three rows) is read, and `ecs::vortex::Create` takes its starting state |
| The incoming vortex starts by fading in; the outgoing and volcano vortices start fully open | done | `ecs::vortex::Create` sets each kind's starting state from its row; fades in `vortex::Openness` (`src/Magic/VortexRules.h`; `test/test_vortex.cpp` Vortex.Fade); see [vortex.md](../../../world/vortex.md#states-and-fades) |
| Each kind has its base size: 0.28 for the land vortices, 0.75 for the volcano; the model and the ground ring are scaled by it | done | the depth box is scaled by it (`vortex_draw::ZBoxModel`) and the ground decal is 160 × it across (`vortex_draw::DecalUvTransform`, `Graphics/VortexDraw.cpp`, `test_vortex`); unverified against v1.42 ([vortex.md](../../../world/vortex.md#the-mesh-and-the-z-box)) |
| Each kind names its own four effects: before the land is drawn, after it, the object mover and the glow on the ground | partial 80% | the effect names are mapped in `src/Particles/ParticleTypes.cpp`; `ecs::vortex::Create` makes all four, the object mover synchronised (unverified against v1.42); re-measured 2026-10-21: all four are made; unverified against v1.42 |
| The table's "fade when deleted" column (0 / 1 / 0) is never read: every vortex fades out only when a script tells it to | done | our `ecs::vortex` reads no such column: deletion comes only from a fade-out |
| The table's limits (at most 1000 objects for the outgoing vortex, counts of food, wood, villagers and one-shots) are never read; the only limit is a fixed 30 villagers (see [Arriving](#arriving)) | done | our Out stops at a fixed 30 villagers (`ProcessOut`) and reads none of the limits |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex draws a depth-only box under it (mesh 560), scaled by its base size, before the land; its own model, the vortex cylinder, is never drawn and only gives its radius | done | `Renderer::DrawVortexBoxes` (`Graphics/RendererVortex.cpp`) draws the box after the sea and before the land, in the main view; unverified against v1.42 ([vortex.md](../../../world/vortex.md#the-mesh-and-the-z-box)) |
| Before the land is drawn, a spinning funnel of two swirl textures turns on the spot (the "before" effect) | done | `ecs::vortex::Create` makes it and it is stepped every frame; its funnels are drawn before the land (`Renderer::DrawVortexGroundEffects`, `Graphics/RendererVortex.cpp`), and not at all while the vortex is faded out; unverified against v1.42 |
| After the land is drawn, a cloud of stars spins over it (the "after" effect), the incoming and outgoing vortex sharing it | partial 80% | `ecs::vortex::Create` makes it and it is stepped every frame; its stars are drawn with the other sorted effects before the depth-sorted drain, not at the vortex's own place in it ([vortex.md](../../../world/vortex.md#the-z-sorted-part)); re-measured 2026-10-21: made and stepped; drawn before the depth-sorted part, not in it |
| The outgoing vortex uses the incoming vortex's "before" effect (the game's table points both at the same file) | done | `src/Particles/ParticleTypes.cpp` maps the outgoing "before" type to the incoming file |
| A light map paints a glow on the ground around the vortex | pending | the vortex makes no light map yet |
| A ring texture and its alpha mask are laid on the land block under the vortex, scaled to the vortex's base size: the multi-ring base for the land vortices, the volcano's own base and mask for the crater | pending | pending: the ground decal ([vortex.md](../../../world/vortex.md#drawing)) |
| An incoming or outgoing vortex levels the ground under it: in an 11-by-11-cell square around it the land is pulled towards the square's average height, fully within 50 of the middle and fading to nothing at 56 | done | ported: `FlattenLand` (`src/ECS/Vortex.cpp`) through `LevelSquare` (`src/Magic/VortexRules.h`), tests Vortex.LandFactor and the VortexRules levelling tests; see [vortex.md](../../../world/vortex.md#the-land-under-a-vortex) |
| The levelling grows with the fade-in, eased out (one minus the square of what is left), and is only ever added to, never undone, so the flattened pad stays after the vortex has gone | done | `vortex::LevelAmount` grows only and the fade-out counts as complete; see [vortex.md](../../../world/vortex.md#the-land-under-a-vortex) |
| The volcano vortex does not change the land | done | `FlattenLand` skips the Volcano |
| The volcano vortex has its own before, after and light-map effects (fire and rock) | done | the effect files are mapped in `src/Particles/ParticleTypes.cpp` and `ecs::vortex::Create` makes them for the volcano as for the other vortices; unverified against v1.42 |
| Things being pulled in are carried by the object-mover effect: each spirals round the middle, closing in as it ages and rising on a curve, and vanishes when it reaches the middle | done | `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`, the pure spiral `vortex::crossing::Attract`, `test_vortex_crossing_rules`) (unverified against v1.42) |
| A vortex fades in over 7 seconds and is then fully open; told to fade out, it takes 7 seconds and then deletes itself | done | `vortex::Openness` and `ProcessAll`: Active after 7 s, deleted 7 s after a fade-out starts (`test/test_vortex.cpp` Vortex.Fade); see [vortex.md](../../../world/vortex.md#states-and-fades) |

## How it sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex hums: a looping vortex sound starts with the "before" effect of the incoming vortex and the "after" effect of the outgoing one, released softly when they stop | pending | the effects are made (`ecs::vortex::Create`) and their create rule reads the looping sound (`SoundOfCreate`, `src/Particles/PSys.cpp`); which sample the game plays for the vortex's sound action is not traced ([vortex.md](../../../world/vortex.md#pending)) |
| The volcano vortex has a looping volcano rumble of its own, started when it is made | pending | the volcano's sound is not made yet |
| Villagers thrown out of an outgoing vortex scream only for their first 10 turns of flight (15 for an ordinary throw) | done | the clip-sound rule of the vortex clip (`AnimationSounds.cpp`), now that a villager thrown out plays it (unverified against v1.42) |
| A creature that reaches the middle of a vortex makes the teleport "energise" sound as it starts to sparkle away | pending | no creature crosses: the In's creature fizz is not ported and no land change happens |
| Land 4's hidden vortex rumbles: a screen-rumble sound with every camera shake | pending | `ShakeCamera` and `PlaySoundEffect` are real; Land 4 is never reached; see [portals_per_land.md](./portals_per_land.md#land-4-exit) |
| The hand over a vortex sends its own force-feedback effect | pending | to port as the original's (user, 2026-10-19) |

## Being sucked in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only a fully open incoming vortex pulls: not while it fades in or out | done | `ProcessIn` returns unless Active; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| Every turn it searches the cells around it in a spiral out from its own cell, stopping at the first cell beyond its reach (or after 9999 cells) | done | the spiral walk (`ProcessIn`, `map_coords::Spiral`); the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| On two turns out of three it only reaches half its radius (25); on every third turn the whole 50 | done | the 50 / 25 rule (`ProcessIn`); the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| On each cell it takes every thing whose own cell it is and that may be sucked in; a thing that may be sucked in is anything that could be thrown or knocked flying (see [What goes in](#what-goes-in-kind-by-kind)) | done | `CanBeSuckedIntoVortex` uses `CanBecomeAPhysicsObject`; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| A thing thrown into the vortex that hits its model while it is open is taken at once, keeping its speed and turn as it starts to spiral | done | taken with the vortex's own body's velocity and matrix, as the original hands it (`DynamicsSystem::ReactToPhysicsImpact`, `vortex::TakesWhatStrikesIt`) (unverified against v1.42) |
| Things merely carried over the vortex in the hand are not taken: nothing in the hand is on the ground | done | nothing in the hand is in a map cell; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| Food and wood piles too big to fly are drained instead: each time its cell is searched a pile gives up a pot of up to 1000 of its food or wood (of the kind dropped from the hand), and that pot is sucked in | done | `ProcessIn` through `object_resources::SplitOffPile` (shared with the tornado), in any of the pile's cells (unverified against v1.42) |
| Pots made from a pile are drawn at 30% to 42% of their usual size at random as they fly in | done | `vortex::crossing::PileShare` (unverified against v1.42) |
| Nothing walks into the vortex on purpose: no villager, disciple or animal is ever sent to it; only those that wander within reach, are dropped or thrown in, or stand where it opens are taken | done | our tree sends nobody to a vortex either; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| Villagers hiding in a building (the run-and-hide state), villagers inside buildings and villagers in the hand are never taken | done | only map-cell objects are walked; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| Each thing taken is handed to the object-mover effect; if the vortex has no object mover, nothing is taken | done | `TakeIn` takes nothing without the object mover, as the original; `ecs::vortex::Create` makes it now (unverified against v1.42) |
| A thing is written to the crossing file the moment it is caught, before it is seen to fly in; a villager leaves its town and an animal drops everything that depends on it at that moment | done | as `UR_VortexAttract` starts carrying it (`ecs::vortex::HandOff`): a villager leaves its town, an animal its town and flock (`animal_ai::DeleteDependencies`), then its line is written (unverified against v1.42) |
| A thing that is indestructible (made so by a script, or one still being thrown out by an outgoing vortex) is pulled round and in like any other but is not written down: at the middle it is thrown back out at a random angle | done | pulled in and not written (`HandOff`); at the middle it goes with its particle: the original's throw back out needs a vortex only a saved game gives the particle (UR_VortexAttract 0x6D3E43), so in play it is gone, as here (unverified against v1.42) |
| A creature close to the middle of the vortex (within seven tenths of the vortex's own width) sparkles away over 3 seconds instead; it is never written to the crossing file | done | `TakeIn`'s creature branch: `SetFizz(1, 3, true)` within `Get2DRadius x 0.7` of the vortex's cylinder (unverified against v1.42) |
| Things in flight towards the vortex are not counted as thrown by anyone and do not react | partial 70% | our tree records no thrower for them; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42); re-measured 2026-10-21: no thrower is recorded; unverified against v1.42 |
| The player can throw things in by hand; the advisors tell the player to send villagers, food, wood and followers in | partial 60% | the advisors' lines are real natives; see [portals_per_land.md](./portals_per_land.md) and [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md); the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42); re-measured 2026-10-21: the advisors' lines are real natives; see the per-land pages |

## What goes in, kind by kind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers go in, alive or dead; each is written as its tribe and job (or its exact kind, for a villager of no tribe), its age, its place relative to the vortex and the number of its old town | done | `vortex_save_lines::LineFor` and `Text` (`lhscriptx::WriteCommand`) (unverified against v1.42) |
| Disciples go in as plain villagers: their discipleship is not kept | done | the villager line keeps no discipleship (unverified against v1.42) |
| Animals go in (cows, sheep, pigs, horses, wild animals and birds); each is written as its kind, its age, its flock and its flock's town | done | `vortex_save_lines::LineFor`; the age is the animal's own (`Animal::age`) (unverified against v1.42) |
| Doves that are pulled in are written down as any animal (the row once said they were lost: the original's dove classes save themselves with the animals' writer) | done | our writer writes a dove as any animal (`vortex_save_lines::LineFor`), as the original's `Dove` and `SpellDove` do (see [vortex.md](../../../world/vortex.md#the-land-script-round-trip-of-vortextxt)) |
| The player's creature, other gods' creatures and story creatures never go into the crossing file | done | a creature is never carried in (`TakeIn`) and has no line (unverified against v1.42) |
| Food and wood dropped from the hand (hand piles) go in whole, as a pot of their kind and amount | done | the pot line (`vortex_save_lines`) (unverified against v1.42) |
| Big wood piles, food piles and magic food and wood piles cannot fly: they are drained into pots of up to 1000 (above) | done | `ProcessIn`'s pile branch (unverified against v1.42) |
| A pot that is part of a structure is never written down | done | a storage pit's piles have no line (unverified against v1.42) |
| One-shot miracle globes go in, each written as its miracle with the command the lands use for powered-up globes | done | `CREATE_ONE_SHOT_SPELL_PU` with the miracle of its seed at its power-up (`vortex_save_lines`) (unverified against v1.42) |
| Seeds made from worship-site or village-centre icons and spell icons themselves never go in | partial 80% | they cannot become physics objects in our tree either; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42); re-measured 2026-10-21: they cannot become physics objects; unverified against v1.42 |
| Rocks, fragments and other loose statics go in, each written as its kind, size and turn | done | mobile statics are written with their kind, height, angles and size (`vortex_save_lines`), the angles as the thing keeps them, as every save writer of the original does, not taken apart again from its turn (tests `VortexSaveLines.ALeaningStaticIsWrittenWithTheAnglesItKeeps`, `AThingTurnedSinceItKeptItsAnglesIsWrittenWithTheAnglesOfItsTurn`; xref note #963); a fragment as a rock, as the original's fragments are rocks (test `VortexSaveLines.APieceKnockedOffABuildingIsWrittenAsARock`) (unverified against v1.42) |
| Toys go in and come out in the next land as the same toy | pending | a ball cannot be ported as the original: its line names a row outside the mobile objects' table, and what the next land makes of it is undefined (see [vortex.md](../../../world/vortex.md#the-land-script-round-trip-of-vortextxt)); openblack writes no line for it |
| An artefact goes in and comes out still an artefact of the same god with the same worth; only its town is forgotten | done | written with its `MAKE_LAST_OBJECT_ARTIFACT` line (`vortex_save_lines`, `TownArtefact`); its reader makes the thing an artefact of that god again, worth the same, in the nearest town when that god owns it, else in none (`FeatureScriptCommands::MakeLastObjectArtifact`, `artefact_rules::TownForScriptArtefact`; test `ArtefactRules.AScriptsArtefactGoesToTheNamedTownElseTheNearestOfItsPlayerElseToNone`) (unverified against v1.42) |
| Loose scaffolds go in and come out as scaffolds of the same kind; a scaffold in a workshop's yard, on a building site or tied to a building plan is never taken | partial 60% | a scaffold is written with its town, angle and size (`CREATE_SCAFFOLD`, `vortex_save_lines`; test `VortexSaveLines.AScaffoldIsWrittenWithItsTownAsAScaffold`) and made again by that command's reader (`FeatureScriptCommands::CreateScaffold`); which scaffolds the In may take is not checked (unverified against v1.42); re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Living trees within reach are uprooted and go in, each written as its kind and size, and grow again in the next land | done | the tree line (`vortex_save_lines`), with the angle the tree keeps, not wrapped (test `VortexSaveLines.AnUprightThingIsWrittenWithItsAngleAsItWasGivenNotWrapped`) (unverified against v1.42) |
| Dead and felled trees go in, keeping their kind, size and owning god | partial 90% | written with its owning god (`DeadTree::owner`, `vortex_save_lines::OwnerOf`), kind, life and the angles it keeps (`vortex_save_lines`, `DeadTreeLine`; tests `VortexSaveLines.ADeadTreeIsWrittenAsThePlayersItIs`, `ALeaningDeadTreeIsWrittenWithTheAnglesItKeeps`): the god a thrown tree that landed dead or a land script's line gave it, a felled tree the neutral player's, as the original. Until 2026-10-20 a crossing wrote every dead tree as the neutral player's (unverified against v1.42); re-measured 2026-10-21: written with owner, kind, life and angles; tested |
| Field crops, reward chests that are ready to open, barrels, balls and other loose objects go in, each written as its kind, size and turn | partial 70% | mobile objects are written (kind, the angle they keep, size in thousandths); reward chests and balls have no line: the original's line for them names a row outside the mobile objects' table, so what it makes of them is undefined (see [vortex.md](../../../world/vortex.md#the-land-script-round-trip-of-vortextxt)) (unverified against v1.42); re-measured 2026-10-21: mobile objects are written; chests and balls have none, as the original's line is undefined |
| A loose object stuck to something (carried, or fixed to another object) is pulled in but not written down | pending | the In takes through its object mover (`ecs::vortex::Create` makes it, `UR_VortexAttract`) and writes each thing's line (`ecs::vortex::HandOff`); not checked for this kind of thing |
| Buildings, fields, the temple, worship sites, village centres, wonders, spell dispensers, storehouses, lanterns, bonfires, fires, shields, fireballs, the Creed, whales and other vortices are never taken | partial 80% | only objects that can become physics objects pass `CanBeSuckedIntoVortex`; the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42); re-measured 2026-10-21: only physics-capable objects pass; unverified against v1.42 |
| Other gods' villagers, animals and things are taken just like the player's; a villager keeps nothing of its old god | done | the In takes through its object mover and `UR_VortexAttract` (`src/Particles/Rules/VortexAttract.cpp`) (unverified against v1.42) |
| Objects are written in the order they are caught, and nothing limits how many | done | each as it is caught, once (`vortex_save::Writer::written`) (unverified against v1.42) |

## The crossing file

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What goes into an incoming vortex is written to a crossing file in the save folder, one line per object, in the same language as the lands' own set-up files | done | `SaveGame/vortex.txt` under the game's folder (openblack has no save games), opened by every new In; never in a run of the checks (`EngineConfig::checkRun`) (unverified against v1.42) |
| Each line places the object relative to the vortex, so things come out spread round the arrival vortex as they lay round the exit | done | the writer's offset from the vortex, the reader's at the emission point (`vortex_save::ReadNext`) (unverified against v1.42) |
| Every new incoming vortex empties the file when it opens, so only the last incoming vortex of a land counts | done | `vortex_save::OpenWriter` truncates it (unverified against v1.42) |
| An outgoing vortex opens the same file for reading when it is made; if there is no file it only makes new villagers | done | the Out opens the reader when made (`vortex_save::OpenReader`) and makes only villagers without one (unverified against v1.42) |
| Nothing else about the land is written to the file: the creature, the hand, towns, buildings and statistics are handled elsewhere | done | only objects have lines (unverified against v1.42) |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Saving a game while a vortex is open copies the crossing file beside the save and records how far it has been read | pending | openblack has no saved games |
| Loading that save copies the crossing file back, so a crossing can be saved and resumed from either side | pending | openblack has no saved games |
| A loaded incoming vortex carries on adding to the end of the file; a loaded outgoing vortex skips the lines it had already thrown out | pending | openblack has no saved games |
| The vortex's state, fade, gathering place, flock, town, counts and the list of things it is throwing are saved and restored | pending | openblack has no saved games |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature crosses in its own files: when the land is cleared its mind and its body are saved to the creature-mind folder under the player's name | pending | our tree reads a creature's mind file (`CreatureMindModel`, `--creature-file`), but nothing writes it on a land change; see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The mind file keeps what it has learnt, its alignment, its age, size, energy and other body values and its tattoos; the body file keeps its species, its look, its strength and two more body values | partial 40% | the mind file's body part is read (`src/Creature/CreatureMindFileBody.cpp`); nothing writes it; re-measured 2026-10-21: the body part is read; nothing writes it |
| A creature the game has marked as not the player's own to keep is not saved | partial 50% | the local player's creature is saved as its deletion starts (`creature_removal`, `KeepForNextLand`, never in a run of the checks); the mark the original tests (creature +0x1058) is not identified; re-measured 2026-10-21: the local creature is kept; the original's mark is not identified |
| The creature does not physically go through: walking it into the vortex only makes it sparkle away; it crosses because it is saved when the land is cleared, wherever it stands | partial 60% | it sparkles away (`TakeIn`) and is saved as it is deleted; `LOAD_MAP` changes the land (`Game::ChangeLand`); not checked in play against the original; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The creature may go near an incoming vortex only when the script allows it through and it is on the leash, or when a script is moving it; otherwise it keeps away | partial 70% | property 36 (`Creature::mayGoThroughVortex`), `vortex::CreatureMayBeTaken` and `CreatureMustAvoid`; the routes' obstacle planner is not ported; re-measured 2026-10-21: the property and the leash rule are in; the routes' obstacle planner is not |
| A creature allowed near that comes within seven tenths of the vortex's width sparkles out over 3 seconds and stays invisible; nothing can make it sparkle back in that land | done | `TakeIn`'s creature branch and the fizz system (unverified against v1.42) |
| In the next land the arrival script loads the creature at a given spot, turned to face the vortex; it appears out of sparkles over 3 seconds | partial 70% | `LoadMyCreature` is real (`src/ECS/PlayerCreature.cpp`), making the profile's creature at the spot; the sparkle-in is not ported; re-measured 2026-10-21: loaded at the spot; the sparkle-in is not ported |
| Loading the creature does nothing if the player already has one: Land 3 has already put it in Lethys's prison, so it does not come out of that land's vortex | done | `LoadMyCreature` does nothing when the player leads a creature already; see [portals_per_land.md](./portals_per_land.md#land-3-arrival) |
| Only the spot's two map coordinates are used; the creature stands on the ground there | done | `LoadMyCreature` takes x and z and puts the creature on the ground at the middle of that cell |
| What the creature holds and its leash are not kept | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |

## What crosses and what is left

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clearing the land saves the player's creature, then deletes every object, town, building and effect of the old land | partial 80% | `Game::ChangeLand` saves the player's creature and its physique first, then `Game::LoadMap` resets the land; see [land-change-and-saving.md](../../../scripts/land-change-and-saving.md#in-openblack); re-measured 2026-10-21: saved first, then the land is reset; see the wiki |
| Whatever is in the hand when the land is cleared is lost (a held miracle too); only what went into the vortex crosses | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| Buildings, towns, the temple, the worship site and the storehouse's stock stay behind; the next land's script builds a new temple and village centre part-built | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| Prayer power does not cross: the new temple starts with its own (unconfirmed) | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| The player's alignment crosses: later lands check it (for example Land 5's opening) | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| Each player's power with each tribe is reset to the start, and each player's leftover miracle counts are cleared | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| Computer gods are restarted from scratch | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| Script globals survive the land change, so later lands can ask what happened (for example whether Lethys was killed) | done | `LOAD_MAP` changes the land (`Game::ChangeLand`) and the scripts' variables survive it, as in the original (`LandLoadThings` in `test_land_load_globals`); see [land-change-and-saving.md](../../../scripts/land-change-and-saving.md#in-openblack) |
| Bookmarks, highlights, the camera's stack, physics, game statistics, fireflies, special villagers, the camera's no-go zones and the town-building help are cleared | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| The help system's time played is reset with the land | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| The believers count, town happiness and the creature's place in the temple do not cross: they belong to the old towns and temple | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The next land's script makes an outgoing vortex at its arrival point and tells it the home town, the gathering place, a distance and a radius, and an optional flock | pending | `VortexParameters` is real (`ecs::vortex::SetParameters`); the arrival scripts are not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached |
| If no town is given the outgoing vortex uses the nearest town within 200 | done | `ProcessOut` takes the nearest town within 200 (`map_cells::GetNearestTown`) |
| Every second turn the outgoing vortex brings out one thing: it reads the next line of the crossing file, makes the object 16 from the middle at a random angle and throws it outwards | partial 80% | `ProcessOut` emits every even turn at a cell 16 away at a random angle and flings it; the crossing file's reader is ported (`src/ECS/VortexSave.cpp`) and reads `SaveGame/vortex.txt` under the game's folder; for each thing made it counts the object, the wood and food it holds and the villagers in the vortex's statistics, as the original (only a saved game reads them); a run of the checks never writes the file; re-measured 2026-10-21: emitted every even turn from the file; see the notes for what is counted |
| Each thing is thrown out at 8 to 13 outwards and 10 to 15 upwards, spinning end over end faster the shorter it is, as if thrown by the vortex's god | done | `particle_carried_objects::Fling` from `HandOver`; see [vortex.md](../../../world/vortex.md#bringing-them-out-the-out-vortex) |
| When the file runs out it keeps making new villagers until 30 villagers in all have come out, counting those that came through | done | `ProcessOut` counts every villager out and stops at 30 |
| New villagers are of the home town's tribe (Norse when there is no town): half are housewives, the rest a forester, fisherman, farmer, shepherd or leader at random — never a trader | done | `ProcessOut` picks the town's tribe (Norse without one), half housewives, the rest from the five jobs |
| Every fifth new villager is a child (age 1 to 9); the others are adults (16 to 21) | done | `ProcessOut`: every fifth a child of 1 to 9, the others 16 to 21 |
| Villagers that come out leave whatever town they were made in and join the vortex's town | done | `HandOver` moves a villager from its old town into the vortex's (`town_villagers`) |
| With a flock given, villagers that come out become disciples "from the vortex", standing in a crowd, and join the script's flock | done | `HandOver`: FROM_VORTEX (`villager::SetVillagerDisciple`) and the script's flock (`flocks::AddMember`) (unverified against v1.42) |
| A villager that comes out while a script owns the flock is handed to the script | done | `HandOver`: registered, referenced and under the script's control (`script_held::SetControlledByScript`) (unverified against v1.42) |
| Animals that come out are put in one flock the vortex makes at the gathering place, with the given distance and radius; one flock serves every kind of animal | done | `HandOver`: pointed at the flock (`flocks::SetFlock`, not a member), made with the flock creators' info and the script flocks' id, its distance and radius truncated, and the vortex's town (`animal_ai::SetTown`) (unverified against v1.42) |
| Doves that come out are not followed by the vortex | partial 60% | `HandOver` does not fling a dove; following it was not checked; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| Everything thrown out is indestructible, and so cannot be eaten, until the vortex has gone | done | `HandOver` gives it `Indestructible`, which is also what keeps a thing from being hunted or eaten (`script_held::CannotBeEaten`; test `VillagerCoreTest.WhatCannotBeDestroyedCannotBeEaten`); `OnDeleted` takes it off everything thrown (unverified against v1.42) |
| Villagers that came out are kept at full health every turn until the vortex has gone | done | `ProcessOut` sets every thrown villager's life to full each turn until the vortex goes |
| Villagers thrown out by a vortex play the thrown-from-a-vortex animation while they fly | done | `villager_animation::ThrownClip` with `vortex::IsVillagerBeingThrown` (clip 401) (unverified against v1.42) |
| A fade-out started while the vortex still has things to throw waits: the fade only begins 7 seconds after the last thing is out | done | `ProcessOut` holds the state's turn while it still has something to emit, so the fade waits |
| The arrival scripts start the fade-out 15 seconds after making the vortex and count it as closed 8 seconds later | pending | `VortexFadeOut` is real; the arrival scripts are not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached |
| Food and wood come out as loose pots beside the vortex, not in the storehouse; villagers carry them home like any loose pots | partial 60% | the pots' lines are written and read back (`src/ECS/VortexSaveLines.cpp`, `src/ECS/VortexSave.cpp`) and thrown out by `HandOver`; a whole crossing in play is not checked against the original (unverified against v1.42); re-measured 2026-10-21: ported; not checked against the original in a run yet |
| One-shot globes, toys, rocks, trees, scaffolds and artefacts come out as they went in | partial 60% | globes, rocks (pieces knocked off buildings too), trees, scaffolds of their town (`CREATE_SCAFFOLD`) and artefacts are written and made again by the reader (`src/ECS/VortexSaveLines.cpp`, `src/ECS/VortexSave.cpp`); toys have no line (undefined in the original); a whole crossing in play is not checked against the original (unverified against v1.42); re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The outgoing vortex spreads newcomers: Land 2 27 within 48; Lands 3 and 4 20 within 80; Land 5 20 within 50 | pending | `VortexParameters` passes the two numbers to the vortex's animal flock; the arrivals are never reached |

## Going through

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player goes through by clicking the scroll over the vortex or the vortex itself | partial 70% | `CreateHighlight` and `GameThingClicked` are real; Land 1's scroll is made at once with the guide-skip SkipBox answers; re-measured 2026-10-21: real natives; only Land 1's scroll is reached |
| On Lands 2 to 4 an advisor comes out and points at the scroll at most every 30 seconds while the camera is within 100 and it is in view; Land 1 instead repeats a reminder every 30 minutes | partial 60% | the notify scripts call real natives; see the per-land rows and [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md); re-measured 2026-10-21: the script calls only natives that work; not checked in a run yet |
| Each land then dives the camera into the vortex and fades to black, its own way | partial 60% | `MoveCameraPosition`, `RunCameraPath` and `SetFade` are real; see [portals_per_land.md](./portals_per_land.md); re-measured 2026-10-21: real natives; see the per-land page |
| Fades to and from black | done | `SetFade`, `SetFadeIn` in `src/CHLApi.cpp` (`ScreenFade`) |
| When the crossing ends all the land's scripts are stopped except the story's control script, and the next land is loaded behind a loading screen | partial 60% | `StopAllScriptsExcluding` is real; `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original; re-measured 2026-10-21: ported; not checked against the original in a run yet |
| The game clears the land, loads the next one's set-up file, then assigns the towns' features | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but this part of the change is not checked against the original |
| The story's control script is started by the game itself and runs the five lands in turn | partial 60% | `Game.cpp` starts `LandControlAll`, which runs `LandControl1`; it goes on to Land 2 through `LOAD_MAP` (`Game::ChangeLand`) once Land 1 ends, but the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |

## Script commands for vortices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Create a vortex of a kind (in, out, volcano) at a place | done | `CREATE` of a vortex calls `ecs::vortex::Create` (In, Out or Volcano; any other kind makes nothing) |
| Set the outgoing vortex's town, gathering place, distance, radius and flock; a bad vortex or town is reported as an error | done | `VortexParameters` in `src/CHLApi.cpp` (`ecs::vortex::SetParameters`), with its errors |
| Start a vortex's fade-out; a thing that is not a vortex is reported as an error | done | `VortexFadeOut` in `src/CHLApi.cpp` (`ecs::vortex::StartFadeOut`, nothing for a thing that is not a vortex) |
| Allow or forbid the creature to go through a vortex | done | property 36 in GET_PROPERTY and SET_PROPERTY (`src/CHLApi.cpp`) (unverified against v1.42) |
| Load the player's creature at a place | done | `LoadMyCreature` (`src/ECS/PlayerCreature.cpp`); see [players-creature.md](../../../creature/players-creature.md#the-players-creature) |
| Load another land | done | `LOAD_MAP` changes the land (`Game::ChangeLand`); see [land-change-and-saving.md](../../../scripts/land-change-and-saving.md#in-openblack) |
| The scripts keep a "vortex open" flag that other scripts wait on | done | ordinary script globals in the virtual machine |
| Stop all scripts but the named ones | done | `StopAllScriptsExcluding` in `src/CHLApi.cpp` |

## Opening the exit (per land)

Each land's exit, with its conditions, scenes, dialogue and quirks, is in
[portals_per_land.md](./portals_per_land.md).

## Arrival scenes

Each land's arrival scene is in [portals_per_land.md](./portals_per_land.md).
