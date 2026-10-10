# Land 1

The first land: the player's own island, where the people build the temple, the advisors teach the hand and the camera,
the player chooses and trains a creature, and the land ends with the player leaving through the vortex for Khazar's land.

Every silver scroll of the game, land by land, with scores: [silver_scrolls.md](silver_scrolls.md).

How the player can lose a land, and the game over: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 1/33 done, 19 partial — 32%**

How the original does it, in our wiki: [The Land 1 intro and the tutorial's script side](../../bw1-notes/intro.md).

## Opening and setup

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's map loads and its story begins with the land's control script | partial | `Game.cpp` loads `challenge.chl` and starts `LandControlAll`, which runs `LandControl1`; about half of the 464 natives are real (`src/CHLApi.cpp`). The intro is checked in game up to the hand-over; the land script's contents: see ../scripts/land1_script.md |
| Setup places the first did-you-know scrolls, sets the starting belief of the villages and the weather | partial | `SetupLand1` places the did-you-know scrolls (`CreateHighlight`) and sets the weather; `SetVirtualInfluence`, `BeliefForPlayer` and `SetPlayerBelief` are stubs in `src/CHLApi.cpp`, so the starting belief is not set |
| The family leads the player to their village ("follow us"); the mother waits if the hand falls behind and says so if it runs ahead | partial | `FollowUs` runs, checked in game up to the hand-over; the follow part after it is not checked; see tutorial.md |
| The villagers finish building the temple and the advisors show the player its entrance and how to go in | partial | `CitadelGuide` calls only real natives (camera, advisors, `SetInterfaceCitadel`, `InsideTemple`), not checked in game; see [gold_scrolls/the_temple_is_finished.md](gold_scrolls/the_temple_is_finished.md), ../temple/temple_exterior.md; a cut early draft: [silver_scrolls/see_the_citadel.md](silver_scrolls/see_the_citadel.md) |
| Camera zones keep the camera inside the parts of the land opened so far, widened as gates open | partial | `SetCameraZone` reads the zone files (`src/Camera/PlayerCameraScript`), but the player camera does not keep to them yet ([../../bw1-notes/script-camera.md](../../bw1-notes/script-camera.md#zones-and-fixed-rotation)); see ../camera/camera_limits.md |
| The hidden phone box of the first land, with its jokey recorded messages | todo | its script was not traced here; see ../scripts/challenge_scripts.md |
| A new game can skip the opening (straight to choosing a creature), skip all of the first land's story, or keep the old creature (patch 1.1) | done | the SkipBox (`src/Gui/SkipBox`) with its four answers and `CanSkipTutorial`, `CanSkipCreatureTraining`, `IsKeepingOldCreature`, `CurrentProfileHasCreature` (the `--creature-file` profile creature) are real; see [../../bw1-notes/map-loading.md](../../bw1-notes/map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |

## Choosing the creature (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "Choose Your Creature" — three gate stones, placed in turn (the tiger stone, the ape stone, then the cow stone carved from a blank rock), open the creatures' gates; the three creatures show off in their glade and the player picks one | partial | see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md): the trainer's scene uses real natives, but the gate-stone loop never ends (`ObjectInfoBits` is a stub) and the creatures are never made |
| Gold scroll: "The Lost Brother" — a woman's brother has wandered off sick; bringing him home earns the ape gate stone (worse alignment for killing or dropping him) | todo | never started: see [gold_scrolls/the_lost_brother.md](gold_scrolls/the_lost_brother.md) |
| Gold scroll: "The Sculptor" — the player brings a rock from the quarry, the sculptor carves the third gate stone from it | todo | never started: see [gold_scrolls/the_sculptor.md](gold_scrolls/the_sculptor.md) |
| Gate stones taken away or thrown into the sea come back to where they belong | partial | the guards (`ProtectGateKeys`) call only real natives, but the held read is not handled; see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md) |
| The advisors point out the gate stones and the quarry rock while the player hasn't got them | partial | real advisor natives; see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md) and [gold_scrolls/the_sculptor.md](gold_scrolls/the_sculptor.md) |
| The creatures of the glade the player didn't choose wander off | todo | the glade's creatures are never made (`CREATE` of a creature is not done); in the game the two not chosen are deleted during the fade, see [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md); see ../creature/ |

## The creature's learning (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: "The Creature's Learning" — Sable the trainer teaches the creature in five lessons: seeing its home pen, learning to eat, being slapped and stroked, the Leash of Learning, and tying the leash to a tree, where she also shows the leashes of aggression and compassion; the good-and-evil leash lesson itself is cut and never played | todo | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md): the skip branch of the first lesson runs in our game; the lessons themselves are never reached in a no-skip game: Choose Your Creature waits for ever at the gate stones (`ObjectInfoBits` is a stub) |
| Each stage has its own scroll, reminder and advisor lines, and the next waits for the last | partial | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md); a no-skip game never gets there: Choose Your Creature waits for ever at the gate stones (`ObjectInfoBits` is a stub) |
| After the last lesson Sable joins the player's Norse village as an ordinary villager; after the earlier lessons she goes back into the temple, or vanishes | partial | see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md#the-trainers-exit-and-aftermath); a no-skip game never gets there: Choose Your Creature waits for ever at the gate stones (`ObjectInfoBits` is a stub) |
| How the creature learns from these lessons | todo | the creature teaching natives are stubs; see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md), ../creature/lessons_and_help.md, ../creature/feeding_and_thrown_things.md |

## The big creature and the storm (gold)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide (a big creature) wanders the land and asks to meet the player's creature | todo | the guide is never made (`CREATE` of a creature is not done); see [creature_guide.md](./creature_guide.md) |
| The guide teaches the creature to impress a village and then to do it on its own | todo | see [creature_guide.md](./creature_guide.md) |
| The guide teaches the creature to fight, healing both after the bout | todo | see [creature_guide.md](./creature_guide.md) |
| The storm ends the land's lessons: Nemesis's storm kills the guide and the scripts of the land wind down | todo | see [creature_guide.md](./creature_guide.md) |

## Silver scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Silver scroll: "The Ogre" — an ogre's guardian stone blocks the way; the player's creature fights the ogre (or puts him to sleep) for the reward | todo | see [silver_scrolls/the_ogre.md](silver_scrolls/the_ogre.md); the ogre is made from a creature (`CreatureCreateRelativeToCreature` is a stub) and the reward is a stub |
| Silver scroll: "The Saviour" — a freak wave leaves five men drowning; only the creature is tall enough to wade out and save them | partial | see [silver_scrolls/the_saviour.md](silver_scrolls/the_saviour.md); `CreatureSavingPeople` needs only `GetTownWithId` and the log among stubs, but needs a creature |
| Silver scroll: "Throwing Stones" — a target game of throwing rocks at targets (and not at the houses) | partial | see [silver_scrolls/throwing_stones.md](silver_scrolls/throwing_stones.md); started after the temple scene; `GameThingHit` and the log are stubs |
| Silver scroll: "The Lost Flock" — a shepherd's sheep have strayed; bring them back to his pen (better alignment the more come back) | partial | see [silver_scrolls/the_lost_flock.md](silver_scrolls/the_lost_flock.md); started after the temple scene; `PopulateContainer`, `CallNearInState` and `InCreatureHand` are stubs |
| Silver scroll: "The Pied Piper" — a piper lures the village's children into his cave; the creature leashes him, drags him out and carries him back to free them (eating or drowning him is the evil ending) | partial | see [silver_scrolls/the_pied_piper.md](silver_scrolls/the_pied_piper.md); started only past the creature gate (skip games); `SetScriptStatePos`, `SetScriptFloat` and the log are stubs |
| Silver scroll: "The Hermit" — a hermit won't worship until he sees a huge creature; impressing him, damaging his hut or killing him changes the alignment | partial | see [silver_scrolls/the_hermit.md](silver_scrolls/the_hermit.md); started only past the creature gate (skip games); creature natives and wander natives are stubs |
| Silver scroll: "The Explorers" — missionaries want a boat to sail away in and sing three verses while it is made; the player helps them leave | partial | see [silver_scrolls/the_explorers.md](silver_scrolls/the_explorers.md); started only past the creature gate; `SexIsMale`, `GetObjectHeld`, `InCreatureHand`, `GetResource` are stubs |
| Silver scroll: "The Singing Stones" — a circle of stones sings when they are put back in the right order | partial | see [silver_scrolls/the_singing_stones.md](silver_scrolls/the_singing_stones.md); `SingingStoneCircle` is started by `LandControl1` at once and calls only `UpdateSnapshot` among stubs |
| Silver scroll: "The Immersion Mushrooms" — a man wants the most powerful mushroom, the one that shakes most, for an experiment | todo | see [silver_scrolls/the_immersion_mushrooms.md](silver_scrolls/the_immersion_mushrooms.md); only runs with a force-feedback mouse (`ImmersionExists` is a stub, so never) |
| Silver scroll (no title in the game's text): a creature breeder offers other creatures to swap for | partial | see challenges_and_rewards.md; [silver_scrolls/the_creature_breeder.md](silver_scrolls/the_creature_breeder.md); started only past the creature gate |

## Leaving the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gold scroll: leave through the vortex — when the guide's lessons are done the vortex opens; the player sends people through and follows to the second land | partial | see [gold_scrolls/leave_through_the_vortex_land_1.md](gold_scrolls/leave_through_the_vortex_land_1.md): with the guide-skip answers the vortex and its scroll are made at once with real natives; vortex mechanics: [portals.md](portals.md) |
| Loading the second land from the story | todo | `LoadMap` in `src/CHLApi.cpp` is an empty native, so no land is loaded |
| An unused script for taking over the land's villages with the guide's help | n/a | in the scripts but never started by the game; see [creature_guide.md](./creature_guide.md) |
