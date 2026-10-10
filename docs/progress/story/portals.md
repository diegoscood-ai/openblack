# Portals between lands

At the end of each land a swirling vortex opens in the ground. Whatever it pulls in or is thrown into it (villagers,
animals, food and wood, one-shot miracles, rocks, toys, trees, scaffolds) is written to a crossing file, the player's
creature is carried over in its own files, and clicking the vortex's scroll flies the camera down into it and loads the
next land, where an outgoing vortex throws everything back out beside the new home town and tops the newcomers up to
30 villagers. The same kind of object makes the glowing crater of Land 5's volcano. Each land's own vortices (where,
when, what is said, what differs) are in [portals_per_land.md](./portals_per_land.md); the quests that open them are in
[gold_scrolls/](./gold_scrolls/); the lands in [land_1.md](./land_1.md) to [land_5.md](./land_5.md). Script commands in
general are in [../scripts/](../scripts/).

**Progress: 28/136 done, 37 partial — 34%**

How the original does it, in our wiki: [Vortexes and the tornado: objects swallowed, carried and flung](../../bw1-notes/vortex.md).

Sources: the executable (the vortex objects, the crossing file, the land change, the creature's files), the shipped
challenge scripts' source text and the game's tables. openblack is judged on this tree: it has the vortex object
(`src/ECS/Vortex.cpp`) and writes what a vortex takes in for the next land (`src/ECS/VortexSave.cpp`), but the next land
is never loaded (`LOAD_MAP` does nothing), so much here is still to do.

## The three kinds of vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An incoming vortex (the exit from a land) pulls things in and writes each one to the crossing file for the next land | partial | the In is made and run each turn (`ecs::vortex`, `src/Magic/MagicLoop.cpp`); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`); see [vortex.md](../../bw1-notes/vortex.md#taking-objects-in-the-in-vortex) |
| An outgoing vortex (the arrival in a land) reads the crossing file back, throws each thing out, then makes new villagers | partial | the Out's emission is ported and wired (`ProcessOut`): every second turn it makes new villagers up to 30; the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing; see [vortex.md](../../bw1-notes/vortex.md#bringing-them-out-the-out-vortex) |
| A volcano vortex is the glowing mouth of Land 5's volcano; it pulls nothing in and throws nothing out, and flying things pass straight through it | partial | the Volcano is made and has no contents and leaves the land alone (`ecs::vortex`); its effects, mesh and sound are not made |
| Every vortex is made with the same pull radius, 50 (five land cells), whatever its kind | done | `ecs::vortex::Create` gives every kind a radius of 50; `test/test_vortex.cpp` |
| A vortex is an object the creature cannot pick up, throw, eat, stomp on, set fire to, fight, examine, destroy by stoning or put in a store, and it cannot act as a container | todo | the creature's own rules for a vortex were not found in our creature code |
| A vortex is not touched by miracles or other effects | partial | a vortex is not an effect receiver in our tree either (no effect component); not checked against every miracle |
| The hand never takes a vortex and does not rest on its model; a vortex can never be put in the hand | partial | the hand's cursor ignores a vortex (`HandSystem.cpp`); whether every pick-up path refuses it was not checked |
| An incoming vortex is solid to flying things: a thrown object hits it rather than passing through, and is not lifted up over it | todo | `vortex::ReactToPhysicsImpact` exists but nothing in our physics calls it, and the vortex has no body |
| The creature keeps away from an incoming vortex unless it may go in (see [The creature](#the-creature)) | todo | no creature crosses: the In's creature fizz is not ported and no land change happens |
| Scripts see a vortex as a vortex object and can find one at or near a place | partial | `CREATE` returns the vortex to the script and `VortexFadeOut`, `VortexParameters` find it; searching by place uses the general `CallNear` |
| The vortex is saved and loaded with a saved game, with its state, its fade, its counts and what it is in the middle of throwing | todo | openblack has no saved games |

## The vortex table

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack reads the vortex table (particles, starting state, fade, scale, limits) from the game's info file | done | `InfoConstants::vortex` (three rows) is read, and `ecs::vortex::Create` takes its starting state |
| The incoming vortex starts by fading in; the outgoing and volcano vortices start fully open | done | `ecs::vortex::Create` sets each kind's starting state from its row; fades in `ecs::vortex::FadeValue` (`test/test_vortex.cpp` Vortex.Fade); see [vortex.md](../../bw1-notes/vortex.md#states-and-fades) |
| Each kind has its base size: 0.28 for the land vortices, 0.75 for the volcano; the model and the ground ring are scaled by it | todo | the vortex mesh is not drawn yet ([vortex.md](../../bw1-notes/vortex.md#drawing)) |
| Each kind names its own four effects: before the land is drawn, after it, the object mover and the glow on the ground | partial | the effect names are mapped in `src/Particles/ParticleTypes.cpp`; the vortex makes none of them |
| The table's "fade when deleted" column (0 / 1 / 0) is never read: every vortex fades out only when a script tells it to | done | our `ecs::vortex` reads no such column: deletion comes only from a fade-out |
| The table's limits (at most 1000 objects for the outgoing vortex, counts of food, wood, villagers and one-shots) are never read; the only limit is a fixed 30 villagers (see [Arriving](#arriving)) | done | our Out stops at a fixed 30 villagers (`ProcessOut`) and reads none of the limits |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex draws its own model, the spinning vortex cylinder, scaled by its base size | todo | pending: the vortex mesh ([vortex.md](../../bw1-notes/vortex.md#drawing)) |
| Before the land is drawn, a spinning funnel of two swirl textures turns on the spot (the "before" effect) | todo | the vortex makes no particle systems yet |
| After the land is drawn, a cloud of stars spins over it (the "after" effect), the incoming and outgoing vortex sharing it | todo | the vortex makes no particle systems yet |
| The outgoing vortex uses the incoming vortex's "before" effect (the game's table points both at the same file) | done | `src/Particles/ParticleTypes.cpp` maps the outgoing "before" type to the incoming file |
| A light map paints a glow on the ground around the vortex | todo | the vortex makes no light map yet |
| A ring texture and its alpha mask are laid on the land block under the vortex, scaled to the vortex's base size: the multi-ring base for the land vortices, the volcano's own base and mask for the crater | todo | pending: the ground decal ([vortex.md](../../bw1-notes/vortex.md#drawing)) |
| An incoming or outgoing vortex levels the ground under it: in an 11-by-11-cell square around it the land is pulled towards the square's average height, fully within 50 of the middle and fading to nothing at 56 | done | ported: `FlattenLand` and `LandOffset` (`src/ECS/Vortex.cpp`), tests Vortex.LandFactor and Vortex.LandOffset; see [vortex.md](../../bw1-notes/vortex.md#the-land-under-a-vortex) |
| The levelling grows with the fade-in, eased out (one minus the square of what is left), and is only ever added to, never undone, so the flattened pad stays after the vortex has gone | done | `LandFactorValue` grows only and the fade-out counts as complete; see [vortex.md](../../bw1-notes/vortex.md#the-land-under-a-vortex) |
| The volcano vortex does not change the land | done | `FlattenLand` skips the Volcano |
| The volcano vortex has its own before, after and light-map effects (fire and rock) | partial | the effect files are mapped in `src/Particles/ParticleTypes.cpp`; nothing places them |
| Things being pulled in are carried by the object-mover effect: each spirals round the middle, closing in as it ages and rising on a curve, and vanishes when it reaches the middle | todo | the object-mover spiral is pending (the particle rule) |
| A vortex fades in over 7 seconds and is then fully open; told to fade out, it takes 7 seconds and then deletes itself | done | `FadeValue` and `ProcessAll`: Active after 7 s, deleted 7 s after a fade-out starts (`test/test_vortex.cpp` Vortex.Fade); see [vortex.md](../../bw1-notes/vortex.md#states-and-fades) |

## How it sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The vortex hums: a looping vortex sound starts with the "before" effect of the incoming vortex and the "after" effect of the outgoing one, released softly when they stop | todo | the vortex makes no particle systems, so no hum |
| The volcano vortex has a looping volcano rumble of its own, started when it is made | todo | the volcano's sound is not made yet |
| Villagers thrown out of an outgoing vortex scream only for their first 10 turns of flight (15 for an ordinary throw) | todo | no clip-sound rule for villagers thrown out of a vortex was found in our tree |
| A creature that reaches the middle of a vortex makes the teleport "energise" sound as it starts to sparkle away | todo | no creature crosses: the In's creature fizz is not ported and no land change happens |
| Land 4's hidden vortex rumbles: a screen-rumble sound with every camera shake | todo | `ShakeCamera` and `PlaySoundEffect` are real; Land 4 is never reached; see [portals_per_land.md](./portals_per_land.md#land-4-exit) |
| The hand over a vortex sends its own force-feedback effect | n/a | force-feedback mice are not supported (see ../pc_integration/online_services.md) |

## Being sucked in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only a fully open incoming vortex pulls: not while it fades in or out | partial | `ProcessIn` returns unless Active; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Every turn it searches the cells around it in a spiral out from its own cell, stopping at the first cell beyond its reach (or after 9999 cells) | partial | the spiral walk is ported (`ProcessIn`, `map_coords::Spiral`); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| On two turns out of three it only reaches half its radius (25); on every third turn the whole 50 | partial | the 50 / 25 rule is ported (`ProcessIn`); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| On each cell it takes every thing whose own cell it is and that may be sucked in; a thing that may be sucked in is anything that could be thrown or knocked flying (see [What goes in](#what-goes-in-kind-by-kind)) | partial | `CanBeSuckedIntoVortex` uses `CanBecomeAPhysicsObject`; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| A thing thrown into the vortex that hits its model while it is open is taken at once, keeping its speed and turn as it starts to spiral | todo | `ReactToPhysicsImpact` is ported but never called by our physics |
| Things merely carried over the vortex in the hand are not taken: nothing in the hand is on the ground | partial | the same rule (nothing in the hand is in a map cell); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Food and wood piles too big to fly are drained instead: each time its cell is searched a pile gives up a pot of up to 1000 of its food or wood (of the kind dropped from the hand), and that pot is sucked in | todo | the pile split is pending (`ProcessIn` notes it) |
| Pots made from a pile are drawn at 30% to 42% of their usual size at random as they fly in | todo | the pile split is pending |
| Nothing walks into the vortex on purpose: no villager, disciple or animal is ever sent to it; only those that wander within reach, are dropped or thrown in, or stand where it opens are taken | partial | our tree sends nobody to a vortex either; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Villagers hiding in a building (the run-and-hide state), villagers inside buildings and villagers in the hand are never taken | partial | only map-cell objects are walked; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Each thing taken is handed to the object-mover effect; if the vortex has no object mover, nothing is taken | done | `TakeIn` takes nothing without the object mover, as the original; in our tree it is never made, so nothing is taken |
| A thing is written to the crossing file the moment it is caught, before it is seen to fly in; a villager leaves its town and an animal drops everything that depends on it at that moment | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`). Our wiki differs: what arrives at the end of the spiral is what gets written, not what is caught ([vortex.md](../../bw1-notes/vortex.md#taking-objects-in-the-in-vortex)) |
| A thing that is indestructible (made so by a script, or one still being thrown out by an outgoing vortex) is pulled round and in like any other but is not written down: at the middle it is thrown back out at a random angle | todo | the script-held flag is pending in `TakeIn`; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| A creature close to the middle of the vortex (within seven tenths of the vortex's own width) sparkles away over 3 seconds instead; it is never written to the crossing file | todo | the creature's fizz is not ported (`TakeIn` notes it) |
| Things in flight towards the vortex are not counted as thrown by anyone and do not react | partial | our tree records no thrower for them; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| The player can throw things in by hand; the advisors tell the player to send villagers, food, wood and followers in | partial | the advisors' lines are real natives; see [portals_per_land.md](./portals_per_land.md) and [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |

## What goes in, kind by kind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers go in, alive or dead; each is written as its tribe and job (or its exact kind, for a villager of no tribe), its age, its place relative to the vortex and the number of its old town | todo | the villager writer is pending (`lhscriptx::WriteCommand` covers the physics classes); the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Disciples go in as plain villagers: their discipleship is not kept | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Animals go in (cows, sheep, pigs, horses, wild animals and birds); each is written as its kind, its age, its flock and its flock's town | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Doves are pulled in but never written down: they are lost | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| The player's creature, other gods' creatures and story creatures never go into the crossing file | partial | `CanBeSuckedIntoVortex` refuses creatures in our tree |
| Food and wood dropped from the hand (hand piles) go in whole, as a pot of their kind and amount | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Big wood piles, food piles and magic food and wood piles cannot fly: they are drained into pots of up to 1000 (above) | todo | the pile split is pending |
| A pot that is part of a structure is never written down | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| One-shot miracle globes go in, each written as its miracle with the command the lands use for powered-up globes | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`); `CREATE_ONE_SHOT_SPELL_PU` itself is real (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Seeds made from worship-site or village-centre icons and spell icons themselves never go in | partial | they cannot become physics objects in our tree either; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Rocks, fragments and other loose statics go in, each written as its kind, size and turn | todo | the physics classes' writers exist (`lhscriptx::WriteCommand`, see ../../bw1-notes/land-script-save.md), but the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Toys go in and come out in the next land as the same toy | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| An artefact goes in and comes out still an artefact of the same god with the same worth; only its town is forgotten | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Loose scaffolds go in and come out as scaffolds of the same kind; a scaffold in a workshop's yard, on a building site or tied to a building plan is never taken | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Living trees within reach are uprooted and go in, each written as its kind and size, and grow again in the next land | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Dead and felled trees go in, keeping their kind, size and owning god | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Field crops, reward chests that are ready to open, barrels, balls and other loose objects go in, each written as its kind, size and turn | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| A loose object stuck to something (carried, or fixed to another object) is pulled in but not written down | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Buildings, fields, the temple, worship sites, village centres, wonders, spell dispensers, storehouses, lanterns, bonfires, fires, shields, fireballs, the Creed, whales and other vortices are never taken | partial | only objects that can become physics objects pass `CanBeSuckedIntoVortex`; the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Other gods' villagers, animals and things are taken just like the player's; a villager keeps nothing of its old god | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |
| Objects are written in the order they are caught, and nothing limits how many | todo | the In's take is ported (`ecs::vortex::ProcessIn`, `TakeIn`) but takes nothing yet: it needs the vortex's object-mover particle system, which is never made (`src/ECS/Vortex.cpp`) |

## The crossing file

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What goes into an incoming vortex is written to a crossing file in the save folder, one line per object, in the same language as the lands' own set-up files | todo | the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| Each line places the object relative to the vortex, so things come out spread round the arrival vortex as they lay round the exit | partial | the reader places each line at the emission point (`vortex_save::ReadNext` with the position offset); the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| Every new incoming vortex empties the file when it opens, so only the last incoming vortex of a land counts | todo | the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| An outgoing vortex opens the same file for reading when it is made; if there is no file it only makes new villagers | partial | the Out opens the reader when made (`vortex_save::OpenReader`) and makes only villagers without one; with no save folder there is never a file |
| Nothing else about the land is written to the file: the creature, the hand, towns, buildings and statistics are handled elsewhere | partial | the same: only objects; the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |

## Saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Saving a game while a vortex is open copies the crossing file beside the save and records how far it has been read | todo | openblack has no saved games |
| Loading that save copies the crossing file back, so a crossing can be saved and resumed from either side | todo | openblack has no saved games |
| A loaded incoming vortex carries on adding to the end of the file; a loaded outgoing vortex skips the lines it had already thrown out | todo | openblack has no saved games |
| The vortex's state, fade, gathering place, flock, town, counts and the list of things it is throwing are saved and restored | todo | openblack has no saved games |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature crosses in its own files: when the land is cleared its mind and its body are saved to the creature-mind folder under the player's name | todo | our tree reads a creature's mind file (`CreatureMindModel`, `--creature-file`), but nothing writes it on a land change; see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The mind file keeps what it has learnt, its alignment, its age, size, energy and other body values and its tattoos; the body file keeps its species, its look, its strength and two more body values | partial | the mind file's body part is read (`src/Creature/CreatureMindFileBody.cpp`); nothing writes it |
| A creature the game has marked as not the player's own to keep is not saved | todo | no creature crosses: the In's creature fizz is not ported and no land change happens |
| The creature does not physically go through: walking it into the vortex only makes it sparkle away; it crosses because it is saved when the land is cleared, wherever it stands | todo | no creature crosses: the In's creature fizz is not ported and no land change happens |
| The creature may go near an incoming vortex only when the script allows it through and it is on the leash, or when a script is moving it; otherwise it keeps away | todo | the permission (property 36) is not handled by `SetProperty` and our creature does not keep away from vortices |
| A creature allowed near that comes within seven tenths of the vortex's width sparkles out over 3 seconds and stays invisible; nothing can make it sparkle back in that land | todo | no creature crosses: the In's creature fizz is not ported and no land change happens |
| In the next land the arrival script loads the creature at a given spot, turned to face the vortex; it appears out of sparkles over 3 seconds | partial | `LoadMyCreature` is real (`src/ECS/PlayerCreature.cpp`), making the profile's creature at the spot; the sparkle-in is not ported |
| Loading the creature does nothing if the player already has one: Land 3 has already put it in Lethys's prison, so it does not come out of that land's vortex | done | `LoadMyCreature` does nothing when the player leads a creature already; see [portals_per_land.md](./portals_per_land.md#land-3-arrival) |
| Only the spot's two map coordinates are used; the creature stands on the ground there | done | `LoadMyCreature` takes x and z and puts the creature on the ground at the middle of that cell |
| What the creature holds and its leash are not kept | todo | `LOAD_MAP` is an empty native, so no land change happens |

## What crosses and what is left

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clearing the land saves the player's creature, then deletes every object, town, building and effect of the old land | partial | `Game::LoadMap` (`src/Game.cpp`) resets the land, but `LOAD_MAP` is an empty native, so no land change happens and the creature is not saved |
| Whatever is in the hand when the land is cleared is lost (a held miracle too); only what went into the vortex crosses | todo | `LOAD_MAP` is an empty native, so no land change happens |
| Buildings, towns, the temple, the worship site and the storehouse's stock stay behind; the next land's script builds a new temple and village centre part-built | todo | `LOAD_MAP` is an empty native, so no land change happens |
| Prayer power does not cross: the new temple starts with its own (unconfirmed) | todo | `LOAD_MAP` is an empty native, so no land change happens |
| The player's alignment crosses: later lands check it (for example Land 5's opening) | todo | `LOAD_MAP` is an empty native, so no land change happens |
| Each player's power with each tribe is reset to the start, and each player's leftover miracle counts are cleared | todo | `LOAD_MAP` is an empty native, so no land change happens |
| Computer gods are restarted from scratch | todo | `LOAD_MAP` is an empty native, so no land change happens |
| Script globals survive the land change, so later lands can ask what happened (for example whether Lethys was killed) | todo | `LOAD_MAP` is an empty native, so no land change happens; the VM's globals would survive |
| Bookmarks, highlights, the camera's stack, physics, game statistics, fireflies, special villagers, the camera's no-go zones and the town-building help are cleared | todo | `LOAD_MAP` is an empty native, so no land change happens |
| The help system's time played is reset with the land | todo | `LOAD_MAP` is an empty native, so no land change happens |
| The believers count, town happiness and the creature's place in the temple do not cross: they belong to the old towns and temple | todo | `LOAD_MAP` is an empty native, so no land change happens |

## Arriving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The next land's script makes an outgoing vortex at its arrival point and tells it the home town, the gathering place, a distance and a radius, and an optional flock | todo | `VortexParameters` is real (`ecs::vortex::SetParameters`); the arrival scripts are never reached (`LOAD_MAP` is an empty native, so no land change happens) |
| If no town is given the outgoing vortex uses the nearest town within 200 | done | `ProcessOut` takes the nearest town within 200 (`map_cells::GetNearestTown`) |
| Every second turn the outgoing vortex brings out one thing: it reads the next line of the crossing file, makes the object 16 from the middle at a random angle and throws it outwards | partial | `ProcessOut` emits every even turn at a cell 16 away at a random angle and flings it; the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| Each thing is thrown out at 8 to 13 outwards and 10 to 15 upwards, spinning end over end faster the shorter it is, as if thrown by the vortex's god | done | `particle_carried_objects::Fling` from `HandOver`; see [vortex.md](../../bw1-notes/vortex.md#bringing-them-out-the-out-vortex) |
| When the file runs out it keeps making new villagers until 30 villagers in all have come out, counting those that came through | done | `ProcessOut` counts every villager out and stops at 30 |
| New villagers are of the home town's tribe (Norse when there is no town): half are housewives, the rest a forester, fisherman, farmer, shepherd or leader at random — never a trader | done | `ProcessOut` picks the town's tribe (Norse without one), half housewives, the rest from the five jobs |
| Every fifth new villager is a child (age 1 to 9); the others are adults (16 to 21) | done | `ProcessOut`: every fifth a child of 1 to 9, the others 16 to 21 |
| Villagers that come out leave whatever town they were made in and join the vortex's town | done | `HandOver` moves a villager from its old town into the vortex's (`town_villagers`) |
| With a flock given, villagers that come out become disciples "from the vortex", standing in a crowd, and join the script's flock | partial | `HandOver` adds them to the script flock; the disciple from the vortex is pending |
| A villager that comes out while a script owns the flock is handed to the script | partial | `HandOver` adds them to the flock; handing them to the script is pending |
| Animals that come out are put in one flock the vortex makes at the gathering place, with the given distance and radius; one flock serves every kind of animal | partial | `HandOver` points animals at the vortex's animal flock; its info, player and town are pending |
| Doves that come out are not followed by the vortex | partial | `HandOver` does not fling a dove; following it was not checked |
| Everything thrown out is indestructible until the vortex has gone | partial | the thrown list is kept (`thrownVillagers`); the indestructible mark is pending (`HandOver` notes a flag bit) |
| Villagers that came out are kept at full health every turn until the vortex has gone | done | `ProcessOut` sets every thrown villager's life to full each turn until the vortex goes |
| Villagers thrown out by a vortex play the thrown-from-a-vortex animation while they fly | todo | no vortex clip choice for thrown villagers was found in our tree |
| A fade-out started while the vortex still has things to throw waits: the fade only begins 7 seconds after the last thing is out | done | `ProcessOut` holds the state's turn while it still has something to emit, so the fade waits |
| The arrival scripts start the fade-out 15 seconds after making the vortex and count it as closed 8 seconds later | todo | `VortexFadeOut` is real; the arrival scripts are never reached (`LOAD_MAP` is an empty native, so no land change happens) |
| Food and wood come out as loose pots beside the vortex, not in the storehouse; villagers carry them home like any loose pots | todo | the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| One-shot globes, toys, rocks, trees, scaffolds and artefacts come out as they went in | todo | the crossing file's reader is ported (`src/ECS/VortexSave.cpp`), but with no save folder no file is opened, and nothing is ever written since the In takes nothing |
| The outgoing vortex spreads newcomers: Land 2 27 within 48; Lands 3 and 4 20 within 80; Land 5 20 within 50 | todo | `VortexParameters` passes the two numbers to the vortex's animal flock; the arrivals are never reached |

## Going through

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player goes through by clicking the scroll over the vortex or the vortex itself | partial | `CreateHighlight` and `GameThingClicked` are real; Land 1's scroll is made at once with the guide-skip SkipBox answers |
| On Lands 2 to 4 an advisor comes out and points at the scroll at most every 30 seconds while the camera is within 100 and it is in view; Land 1 instead repeats a reminder every 30 minutes | partial | the notify scripts call real natives; see the per-land rows and [gold_scrolls/leave_through_the_vortex_land_1.md](./gold_scrolls/leave_through_the_vortex_land_1.md) |
| Each land then dives the camera into the vortex and fades to black, its own way | partial | `MoveCameraPosition`, `RunCameraPath` and `SetFade` are real; see [portals_per_land.md](./portals_per_land.md) |
| Fades to and from black | done | `SetFade`, `SetFadeIn` in `src/CHLApi.cpp` (`ScreenFade`) |
| When the crossing ends all the land's scripts are stopped except the story's control script, and the next land is loaded behind a loading screen | partial | `StopAllScriptsExcluding` is real; `LOAD_MAP` is an empty native, so no land change happens |
| The game clears the land, loads the next one's set-up file, then assigns the towns' features | todo | `LOAD_MAP` is an empty native, so no land change happens |
| The story's control script is started by the game itself and runs the five lands in turn | partial | `Game.cpp` starts `LandControlAll`, which runs `LandControl1`; it can never go on to the next land (`LOAD_MAP` is an empty native, so no land change happens) |

## Script commands for vortices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Create a vortex of a kind (in, out, volcano) at a place | done | `CREATE` of a vortex calls `ecs::vortex::Create` (In, Out or Volcano; any other kind makes nothing) |
| Set the outgoing vortex's town, gathering place, distance, radius and flock; a bad vortex or town is reported as an error | done | `VortexParameters` in `src/CHLApi.cpp` (`ecs::vortex::SetParameters`), with its errors |
| Start a vortex's fade-out; a thing that is not a vortex is reported as an error | done | `VortexFadeOut` in `src/CHLApi.cpp` (`ecs::vortex::StartFadeOut`, nothing for a thing that is not a vortex) |
| Allow or forbid the creature to go through a vortex | todo | the property (36) is not handled by `SetProperty` |
| Load the player's creature at a place | done | `LoadMyCreature` (`src/ECS/PlayerCreature.cpp`); see [../../bw1-notes/creature.md](../../bw1-notes/creature.md#the-players-creature) |
| Load another land | todo | `LoadMap` is an empty native in `src/CHLApi.cpp` |
| The scripts keep a "vortex open" flag that other scripts wait on | done | ordinary script globals in the virtual machine |
| Stop all scripts but the named ones | done | `StopAllScriptsExcluding` in `src/CHLApi.cpp` |

## Opening the exit (per land)

Each land's exit, with its conditions, scenes, dialogue and quirks, is in
[portals_per_land.md](./portals_per_land.md).

## Arrival scenes

Each land's arrival scene is in [portals_per_land.md](./portals_per_land.md).
