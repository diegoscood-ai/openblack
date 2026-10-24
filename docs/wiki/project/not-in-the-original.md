# Not in the original

## Summary

The behaviour listed here is **not part of the game as it plays**: written for it but never run, cut before
release, served by online services that are gone, a developer tool, or a guess an earlier tracker made that the
original did not bear out. By the user's rule of 2026-10-21 it is not progress, so it has no row in the
[progress files](progress/README.md); this page keeps what was found, so that nobody adds the row back. Before
adding a row, look here first.

## The original

Grouped by the progress domain it was read for. Each line is the behaviour, then why the game does not do it.

### World

#### terrain/footpaths

From [Footpaths and roads](progress/terrain/footpaths.md).

- Footpaths are drawn for debugging as lines: openblack-only (`RenderingSystemCommon.cpp`, the Footpaths switch of the debug bar's View menu)

#### terrain/land_height

From [Land height and mountains](progress/terrain/land_height.md).

- openblack's flat test land, with a lake, shallows, a bank and patches of sand and snow: openblack-only, the testbed's land (`src/3D/FlatLand.h`), a debug tool; see [../debug/](progress/debug/)

#### terrain/rivers

From [Rivers and lakes](progress/terrain/rivers.md).

- Rivers are drawn for debugging as lines between their points: openblack-only (`RenderingSystemCommon::PrepareDraw`, its streams option: the Streams switch of the debug bar's View menu)

#### sky/mist

From [Mist and haze](progress/sky/mist.md).

- Creatures notice mists as objects in the world: Not in the original: a mist is kept only on the game's list of mists, never linked into a map cell, and a creature looks at the things linked into the map cells about it; a mist has a belief kind of its own only for one a script hands it ([haze-and-mist.md](../world/haze-and-mist.md#map-mist-create_mist), [mind.md](../creature/mind.md))

#### sky/sun_and_moon

From [Sun and moon](progress/sky/sun_and_moon.md).

- On openblack's flat test land the sun and moon behave as on any land: openblack-only, the testbed's land (`src/3D/FlatLand.h`); see [../debug/](progress/debug/)

#### ocean/sea_surface

From [The sea](progress/ocean/sea_surface.md).

- Ambient wind would drift the sea's texture: The game's ambient wind is always 0 in play, so the drift never shows; our tree keeps the drift code (`sea::Drift`)

#### ocean/shore

From [The shore](progress/ocean/shore.md).

- Splash sprites rise round a creature's feet as it walks through water: Not in the original: the exe's splash-sprite code is never called, and a creature's step in water makes only its dust, its footprint and the fish-scare point ([water.md](../world/water.md#splashes-the-original-does-not-make))

#### ocean/things_on_the_water

From [Things on the water](progress/ocean/things_on_the_water.md).

- Big splashes of spray when something heavy lands in the sea: Not in the original: a heavy thing splashes as anything else does, its ring, foam and sound scaled by its size and impact (the row above) ([water.md](../world/water.md#splashes-the-original-does-not-make))

#### weather/scripted_weather

From [Scripted and real-world weather](progress/weather/scripted_weather.md).

- The weather debug window shows the climates and storms, forces storms and strikes lightning: openblack-only, `src/Debug/Weather.cpp`; see [../debug/](progress/debug/)
- The player gives their country and town, from the game's country list: the place came from the player's account on Lionhead's web site (`LHInetWeather::GetLocationDataForUser` 0x5E8410, an HTTP query), which is gone; see [pc_integration/real_weather](#pc_integrationreal_weather)
- Outside multiplayer, the game asks a weather server for the day's weather where the player lives: `LHInetWeather::QueryWeather` 0x5E88B0 over HTTP; the service is gone and openblack has no network code
- The real weather is turned into a preset that sets the land's rain, snow, temperature and wind: only after both queries succeed (`InitForGame` 0x5E98B0 returns at 0x5E9C33 / 0x5E9C43 otherwise), so never without the service
- The last weather fetched is kept on disk for when there is no connection: the reply is written to `weather.dat` (fn_005E9130, `wb`, from the reply's reader fn_005E8C60), but the game never reads it back: its reader (0x5E91C0, `rb`) has no caller and no pointer to it anywhere in the exe

#### weather/storms

From [Storms and thunder](progress/weather/storms.md).

- A storm can be forced over the whole island, and storms ended, from the debug window: openblack-only, `src/Debug/Weather.cpp`; see [../debug/](progress/debug/)

#### nature/fireflies

From [Fireflies](progress/nature/fireflies.md).

- The land editor writes each firefly into the land's script as a placement at its spot: openblack has no land editor; its in-game editor (F2, a debug tool) places things in the running world and writes no land script

#### nature/toys

From [Toys](progress/nature/toys.md).

- Black & White's model list has no separate bowling pin or toy-only bowling model: its skittle and toy bowling ball are the only bowling models (the bowling lane end and its model belong to Creature Isle): nothing to make
- The "cheat box" model is a plain box 180 wide, 180 deep and 400 tall, all of it below the ground, with eight corners: not used by any shipped land; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- A bowling game: ten villagers stand as pins in a triangle and a cow is the ball; each knocked-over villager scores, the game slows to half speed while anything is down, and all ten plus the cow make a strike ("You got a strike!"): a script source never compiled into the game's challenges; see [../story/minigames.md](progress/story/minigames.md)
- Cow bowling, the same game with its own wording: same; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- A bowling lane end, a bowling model and a skittle knock sound: only in Creature Isle's tables
- The Chimp Posse's die, teddy and ball, made at half size: never started by the game; see [../story/silver_scrolls/chimp_posse.md](#storysilver_scrollschimp_posse)
- The "cheat box" model: see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)

#### resources/poison_and_mushrooms

From [Poison and mushrooms](progress/resources/poison_and_mushrooms.md).

- The unused "Food for Thought" challenge ("The Nomads") checks whether the player hands hungry nomads poisoned food, and kills them all if so: not shipped; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- The game's text still names a "Poisoned Food Miracle" ("Poisoned Food. Boss, this will cause a whole lot of sickness!") left over from before the extreme food miracle made speed-up food: leftover text, never shown for a working miracle

#### resources/stores_and_piles

From [Stores and piles](progress/resources/stores_and_piles.md).

- Only food and wood exist; there is no ore: the game has no other resource

### Beings

#### creature/creature_tattoos

From [Creature tattoos](progress/creature/creature_tattoos.md).

- No challenge script of the five lands reads or changes tattoos: nothing to do
- Creature Isle's tattoo types (the Brotherhood, the super gods, fighters) for its scripts: Creature Isle; the values are listed in `src/ScriptHeaders/ScriptEnums.h` only
- Uploading a creature to the online creature database sends its tattoos with it: the online creature service no longer exists (already listed: "Creatures can be uploaded and downloaded online")

#### creature/decision_making

From [Decision making](progress/creature/decision_making.md).

- Multiplayer locked selection of a creature across the network: network play is not supported; see [../multiplayer/](progress/multiplayer/)
- When free, the creature plans every desire at once before falling back on idling — the original plans its desires in passes, two a turn (`NextGoals`, plans-and-needs.md); openblack's every-desire planning was never called (`PlanCreature`'s `everyDesire` was passed true nowhere), so the row was a guess the exe does not bear out; the switch was removed on 2026-10-22 (7a52bd21f).

#### creature/development_phases

From [Development phases](progress/creature/development_phases.md).

- The testbed can set a creature's stage: openblack-only debug tool (the testbed's "set stage" command, `src/Debug/TestbedScenarioRunner.cpp`); the debug spawner sets the stage too (next line)
- The stage shown and changed in the debug spawner: openblack-only debug tool (Debug > Windows > Creature Spawner, "Set stage"); the game's stage moves are scripts, packets, the mind file and the computer creatures

#### creature/growth_and_size

From [Growth and size](progress/creature/growth_and_size.md).

- Scripts can read a creature's grown-up size: no script command does: `ID_ADULT_SIZE` counts a town's or an abode's adults (see [../scripts/challenge_natives_towns_and_players.md](progress/scripts/challenge_natives_towns_and_players.md))

#### creature/learning_by_observation

From [Learning by observation](progress/creature/learning_by_observation.md).

- Shielding a town: the original never reports this deed. Our wiki differs: the shield miracle gives no deed ([creature.md](../creature/mimicry.md#deed-table))
- Bringing people to worship: the original never reports this deed. Our wiki differs: no call site reports it ([creature.md](../creature/mimicry.md#deed-table))
- Impressing by throwing: the original never reports this deed. Our wiki differs: no call site reports it ([creature.md](../creature/mimicry.md#deed-table))
- Throwing the football into the goal, or catching it: the original never reports these deeds; see [../town/football.md](progress/town/football.md#the-player-and-the-creature). Our wiki differs: neither is reported anywhere ([creature.md](../creature/mimicry.md#deed-table))
- The developer cheats to learn everything, learn ordinary things, or move to the next stage: in the v1.42 exe the only route to them is the handler of network packet 0x50 (GPacket::ProcessPacket at 0x63DB3C -> 0x47B010). That handler takes the leash off, brings a dead creature back and ends its action, or, on its other branch, teaches the creature and the nearest other creature everything (0x4798C0, which teaches the six ordinary skills through 0x4798F0 and then the 42 miracle rows) and forces a fight between them. It has no next-stage step. A byte scan of every packet sender finds none that sends 0x50, so no key or command reaches it. Learning everything is otherwise reached only by the script native CREATURE_LEARN_EVERYTHING (0x4F7080 jumps to 0x4798C0), which keeps its own row (already listed under hidden_keys_and_cheats)
- Scripts can give it the most skill at one miracle or power-up: no script native does; the creature learning natives are CREATURE_LEARN_EVERYTHING, CREATURE_SET_KNOWS_ACTION (which sets a taught miracle's sightings to just the times needed), CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT and CREATURE_LEARN_EVERYTHING_EXCLUDING (bw1-decomp ScriptFunctionTable.cpp)
- Scripts can make it learn everything but some actions (CREATURE_LEARN_EVERYTHING_EXCLUDING): no shipped script calls it
- What it thinks its god wants drives its desire to follow its god's wishes: the perceived desires have no gameplay reader; they are read only by the Creature Cave's personality scroll (GetDominantDesire 0x4C7ED0), the town part only saved and loaded (mimicry.md#gplayermakecreatureempathisewithplayertowndesire)
- Casting wood by a workshop: the original never reports this deed (deed 10); wood cast on a workshop gives the building-site deed while it is being built, otherwise nothing (mimicry.md#deed-table)
- Watching its god be nice, nasty or impressive stirring those desires directly: GPlayer::MakeCreatureEmpathiseWithPlayer(desire, pos) 0x64B5B0, which would feed the "from watching the player" sources, has no caller, pointer or reference in the exe; only the noticed deed stirs them (0x4EA898)

#### creature/lessons_and_help

From [Lessons and creature help](progress/creature/lessons_and_help.md).

- Creatures could be uploaded to the game's online creature service: the service no longer exists
- Help about the creature acting on a thing, and on a place (the advisor clings bottom left and reads out what it does): the scripts HelpForCreatureActingOnObject / ActingOnPos are in challenge.chl but no script runs them and the exe names neither (it starts only HelpForCreaturePointAtCreature, HelpForCreatureJustText, HelpForCreatureMoveToNewDevelopmentPhase): never run
- Help as two lines (HelpForCreatureTwoLines) and help that just focuses on the creature (HelpForCreatureJustFocusOnCreature): in challenge.chl, run by no script and not named by the exe: never run
- What the creature is doing as a line of text (GET_ACTION_TEXT_FOR_OBJECT): the native is used only by the two never-run ActingOn scripts
- It wants a desire more / less (help kinds 2 and 3, texts 664 / 704): none of the 46 calls of the help request (0x4C9FE0) passes them; the feedback's lesson message sends 4/5 or 0/1 (0x4E1067): never raised
- It will choose or avoid a kind of thing for a desire: the game's 42 help kinds have no such message
- It hasn't learnt a skill it is being shown (kind 32), "It can't learn this at its stage" (34), "What it still has to do to reach the next stage" (35), "Something interesting off screen" (36): no call of the help request passes them: never raised
- A problem of the mind (kind 31): never raised, and its tell (0x4CAFC0) starts no script
- Too young to copy the player (Miscellaneous 8) and "Frightening music" (Miscellaneous 2): the help request's calls pass only 0, 1, 3, 4, 5, 6, 7: never raised
- Failure explanations "won't fight because hurt" (0), "can't find a way" (1), "way blocked" (2), "the other creature refused to fight" (4): the calls pass only 3, 5, 6, 7, 8: never raised
- The prompt on how to let the creature go (Unleash): its condition (0x508EF0) always answers no: never told
- The leash stopped it (FailureExplanation 7): the one call that passes 7 for a failure explanation is in the tug's pull-away (0x4CEC71 in 0x4CEC00), reached only from the tug, which nothing sends ([leash-rope.md#the-leashs-tug](../creature/leash-rope.md#the-leashs-tug)); removed with openblack's tug, 2026-10-22 (ea7895309)
- A body-problem message for needing a poo or being ill: the original's body messages are the eight of UpdateHelp 0x4C9C90..0x4C9DBD (energy, life, exhaustion, thirst, each plain and "very")
- A screen to choose which lesson the last feedback teaches and step through recent actions (CreatureLessonChooser 0x4CFB10): a developer screen built from the editor-icon buttons, polled beside the mind and dance editors in ProcessFrameInputs, and opened only by network packet 55 (0x63D030), whose sender was not found

#### creature/saves_and_files

From [Creature saves and files](progress/creature/saves_and_files.md).

- The creature upload tool sends a creature file to the game's website under the player's account: the service no longer exists
- Selecting another player's creature waits for the network to agree: covered under network play

#### creature/species_choice

From [Species and choosing a creature](progress/creature/species_choice.md).

- Chicken and crocodile, listed by the game but with no bodies in its data: never playable in the five lands
- Species added by Creature Isle: Creature Isle

#### villager/daily_routine

From [Villager daily routine](progress/villager/daily_routine.md).

- Housewives fetch food from the storage pit, make dinner, serve it and clear it away: the housewife's states are TODO rows. Our wiki differs: the housewife's day (fetching food, cooking, serving and clearing dinner, housework, gossip) is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births)); see [villager/jobs](#villagerjobs)
- Villagers sit down to dinner at home, waiting for it when the housewife is late: `SITS_DOWN_TO_DINNER` and `WAIT_FOR_DINNER` are TODO rows. Our wiki differs: the housewife's meal call, which makes villagers wait for dinner, is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))
- Idle villagers noticing interesting things nearby (animals, trees, rocks, pots, the ball, fields, abodes, other villagers) and going to interact: the CHECK_INTERACT_WITH_* states 170-181 are reached only from a disciple's landing inspection (FindCloseObjectsForInteract 0x756E80 is called only by Villager::Landed and Villager::InHand; SetupInspectObject 0x76AAB0 only by Landed; docs/wiki/beings/villager-hand-and-death.md, LANDED); row removed from villager/daily_routine.md.

#### villager/disciples

From [Disciples](progress/villager/disciples.md).

- Protection disciple: in the table but never made in the game

#### villager/jobs

From [Villager jobs](progress/villager/jobs.md).

- Housewives fetch food from the storage pit, gossip round it, and cook dinner at home: the housewife's states are TODO rows. Our wiki differs: the housewife's day (states 100 to 109, with the storage pit, the gossip and the dinner) is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))
- Housewives do housework when nothing else needs doing: `HOUSEWIFE_DOES_HOUSEWORK` is a TODO row; part of the housewife's day, dead code in the original (row above)
- Builders never carry scaffolds: the game leaves that empty: See [tools_and_carried_items.md](progress/villager/tools_and_carried_items.md)
- A villager waits for wood when the site has none and none can be found: the state is a TODO row. Our wiki differs: the wait for wood is never entered in this version of the original, as a game flag it needs is never set ([villagers](../beings/villager-jobs.md#builders))
- Villagers carry food from the storage pit to the worship site for the worshippers: the town's desire for worship supplies is 0, and GotoWorshipSiteWithSupplies (state 44) returns 0, so no supplies ever set off in v1.42 ([worship](../magic/worship.md))
- Traders pick up and drop off excess in abodes: ArrivesInAbodeToTrade 0x76B990 and ArrivesInAbodeToPickUpExcess 0x76B980 only return 1
- Villagers take wood from rocks the game counts as wood: CheckInteractWithRock 0x757740 and ArrivesAtRockForWood 0x758EF0 only return 1 (GotWoodFromRock 0x758F00 has code, but nothing was found that enters it other than after the empty arrival; not traced further)
- Worshippers rest at the altar: the altar rest's states 61-64 are in the table, but nothing in the game sends a villager to 61 ([worship](../magic/worship.md))

#### villager/looks_and_voices

From [Villager looks, voices and special villagers](progress/villager/looks_and_voices.md).

- Each villager draws its tribe's model for its job, high or low detail by distance: the job's mesh of the tribe (`src/ECS/Archetypes/VillagerArchetype.cpp`). Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../rendering/meshes-and-lod.md#villager-blobs-object-reflections-and-lod)). openblack switches by no distance either: it draws a villager's high mesh only at detail level 6, where it starts, and its standard mesh at the lower levels, a 🎯 chosen difference of 2026-10-15 ([meshes-and-lod](../rendering/meshes-and-lod.md#notes-on-the-originals-sections))
- A created villager is now and then a special kind (hermit, sculptor, piper, sailor, priest, healer...): the random roll only gives a named ordinary villager; the special kinds are made only by challenge scripts (beings/villager-states.md#the-special-villager; special_villagers.md)
- A tooltip for what a villager is doing: the hand's text over an object has no villager text; a villager gives the pick-up state (interface/tooltips.md, "From the object under the hand" and "State 18: Over Object")
- Special villagers speak with a speech bubble: as the special_villagers lines above

#### villager/play_and_gossip

From [Villager play and social life](progress/villager/play_and_gossip.md).

- Housewives gossip round the storage pit: `HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT` is a TODO row. Our wiki differs: the housewife's day, the gossip included, is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))
- Villagers kick a ball about when one lies near: `CheckInteractWithBall` 0x757720 is 16 bytes and returns 0; picking up a free ball is the ball's reaction (reactions.md)
- Children play rather than work: a child's decision (bw1-decomp `VillagerChild.cpp` `ChildDecideWhatToDo`) is only its check, the crèche, or following its mother; there is no play state for children
- Villagers tell others about something interesting they found: see the villager/reactions line on the telling states
- Villagers celebrate when their town is won over: see the villager/reactions line on reaction 31

#### villager/special_villagers

From [Special villagers](progress/villager/special_villagers.md).

- openblack has a debug overlay that labels villagers with numbers and states: `Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`, which labels a named villager with its name; a debug tool, see [../debug/debug_windows.md](progress/debug/debug_windows.md)
- New e-mail is announced by a named villager in its bubble ("Hey! I've sent you an e-mail about ..." when the sender is that villager's contact, otherwise "Hey! ... has sent you an e-mail about ..."): the game's mail link is gone; see [../pc_integration/villager_names_from_contacts.md](progress/pc_integration/villager_names_from_contacts.md)
- A script can ask for a named villager of a given town: the only caller of the special villager's creation (0x71F1A0) is the plain villager creation (0x74FC13), which always passes no special row (0x74FC0D); no script command makes or asks for a named villager (xref callers)
- A named villager speaks in a speech bubble over its head (one speaker at a time; 250 by 175, text size 19, up to 1023 characters; 1.5 above the head, closing after 6 s or when the villager dies, is deleted or hidden; shown instead of its name): the bubble's only caller with a text is the gathering box's incoming message (GatheringInterface, the e-mail and ping-server messages, gone); the others close it (ToBeDeleted, Draw) and one has no caller (0x71F090) (xref callers of 0x71EF70)

#### villager/tools_and_carried_items

From [Villager tools and carried items](progress/villager/tools_and_carried_items.md).

- Housewives carry a bag from the storage pit to their home (picked up with the pot-on-the-head clip, walked home with the carrying-pot clip): the housewife's states are TODO rows. Our wiki differs: the housewife's day, her trips to the storage pit included, is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))

#### animal/animal_behaviour

From [Animal behaviour](progress/animal/animal_behaviour.md).

- Animals react to a shield, a teleport and a magic tree: Our wiki differs: no animal reacts to a shield, a teleport or a magic tree ([animals](../beings/animal-life.md#reactions))
- Animals look at a death and at someone fainting: Our wiki differs: no animal reacts to a death or a fainting ([animals](../beings/animal-life.md#reactions))
- Animals chase a ball (unconfirmed): Our wiki differs: no animal reacts to a ball ([animals](../beings/animal-life.md#reactions))
- Listeners told as an animal dies (an openblack-only list of callbacks, and a single slot for the flocks): the original's `Living::SetDying` 0x5EC390 sets the life, the dying state and the turns and tells no one, and nothing in openblack registered one; removed with the API (`AddDeathListener`, `RemoveDeathListener`, `SetDeathCallback`), [animal-life.md](../beings/animal-life.md#hand-flight-and-death).
- A starving animal dies: animals never starve; `Animal::ProcessNeeds` 0x417DC0 only counts (callees vt+0x8D0 GetAge, fn_00418220 the breed count, IAmFlockLeader), a hungry animal with no food goes on to sleep (`Animal::ReactToAnimalNeeds` 0x4183C0), and an animal's death reason is written only by the sacrifice (`Animal::ApplyThisToObject` 0x41B403); docs/wiki/beings/animals.md#turn, animal-life.md#flocks-age-and-smoke.
- (thirst and water, mates; part of the needs rows, rewritten): animals have no thirst and never look for water or a mate; the needs are hunger, sleep and breeding (ProcessNeeds 0x417DC0; birth by GIVES_BIRTH when the flock is short); animals.md#herbivores-sheep-tortoise-cow-horse-pig.
- (falling trees, fights, wood; part of the reactions rows, rewritten): the falling-tree reaction 27 is created by nobody, no animal reacts to a fight or to wood, and the magic food piles (foodType 0) draw no animal; animal-life.md#reactions.
- Animals react to the creature's gifts: no animal reaction answers a creature's gift; the animals take reactions 0, 1, 3, 6, 7, 10, 28 (and 9 for hunters) only; animal-life.md#reactions.

#### animal/birds

From [Birds](progress/animal/birds.md).

- Seagulls keep to the coast (unconfirmed): Our wiki differs: every bird kind flies the same legs from where it is; nothing keeps seagulls to the coast ([animals](../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat))
- Birds land to eat and to sleep, on the ground or on objects, and take off again: Our wiki differs: birds never land in this version of the original, as every bird's sleep value is 0 ([animals](../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat))
- Birds sleep at night: Our wiki differs: birds have no day and night; the bats fly by day ([animals](../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat))
- Birds look at and flee from miracles and the hand (unconfirmed which kinds): Our wiki differs: birds take no reactions ([animals](../beings/animal-life.md#reactions))
- Flocks of the same kind can merge, and a small flock joins a larger one: flocks merge only after a landing from the hand (`flocksCanMerge` is 0 for all 27 species), and no bird can be picked up (playerCanPickUp 0) nor lands (info.sleep 0); animal-life.md#flocks-age-and-smoke, animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat.
- The hand can pick up birds the table allows, and throw them: no bird's table entry allows it (playerCanPickUp 0 for every bird), so the hand never holds a bird; animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat.

#### animal/wild_animals

From [Wild animals](progress/animal/wild_animals.md).

- The chess puzzle's animals and villagers (Creature Isle): Creature Isle only
- Grazers drink at the land's drinking places: Our wiki differs: animals have no thirst and never drink ([animals](../beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig))
- Animals keep to the surroundings they like (land, coast): `Animal::CheckSuitableEnvironment` 0x417850 answers 100 for every class (one entry in all 36 animal vtables, bw1-decomp Animal.cpp), and `Animal::SeekEnvironment` 0x4181B0 has no direct caller (only state 36 of the state table; xref vtcalls +0xB5C: 0 sites).

#### town/football

From [Football](progress/town/football.md).

- The switch is a binary value, `BWFeatureKey`, under `Software\Lionhead Studios Ltd\Black & White\BWSetup` in the registry. It holds a list of 32-bit codes. Each code is a scrambled checksum of the feature's name minus the same checksum of the Windows product ID, so a code only works on the machine it was made for: copy protection tied to the Windows product ID. openblack should use a plain setting (row above). The product ID is read from the Windows 9x location, which is probably why the unofficial 1.42 patch (April 2019) lists "fixed addons like football mod not activating"
- The same switch turns on four other add-ons: an MP3 player, creature music moods, the creature dancing to the sound card's line-in, and a Pentium 4 option: not football; listed because they share the mechanism
- The switch first appears in patch 1.1. Version 1.00 has the football code but not the feature key, and press reports say the add-on needs patch 1.1: checked in the 1.00, 1.10 and 1.20 executables. How 1.00 treated football was not traced
- The download (about 1.1 MB, Lionhead, listed August 2001): the installer is not in the game folder. The ball and goal model `Data\football.l3d` is dated 22 February 2001, so it is part of the base game. What the installer copies besides the registry code is undetermined
- Cup final: seven Norse (away) and seven Celtic (home) footballers, five on and two subs, a referee and up to 15 fans from each town. The evil advisor says a crowd is forming: the challenge's script is not compiled into the shipped challenges. See [../story/minigames.md](progress/story/minigames.md)
- In the cup final, a player whose health runs out is replaced by a sub, with a cry, the camera following him and the advisor's line ("Hard tackle. Not a foul, though, I say."): as above
- The cup final lasts 300 s or until one side loses all seven. With 30 s left the first Norse sub is set on fire and killed, so the Celts "cheat": as above. No winner is declared; everyone is sent home
- The sound list names a referee's long and short whistle, a huge crowd and a goal sound, but no recordings by those names ship: no football music or chants exist
- The game counts no football statistics: the score lives only in the pitch and its debug text
- A debug view shows the match state and the score: developer only; see [../debug/](progress/debug/)
- An older pitch kind (with its own ball and place table) remains, but every villager hook it relies on is empty: no table or script makes one
- A "Miraculous soccer ball" miracle was cut: a creature-lesson line and a miracle-icon model remain: the shipped miracle list has none

#### town/town_desires

From [Town desires](progress/town/town_desires.md).

- The flags hang still: their meshes are static, with no ripple (raffclar PR #275): the row once said they ripple in the wind; vanilla's flag meshes have no animation

#### building/abodes

From [Abodes](progress/building/abodes.md).

- Low-detail versions of buildings far away: openblack always draws the most detailed meshes. Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../rendering/meshes-and-lod.md#villager-blobs-object-reflections-and-lod)); See [../rendering/](progress/rendering/)

#### building/civic_buildings

From [Civic buildings](progress/building/civic_buildings.md).

- Walls (planned wall sections): no land or playground script uses the command
- Creature pens: no land or playground script uses the command

#### building/workshop_and_scaffolds

From [Workshop and scaffolds](progress/building/workshop_and_scaffolds.md).

- An "old scaffold" object exists in the tables but nothing places it: no land or script uses it
- Villagers never carry scaffolds: the game leaves that empty: See [../villager/tools_and_carried_items.md](progress/villager/tools_and_carried_items.md)

#### worship/worshippers

From [Worshippers and dancing](progress/worship/worshippers.md).

- Coloured lights follow the dancers: the game keeps them but never draws them; see [../rendering/light_beams.md](progress/rendering/light_beams.md)
- Dancers rest at the altar to get their strength back: the altar rest's states 61-64 exist, but nothing in the game sends a villager to 61 ([worship](../magic/worship.md#going-to-the-site))
- Worshippers ask to go home when tired or hungry: `Villager::CheckRequestGoHome` 0x76C8D0 has no caller, so nobody asks; the row is rewritten to what the original does (any worshipper goes home once fewer are needed)
- Villagers who are impressed enough by a creature worship it, dancing round it: no such villager behaviour was found; the creature itself may dance with villagers to impress them (`CreatureAgenda::ConstructSubActionsForDanceImpressivelyWithVillagers` 0x49C960); the row is rewritten to that

#### animal/fish

From [Fish and fish farms](progress/animal/fish.md).

- A town's desire to fish follows how full its farms are: a farm's score counts only its fishermen, and fishing never touches the farm's stock (`Villager::Fishing` 0x75B6A0; `fish_farms::Score`); beings/villager-jobs.md#fishermen.
- Stoning a fish farm damages it: `FishFarm::CanBeDestroyedByStoning` 0x52C530 answers 0 (bw1-decomp FishFarm.cpp). (The burning part is kept as an inferred row.)
- A fish farm is drawn and repaired as the town's: `FishFarm::Draw` 0x52C830 is empty (what shows is its shoal, sea pages) and `FishFarm::NeedsRepair` 0x52C580 answers 0; creature/plans-and-needs.md (mending).

#### animal/livestock

From [Livestock](progress/animal/livestock.md).

- Livestock drink: no thirst and no drinking state (as above); the only link to water is a shepherd moving its flock's home to its abode's nearest water (`Villager::ShepherdMoveFlockToWater` 0x768CC0, a villager state), kept in the shepherd row.
- (herding by the creature; part of the creature row, rewritten): the creature herds cattle only in the "Swap To Cow" challenge, which nothing ever starts; see [Swap To Cow](#storysilver_scrollsswap_to_cow).

#### worship/prayer_power

From [Prayer power](progress/worship/prayer_power.md).

- Infinite prayer power and "Free upkeep for running miracles": the site's flags are set only by `WorshipSite::Cheat` 0x77BC70 <- `Citadel::Cheat` 0x463510 <- `GPlayer::Cheat` 0x64A8F0 <- the cheat packet in `GPacket::ProcessPacket` (0x63D1A6), sent only from the developers' key menus, unreachable in the release (see easter_eggs/hidden_keys_and_cheats); openblack keeps them behind one debug switch, "Infinite prayer power at every worship site", which sets both on every site (Magic window, Running tab)

#### worship/worship_sites

From [Worship sites](progress/worship/worship_sites.md).

- A cheat gives a site endless prayer power: the same unreachable cheat as above
- Worship site upgrades: the classes and their info table are loaded, but the only maker of a `WorshipSiteUpgrade` (ctor 0x77EBF0, `Create` 0x77ED10) is `PlannedWorshipSiteUpgrade::CreatePlanned` 0x77EEA0, and the only maker of a planned one (ctor 0x77EE40) is `WorshipSiteUpgrade::ConvertToPlanned` 0x77EDD0: nothing starts the chain (xref callers), and no script command names one
- Villagers carry food from their town's store to the site when it runs short: the supply errands exist (states 42-46), but the town's supply desire is 0 unless a script boosts it and `GotoWorshipSiteWithSupplies` (state 44) returns 0, so no food ever sets off for the site ([worship](../magic/worship.md#going-to-the-site))
- In danger, a town's villagers run to the site to hide: the only caller of `Villager::StartHidingAtWorshipSite` 0x76C550 is `ArrivesAtWorshipSiteForWorship` 0x76BE8A (a worshipper beyond the site's room); a town in danger only stops worshipping (`Town::ProcessTownEmergency` 0x7477A0)

#### villager/reactions

From [Villager reactions](progress/villager/reactions.md).

- Villagers approach the hand when it is near: no code of the frozen exe sets APPROACH_HAND_REACTION (158) (no push 0x9E before an AddReaction, SetTopState or walk call), and the villager's hand pick-up reaction (16) set-up is the Living's empty one (0x4170D0, `ret 8`, Villager vtable +0x9E0)
- Villagers react to something put in the storage pit by the hand, and to a hand using a totem: the villager's set-ups for reactions 22 and 17 are the Living's empty ones (0x417120 at Villager vt +0x9F4, 0x4170E0 at +0x9E4)
- Villagers react to an object crushed nearby: reaction 18 is made, but the villager's set-up is the Living's empty one (0x4170F0 at Villager vt +0x9E8)
- Villagers get out of the way of a falling tree: no call of `Reaction::CreateReaction` 0x6E3D70 makes reaction 27 (the 45 callers read in the exe; a felled tree makes 12, the wood reaction)
- Villagers crowd round to watch something (CROWD_REACTION 214): no call makes reaction 29
- Villagers are confused at shocking sights: no call makes reaction 26 ([villager-reactions](../beings/villager-reactions.md#fainting-and-confused-reactions-25-and-26))
- Villagers inspect an object new to them and tell others about it: no code enters INSPECT_OBJECT_REACTION (9) for a villager (132 is set only from 9's own chain); INITIALISE_TELL_OTHERS_ABOUT_OBJECT (134) is never set, and TELL_OTHERS_ABOUT_INTERESTING_OBJECT 0x764650 and the look-around 0x764660 are 16-byte functions that return 0
- A villager is scared stiff by a frightening creature: no code sets MAKE_SCARED_STIFF (88); the inspecting clip's scared-stiff pose (one draw in three for women and children) is the creature reaction's, already counted
- Villagers fight a villager of another town: the fight reaction (19) is made only by creature-fight arenas, and its states 143 / 144 walk to a ring round the arena and watch; no villager state fights
- Villagers celebrate (dance) when their town is won over: `GPlayer::ClaimTown` makes reaction 31 from the town centre when there is one, and `Villager::ReactToTownCelebrationPriority` 0x767180 answers non-zero only when the initiator is the villager's own town; with no centre the town branch of the set-up only looks at the town, so the town-celebration dance is never joined (the reactions row now says this)
- A stealthy reaction, which skips at random a cell whose first thing is another living thing as it spreads: the spread's stealth branch (`SpreadReaction` 0x6E3E10: with the reaction's flag +0x20 set, `GameRand(1000) > stealthRandomChance` skips the cell, otherwise the flag is cleared) never runs, as no creator of a reaction sets the flag ([reactions](../magic/influence-and-alignment.md#reactions)); openblack's reaction keeps the flag, off when made, and leaves the branch out (`src/ECS/Effects/Reactions.h`)

#### creature/learning_from_feedback

From [Learning from feedback](progress/creature/learning_from_feedback.md).

- A sleeping creature can't be held: not the original's test; `ValidForLockedSelectProcess` 0x476E10 refuses an ogre, a script-controlled creature (+0x25 & 4), one at the first steps of action 9 (CREATURE_DEAD), one with no player and a frozen one, never a sleeping one ([states.md](../hand/states.md#the-hand-on-a-creature)); openblack's `creature_hand::MayHold` still refuses a sleeping creature (a row for the original's tests is pending)
- Letting go of one creature to hold another delivers the first's feedback first: the original's hand holds one creature at a time in a locked select that runs to the release (action states 3..6), so there is no change of creature while holding ([states.md](../hand/states.md#the-locked-select-on-a-creature))
- After any stroke or slap the creature at once shows the player its pleasure or sorrow: the original's feedback handler 0x4E06A0 has no such step, only the young creature's kept next action one time in two ([mind.md](../creature/mind.md#the-players-feedback)); openblack's stand-in (the cleared show-desire wait in `CreatureMindSystem::ReceiveFeedback`, the happy or sad clip) was removed with the old idle policy, 2026-10-22 (e51b27ed1)
- A slap marks the creature as punished, which its reactions follow: the handler 0x4E06A0 sets no such mark; a slap clears the mimicry (+0x1C38) and the kept next action (+0x21D0) only ([mind.md](../creature/mind.md#the-players-feedback))
- Seen to help, an action teaches it a little about what it acted on and used: a seen action passes the forgotten context's first test but sub_4E1150 teaches only a forced one, and no lesson reads the thing used ([mind.md](../creature/mind.md#the-players-feedback))
- Stroke and slap buttons in the debug spawner and the testbed deliver feedback directly: openblack debug controls (`src/Debug/CreatureSpawnerMind.cpp`, `TestbedScenarioRunner.cpp`)

#### creature/feeding_and_thrown_things

From [Feeding and thrown things](progress/creature/feeding_and_thrown_things.md).

- Some things the game marks as not to be eaten are spat out and the creature is sick: the end of a meal (0x500540) spits out only what is worth less than 5 (or not a number) and what is poisoned; there is no other mark ([plans-and-needs.md](../creature/plans-and-needs.md#the-end-of-a-meal))
- The creature of one kind of player never eats its own player's people unless a script controls it: the one test with that rule (0x4E4B90, with the not-food counts and the computer's own villagers) is never called, directly or through its slot ([plans-and-needs.md](../creature/plans-and-needs.md#what-a-creature-can-eat))
- The "catch a fireball and throw it back" action: it always fails in the game, so creatures never do it ([hits-and-catch.md](../creature/hits-and-catch.md#the-creatures-catch))

#### creature/object_actions

From [Object actions](progress/creature/object_actions.md).

- An angry creature hurling what it picks up at the nearest home or tree: only openblack's old idle policy, removed with its switch on 2026-10-22 (e51b27ed1; plans-and-needs.md#a-creature-with-no-plan-in-openblack); the original's hurl is the planner's Hurl action, which keeps its own row.
- Start a fire, or set something on fire (the creature's StartFire action): its agenda builder in the v1.42 exe (0x4A41D0, 0x20 bytes) only calls FinishActionUnsuccessfully (xref; bw1-decomp CreatureAction.cpp the same), so a creature never starts a fire by it.
- Watch the telly: the WatchTelly action's builder (0x4B04B0, 0x10 bytes) only calls the poo's builder (xref; bw1-decomp the same); it is a poo, covered by the poo rows, with nothing on a screen.
- Run around a race track: the RunAroundRaceTrack builder (0x4A6F50, 0x10 bytes) has no callees and returns 0 (xref; bw1-decomp: `return 0;`), an empty agenda.
- Eats the grain from fields only when the time is ripe: a guess; the field's rule is growth and food both above 0 (Field::IsFieldWithFoodInIt 0x4E4930, covered by the field row), and puzzle grain can never be eaten (PuzzleGrain 0x561920 answers 0, oracle 2026-10-18; plans-and-needs.md#what-a-creature-can-eat).

#### creature/marks

From [Wounds, burns and blood](progress/creature/marks.md).

- The debug spawner can add wounds, burns and blood: an openblack debug control (src/Debug/CreatureSpawnerAppearance.cpp), not a behaviour of the game

#### creature/idle_behaviour

From [Idle behaviour](progress/creature/idle_behaviour.md).

- Watches the telly: the action's builder only runs the poo's agenda (bw1-decomp `ConstructSubActionsForWatchTelly`, 0x4B04B0, 16 bytes), and no desire's row of the desire action table nor the town table lists action 176 (info.dat, read 2026-10-21): nothing in the game chooses it. openblack has it as a forced-only action with the poo's agenda, which its planner never chooses (764e131fa; test `CreatureFart.WatchingTheTellyIsOnlyEverForcedAndIsAPoo`). Note: the page's line "the creature's watch telly and fart actions exist but are empty" is wrong for the fart, which is the poo desire's second action and runs the poo's agenda with its own puffs (creature/plans-and-needs.md#the-poo-desires-fart)
- It looks down a cliff: the action's builder is empty and returns failure (`ConstructSubActionsForLookDownCliff` 0x4B18E0, 16 bytes, `return 1`; `ConstructSubActionsFromActionAgenda` 0x4D36A0 answers success only for 0), and no desire lists action 195; openblack has no executor for it, so a forced one fails too (da83d404f; test `CreatureFart.LookingDownACliffIsNeverBuilt`; also listed for easter_eggs/secret_behaviours)
- With a need and the means at hand, it sees to the need before idling: openblack's own idle choice (the "old idle policy" stand-in, removed with its switch on 2026-10-22, e51b27ed1); in the game a creature with no plan does nothing, and needs are weighed by the planner like any desire (decision_making.md)

#### creature/animation

From [Creature animation](progress/creature/animation.md).

- The debug spawner plays any animation, face or gesture: a developer tool (src/Debug/CreatureSpawner.cpp), openblack only; it stays as a debug control, not a progress row

#### creature/beliefs_and_opinions

From [Beliefs and opinions](progress/creature/beliefs_and_opinions.md).

- The creature's decision trees read as text in the game's own mind viewer: a developer tool (the exe's belief Dump functions, e.g. `CreatureBeliefAboutContext::Dump` 0x4C7BE0), not seen in play; openblack's debug spawner shows the trees as a debug control

#### creature/friends_and_other_creatures

From [Friends and other creatures](progress/creature/friends_and_other_creatures.md).

- Eating with a friend: the exe's builder `ConstructSubActionsForEatWithFriend` 0x4B1980 only ends the action unsuccessfully ("failed to construct subactions", 0x9D961C), so it never runs
- Showing a friend an object: `ConstructSubActionsForShowFriendObject` 0x4A7D20 is the same failing stub; only showing its home (0x4A7D40) and its citadel (0x4A8410) are built
- The opposing creature in a challenge set by script: OPPOSING_CREATURE only reads the species opposite a god's creature (`GScript::GetOpposingCreatureType` 0x6F5AC0), and no shipped script calls it ([../scripts/challenge_natives_creature.md](progress/scripts/challenge_natives_creature.md))

#### creature/leash

From [Leash](progress/creature/leash.md).

- A tug on the leash makes the creature walk to the hand: the tug's sender (0x4CE750, packet 0x57) has no call, jump or stored pointer in the exe, so the tug never happens ([leash-rope.md#the-leashs-tug](../creature/leash-rope.md#the-leashs-tug)); openblack's tug and its spawner button were removed on 2026-10-22 (ea7895309)
- The harder the leash's pull the faster the creature goes, up to twice its running pace, the pull fading each turn: the pull (+0x48B0) is set only by the tug's paths, which nothing calls, so it stays 0 and the fade's branch never runs (same page)
- Already led, with a plan about something else and close to the hand, a tugged creature stays where it is: part of the tug's handler 0x4CE8E0, reached only from the tug's packet (same page)
- Pulled away from its plan for the same desire twice, the creature goes off that desire for a while: the count is made only by the tug's pull-away 0x4CEC00, reached only from the tug (same page)
- A tug on a tied leash leads the creature onto what it is tied to: the tug with an object (0x4CEA5E..0x4CEAE8, 0x4CEE70 / 0x4CEE80) is reached only from the tug, which nothing calls (same page)
- In a network game the leash's pull is sent to the other players: that packet (0x57) is the tug's, which nothing sends (same page)
- While leashed the creature can be dragged over land it could not walk: the rope moves nothing, a led creature moves only by the walk back, and the walk back to a point it can't stand on fails and leaves it standing ([leash-rope.md#the-walk-back-into-the-kept-area](../creature/leash-rope.md#the-walk-back-into-the-kept-area))
- The leash can only be tied to things once the creature has grown past its early stages: the tie's packet handler tests only the learning leash known, a leash on and its working, no development phase ([leash.md#tying-the-leash-with-the-hand](../creature/leash.md#tying-the-leash-with-the-hand))
- Leashes can't be put on a creature in a fight or knocked out: `GLeashStatus::SetOn` needs only the learning leash known, and the walk back has no fight or faint test ([leash.md](../creature/leash.md), [leash-rope.md#the-walk-back-into-the-kept-area](../creature/leash-rope.md#the-walk-back-into-the-kept-area)); the real behaviour (a fighter walked back, its fight ended) is a pending row
- The leash overrides what a script has the creature doing: the walk back skips a creature with `CONTROLLED_BY_SCRIPT` (`Creature::ProcessState` 0x4730DC..0x473165, same page); replaced by a done row saying so

#### creature/creature_cave

From [The Creature Cave](progress/creature/creature_cave.md).

- The creature turns to face the hand as the hand moves about the cave: the exe's room turns its copy only to the tattoo editor's eye as the editor opens and back to the fire once it closes (`CreatureRoom::Draw` 0x786C86..0x786F75, [tattoos.md#the-copys-turns-and-looks](../creature/tattoos.md#the-copys-turns-and-looks)); replaced by a done row
- The creature looks at the hand with a static pose, at random mirrored, when the hand is in front of it: the room's only random flag is the rest's (sleep or sit) at its start, and the only look at the pointer is the first second and a half of a press with the editor open (same page); replaced by done rows
- A picture of the creature can be taken from the cave and saved: `CreatureRoom::TakeScreenShot` 0x789A70 writes `creatureshot_%i.jpg`, which only the creature web page export for the online creature service reads (fn_00578330, `%s\creatureshot_%d.jpg`); the service no longer exists (already listed: "The creature's details are exported to a web page in the profile for the online creature service")
- The debug spawner shows and sets every value of the creature's body: an openblack developer tool (`src/Debug/CreatureSpawnerBody.cpp`), the original has no such window; it stays as a debug control, with no progress row.

#### creature/face_eyes_hair

From [Face, eyes and hair](progress/creature/face_eyes_hair.md).

- Mood faces happy, in pain, irritable and lonely: the exe has the four face functions (SetFaceForMoodHappy 0x4C8600, InPain 0x4C8500, Irritable 0x4C8570, Lonely 0x4C85C0) but nothing calls or references them (xref refs: 0 each); docs/wiki/creature/pose-and-animation.md#the-faces-a-creature-pulls ("no agenda uses them")
- The hair hidden altogether: openblack-only debug switch (CreatureHairSystem::SetShown, the debug spawner's "Show hair"); no such switch in the original

#### creature/reactions

From [Reactions](progress/creature/reactions.md).

- Its priority for each kind of reaction depends on the creature (how frightened or curious it is): a guess the exe does not bear out; the creature's priorities are the reaction table's priority, by distance for a nasty miracle, by the facts of the kind (bw1-decomp CreatureReaction.cpp, docs/wiki/creature/mimicry.md#reactions-to-miracles); fear and curiosity only choose what it does once it has taken a reaction up
- Only the nearest few creatures cast shadows (appearance): every creature body makes its own projected shadow (`CreateDynamicShadow`, LH3DCreature 0x47F543) on the one shadow list with no count or distance cap; the row was written from expectation ([shadows](../rendering/shadows-and-reflections.md#projected-shadows-shadowinfo))
- Scripts can wipe a creature's mind (saves_and_files): `CLEAR_ACTOR_MIND` (0x6FE750) never touches a creature's mind; it tests for a town, ignores the answer and deletes a villager's script memory ([vanilla_bugs/creature.md](progress/vanilla_bugs/creature.md))
- The game makes a web page about the creature from a template, with pictures (saves_and_files): the `creature.lhp` archive of `creature_html.tmpl` and `creatureshot_%d.jpg` written every 600 ticks is for the online creature service, which is gone, and nothing reads it back ([players-creature.md](../creature/players-creature.md#the-creatures-files)); already listed under engine/saving_and_loading
- The Creature Cave can take a picture of the creature for that page (saves_and_files): the pictures are the same archive's `creatureshot_%d.jpg`, for the online service ([players-creature.md](../creature/players-creature.md#the-creatures-files))
- The game can print a creature's information and statistics (saves_and_files): `Creature::DisplayStatistics` 0x476070 is a debug dump, not the status panel ([fights.md](../creature/fights.md)): developer output
- A land script command lists all creatures (saves_and_files): `OUTPUT_CREATURES` is developer output, not used by the shipped map script (already listed under [scripts/land_script_commands](#scriptsland_script_commands))
- Creatures far from the camera are drawn with simpler meshes (appearance): in the v1.42 exe every model draws one level of detail, with no switch by distance ([meshes-and-lod](../rendering/meshes-and-lod.md), [visible-blocks-and-lod](../rendering/visible-blocks-and-lod.md#the-model-level-of-detail-in-v142))
- A planned creature pen is shown as a plan before the citadel is built (home_and_pen): the pen is the temple mesh's pen point; the civic creature pen (`CREATE_CREATURE_PEN`) is used by no land or playground script (already listed under building/civic_buildings) and the shipped lands make no pen as a thing of its own ([plans-and-needs.md](../creature/plans-and-needs.md))
- The pen's size and look come from the game's pen tables (home_and_pen): same evidence; the pen tables belong to the unused civic creature pen
- Scripts can call the player's creature home (home_and_pen): `CALL_PLAYER_CREATURE` (native 197, 0x6F3D40) only hands the player's creature to the script; no native calls it home ([players-creature.md](../creature/players-creature.md#the-natives-land-1-reaches-on-it))
- The creature creates its own home where it chooses (home_and_pen): the exe's CreateHome action is a stub that fails at once (`ConstructSubActionsForCreateHome` 0x4A5320 calls only `FinishActionUnsuccessfully`)

#### creature/creature_casting

From [Creature casting](progress/creature/creature_casting.md).

- It draws the miracle's gesture in the air before casting — the constructs add the gesture sub-action (0x58) only when `GMagicInfo[magic]` +0x2C is not 0, and no miracle of the game's tables has one (casting.md, the constructs and Pending): written but never run.
- It casts warming or cooling spells ... on a friend (the warming and cooling half of the row; the cure stays as its own row) — rows 136 CastWarmingSpellOnCreature and 137 CastCoolingSpellOnCreature have magic 0 in info.dat and `CanCreatureCastSpell` 0x4D2940 refuses magic 0, so the planner and the mimic copy never weigh them (casting.md, "The teleport, warming and cooling rows" [V]); whether a script's CREATURE_DO_ACTION reaches them is not read.
- The debug tools and testbed can tell a creature to cast any miracle at something — a developer tool of openblack's (the testbed's `TellCast`), not the original's behaviour; the code stays as a debug control.

### Player and interface

#### hand/creature_contact

From [Creature contact](progress/hand/creature_contact.md).

- Punching the creature: the punch cycle is in the hand's animations but the game never plays it
- Tickling as a separate act: the game has no tickle apart from stroking; its tickle cycle is what the hand plays while stroking

#### hand/hand_animations

From [Hand animations](progress/hand/hand_animations.md).

- The two spare slots of the standard set: empty in the game's file
- The spare slots of the reward and punishment sets: empty in the game's file
- The spare slot of the camera set: empty in the game's file
- The spare slot of the special holds: empty in the game's file

#### hand/hand_sounds

From [Hand sounds](progress/hand/hand_sounds.md).

- Guidance sounds follow a village totem's raising and lowering: not in the original: the guidance's start only remembers the share the hand took hold at, and its end, when the share let go is higher, only asks whether that guidance may play and reads the player's alignment class, then plays nothing ([the village totem](../magic/village-totem.md#the-original)); `audio::guidance::StartTotemRaiseSound` and `EndTotemRaiseSound` do the same and play nothing

#### hand/other_players_hands

From [Other players' hands](progress/hand/other_players_hands.md).

- A computer god's hand raises and lowers village totems: not in the original: the computer mind's score for raising a totem in a town is always none, so the action is never chosen ([the computer minds](../hand/computer-minds.md#scoring-a-node)); our score is none too

#### hand/totem

From [Totem](progress/hand/totem.md).

- A sound as the hand takes hold of a totem and as it lets go: not in the original: see hand/hand_sounds above; the totem's own rising sound and bell come from the worship share being set after the let go, as ours
- Computer gods raising totems: see hand/other_players_hands above

#### hand/pointing_and_tooltips

From [Pointing and tooltips](progress/hand/pointing_and_tooltips.md).

- Over a leash post: "Leash" and the leash's kind (Aggression, Compassion, Learning): The original never shows it: a post is no locked select, so the hand over one is Over Object and shows "Tap" ([tooltips](../interface/tooltips.md#state-13-can-select-lock)), as ours does
- Over a thing that can be asked: «Preguntar» 0xE7C "Ask": the original's state 24 gives it only for a query icon (`IsQueryIcon`, vtable slot 0x1FC), which no class overrides (every vtable's slot is 0x401B50, `xor eax, eax`), so it is never shown
- Showing a tooltip triggers the help system's matching help event: each builder passes a help event with its text (0x22 / 0x23 with a highlight's, 41 / 40 / 47 with the leash's), but `SubmitToolTips` 0x5C9A70 never reads that argument (no read in 0x5C9A70..0x5C9C60) and nothing reads it back, so no event is raised; the focus-leash `HelpProfile::Trigger(0x2F)` of «Traer Criatura» is a call of its own, ported

#### gesture/creature_gestures

From [Creature gestures](progress/gesture/creature_gestures.md).

- The game's tables have gestures for zooming to the creature and for ending a gift to it, both left empty: Not used by the original game
- A glowing chain trails behind the creature's hand while it draws a miracle's gesture before casting it: the
  creature's Gesture sub-action (88) starts SF_CreatureGestureChain at the point it points at, but a cast adds that
  sub-action only for a miracle whose `GMagicInfo` gesture is set, which the game fills only for the power-up types,
  and no creature cast in the game's tables is one ([casting](../creature/casting.md)); its only other caller is the
  dance editor's sub-action (DancingDuringEditor), a developer tool. No other start of that effect: written but never
  run (frozen v1.42 exe, 2026-10-22)
- A miracle cast on the creature shows a gesture effect that starts and dies away with the spell: the only gesture
  effect the creature's received-spell object keeps is the Gesture sub-action's chain above, and no miracle's particle
  type in info.dat is the creature gesture effect (31): a guess the original does not bear out (2026-10-22)

#### gesture/gesture_effects

From [Gesture trails and effects](progress/gesture/gesture_effects.md).

- Another player's gesture is heard from where their hand was: No network play
- Scripts can lay a gesture's trail on the land as if the player had drawn it (PLAY_GESTURE): the native is in the
  exe (`GScript::PlayGesture` lays the gesture from a point, so many metres across, for this computer's interface),
  but no script of the game calls it: `Scripts\Quests\challenge.chl`, the only compiled script, has no call of
  native 204, where PLAY_HAND_DEMO (266) is called 18 times and IS_FIGHTING (229) 16 times: written but never run
  (2026-10-22)

#### gesture/leash_gestures

From [Leash gestures](progress/gesture/leash_gestures.md).

- Only the player who leashed the creature can change or remove the leash by gesture: Only one player owns a creature in openblack's single-player game

#### gesture/recognition

From [Gesture recognition](progress/gesture/recognition.md).

- Recognised gestures are sent to the other players in a network game with where they were drawn: No network play

#### miracles/dispensers_and_seeds

From [Dispensers and one-off seeds](progress/miracles/dispensers_and_seeds.md).

- The disk is hidden while the dispenser is being built: openblack dispensers are never under construction
- In the testbed and debug tools a seed can be summoned straight into the hand as if from an icon: Debug and testbed tools don't count here (this file's rule); openblack's are `OPENBLACK_TEST_SEED` and the debug menus

#### miracles/forest

From [Forest](progress/miracles/forest.md).

- The flight no longer locks the camera and the player's movement (user-reported defect): the original's flight never locks them: a movement key that would move the camera, or a land grip, gives it back before the flight ends ([forest](../magic/forest.md#what-is-seen-the-sf_forest-effect)). openblack flies it too (the forest's camera particle, `src/Particles/Rules/Forest.cpp`), and the original's way of giving the camera back is tracked in [the caster's camera](progress/miracles/forest.md#the-casters-camera)

#### miracles/miracle_core

From [Miracle effects and reactions](progress/miracles/miracle_core.md).

- A history of alignment deeds is kept for statistics: Our tree keeps none. Our wiki differs: the history only feeds a debug overlay that retail never shows, so nothing in the game reads it ([alignment](../magic/influence-and-alignment.md#player-alignment))

#### miracles/water

From [Water](progress/miracles/water.md).

- Old rings keep drifting with the wind the way a reused ring slot did in the game: Not in the original: nothing ever sets a ring's drift, and a reused slot keeps nothing that is drawn ([water.md](../world/water.md#water-rings)); test `WaterRingSystem.RingsStayWhereTheyWereMade`

#### miracles/wood

From [Wood miracle](progress/miracles/wood.md).

- There is no extreme version: the game has none

#### camera/follow_cameras

From [Following cameras](progress/camera/follow_cameras.md).

- The editor's orbit and follow cameras: openblack-only, `src/Camera/EditorCameraModel.cpp`; see [../debug/](progress/debug/)

#### camera/world_camera_controls

From [World camera controls](progress/camera/world_camera_controls.md).

- A setting to speed the movement keys up or down: openblack-only (editor camera speed), `src/Camera/KeyboardMoveSpeed.h`; see [../debug/](progress/debug/)

#### interface/options

From [Options](progress/interface/options.md).

- Screen resolution and the 3D card are picked outside the game, in its setup: openblack takes `--width`, `--height` and `--backend-type` on the command line

#### interface/profiles

From [Profiles](progress/interface/profiles.md).

- Creatures can be uploaded and downloaded online: the service no longer exists

#### interface/statistics

From [Statistics](progress/interface/statistics.md).

- At the end of an online game the figures are also sent to Lionhead's statistics database: the server is gone; see [statistics_counted.md](progress/interface/statistics_counted.md) and [../pc_integration/online_services.md](progress/pc_integration/online_services.md)
- The text tables hold labels among the box's that nothing uses: "Advanced", a second "Back", "Credits" and "Total Polygons Drawn": unused by the game; nothing to do
- The text tables hold labels for five creature miracles the box never lists (Hungry, Frightened, Tired, Ill, Thirsty): unused by the game; see [statistics_counted.md](progress/interface/statistics_counted.md)
- Older versions saved the graph lines as objects of their own; the save code still names that type as obsolete: nothing to load

#### interface/statistics_counted

From [Statistics: what is counted](progress/interface/statistics_counted.md).

- In a network game each machine counts every player's figures itself; nothing extra is sent: see [../multiplayer/network_play.md](progress/multiplayer/network_play.md)
- "Total Polygons Drawn" has a label but is neither counted nor listed: nothing to do
- At the end of an online game one record goes to Lionhead's database: the game's version and map, the places, the players' accounts and clan, and every figure above by name: the server is gone; see [../pc_integration/online_services.md](progress/pc_integration/online_services.md) and [../multiplayer/clans.md](#multiplayerclans)
- If the database can't be reached the player is told "Could not connect to database. Your creature and/or statistics will not be updated.": the server is gone

#### interface/tooltips

From [Tooltips](progress/interface/tooltips.md).

- The mouse picture's blink kind 2 (the button lit twice for 0.16 s in each 0.96 s, and the wheel's frames chosen from a cycle of 12 eightieths): written but never run, every caller asks for the key, the always lit button or the plain blink; see [../interface/tooltips.md](../interface/tooltips.md) ("animType 2 and 3 are never passed"). openblack's `Renderer::DrawKeyOrMouseMouse` had kind 2's blink branch, which nothing of ours passed either; it is removed (types 0 and 1 draw as before)

#### temple/main_room

From [Main room](progress/temple/main_room.md).

- The temple's help held back (`Temple::HelpSuppressed`, +0x44): its only writer, `Temple::InitEngine` 0x793C63, sets it to 0 on every entry, and `InitEngine` ignores the help flags `GoInsideCitadel` passes it, so no help of the temple's rooms, scrolls or places ever waits ([the temple's help scripts](../interface/temple-interior.md#the-temples-help-scripts)); openblack's never-set `temple_help::HelpHeldBack` is removed (5a2c31544)
- The temple scrolls' gesture pictures ("$g", `TempleRoom::FormatTextureForScroll` 0x7998C9, fn_005760C0): written, but the only texts holding one (HELP_TEXT_DYK_15 and DYK_16) are never placed by `challenge.chl` nor named by the exe, and no dialogue line has one, so no scroll shows one (scanned 2026-10-22, [the world's statistics scroll](../interface/temple-interior.md#the-worlds-statistics-scroll)); openblack leaves a gesture's line blank
- The scroll font's half-height glyphs (`GatheringText::DrawChar2Texture` 0x8319E0 at sizes of 26 and over): the only writer of text into textures, `DrawText2Texture` 0x831DF0, is called only from `TempleRoom::FormatTextureForScroll` (0x799671 / 0x7996C4) at the line height [0xE36104], which is always 10 (written only at 0x79A6ED = 256 / 24 and 0x79A747 = 10): never run; openblack draws only the quarter-height path

#### temple/creature_cave_room

From [Creature cave room](progress/temple/creature_cave_room.md).

- The cave's attack dummies starting the creature's fight practice when no temple script runs (an earlier guess from a TODO since removed): the zoom to the belts and medals (fn_00789FA0, its only caller `CreatureRoomCamera::UpdateMain` 0x78AB4C) only starts the help scripts "CitadelCreatureRoomAttackDummies" and "CitadelCreatureRoomMagicPlinths", and only while no temple script runs; nothing in the creature room starts a creature action but the copy's idle and yawn animations (`InitEngine` 0x787D64 / 0x787D82, `DrawAdditional` 0x7887D0), and the exe has no practice of fighting (its only "Practice" strings are the creature mind's `PracticeDance` and `PracticeThrow`) ([the temple's help scripts](../interface/temple-interior.md#the-temples-help-scripts)); openblack has no such code

#### temple/options_room

From [Options room](progress/temple/options_room.md).

- Closing the options as the player leaves the temple: the original's way out (`GGame::LeaveInsideCitadel` 0x553A70, the rooms closed through fn_00793D00 and their `CloseEngine`) hides no dialog, and none of `DialogBoxBase::Hide` or `HideAll`'s callers is on it (only `Temple::GoToRoom` 0x793DD7, room to room, and `GoInsideCitadel` 0x554061 when already inside hide them), so options left up by a script taking the player out stay up on the land ([the options inside the temple](../interface/temple-interior.md#the-options-inside-the-temple)); openblack's close in `TempleInterior::Deactivate` is removed (ff05f7259)

#### temple/temple_hand_and_controls

From [Temple hand and controls](progress/temple/temple_hand_and_controls.md).

- The hand holding the creature's leash collar inside the temple (an earlier guess): going in takes the leash off (`GGame::GoInsideCitadel` 0x553F5A sends packet 0x5B, whose handler 0x63D3CE calls `GLeashStatus::SetOn(creature, 0)`); the hand keeps any other thing it holds, in the citadel's state (see the row "Going inside takes the creature's leash off")

#### temple/temple_exterior

From [Temple exterior and entrance](progress/temple/temple_exterior.md).

- The camera flying up to the temple's entrance before going in: `GGame::GoInsideCitadel` 0x553E10 makes no camera call and all ten of its callers go in at once; `Temple::InitEngine` 0x793C60 starts the room's intro with `TriggerIntroCamera(0, 0, 0)`, so `InnerCamera::TriggerIntro` 0x7957C0 does not start from the island's camera (only `Temple::GoToRoom` 0x793E26 passes 1, room to room); the way in is the room camera's own path, as openblack's (`TempleInterior::Activate`, `StartIntro(room, false)`)

#### multiplayer/clans

Clans: Clans of players who play internet games together: a team can play for a clan, which then scores instead of its players, brings its own creature from game to game, and shows its logo as the team's symbol. The game asks the online servers for all of it; the clans themselves were run on the game's web site.

- The game lists the clans a player belongs to: Online service, not part of this codebase: it lives in bwgame-service (there: `online/services.py`, `tests/test_clans.py::test_game_clan_list_puts_the_first_joined_clan_first`); our tree has no online code
- Each clan in that list carries a yes/no flag: Online service, not part of this codebase: it lives in bwgame-service
- A player's details show their clan: Online service, not part of this codebase: it lives in bwgame-service
- The clan details: name, logo, and the lines "Clan Description:", "Clan Leader:", "Clan URL:" and "Credits": Online service, not part of this codebase: it lives in bwgame-service
- The clan logo is a picture file the game loads, keeping a 64 by 64 shape in 16 shades: Online service, not part of this codebase: it lives in bwgame-service (there: `online/logo.py`, `docs/research/clan-logo.md`); our tree has no online code
- A team picking a clan gets the clan's creature: Online service, not part of this codebase: it lives in bwgame-service
- A clan with no creature yet tells the game to create one of the chosen type and name: Online service, not part of this codebase: it lives in bwgame-service (there: `tests/test_clans.py::test_clan_creature_numbers_are_the_games_table_rows`); our tree has no online code
- At the end of a clan game, the team leader's game uploads the clan creature: Online service, not part of this codebase: it lives in bwgame-service
- At the end of an internet game, the host reports the results and gets everyone's new points: Online service, not part of this codebase: it lives in bwgame-service (there: `online/services.py`); our tree has no online code
- A team playing for a clan scores for the clan instead of its players: Online service, not part of this codebase: it lives in bwgame-service
- Clan names and texts reach the game in its character set: Online service, not part of this codebase: it lives in bwgame-service
- The clan logo becomes the symbol of a team playing for the clan, over its towns and on its hand: Online service, not part of this codebase: it lives in bwgame-service; no clans in our tree; the symbol over towns is drawn from the `ChooseSymbol` sheet (`src/Particles/TownBelief.cpp`), never a clan logo
- The clan logo shows in the multiplayer screens' clan box: Online service, not part of this codebase: it lives in bwgame-service; no multiplayer screens in our tree
- Results count only when sent with the password of an account that played in that game: Online service, not part of this codebase: it lives in bwgame-service (there: `tests/test_abuse.py::test_results_need_a_valid_password`, `test_results_only_from_a_player_of_that_game`); our tree has no online code
- A team scores for a clan only when all its players are members of it: Online service, not part of this codebase: it lives in bwgame-service (there: `tests/test_abuse.py::test_a_team_scores_for_a_clan_only_when_all_are_members`); our tree has no online code
- Oversized result reports are refused: Online service, not part of this codebase: it lives in bwgame-service
- A signed-in player founds a clan and leads it: Online service, not part of this codebase: it lives in bwgame-service (there: `online/clans.py`, `web/views.py`); our tree has no online code
- A clan has one leader, officers and members: Online service, not part of this codebase: it lives in bwgame-service (there: `online/models.py`); our tree has no online code
- Each clan chooses who can join: anyone, by request, or by invitation only: Online service, not part of this codebase: it lives in bwgame-service
- Players ask to join; the leader or an officer approves or declines; the player can withdraw: Online service, not part of this codebase: it lives in bwgame-service
- The leader or an officer invites a player by name; the player accepts or declines; the invitation can be withdrawn: Online service, not part of this codebase: it lives in bwgame-service
- An invitation lets a player in whatever the clan's joining rule: Online service, not part of this codebase: it lives in bwgame-service
- The leader or an officer removes members; officers can't remove officers or the leader: Online service, not part of this codebase: it lives in bwgame-service
- Removed players can be banned from rejoining, asking or being invited, and the ban lifted: Online service, not part of this codebase: it lives in bwgame-service
- The leader makes members officers and back: Online service, not part of this codebase: it lives in bwgame-service
- The leader hands the leadership to a member, becoming an officer: Online service, not part of this codebase: it lives in bwgame-service
- A leader who leaves is followed by the longest-serving officer, else the longest-serving member: Online service, not part of this codebase: it lives in bwgame-service
- A clan whose last member leaves is disbanded: Online service, not part of this codebase: it lives in bwgame-service
- The leader disbands the clan, typing its name to confirm: Online service, not part of this codebase: it lives in bwgame-service
- The leader edits the name, motto, web site, joining rule, and the creature's type and name: Online service, not part of this codebase: it lives in bwgame-service
- The leader uploads a logo, shown on the site as the game will draw it: Online service, not part of this codebase: it lives in bwgame-service
- The leader removes the logo: Online service, not part of this codebase: it lives in bwgame-service
- The leader resets the clan creature, so the next clan game starts a new one: Online service, not part of this codebase: it lives in bwgame-service
- A player belongs to at most a few clans, and a clan has at most a set number of members: Online service, not part of this codebase: it lives in bwgame-service
- Founding clans, joining requests and clan changes are rate-limited: Online service, not part of this codebase: it lives in bwgame-service (there: `accounts/throttle.py`); our tree has no online code
- Clan names, mottos, web sites and creature names are checked for blocked words, reserved names and hidden characters: Online service, not part of this codebase: it lives in bwgame-service (there: `moderation/`); our tree has no online code
- The clan list shows each clan's logo, leader, members, joining rule and points: Online service, not part of this codebase: it lives in bwgame-service
- The ranking lists clans by points: Online service, not part of this codebase: it lives in bwgame-service
- Clan tags shown beside players' names: the game has no clan tags; a player could only type one into their own name
- Every new logo is put on the admins' review list (pictures aren't checked automatically): Online service, not part of this codebase: it lives in bwgame-service
- Admins see and change a clan's members, requests, invitations and bans: Online service, not part of this codebase: it lives in bwgame-service
- Admins remove a clan's logo, or clear its motto, web site and creature name: Online service, not part of this codebase: it lives in bwgame-service
- Clan text that matches a blocked word added later is hidden on the pages: Online service, not part of this codebase: it lives in bwgame-service

#### multiplayer/gathering_box_and_ping_server

The gathering box and the ping server: While a game world runs, the game keeps in touch with Lionhead's "ping server" (`bwping.bwgame.com`, UDP port 2611): it says it is playing, asks which of its friends are online, and takes messages for its gathering box, the in-game box of players, friends and others. Lionhead's staff used it to speak to players under ids the game draws in yellow.

- Every 20 seconds of play the game pings the server with its player's id; it expects no answer: Online service, not part of this codebase: it lives in bwgame-service (there: `bwgame/bwping.py`, `tests/test_bwping.py`); our tree has no online code
- It sends the ids of its player's friends (at most 25); the server answers with those who are online and where: Online service, not part of this codebase: it lives in bwgame-service (there: `BW_PING_FRIENDS`); our tree has no online code
- A friend listed online keeps the online mark for 7.5 minutes: Online service, not part of this codebase: it lives in bwgame-service
- Chat between players in the gathering box goes to the address the game holds for that player: Online service, not part of this codebase: it lives in bwgame-service
- The host of an internet game reports its teams and the map's conditions every 20 seconds and when the game is decided: Online service, not part of this codebase: it lives in bwgame-service
- The game takes these messages from anyone, in single packets with no handshake: Online service, not part of this codebase: it lives in bwgame-service
- Players with ids 9999999 to 10000014 are drawn in yellow in the gathering box: Online service, not part of this codebase: it lives in bwgame-service
- A message from id 9999999, the admin, opens every player's gathering box: Online service, not part of this codebase: it lives in bwgame-service
- A staff member's chat line shows as "name: text" in the gathering box, and in the in-game chat and its log: Online service, not part of this codebase: it lives in bwgame-service
- A message starting with a secret word opens a Yes/No question, or a box to type a line into; the answer goes back to the sender: Online service, not part of this codebase: it lives in bwgame-service
- What a player types to a staff entry in the gathering box reaches the server: Online service, not part of this codebase: it lives in bwgame-service
- Lionhead's announcements: a text shown as up to 15 yellow names in the "Others" list: Online service, not part of this codebase: it lives in bwgame-service
- Who is playing right now: Online service, not part of this codebase: it lives in bwgame-service
- The gathering box's lists, friends list and chat window themselves: Online service, not part of this codebase: it lives in bwgame-service; no gathering box, friends list or chat window in our tree

#### multiplayer/maps/death_comes_to_those_that_wait

From [Death Comes To Those That Wait](progress/multiplayer/maps/death_comes_to_those_that_wait.md).

- A fan's land, signed "By Fat Omen" in its title, built on Four Gods: the same land size, the same corner temples and home towns for players three and four, and the same spot for the computer creatures: history of the file; neither file is among those the game installs

#### multiplayer/maps/firestorm

From [Firestorm](progress/multiplayer/maps/firestorm.md).

- The script is the developers' three-player online map with only its landscape path and some villagers' ages changed; the game's own online condition list still names that map (`mpm_3p_2`): history of the file; the original download is lost (see bwgame-service's notes on lost maps)
- Neither the script nor its landscape is among the files the game installs; the skirmish box shows any script placed in its folder: how it reached this install is not part of the game
- Online this map was downloaded from the servers like the three shipped ones: the servers and this map's online file are gone; bwgame-service could offer the copy

#### multiplayer/maps/island_wars

From [Island Wars](progress/multiplayer/maps/island_wars.md).

- The script opens with a note that it was modified with a fan-made map editor; it is the developers' four-player online map (named `mpm_4p_2` in the game's online condition list) with three computer gods and their creatures added, temples moved after their towns, and the two small Greek towns' miracles reshuffled: history of the file; the original online download is lost
- Neither the script nor its landscape is among the files the game installs; the skirmish box shows any script placed in its folder: how it reached this install is not part of the game
- The developers' original of this land was downloaded from the online servers; its winning conditions started at 11,000 food or wood, 2,100 belief and up to 8 towns taken: the servers and that map's online file are gone; the game's condition list still holds its numbers ([../multiplayer_rules.md](progress/multiplayer/multiplayer_rules.md))

#### multiplayer/multiplayer_rules

From [Multiplayer rules](progress/multiplayer/multiplayer_rules.md).

- A team's leader can play for a clan, whose creature it then uses (patch 1.2): clan creatures came from the original's servers, which are gone; our tree has no online code (the server side is only in bwgame-service, see [clans.md](#multiplayerclans))
- Clan details and lists: the original's servers are gone; our tree has no online code (bwgame-service only, see [clans.md](#multiplayerclans))
- Internet games earn points for the online ranking, more for beating a better player: the original's ranking servers are gone

#### multiplayer/network_play

From [Network play](progress/multiplayer/network_play.md).

- Internet games are listed in the online lobby (GameSpy): the original's lobby servers are gone
- Logging in to the internet game with a name and password: the original's servers are gone

#### multiplayer/online_services

From [Online services](progress/multiplayer/online_services.md).

- Real weather: the land's weather follows the weather at the player's town, fetched online: the original's weather service is gone; our tree has no online code; see [../pc_integration/real_weather.md](progress/pc_integration/real_weather.md)
- Choosing the player's country and town for the real weather: as above
- New e-mail is announced by a villager in the game: relies on Outlook or a POP3 account
- The registration site for online play: the original's site is gone

### Story

#### story/advisors

From [Advisors and characters](progress/story/advisors.md).

- The audio CD left in the drive: the game's own disc check
- Multiplayer games have their own banter set of taunts: openblack has no multiplayer (the `MultiBanter` script exists, but there is no multiplayer game)

#### story/creature_guide

From [Creature Guide](progress/story/creature_guide.md).

- The guide exists only in the first land of the story; not in skirmish, multiplayer or the Creature Isle: nothing to do beyond the story itself
- The development stages "meeting its guide", "the guide explains the history" and "helping a town" are never set by any shipped script (the guide's chapters jump from making friends to miracles, impressing, fighting and full maturity): data quirk
- An older, unshipped set of lessons: a horse guide explaining its past on the mountain (Nemesis's temple, its loneliness), teaching miracles with fireball and heal seeds, helping and impressing a town, a friendship scene where it shrinks itself with a miracle: in the source but not in the program
- An unshipped storm brewing scene and older stand-alone storm, meeting and fight scripts: not in the program
- A shipped but never started script for taking over the land's villages, where the advisors guess the big creature wants them to be the Aztec village's god: only started by an unshipped control script
- Test scripts that run the whole guide story or the storm alone: not started by the game
- A cut line in a later land says the Creed lies in the body of the one the player knew as the Guide: commented out of the fourth land's script

#### story/creature_isle

Creature Isle: The expansion: a new island where the creature, not the player, is the hero, with the Brotherhood of Creatures and Tyke. openblack targets the original game only.

- The Creature Isle land and its challenges: the expansion is not supported
- Tyke, the young creature who follows the player's: the expansion is not supported
- The Brotherhood's trials: the expansion is not supported
- The expansion's extra creature spells: the expansion is not supported (its spells are commented out in `src/Enums.h`)

#### story/ending

From [The ending](progress/story/ending.md).

- Nemesis's mirror fight against the player's creature's opposite self: in the scripts but never started; [gold_scrolls/so_this_is_a_fight_to_the_death.md](progress/story/gold_scrolls/so_this_is_a_fight_to_the_death.md)
- A script steering Nemesis's battle tactics: in the scripts but never started

#### story/gold_scrolls/choose_your_creature

From [Choose Your Creature](progress/story/gold_scrolls/choose_your_creature.md).

- The cow's look-at point in the opening of the glade has a typing slip (a coordinate ten times too far north), so it stares far off past the glade: a data quirk in the script itself; our tree runs the script as shipped
- A cut placement of the ape stone first (and its line "Right. We need another Gate Stone…", missing from the text table) is commented out of the script: not in the program
- An older, unshipped version of the quest (a good-advisor "Shh." line and a swap-style "are you sure" line) and a bare test script ("Choose one of these mothers"): not in the program
- A cut silver scroll version of the sculptor's part: see [../silver_scrolls/the_sculptor_cut_silver.md](#storysilver_scrollsthe_sculptor_cut_silver)

#### story/gold_scrolls/choose_your_creature_bafta_draft

Choose Your Creature (BAFTA draft): A cut, shortened draft of the first land's "Choose Your Creature" gold scroll: clicking the scroll at the creature gate skips the hunt for the gate stones and plays straight into the stones sinking, the gates opening, the flight to the valley and the choice of cow, ape or tiger. It sits in an uncompiled file and is started only by an uncompiled "BAFTA" control script, apparently made for an awards demonstration. The shipped quest belongs to the first land (see [../land_1.md](progress/story/land_1.md)); this file only covers the draft.

- The draft file is not among the files compiled into the shipped `challenge.chl`; the shipped game has the full "Choose Your Creature" from its own file instead (same script name, so both could never be compiled together): never started by the game
- The only thing that would run it is the uncompiled demo control script, which starts "old" versions of the Lost Brother, the Pied Piper and Choose Your Creature; none of those "old" names exist in any script, so even that script would fail: never started by the game; see [../../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- All its lines are in the shipped text table (they are the shipped quest's lines); its markers are the first land's creature gate, plinth and valley: never started by the game
- A gold scroll is put at the creature gate, 14 up; the evil advisor points it out when the camera comes near: "There's a Silver Reward Scroll down here, Boss." (the shipped line, wrong scroll colour and all): never started by the game
- It never opens a story-log entry: it only updates one (to 0.75, reminder "We need to choose a Creature!"), so the log entry would have to come from elsewhere: never started by the game
- Two helper scripts for the gate-stone hunt (the advisor spotting the second gate stone, the trainer's reminder that the stone isn't carved) are in the file but never run; a "start russ" note marks where the hunt was cut out: never started by the game
- The three creatures are made in the valley; the trainer is made at her hut, walks to her door and to a talking spot, and turns to the camera: never started by the game
- Trainer: "The Gate Stones are together. Pass through and claim your Creature!" (a comment holds an earlier joke line): never started by the game
- The camera moves to the plinth; the gate-stone sound plays, the camera shakes (amplitude 0.1 for 7 seconds within 20) and the plinth opens with the stones sinking: never started by the game
- The camera looks at the gate chain; the gates open with the bolt sound; the trainer beckons; the gate-open sound plays as the camera pulls back: never started by the game
- The camera flies to the valley along the prepared path with the creature-choosing music; the creatures turn to the camera: never started by the game
- The advisors talk as the creatures act up: good "Look at them. Just look at them. These are Miraculous Creatures indeed."; evil "They certainly are. But not quite as big as I expected."; good "Not yet, maybe. But they can become the most powerful Creatures in the world."; evil "Now that I'd like to see. We must have one. Which should we choose?"; good "Any. They're all special."; evil "If rather small at the moment."; good "Shh.": never started by the game
- The music stops and the camera is kept to the valley: never started by the game
- Holding the hand over a creature makes it beg to be picked; the first time over each, the advisors pitch it: cow (good "We could have the cow. A strong and noble beast." evil "What? Not the fierce, lethal tiger? Click the Action Button on him!"), ape (good "Hmm. How about the ape. Intelligent and quick to learn?"), tiger (evil "I'm up for the tiger. Look at those claws."): never started by the game
- Each creature turns to the camera again every 5 seconds: never started by the game
- Clicking a creature once asks "Are you sure you want this Creature? …" (good); clicking the same one again chooses it; clicking another resets the count: never started by the game
- On choosing: a 10-second close-up, evil "We've chosen a Creature.", a 3-second fade to black, the creature looks at the camera, becomes the player's creature, is moved to its pen and given it as home; the other two are deleted: never started by the game
- The creature development scripts are started and the scroll is marked complete with the reminder "We should speak to the sculptor about the Gate Stones." (a leftover from the full version's order): never started by the game

#### story/gold_scrolls/destroy_it

From [Destroy it!](progress/story/gold_scrolls/destroy_it.md).

- The creature takes no part in this scroll: nothing in the script touches it
- Two flags meant to say whether the left and right towns are needed are set but read only by the fly-over itself: nothing else uses them

#### story/gold_scrolls/fire_fire_im_on_fire

From [Fire! Fire! I'm on Fire!](progress/story/gold_scrolls/fire_fire_im_on_fire.md).

- A test switch skips the 60-second wait: only the test launcher uses it

#### story/gold_scrolls/i_have_a_surprise_for_you

From [I have a surprise for you.](progress/story/gold_scrolls/i_have_a_surprise_for_you.md).

- The "village taken back" lines can never play: the check for Nemesis retaking a village tests the same condition as the check for the player owning it, so it is never reached, and the player's "retaken the town" lines never play either because a village is never marked as lost. Unreachable lines: good "Excellent! We've retaken the town!", Nemesis "Ha! That is how it should be!", evil "We're the best. We are! Way to go!", Nemesis "Ha. That was too easy to be a Challenge!", evil "Ha! Maybe Nemesis isn't as powerful as he likes to think." (which would have pointed at the Greek village while speaking of the Tibetan one) and Nemesis "Welcome back, my worshippers. Do not stray again.": never reachable in the game
- Nemesis's "You'll find he won't be of much use to you now." is commented out of the opening film: never in the game
- The advisors' "Where's our Creature going?" and "Where's he off to now?" were cut from the nightly lines ("the creature doesn't really move too much into Nemesis's land"): never in the game
- Nemesis's "I have one more surprise for you." has no script that says it: never in the game
- An earlier curse script is compiled into the game but nothing starts it: it struck every 60 seconds at night, took 0.1 strength (Aztec), 0.1 fatness (Greek, not height) and 0.2 alignment (Tibetan) per strike, played a first film ("Nemesis curse has harmed our Creature!") and later "Our Creature is suffering further from Nemesis' curse!" then "He's heading towards Nemesis himself!" or, if leashed, "The dark of night must be the key to this!", kept the creature's gains while cursed so they could be added back, and ended only when the player owned Nemesis's village: never started by the game
- Its helper that walks the creature to Nemesis's village and waits until it is leashed or within 100 is also unused: never started by the game

#### story/gold_scrolls/impress_village

From [Impress Village](progress/story/gold_scrolls/impress_village.md).

- Two spare influence demo scripts exist in the hand demo file; one has its demo commented out, the other plays a second influence recording (`influence2.hnd`); nothing in the game runs them: never in the shipped game
- An earlier place for the 0 log entry, after the gesture demo, is commented out: commented out
- A wood check on the store (2,800 wood as well as food) is commented out: commented out
- A note to clear the repeat-miracle icon and remove the leash "for confusion" is left as a to-do: a to-do note in the source

#### story/gold_scrolls/khazars_fireball_challenge

From [Khazar's Fireball Challenge](progress/story/gold_scrolls/khazars_fireball_challenge.md).

- Unused lines in the text table: Khazar's "Here are some Fireball Miracle seeds." and "Using these fireballs.", the evil advisor's "Oh Boss. You didn't hit anything at all.", and two entries marked "NOT USED": in the text table but never spoken by the shipped script

#### story/gold_scrolls/khazars_shield_challenge

From [Khazar's Shield Challenge](progress/story/gold_scrolls/khazars_shield_challenge.md).

- The hand demo's line "Make sure you join up with the start position." is cut; the script's comment says the speech was too slow for the demo: in the text table, commented out in the demo
- Unused retry lines in the text table: "I give you more Seeds to practice with.", "You were unsuccessful. Try again.", "Try recasting the Shield Miracle." and "Draw a circle with the Action Button pressed to mark the position and size of the Shield.", which fit the multi-attempt loop the shipped script never gives; two more entries are marked "NOT USED": never spoken by the shipped script
- A commented-out timer that would have dropped the boulders after 30 seconds whatever the player did: left in the source as comments
- A commented-out final log update at the end: commented out

#### story/gold_scrolls/leave_through_the_vortex_land_1

From [Leave Through the Vortex (Land 1)](progress/story/gold_scrolls/leave_through_the_vortex_land_1.md).

- The prepared "Click the Action Button on the Scroll and we can go through the Vortex." line is never said (the reminder's own first notice is commented out): cut use of an existing line

#### story/gold_scrolls/leave_through_the_vortex_land_2

From [Leave through the vortex (Land 2)](progress/story/gold_scrolls/leave_through_the_vortex_land_2.md).

- The player has no creature here: Lethys has taken it: the script doesn't touch a creature

#### story/gold_scrolls/lethys_has_taken_our_creature

From [Lethys has taken our Creature!](progress/story/gold_scrolls/lethys_has_taken_our_creature.md).

- A script that would ring Lethys's temple with five lightning storms (time 80, no wind, heavy forks) is written but its start is commented out: never started by the game
- A high camera spot is set up but never used; the good advisor's "No!" is in the text table but unused: unused
- Pointing the advisors at the creature is commented out: commented out

#### story/gold_scrolls/nemesis_no

From [Nemesis. No!](progress/story/gold_scrolls/nemesis_no.md).

- Lines written but not played: Nemesis "Did you think you could hide the Creed from me?", "Creature, you have something for me.", "Lethys, retrieve the Creed for me."; the good advisor "That must have been the Creed the Guide was talking about!" and the evil advisor "And we lost it."; good "Maybe we haven't lost it forever, Demon." and evil "I hope you're right, Beardy.": commented out in the source
- Lines in the text table never used by any script: Nemesis "I will be the only god." (an earlier copy), Khazar "I'm losing my…", good "Oh no! Khazar's Creature!", Nemesis "Lethys, use your Creature to bring the Creed to me.", evil "He's taken the Creed from Khazar's Creature!", good "So this is what the Guide meant!": never used by any script
- Dropping the Creed as an object for Lethys's creature to pick up, and freezing Khazar's creature with a creature spell, were tried and replaced by the glow moving from one creature to the other: commented out in the source

#### story/gold_scrolls/nemesis_shielded_village

From [Nemesis's Shielded Village](progress/story/gold_scrolls/nemesis_shielded_village.md).

- The shield's creature check at the centre stone does nothing: the reaction it was meant to start (the creature running away) is commented out: never in the game
- The control script still holds a comment that the shield script replaced the Tibetan village's old lines, though its capture film is still there: nothing to do
- The third worshipper's own line "No one shall harm us or test our faith." was dropped as a bad recording (it is filed under the good advisor's voice): never in the game
- A fuller version had shouts when worshippers gave up or were hit ("Argh! I'm giving up!", "Ow! I give up!", "I've had enough. This is too dangerous.", screams), advisors' comments on each ("Perfect. He survived. Let's move quickly before he recovers.", "I spy a dead guy", "He's toast.", "You iced him, Boss.", "You got him, but the guy ran off.", "He'll be back sooner or later to reactivate the Shield, though.") and "One of the rays has been reactivated!"; no script says them: never in the game
- A battle plan for Nemesis is compiled into the game but nothing starts it: every 30 seconds it would have had him defend his own village when the creature or camera came within 250, destroy villages the player had won, impress villages the player was gaining, and attack the creature with spells when it was outside the player's influence: never started by the game; see [../../rival_gods/nemesis.md](progress/rival_gods/nemesis.md)

#### story/gold_scrolls/release_the_creature

Release the Creature: A cut gold-scroll story chapter in which the player's creature is found tied by its leash to a boulder in a prison on a hill, watched by a hostile creature, and the player frees it by hitting the boulder with thrown rocks. It was never compiled into the game and its title and dialogue were never written into the text table; the shipped third land frees the creature from Lethys's prison statues instead ([so_you_couldnt_bear_to_be_without_your_creature.md](progress/story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md)).

- The script is not among the files compiled into the shipped `challenge.chl`: never started by the game: only its own (uncompiled) test launcher runs it
- Its challenge name keeps an entry in the compiler's list of challenges: never started by the game
- Its title (title number 22), its reminder and all seven of its "save your creature" lines are missing from the shipped text table and from the development build's text source, so it could not even be compiled against the shipped text: never started by the game
- The shipped third land's arrival script still has a commented-out log line with this same title and reminder, showing the chapter was folded into the third land's arrival: see [so_you_couldnt_bear_to_be_without_your_creature.md](progress/story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md)
- Its markers do not match a village on any shipped land closely (the nearest town to the prison is 110 away on the first land and 200 to 480 away on the others): never started by the game
- It waits on a "Wonder built" flag that is declared for every script but that no script, shipped or cut, ever sets, so even if started it would stop for good before its cut scene: never started by the game
- The player's creature is moved into the prison on the hill; a boulder is made beside it at one and a half times size, which can't be picked up or moved, and the creature's leash is tied to the boulder: never started by the game
- The creature is made fully mature (marked in the source as something to remove) and made to look sad: never started by the game
- An opposing creature is made beside the prison as a copy of the player's creature, but a cow, fully mature, wanting only to be angry (a note says it should become the opponent's real creature): never started by the game
- Five half-size rocks are put around the opponent, which looks at the player's creature and then at each rock in turn without approaching: never started by the game
- Five rocks are put at a spot near the player's influence as ammunition; each one that is thrown away more than 20 from its spot is put back there and counted as used, and a lost one is made again, until the creature is freed: never started by the game
- Once the Wonder flag is set, an area of influence of radius 80 is made at the ammunition spot: never started by the game; the flag is never set
- It starts when the camera comes within 150 of the creature and the creature is on screen; both advisors step out: never started by the game
- The story log gets a gold scroll (title 22, reminder 21) at nothing done: never started by the game; neither text exists
- Seven lines are spoken in turn: never started by the game; their words were never written, so who speaks them is unknown
- When anything hits the boulder, it becomes movable and can be picked up, the leash is untied and the creature is free: never started by the game
- Until then the creature cycles between looking sad, looking at the opponent and standing idle: never started by the game; a mistake in the counting (the idle step waits for a count that is never reached) means it is sad once, looks at the opponent once and then stops changing
- Nothing marks the scroll complete or reports the rescue: the script just ends: never started by the game

#### story/gold_scrolls/so_this_is_a_fight_to_the_death

From [So this is a fight to the death](progress/story/gold_scrolls/so_this_is_a_fight_to_the_death.md).

- Nothing comes after it: it is the game's last quest: story order: curse lifted, Nemesis's last town, this
- An earlier mirror fight ("Creature Mirror Fight") is compiled into the game but nothing starts it (only its single-quest test launcher): Nemesis's hand flies around the player's creature, he says "Your Creature has proved itself many times. But can it beat its true self?", makes a copy of the creature next to it with the opposite alignment, only anger as a desire (at half) and fully grown, and says "This is your Creature if you'd been the god you should have been." and "Watch and weep as it shows its superior strength and intellect.": never started by the game
- In it the advisors cling to the screen edges and say "We'll give it a pummelling!" (evil) and "Er, yes. I hope so." (good); the fight starts when the creatures are within 50; winning: "We did it! Did you see that? Oh yeah! The crowd goes wild!" and the copy is deleted; losing: "We're losing. Stop and let our Creature heal." and "Come on! Crush him!": never started by the game
- A cut speech by Nemesis over a monsoon storm at 4:30 pm ("My dream was to be the only god.", "To have the whole of Eden worship me.", "But you stand in opposition.", "So this is a fight to the death") is not compiled into the game; its last line is used as this quest's log title: not in the shipped `challenge.chl`
- A script for Nemesis's battle tactics is compiled but never started: see [ending.md](progress/story/ending.md)
- A commented-out line would have made the mirror from the player's creature directly instead of loading Nemesis's mind file; a commented-out step would have grown a tree after the temple's fall; a commented-out script would have pushed the creature's alignment to an extreme during its return: comments in the source only

#### story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature

From [So You Couldn't Bear to Be Without Your Creature?](progress/story/gold_scrolls/so_you_couldnt_bear_to_be_without_your_creature.md).

- A commented-out log entry with a proper title and reminder (title text 22, reminder 21) was replaced by this; neither text exists in the text table: cut before release
- A commented-out end to the creed scene would have blown up Lethys's creature with three explosions: cut before release
- A debug switch makes every pillar fall at once: developer switch, never set by the shipped scripts
- A land-specific copy of the arrival script, which would have thrown out alternating Celtic men and housewives, is compiled but never used: the shared arrival script is used instead

#### story/gold_scrolls/the_ally_speaks

The Ally Speaks: A cut story cut scene, once meant to be offered from a gold scroll at the player's temple, in which the camera flies a prepared path while the advisors talk about another god's temple across the land. It was never compiled into the game, its gold scroll was already commented out, and none of its fifteen lines were ever written. It has no in-game title; this file uses the script's own name.

- The script is not among the files compiled into the shipped `challenge.chl`: never started by the game: only its own (uncompiled) test launcher runs it
- Its challenge name keeps an entry in the compiler's list of challenges: never started by the game
- Its camera position and camera path are in the compiler's camera list, and the shipped camera data file still holds an entry under that name: never started by the game; the path data is left over in `Data/camera.edt`
- None of its fifteen lines, nor the advisor's scroll prompt it names, are in the shipped text table or the development text source: never started by the game; who would speak each line is unknown
- Its "my temple" marker is 19 from the player's planned temple on the second land; the other temple it looks at is not near any shipped temple (the nearest, Lethys's on the third land, is about 290 away): never started by the game; the second land's other two temples are about 475 and 1900 away; which god the "ally" is cannot be told from the script
- A gold scroll at the player's temple, with the evil advisor pointing it out when the camera comes near: never started by the game: both lines are commented out in the source, so as written it would play at once with no scroll and no challenge log entry
- The camera's position is saved, then the camera jumps to the start of a prepared camera path and flies along it; two seconds after it arrives the talk starts: never started by the game
- One line is spoken; the evil advisor steps out and a second line starts; a second later the good advisor steps out; a third line follows and a two-second pause: never started by the game
- Both advisors turn to look at the other god's temple, a second apart; two lines; the evil advisor looks away for one line and back; three more lines: never started by the game
- The good advisor looks away for one line (with a two-second pause) and back; one line; both look away for three lines (another two-second pause); both look at the other temple for the last line: never started by the game
- When the camera path ends the camera returns to where the player left it and the cut scene ends: never started by the game; nothing is logged or rewarded

#### story/gold_scrolls/the_creatures_learning

From [The Creature's Learning](progress/story/gold_scrolls/the_creatures_learning.md).

- The lessons are only in the first land of the story, not in skirmish, multiplayer or the Creature Isle: nothing to do beyond the story itself
- A compiled but never started good-and-evil leash lesson: back at the pen the trainer shows the fluffy Leash of Compassion and the spiky Leash of Aggression, a dying farmer appears, the player leads the creature to him on either leash; "That's it! I can break the curse now!", "I have nothing more to teach you.", "He's all yours. Now it's up to you." and she walks off to join the village (scroll at 0.92): nothing runs it
- A compiled script that runs the five lessons in a row, started only by a commented-out line of the land's control script and an unshipped test launcher: nothing runs it
- Older, shorter drafts of the five lessons as separate files, with placeholder lines typed into the script and a man as trainer: not in the game's program
- An unshipped lesson where the creature needs to relieve itself and is led to a spot ("Er, Mighty One? Your Creature needs to relieve himself.", advisors' bowel jokes), and a leash version of it: not in the game's program
- An unshipped thirst lesson: nothing to drink at home, so the creature is leashed and dragged to water ("I think your Creature is thirsty, powerful one."), and a leash version of it: not in the game's program
- An unshipped anger lesson: villagers crowd the creature, it gets angry, and stroking or slapping changes how easily it angers ("Remember, punishment and reward is the path to having the Creature you want."): not in the game's program
- Unshipped leash drafts: tying the leash to an object (with the three leashes explained), tying it to a walking villager so the creature follows and learns from them, pulling to a tree and picking up a cow: not in the game's program
- A misnamed "learn to play" file that holds a draft of the guide's help-a-town lesson: the guide's unshipped lessons are listed in [creature_guide.md](progress/story/creature_guide.md)

#### story/gold_scrolls/the_defending_ogres

From [The Defending Ogres](progress/story/gold_scrolls/the_defending_ogres.md).

- A notify script written for this scroll (an advisor nagging until the lair is clicked or the creature comes within 100) is never used by the game: written but never run
- A test launcher for the ogre alone (it sets up the land, logs the scroll as a silver challenge, puts up the lair's scroll and runs Sleg) exists in the source but is not in the shipped scripts: not compiled into `challenge.chl`
- Curing a gremlin back into a villager (turning it human and adding it to the home village) was planned but is commented out, so the "You've cured all the cursed Villagers." / "They've recovered fully." and "All those little ogres have been dealt with." / "You haven't cured all the cursed Villagers." lines can never play: the count of cured gremlins never rises
- An unused gold-scroll notify for the ogre (above) and the ogre-only test launcher: unused or not compiled

#### story/gold_scrolls/the_heartbroken_man

From [The Heartbroken Man](progress/story/gold_scrolls/the_heartbroken_man.md).

- An early draft of the lines survives in the source comments: Adam was "Knobbo", Keiko his "girlfriend", and the woman was simply his friend and neighbour: comments only
- A test launcher (not compiled into the game) runs this quest on its own by setting the start flag: never in the shipped `challenge.chl`

#### story/gold_scrolls/the_lost_brother

From [The Lost Brother](progress/story/gold_scrolls/the_lost_brother.md).

- A cut ending where both siblings arrive home together: "We offer thanks, O mighty spirit. Here. The Gate Stone." and the good advisor: "A Gate Stone! Let's give it to the woman at the Gate!": commented out of the script; the lines are in the text table
- A cut scene for both siblings killed: "The siblings are dead. You gotta trash the house and get the Gate Stone now, Boss.": commented out
- A cut 60-second countdown and slow health loss, ending with the brother dying of his fever: "The lost one must be at death's door. This is dreadful!" / "Maybe we've got better things to do. You thought of that?": commented out
- A cut scene for flying the brother to his sister: only a commented-out call; no such script
- An older copy of the quest run by an unshipped demo control script: not in the game's program

#### story/gold_scrolls/the_madness

The Madness: A cut gold-scroll story chapter in which Nemesis takes over the player's creature: it is made strong, turned to the opposite alignment and sent to throw fireballs at a village's store and houses while the advisors look on, then confined for a minute and a half until the madness wears off. It was never compiled into the game; the shipped fifth land does the same story beat as the curse ([i_have_a_surprise_for_you.md](progress/story/gold_scrolls/i_have_a_surprise_for_you.md)).

- The script is not in the list of files compiled into the shipped `challenge.chl`, so the game does not contain it: never started by the game: only its own test launcher (also uncompiled) runs it
- Its challenge name still has an entry in the compiler's list of challenges, so it was a planned story chapter: never started by the game
- Its title "The Madness" and the reminder "Your Creature is still out of control!" are in the shipped text table, and so are all eleven lines of its dialogue (Nemesis's four and the advisors' seven): never started by the game: the text survived the cut; no shipped script uses the title
- Its markers fit the fifth land: the village it attacks is the Japanese village there (its centre is about 3 units from the land's town 7, a neutral town at the start), and its camera points are around that village: never started by the game; no other land has a town within 230 units
- The shipped fifth land tells the same story differently: Nemesis curses the creature's strength, size and alignment at the start and the player lifts it in three steps: see [i_have_a_surprise_for_you.md](progress/story/gold_scrolls/i_have_a_surprise_for_you.md)
- There is no scroll to click: the chapter would start as soon as whatever started it ran the script: never started by the game
- The creature's leash is switched off and the creature is walked to a spot beside the village; the script waits until it is within 150 of it: never started by the game
- The scroll is logged in the story log as a gold quest titled "The Madness", at nothing done, with the reminder "Your Creature is still out of control!" (good advisor): never started by the game
- The creature is given a Strength miracle that never runs out, and is taught everything: never started by the game
- The time of day is set to 18:30 and game time is stopped: never started by the game
- The camera locks onto the creature and follows it, the lens widening to 35 over 8 seconds: never started by the game
- Nemesis: "You are close to your Creature." then "After all this time, you think you know him.": never started by the game
- The camera swings round to a point east of the village over 5 seconds, aimed at the creature's chest (80% of its height), then rises 20 over 9 seconds: never started by the game
- Nemesis: "But I am stronger than you will ever be."; when the creature reaches its spot (within 5): "And now he obeys only me.": never started by the game
- The creature looks into the camera; two seconds later the camera cuts in close to it by the store and rises slowly: never started by the game
- A target marker effect is put on the creature for 30 seconds; it is made to want only to be angry, plays its angry animation twice, and the camera shakes (amplitude 0.5 for 2 seconds within 200): never started by the game
- The creature's alignment is flipped to its mirror (a good creature becomes as evil as it was good, and the other way round), in steps of 0.025: never started by the game
- The creature is allowed to attack its own side's town, looks at the store and throws a fireball at it at full energy, then plays angry again: never started by the game; the store is looked up by its position, about 50 from the village centre
- Both advisors step out. Good: "He's out of control! Oh this is terrible!" Evil: "We've got to prevent him from doing too much damage.": never started by the game
- The creature is moved to the village centre; the camera pulls back above the village. Good: "We've got to get him back, too. He's ours!": never started by the game
- It throws a fireball at a house; the cut scene ends but the advisors keep talking. Evil: "Restrain him first!" It throws a fireball at a second house. Evil: "That's it. We must keep him harmless until this wears off." Both advisors go home: never started by the game
- The creature is let go, still wanting only anger, and kept within 90 of the first house for 90 seconds; the player can only limit the damage it does: never started by the game; there is no success or failure test: the chapter simply waits
- After 90 seconds the leash is switched back on, the Strength miracle is removed, the confinement and the right to attack its own town are taken away: never started by the game
- Good: "The Creature did some damage when he was out of our control." Evil: "I know. Let's get it cleaned up. As if we haven't got better things to do." Good: "Cleanliness is next to godliness." Evil: "Doh!": never started by the game
- The gold scroll is marked complete: never started by the game
- A target effect is put on the creature again and its alignment is turned back to what it was before, in 0.025 steps; game time runs again: never started by the game
- The stored "mirror" alignment is negated a second time to restore it, so the creature ends where it began; the madness leaves nothing behind except the damage: never started by the game
- Commented-out lines show it once looked for the nearest town and its store wherever the creature was, and once made the creature examine the store before attacking; a "TODO cut to creature being horrible to town" note shows it was unfinished: never started by the game
- If the targeted village is not the player's, "attack own town" means nothing to it: the fireballs are forced anyway: never started by the game; on the shipped fifth land this village starts neutral

#### story/gold_scrolls/the_sculptor

From [The Sculptor](progress/story/gold_scrolls/the_sculptor.md).

- A commented-out lightning-bolt reward from the sky for bringing the rock: commented out
- A commented-out older carving scene with a sledgehammer and a 90-second timer, and older code for walking the sculptor back to the rock: commented out
- Commented-out updates to the Choose Your Creature record at each step: commented out
- An earlier silver-scroll version of the quest, never compiled: see [../silver_scrolls/the_sculptor_cut_silver.md](#storysilver_scrollsthe_sculptor_cut_silver)

#### story/gold_scrolls/the_temple_is_finished

From [The Temple Is Finished (the first gold scroll)](progress/story/gold_scrolls/the_temple_is_finished.md).

- A second line, "It's beautiful and it will be extremely useful later.", is in the text table but commented out of the scene: cut line
- No creature is involved (it hasn't been chosen yet): no creature is involved
- Commented-out camera moves straight onto the gold scroll and back to the temple: cut from the scene
- An older draft of the temple's opening (rotating the camera, carrying wood and food to the builders, the kinds of scroll): not in the shipped program: [../silver_scrolls/see_the_citadel.md](#storysilver_scrollssee_the_citadel)

#### story/gold_scrolls/the_totem_puzzle

From [The Totem Puzzle](progress/story/gold_scrolls/the_totem_puzzle.md).

- A first version of this quest used the game's built-in totem puzzle (the third totem layout) at the stone and waited for it to be solved; it is left commented out: never in the shipped game
- A line from the elder, "The rewards are surely worth the effort. But you must first gain influence over them.", is in the text table but cut from the scene: cut from the scene
- A test launcher, "Simon Says" (not compiled into the game), runs this quest on its own: never in the shipped `challenge.chl`

#### story/gold_scrolls/the_workshop

From [The Workshop](progress/story/gold_scrolls/the_workshop.md).

- A second info sign on combining scaffolds into buildings (one to seven scaffolds) is commented out; its text is still in the text table: never in the shipped game
- Earlier places for the 0 and 0.1 log entries, after the films, are commented out: never in the shipped game
- A comment "need a highlight to say about the homeless" marks where the second scroll was added: a source comment only

#### story/gold_scrolls/undead_village

From [Undead Village](progress/story/gold_scrolls/undead_village.md).

- A line "And they know the location of the object you desire." is recorded but taken out of the scene and marked as no longer needed: never played
- The quest's "finished" entry is marked in the script as possibly belonging elsewhere, but it stays at the moment the curse lifts: a note only; no behaviour

#### story/gold_scrolls/villager_catch

Villager Catch: A cut gold-scroll mini-game in which a hostile god flings ten villagers out of a town, one at a time, and the player must catch each in the hand before it lands. It was never compiled into the game and its lines were never written into the text table; it has no in-game title, so this file uses the script's own name.

- The script is not among the files compiled into the shipped `challenge.chl`: never started by the game: only its own (uncompiled) test launcher runs it
- Its challenge name keeps an entry in the compiler's list of challenges: never started by the game
- None of its 35 lines are in the shipped text table; the source's comments give the intended words of the main ones, and four "Nemesis comment required" notes show his lines were never written: never started by the game
- It sets a "villager catch completed" flag "that the control script uses" (a comment credits another designer), but no land's control script, shipped or cut, reads it: never started by the game
- Its town marker is 67 from the player's home town on the second land (the Norse town); the source calls it Nemesis's town; no other land has a town within 230: never started by the game; which land it was meant for is not certain
- A gold scroll is put over a point just north of the town as soon as the script starts: never started by the game
- The evil advisor's scroll prompt ("Whew! As if you ain't busy enough, here's something for you to do.") is commented out, so nothing points the scroll out: never started by the game
- It starts when, checked once a second, the camera is within 100 of the town, the town is on screen and the hand is empty; the scroll does not need clicking: never started by the game
- A cut scene: a line meant as "You want the souls in this town? Catch!" (by its wording the hostile god); the good advisor steps out: "Quick! Save them!" and goes home: never started by the game; words from the source's comments
- Ten male villagers are thrown one at a time from a point in the town towards the scroll point, aimed 200 up, with a random spread of up to 10 either side and a speed between 100 and 150: never started by the game
- After the first, each throw waits a random 1 to 8 seconds: never started by the game
- For each throw a short cut scene puts the camera at a fixed spot and follows the flying villager for 3 seconds, while he shouts one of five random cries ("Help!"): never started by the game
- The script waits while he is flying; if the player catches him, it waits until he is put down, then a second, then follows him with the camera if he was thrown again until he lands: never started by the game
- A caught villager who is still alive says one of five random thank-you lines and counts as saved; one who died says one of five random curses ("Argh, damn you!"): never started by the game
- A villager who is not caught is not counted at all, and his fall is not watched: never started by the game
- None saved: the good advisor: "Tragic. You haven't caught a single person." then a line for Nemesis: never started by the game
- One to four saved: evil: "You'll have to be faster than that."; good: "Oh this is terrible."; then a line for Nemesis: never started by the game
- Five to nine saved: good: "Impressive stuff."; evil: "All those saved people. Lovely."; then a line for Nemesis: never started by the game
- All ten: the evil advisor: "OK. So you're a hotshot at catching. Whatever." then a line for Nemesis: never started by the game
- Nothing is logged, rewarded or marked complete beyond the completion flag; the gold scroll is never removed: never started by the game; no challenge log entry is ever made

#### story/gold_scrolls/worship_site

From [Worship Site](progress/story/gold_scrolls/worship_site.md).

- The creature takes no part in the quest; nothing in it moves or locks the creature: the creature takes no part
- An earlier place for the "Worship Site" log entry, in the middle of the builder film, is commented out: never in the shipped game
- A separate hand demo script for casting is commented out in favour of playing the cast demo directly: never in the shipped game

#### story/land_1

From [Land 1](progress/story/land_1.md).

- An unused script for taking over the land's villages with the guide's help: in the scripts but never started by the game; see [creature_guide.md](progress/story/creature_guide.md)

#### story/land_5

From [Land 5](progress/story/land_5.md).

- Cut challenge: "Chimp Posse" — three chimp creatures on a hilltop have lost their ball to Nemesis's creature: never compiled into the game, no scroll and no reward: [silver_scrolls/chimp_posse.md](#storysilver_scrollschimp_posse)

#### story/losing_and_game_over

From [Losing and game over](progress/story/losing_and_game_over.md).

- A test script that just runs the game-over script: in the script sources but not in the shipped challenge file
- The game-over script's own temple explosion, two more of Nemesis's laughs and a closing window are commented out: the temple is already being destroyed when the script starts
- "You have lost the game. Click YES to watch the other players, NO to quit the Multiplayer Game." (twice) and the same for "the Skirmish Game": nothing in the 1.2 program shows them; the end box uses "You have lost the game." with Watch Game and Leave Game buttons
- "You have lost the game. The winner was %s.", "Game winner: %s" and "All the players lost. It's a draw.": nothing in the 1.2 program shows them
- There is no game-over video: the game ships only the intro, logo, tips and falling spell videos

#### story/minigames

From [Puzzles and games](progress/story/minigames.md).

- Two maze puzzles: an engine puzzle kind no land uses
- Three of the four totem puzzle layouts: six totems each turning through five positions, where turning one moves the others by set amounts: no land uses them
- A second lion maze, a second fish puzzle and a second mushroom puzzle: engine puzzle kinds no land uses
- Lions and sheep: a take-away game against the computer, which plays the winning move when there is one: an engine puzzle kind no land uses
- Chess: the kind exists but has no rules in the game
- Cow bowling, bowling, whack-a-villager, catching villagers, a race, Simon says, the cup final, a spitting totem, the big whale, raiding lions: script sources not compiled into the game's challenges; the cup final: [../town/football.md](progress/town/football.md)
- Creature Isle's own games: see [creature_isle.md](#storycreature_isle)

#### story/portals_per_land

From [Portals, land by land](progress/story/portals_per_land.md).

- The tutorial, the Creature Isle, the playgrounds and skirmish games make no vortex: nothing to port; see [tutorial.md](progress/story/tutorial.md) and [creature_isle.md](#storycreature_isle)
- A helper that sets a newcomer's town, flings it out and keeps it at full health while it flies is never used: the vortex does this itself
- An older Land 3 arrival that flung out 30 hand-made Celtic villagers (alternating men of random jobs and housewives, at speed 15 to 30) is never run: never run
- Test scripts that start each exit or arrival on their own (including one that runs Land 1's exit and loads Land 2) are not in the shipped set: not in the shipped set
- A test script with a black storm cloud mentions vortices but is not shipped: not shipped
- The shared arrival script's commented-out lines would have walked the creature to its spot and stopped it reacting to other creatures: commented out
- Land 1's recorded creature kind "for setting up the next land" is never read: never read

#### story/silver_scrolls/big_whale

The Big Whale: A cut first-land challenge, a "Jonah" story: a fisherman's son has been swallowed by a giant whale out at sea; once the player's creature goes to the water the whale is thrown up onto the shore, and hitting it makes it spit the son out, for a storm miracle seed from the sky. It was never compiled into the game, and none of its lines exist in the text table. Its script once borrowed the title "The Beach Temple Puzzle", which the shipped game uses for Land 2's beach temple ([the_beach_temple_puzzle.md](progress/story/silver_scrolls/the_beach_temple_puzzle.md)), but that line is commented out.

- Not in the compiled challenge file; only an uncompiled early Land 1 control script (one that also starts the cut landslide and the cut sculptor's rock) and a single-quest test launcher start it: never started by the game
- Its challenge is named for a whale, and its dialogue lines (the "big whale" lines and its reminder) are all missing from the text table; its advisor nag uses a line from Land 2's lifeguard quest that is missing too: never started by the game
- It calls the scroll's nag helper with one argument too many for the shipped helper, so it would no longer compile against the shipped scripts: never started by the game
- Its fisherman's house matches no house on the shipped Land 1 (the nearest house at that spot is on Land 3, about 38 away), so the land's layout changed after it was written: never started by the game: checked against every `Land<N>.txt`
- The headers still name its camera path ("big whale"), but the shipped data has no whale model or path for it: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A whale is made at the start of its camera path, at double size, and a Norse fisherman at his house: never started by the game
- A silver scroll appears over the house; the evil advisor nags within 100 until it is clicked: never started by the game
- The challenge record lines (open and close) are commented out, so it would never show in the challenge log: never started by the game
- Cut scene: the fisherman steps out of the house; the camera glides to him (6 seconds) and he speaks; he turns to the camera and speaks again while the camera swings out to sea (5 seconds); a "fish in the water" shot was still to do: never started by the game: two lines, text missing
- Then both advisors come out for four lines: never started by the game: text missing
- The quest waits until the player's creature is within 80 of a spot out at sea south of the house, then makes the creature idle: never started by the game
- Cut scene: the camera cuts back to the shore; two lines; the whale swims its path; a giant (three times size) man is made on the beach to stand for the whale thrown up on the shore, with a 1 second camera shake within 100; another line: never started by the game: the real "whale on the shore, crowd gathers, voice from inside" was still to do
- The swimming whale is removed; three more lines: never started by the game
- The quest waits until the beached "whale" is hit by something (the plan: throw a rock at it so it spits out the son): never started by the game
- Cut scene with three closing lines; the creature is released: never started by the game
- No timer, no failure and no way to give up: never started by the game
- A storm miracle seed falls from the sky beside the house, with the reward's own camera move (one of two random swoops, then following the seed down): never started by the game: the shared reward-from-sky script
- A planned ending where the whale swims away into the distance was never written: never started by the game

#### story/silver_scrolls/chimp_posse

Chimp Posse: A cut challenge for the last land: three small, angry chimp creatures on a hilltop have lost their ball in "the neighbour's garden", and Nemesis's creature takes it. Bringing the ball back (with their die and teddy still at home) makes the chimps friends of the player's creature, until Nemesis's creature comes to take the ball again. It makes a challenge record but no silver scroll, has no reward, and was never compiled into the game.

- Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it: never started by the game
- Only its single-quest test launcher starts it, after Land 5's setup script; the developers' quest menu compiles that launcher with Land 5's setup, volcano and returning-explorers scripts: never started by the game: the launcher is not compiled into the shipped game either
- It opens a challenge record but never makes a silver scroll, so even if run there would be no scroll to click; the quest starts on its own when the camera comes near: never started by the game
- Its title "Chimp Posse" and both its reminders exist in the text table, but all fourteen of its spoken lines are missing; their wording survives only in the source's comments (for ten of them): never started by the game
- Its spots fit Land 5: the spot "just outside Nemesis's citadel" is about 50 from Nemesis's citadel on Land 5, and the "big fight arena" is about 95 from Nemesis's Aztec village there: never started by the game: checked against every `Land<N>.txt`
- Not reachable at all in the shipped game: never started by the game
- Three chimp creatures are made at three spots on the hill's edge, about 45 from their home on the hilltop, at half size: never started by the game
- They are fully grown, taught the fireball and lightning miracles, and friends with each other: never started by the game
- Their only desire is anger, with the desires to play and to make friends set to full: never started by the game
- They are kept within 75 of their home: never started by the game
- A die and a cuddly toy, at half size, are put 7 either side of their home: never started by the game
- Until the ball is moved, the chimps stand facing the spot where it lies: one points at it and looks confused in turn, one is sad then embarrassed, one angry then confused: never started by the game
- Once the ball has moved more than 10, they are let go; from then until the quest ends their energy is kept full every second, a comment says so they can cast miracles: never started by the game
- Starts when the camera is within 100 of the chimps' home and looking at it: never started by the game
- Cut scene: the camera glides to 20 above and 20 off the middle chimp over 3 seconds: never started by the game
- A half-size ball appears about 215 away, down near sea level: never started by the game
- The good advisor points at the chimp; the record opens as "Chimp Posse", success and alignment 0, reminder "It looks like these chimps have lost a toy.": never started by the game
- Good advisor (comment's wording): "Oooh look at that. A group of chimps. Aaah sweet.": never started by the game: not in the text table
- After 2 seconds the camera moves to the ball over 4 seconds; the evil advisor points at it: "Ahh diddums. They seem to have lost their ball in the neighbours garden.": never started by the game: not in the text table
- Nemesis's creature is put about 80 from the ball and made to pick it up; the camera follows him, rising to 60 above over 12 seconds: never started by the game
- With the ball in hand he walks off towards the big fight arena, about 320 east: never started by the game
- "Uh Oh. Getting the ball back for the chimps has just got harder." After 4 seconds and the line, the camera returns over 4 seconds: never started by the game: not in the text table
- When Nemesis's creature is within 20 of the arena, he puts the ball down and is let go (once): never started by the game
- Any toy that no longer exists is made again at the chimps' home: never started by the game
- The first time the player's creature comes within 50 of the home while the chimps aren't friendly and it isn't carrying the ball, the evil advisor points at it: "Woah. The Chimps aren't very friendly.": never started by the game: not in the text table
- A toy counts as home when it is within 50 of the home, not in the hand and not in Nemesis's creature's hand: never started by the game
- The ball home for the first time moves the record to 0.5, reminder "These Chimps seem to be friendly towards our Creature.": never started by the game
- A toy coming home with the other two there brings two good advisor lines whose wording is lost; with one still missing: "The chimps will only be happy with you if ALL of the toys are returned.": never started by the game: not in the text table
- The die or the cuddly toy coming home when the others are there moves the record to 1.0: never started by the game: the ball never does, even if it is the last one home
- A toy that ends up more than 100 from home counts as missing again; if the player's creature or the hand has it: "The chimps are not happy with you." "They want the toy back.": never started by the game: not in the text table
- If Nemesis's creature has it: the good advisor points at him: "Oh dear. Nemesis' Creature has taken back his toy from the chimps." "They're not happy about it either": never started by the game: not in the text table
- With all three toys home, the three chimps and the player's creature become friends both ways: never started by the game
- As soon as any toy is missing again they stop being friends: never started by the game
- There is no reward, no alignment change, no timer and no failure: never started by the game
- Two minutes after the chimps first become friends, Nemesis's creature comes back for the ball: every 10 seconds he is made to grab it until it is in his hand: never started by the game
- Each of those 10-second tries while he is within 200 of the chimps' home, the good advisor points at him: "I don't think the toy belongs to the chimps." "Nemesis' Creature looks mighty pissed.": never started by the game: not in the text table
- With the ball he walks to a spot just outside his citadel, being sent again every 20 seconds until within 5, and puts it down there: never started by the game
- The chimps then lose their friendship (the ball is missing) until the player brings it back again: never started by the game
- The quest is meant to end once the revenge is over and the chimps are friends again: the chimps are marked active and the record closes at 1.0 with the opening reminder: never started by the game
- What "active" chimps were to do is never written; nothing else reads it: never started by the game
- The revenge never ends: it loops until a flag that nothing sets, so it never reports back, the chimps never become active and the challenge can never close: never started by the game
- Once Nemesis's creature has put the ball down at his citadel, that loop goes round with no pause: never started by the game
- The main loop also has no pause: never started by the game

#### story/silver_scrolls/creature_ladder

Creature Ladder: A cut fighting tournament: fifteen stone creature statues stand in a half circle around an arena, and each silver scroll there brings one statue to life as a copy of the player's own creature in another species, a little stronger each time, to be beaten in a fight. It was never compiled into the game, has no title or dialogue in the text table, and its reward (a "black belt" tattoo for the creature) was never written.

- Not in the compiled challenge file, and no land's control script, challenge list or the extra-menu list starts it: never started by the game: only listed in the developers' quest menu and project files, where it would run itself when compiled alone
- It runs itself at the top of its own file, the way single-quest test files do: never started by the game
- No title, reminder or dialogue: the only line it uses is the shared "Your attention is required here.": never started by the game
- Its arena sits on open ground at sea level; the "citadel heart" it faces is not near any shipped land's citadel (the closest is Land 4's, about 150 away), so it can't be placed on a shipped land with confidence: never started by the game: checked against every `Land<N>.txt`
- It uses real script commands of the shipped language (making a creature from a creature, stone creature icon features), so it would compile: never started by the game
- Not reachable at all in the shipped game: never started by the game
- Fifteen stone creature icons are placed on a half circle of radius 80 round the arena, 12 degrees apart, each facing the next; they can't be destroyed: never started by the game
- From one end to the middle: sheep, cow, zebra, ape, wolf, leopard, polar bear, lion; from the other end back to the middle: ape, ape, horse, tortoise, ape, bear, tiger; sizes grow from 1 at the ends to 2.4 for the lion in the middle: never started by the game: the chimp, mandrill and gorilla statues use the ape's icon, there being no icon for them
- The comments list the rungs in a different order (sheep, chimp, cow, ape...) from the one the statues and levels actually use: never started by the game
- The rung waits until the player's creature is within 100 of the arena, then a silver scroll appears over it and the good advisor nags ("Your attention is required here.") until it is clicked: never started by the game
- If the creature is within 90 when clicked, it is turned and walked to its starting spot on the west side of the arena: never started by the game
- Cut scene: the camera rises 50 above the player's side and looks at the rung's statue: never started by the game
- The opponent is made at the statue as a copy of the player's creature (its mind) but in the rung's species, fading in over 6 seconds, auto-scaled to 0.6, fully grown and taught everything: never started by the game
- The rungs go sheep, chimp, cow, ape, zebra, horse, mandrill, tortoise, wolf, gorilla, leopard, bear, polar bear, tiger, lion, alternating ends of the half circle towards the middle: never started by the game
- Strength rises by 0.066 a rung, from 0.066 for the sheep to 1 for the lion; odd rungs are fully good and even rungs fully evil: never started by the game
- The statue fades away and the opponent walks to its spot on the east side, the camera following; the two face each other if the player's creature is in place, otherwise the opponent faces the citadel: never started by the game
- The camera settles on a fight view over the arena: never started by the game
- While the player's creature is within 50 of the arena, the two are made to fight each other; the leash comes off: never started by the game
- When neither is fighting any more, the one with more fight health wins: never started by the game
- Losing: the player's creature is knocked out, the camera pulls back, the opponent returns to its spot fully healed and the player's creature is moved to a spot outside the arena; the rung is fought again once the creature comes back within 50: never started by the game
- Winning: the opponent walks back to its statue's place, idles and prays, is frozen for 3 seconds, then an explosion miracle falls on it from above (radius 50) and it is removed; the camera returns to the fight view and the creature is released: never started by the game
- There is no timer and no way to give up; the next rung starts as soon as one is won: never started by the game
- After the lion, a reward step meant to give "a unique tattoo" (a black belt) for the creature runs, but it is empty: never started by the game
- No challenge record is ever opened or closed: never started by the game

#### story/silver_scrolls/creature_swaps

From [Creature swaps](progress/story/silver_scrolls/creature_swaps.md).

- Never offered in the shipped game: a cow ("Swap To Cow" is compiled but never started) and a chimp ("The Miracle Stones" is not compiled): never started by the game: [swap_to_cow.md](#storysilver_scrollsswap_to_cow), [the_miracle_stones.md](#storysilver_scrollsthe_miracle_stones)
- Is it in the game: none of the five is compiled, started, or reachable; the text table has their titles only: never started by the game: checked against every land's control and challenge list
- Swap To Horse: out of view, a horse creature and an Indian trader appear at a fixed spot; a silver scroll and the evil advisor's "Hey. There's a job for you. Wanna do it?"; on click, a cut scene where the trader walks up and speaks four lines, the horse steps forward, both advisors come out for five more lines; then the field offer: never started by the game: all nine lines are missing from the text table; its challenge record lines were commented out because they "don't compile"; the spot is not near any shipped land's town
- Swap To Leopard: the challenge record opens, a leopard creature appears out of view under a silver scroll ("There's a Silver Reward Scroll down here, Boss."), then a cut scene of twelve advisor lines and the field offer; the record closes at once as a success: never started by the game: all twelve lines missing; the leopard became a breeder creature
- Swap To Lion: a tamer and a wild lion (an animal, not a creature) by a flock; the good advisor's "Your attention is required here."; the lion hunts nearby cattle; if the player carries it away from the tamer and puts it down alive, it turns into a lion creature after a cut scene and is offered (record closes good); if it dies, the advisors comment and the record closes evil: never started by the game: all sixteen lines missing; a debug line "Picked up Lion" would have shown while it was held; meant to be placed outside the player's influence
- Swap To Turtle: out of view, a Japanese farmer and a flock of five tortoises at 70% health; heal a tortoise to full health and the farmer gives a tortoise creature after a cut scene: never started by the game: all lines missing; its spot is about 80 from Land 4's Japanese town, the land "The Fish Puzzle" later gave a tortoise on
- Swap To Wolf: a shepherd and a flock of twenty sheep, one of which is secretly a wolf; picking that sheep up turns it into a wolf creature in the hand, with an extra line if it was the first thing picked up; then the field offer: never started by the game: all lines missing (an earlier copy of the same script exists as a test file that starts it at the hand's position)
- When "Swap To Cow" (unused) gave nothing because the creature was already a cow, it still ran the offer with no creature: never started by the game: see [swap_to_cow.md](#storysilver_scrollsswap_to_cow)

#### story/silver_scrolls/food_for_thought

The Nomads (Food for Thought): A cut Land 2 challenge: when an Indian village's storage pit runs low on food, ten wandering homeless men arrive, and the player decides whether and how to feed them: food from the village's own store, food conjured by a miracle, poisoned food, or nothing. It was never compiled into the game; its title "The Nomads" exists in the text table but none of its lines do.

- Not in the compiled challenge file; nothing starts it but a single-quest test launcher: never started by the game
- Its villager's hut and storage pit match real buildings in Land 2's neutral Indian village (an Indian house and its storage pit): never started by the game: checked against `Land2.txt`
- Its title "The Nomads" is in the text table and used by no other script; its reminder and all 21 of its lines are missing: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A leftover header note describes a different quest (raiding marauders: three raids, retreat after two deaths), copied from the cut "The Attackers": never started by the game: see [the_attackers.md](#storysilver_scrollsthe_attackers)
- Every 5 seconds the village storage pit's food is checked; the quest begins once it is 500 or less: never started by the game
- A silver scroll appears over a villager's hut; the good advisor nags "We've got something to do. Let's see what it is." until it is clicked: never started by the game
- A flock of ten homeless men (Norse farmers) gathers at the edge of the village, and an Indian farmer comes out of the hut: never started by the game
- Cut scene: the camera glides to the homeless (4 seconds); the challenge record opens; two lines: never started by the game: lines missing
- When the villager reaches his spot, the camera moves to him: one line; then to the storage pit: one line: never started by the game
- The evil advisor and then the good advisor come out for a line each; the camera returns: never started by the game
- A crying child was planned at the end of the scene: never started by the game: only a note
- The quest watches what the hand holds: miracle food, ordinary food taken within 60 of the storage pit (the village's own), ordinary food from anywhere else, or poisoned food: never started by the game
- Putting food down within 30 of the homeless gets one line of thanks, once: never started by the game
- Food dropped within 20 of them counts as given; food dropped elsewhere gets one disappointed line, once: never started by the game
- The quest ends when a food store appears within 20 of the homeless, when fewer than 3 of them remain, or after 150 checks of 2 seconds (about 5 minutes): never started by the game
- Fed from the village's own store: the villager speaks two lines and the good advisor one; alignment +0.5: never started by the game
- Fed with miracle food (or food brought from outside the village): the villager and the homeless speak, both advisors comment; a large food reward falls from the sky by the homeless with its help text; alignment +1: never started by the game
- Fed with poisoned food: "You gave the wandering tribe poisoned food." the food is removed and every homeless man drops dead; the evil advisor: "Heheh. Classic move, Boss. That'll keep them quiet.": never started by the game: lines quoted from the script's notes, missing from the text table; this ending sets no alignment, so the record closes at 0
- Fewer than 3 left (killed or scattered): the villager speaks and the evil advisor comments; alignment -1: never started by the game
- Time ran out: the homeless walk off at no health and are let go; for about the next 150 seconds, if the camera comes within 30 of the spot, the evil advisor comments once; alignment 0: never started by the game
- Every ending closes the challenge record as complete with that alignment: never started by the game

#### story/silver_scrolls/landslide

Landslide: A cut Land 1 challenge: a rock slide buries three Aztec farmers on a hillside; an Aztec fisherman asks for help, and the player has five minutes to lift the boulders off them, which wins the small Aztec village's belief. It was never compiled into the game; neither its title nor any of its lines exist in the text table.

- Not in the compiled challenge file; only the uncompiled early Land 1 control script and a single-quest test launcher start it: never started by the game
- Its title, reminder and all nine lines are missing from the text table: never started by the game
- It is set on Land 1 (it resets the camera zones to Land 1's at the end), but the Aztec village it looks for within 50 of a spot is 79 away on the shipped map, so it would find no village: never started by the game: checked against `Land1.txt`
- It calls the scroll's nag helper with one argument too many for the shipped helper: never started by the game
- Not reachable at all in the shipped game: never started by the game
- An Aztec fisherman is made on the hillside, with a silver scroll over him; the good advisor nags with an evil advisor's line ("Whew! As if you ain't busy enough, here's something for you to do.") until clicked: never started by the game: the line belongs to the evil advisor's voice
- On click the challenge record opens and three half-size rocks are placed where the farmers lie buried: never started by the game
- Cut scene: the camera flies a path while the ground shakes within 100 of the fisherman for 4 seconds, and nine rocks of sizes 0.1 to 0.8 appear high on the slope over about 5 seconds and tumble down: never started by the game
- One line, then the camera returns; the evil advisor speaks; the good advisor points in turn at the three buried men (who don't exist yet, so the pointing goes nowhere) and speaks two lines: never started by the game: lines missing; pointing at the men before they are made is a script bug
- A 5 minute timer starts: never started by the game
- Picking up or moving one of the three rocks uncovers an Aztec farmer at half health where it lay, who joins the village: never started by the game
- When all three are uncovered, the fisherman joins the village too, both advisors speak three lines, and the village's belief in the player is set to three times relative belief: never started by the game
- When the timer reaches zero first, the ground shakes again, six rocks come down, both advisors speak two lines and the quest ends: never started by the game
- Neither ending closes the challenge record, so it would stay open in the log: never started by the game: script bug
- At the end the camera zones are set to Land 1's: never started by the game

#### story/silver_scrolls/see_the_citadel

See The Citadel: A cut early draft of the first land's opening at the temple (called the citadel here): the advisors teach the player to rotate the camera, then to carry wood and food to the villagers building the temple, and once it is built they show the three kinds of scroll and explain the temple, its info orb and its entrance. It is a tutorial, not a silver scroll: it never opens a challenge record and only shows a silver scroll for two seconds as an example. It was never compiled into the game; the shipped opening does this part of Land 1 its own way.

- Not in the compiled challenge file: it is missing from the compiler's project list, and no script starts it: never started by the game
- The developers' quest menu lists it to be compiled alone, but even then its own line to start itself is commented out: never started by the game
- Its title text (9) and every text-table line it uses (the rotate lessons and two "follow us" extras) are missing from the shipped text table; the rest of its lines are typed straight into the script as draft English: never started by the game
- The only record it would make, a gold-scroll (quest) record, is commented out, so it would not appear as a challenge at all: never started by the game
- Its temple is exactly Land 1's planned temple site, and it uses Land 1's real camera zone files (the building zone, then zone 2): never started by the game: checked against `Land1.txt` and `Data/Zones`
- The village it looks up (about 90 from Land 1's player village) matches no town, but it never uses it: never started by the game
- The shipped Land 1 opening also locks the camera to the building zone while the temple is finished and widens it to zone 2 afterwards, but with different lines and no rotate lesson or scroll demonstration: never started by the game: see [../tutorial.md](progress/story/tutorial.md)
- Not reachable at all in the shipped game: never started by the game
- The temple is set to 30% built: never started by the game
- Until the camera is within 100 of the temple, the good advisor points at it and says a "follow us" line every 30 seconds: never started by the game: the line is missing from the text table
- Then the camera is kept to the building zone: never started by the game
- A wood store is made about 50 east of the temple and a food store about 55 south of it, each given 6,000: never started by the game
- Cut scene: the camera rises to a view of the temple over 3 seconds; the good advisor clings to the bottom of the screen and says "*** HAND DEMO INITIATED ***", then the first rotate line: never started by the game: a comment asks for the rotate hand animation here; the rotate lines are missing from the text table
- The evil advisor clings beside him for the second rotate line: never started by the game
- The player may then only rotate the camera: never started by the game
- While the player has rotated fewer than 15 times, both rotate lines are repeated every 30 seconds: never started by the game
- After the first rotation, a third rotate line is said once: never started by the game
- After more than 30 rotations, the evil advisor says a fifth rotate line (a fourth line and a wait for 60 rotations are commented out, as are two more lines): never started by the game
- The lesson only ends after more than 90 rotations: never started by the game
- The player may now rotate and use the hand: never started by the game
- Cut scene: the camera moves to the wood store (3 seconds); evil advisor: "They'll build the temple a lot quicker if you supply them with the wood they need.", pointing at the store: never started by the game
- Evil advisor, clinging: "*** HAND DEMO INITIATED ***" then "Pick up some wood from the supplies here and drop it near the builders.": never started by the game: a comment asks for a carry-wood hand animation
- The camera moves to the food store; good advisor: "Dont you forget to feed them though." then, pointing, "They need food if they're going to be happy little workers.": never started by the game
- Good advisor, clinging: "*** HAND DEMO INITIATED ***", "Pick up some food from the supplies here and drop it near the builders." and "It's up to you what help you give the villagers." as the camera returns to the temple view: never started by the game
- Until the temple is fully built: each handful of food let go within 20 of the building site brings the good advisor's "Gotta keep up your strength chaps.": never started by the game
- Each handful of wood let go there brings the evil advisor's "Work, bitches, work!": never started by the game
- The script counts the handfuls but never uses the counts; comments plan reminders that were never written: never started by the game
- The script does nothing to build the temple itself; it waits for the villagers to finish it: never started by the game
- The camera is widened to zone 2 and the player gets the full controls back: never started by the game
- Cut scene: camera low by the temple (3 seconds); the good advisor says another "follow us" line: never started by the game: missing from the text table
- A villager appears in front of the temple with an information scroll beside him; the camera moves to him over 4 to 5 seconds: never started by the game
- Good advisor, pointing at it: "This is an information scroll. There's lots more to be learnt from these scrolls": never started by the game
- After 2 seconds it is replaced by a silver scroll: "This kind of scroll will give you rewards.": never started by the game: the only silver scroll in this script, shown for about 2 seconds
- Then by a gold scroll: "And these scrolls are special scrolls. Keep an eye out for them.": never started by the game
- An information scroll appears 10 above the villager: "Click on the one above this bloke's head to learn about the citadel.": never started by the game
- It waits until that scroll or the villager is clicked: never started by the game
- Camera high over the temple (4 seconds): "The citadel is the centre of your power. The citadel blah blah.": never started by the game: placeholder text
- Camera to the info orb at the building site; good advisor, pointing: "This is the info orb. Pick it up and click on things for info.": never started by the game
- Camera to the entrance: "This is the entrance. Click on it to go inside or press F at any time." then "You can go in now or take a look around the village first.": never started by the game
- The script ends there; the villager and the information scroll above him are left in the world: never started by the game
- A handful counts only if it is within 20 of the site at the moment it leaves the hand, so food or wood thrown there from afar never counts: never started by the game

#### story/silver_scrolls/stanley_the_wolf

From [Stanley The Wolf](progress/story/silver_scrolls/stanley_the_wolf.md).

- A commented-out second "complete" snapshot is left after the reward: not in the compiled script

#### story/silver_scrolls/swap_to_brown_bear

From [Swap To Brown Bear](progress/story/silver_scrolls/swap_to_brown_bear.md).

- The source's to-do notes say the trail was meant to lead "through a forest to reach the brown bear" and the camera was meant to "zoom to bear"; the shipped version does both only roughly as described above: development notes, nothing to do
- The text table has no lines numbered 5, 8 to 11 or 16 for this quest, so lines were cut; the source's comments quote older wordings of three lines ("I don't think the owner of that house is gonna be too pleased.", "Urgh. The smell still persists...", "The smell has improved drastically..."): the shipped wordings are the ones above

#### story/silver_scrolls/swap_to_cow

Swap To Cow: A herding challenge written for Land 4: a lame herder asks for the player's creature to drive ten frightened cattle into a pen by the player's home village, and a trader there pays with a strength miracle seed, or with a cow creature to swap for when the player's creature is not already a cow. It is compiled into the shipped game but nothing ever starts it.

- The script is compiled into the shipped challenge file (it is in Land 4's file list): never started by the game: compiled, but no script in the compiled file runs it; Land 4's control script starts the ogre, the fish puzzle, the breeder and others, never this
- Only a single-quest test launcher, which is not compiled, ever runs it: never started by the game
- A note at its top says it was to be triggered after the ogre fight on Land 4: never started by the game
- The herder's house is a real Japanese house in Land 4's neutral Japanese village; the pen is about 190 from the player's Norse home village; the trader's hut position matches no building on Land 4: never started by the game: checked against `Land4.txt`
- The silver scroll over the herder's house and its advisor nag ("Whew! As if you ain't busy enough, here's something for you to do.") are commented out, so even if it were run it would start straight with the cut scene, with no scroll: never started by the game: the compiled version has no highlight at all
- Its title "Swap To Cow" and reminder "Let's get these cows back to the Village." exist in the text table, as do all its dialogue lines: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A cut scene: a Greek farmer comes out of the house and walks a few steps; the camera glides to him over 3 seconds: never started by the game
- Out of view, eight short Celtic fence pieces are set in a ring of radius 10 around the pen spot, each turned to face it: never started by the game
- A still flock is made at the pen (inner 5, outer 10) and a moving flock at the cattle's start (inner 15, outer 20); ten cows are made at the start, scattered up to 10 either way, and join the moving flock: never started by the game
- The herder turns to the camera, prays, and says: "Mighty one. I need to herd my cattle to your home Village." then "But I have injured my ankle and cannot walk that far.": never started by the game
- The camera cuts to the cattle; the challenge record opens; the herder: "But as my beasts are afraid of your Creature I had an idea.": never started by the game
- The camera cuts to the pen: "You could get him to shepherd the cattle to the pen near your Village." and "Please try and chase as many into the pen as possible.": never started by the game
- The herder walks back home and is removed as the camera glides back: never started by the game
- Cows can't be picked up by the hand: never started by the game
- Every second, a cow within 50 of the player's creature, not held, runs from it: it picks a spot 4 to 20 away on the far side (within 60 degrees of straight away), retrying until the ground there is within 2 of the height where it stands, and walks there at speed 0.3 for 3 seconds: never started by the game
- A cow more than 60 from the creature goes back to moving with the flock: never started by the game
- The moving flock's centre follows the cow that has got closest to the pen so far, so the herd drifts along behind the leader: never started by the game: the best distance is shared by all the cows and never reset
- A cow that reaches within 20 of the pen, alive, not held and not flying, counts as arrived, joins the pen's flock and sparkles for 3 seconds: never started by the game
- A cow that dies or disappears counts as lost: never started by the game
- Every 3 seconds the record updates when the counts change: progress is a tenth per cow arrived or lost, alignment a tenth up per arrived and a tenth down per lost: never started by the game
- There is no timer and no failure: the challenge only moves on when all ten cows are either penned or dead: never started by the game: a cow that never arrives and never dies keeps it open forever
- It then waits until the camera is within 150 of the pen and looking at it: never started by the game
- A cut scene: a man walks to the pen and faces the camera; the record closes as a success with the final alignment: never started by the game
- If the player's creature is already a cow, the trader thanks by result and gives a creature strength miracle seed beside himself with a sparkle: all ten ("Thank you, lordly one. You got the herd here safely."), more penned than lost ("You got most of the cattle here safely."), at least one penned ("Well, some cows made it, Mighty One."), each followed by "Here's an offering to show my gratitude.": never started by the game
- If all ten died, a cow owner gets nothing: "None of the cows made it. I shall look elsewhere for my herding requirements in future.": never started by the game
- Otherwise a cow creature appears near the trader's hut, looks at the camera and waves; the lines are "A billion thanks. All the cattle got to the pen safely." / "A hundred thanks. You managed to get most of the cattle here safely." / "Thanks, Holy One. A few cows did make it, in the end." each followed by "Take this Miraculous cow as an offering from me to you.": never started by the game
- Even with every cow dead, a non-cow owner still gets the cow: "Lordly spirit, none of my herd survived. I sense a famine coming." "Here. Take this special beast." "We might have to eat it otherwise.": never started by the game
- The camera returns, the trader is left in the world, and the cow is offered through the shared swap offer: never started by the game: see [creature_swaps.md](progress/story/silver_scrolls/creature_swaps.md)
- When the creature is already a cow, the swap offer is still run with no creature: never started by the game
- Only a cow owner can get the miracle seed; everyone else always gets the cow, whatever the result: never started by the game

#### story/silver_scrolls/the_attackers

The Attackers: A cut raid challenge: a band of ten marauders camps by a bonfire outside a Norse village and raids it three times, first driving off a herd of cattle, then robbing the village store, then setting fire to the crèche, eating their plunder at camp between raids. The player can kill them or keep them fed; a villager who begs for help promises a fireball miracle seed if the marauders are wiped out. It was never compiled into the game.

- Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it: never started by the game
- Only its single-quest test launcher starts it; the developers' quest menu compiles that launcher together with Land 2's setup script: never started by the game: the launcher is not compiled into the shipped game either
- The launcher itself builds a storage pit, a crèche and a house at the quest's spots, marks them fully built and fills the storage pit with 10,000 food and 10,000 wood, so no shipped land has these buildings ready: never started by the game
- Its spots sit 60 to 100 from Land 2's town 0, a Norse town site that starts with no buildings, and the marauder camp is near that town's fish farms; no shipped land has a building within 15 of any of its spots: never started by the game: checked against every `Land<N>.txt`; Land 2 is the best fit (Norse, and the test menu pairs it with Land 2's setup) but not certain
- Its title "The Attackers" and its reminders exist in the text table, and so do all its spoken lines except the three in the cut opening scene: never started by the game
- There is no silver scroll: the line that makes the scroll over the farmer's house is commented out, so where the script waits for "the scroll" it really waits for the house itself to be clicked: never started by the game
- Not reachable at all in the shipped game: never started by the game
- An area of anti-influence, radius 30, is placed on the marauders' camp, so the player's hand has no power there: never started by the game
- A bonfire is made at the camp: never started by the game
- The script waits until the camp is out of view, then makes ten marauders there, each one fleeing from objects thrown at it, gathered in a flock (inner and outer radius 10): never started by the game
- A food store is made at the camp, about 20 from the bonfire: never started by the game
- A herd of five cattle is made in a flock (inner 5, outer 10) about 180 from the camp and 170 west of the village buildings: never started by the game
- The marauders set off for the cattle at speed 0.5: never started by the game
- The challenge record opens as "The Attackers", success and alignment 0, reminder "Cattle are missing and those Marauders are responsible.": never started by the game
- A cut opening scene is commented out: the Norse farmer would have come out of his house, pointed towards the camp and said two lines (neither is in the text table), with the camera following the marauders: never started by the game
- All through the quest, every 2 seconds while any marauder lives: any food store within 50 of the camp that the script didn't make is emptied into the camp's store: never started by the game
- Any cow within 40 of the camp that the script didn't make is killed and adds 500 food to the camp's store: never started by the game
- Either way, the good advisor says "This food should keep the Marauders quiet for a while.", at most once every 30 seconds: never started by the game
- If the camp's store has gone, a new one is made at the camp: never started by the game
- When the marauders are within 30 of the herd, the herd moves onto them: never started by the game
- Once the camera is within 100 of the marauders and looking at them, the good advisor points at them: "These folk are stealing our cattle! Something must be done!": never started by the game
- The marauders head home at speed 0.5, the herd following at 0.4; the record's reminder is set again to the cattle line: never started by the game
- Every 4 seconds while the herd keeps up (within 30) the marauders are sent on again; if the herd falls behind they stop and wait until it is within 10: never started by the game
- Every 5 seconds the script looks for a cow more than 75 from the herd; one found before the marauders get home is taken out of the herd and walks back to the herd's starting spot, joining a rescued herd there (inner 5, outer 30): never started by the game: this is how the player saves cattle, by carrying or scaring a cow away from the herd
- When the marauders are within 30 of the camp, the herd is led in; one cow at a time walks to the camp's store at 0.4 and, once within 20, or dead, or picked up, or thrown, is killed and fades away, adding 100 food to the camp's store: never started by the game: a comment says 300 food per cow; a cow the player is holding is killed in the hand
- The raid is called off at once if two marauders are killed (fewer than nine left) or the herd is empty: the marauders go home and the herd goes back to its starting spot: never started by the game
- After a raid, if the camp's store holds more than 100 food, the marauders stay at camp: one is put at the camp and the band dances round it (a 30-second dance): never started by the game
- Meanwhile one marauder at a time walks to a spot by the store, faces it, eats 100 food and rejoins the band, then waits 30 seconds: never started by the game: so each 100 food buys a little over 30 seconds of peace
- When the store is down to 100 or less, the dance stops and the next raid begins: never started by the game
- The marauders walk at 0.5 to a spot beside the village storage pit and wait until within 15 of it: never started by the game
- Once the camera is within 100 and the pit is in view, the evil advisor points: "Argh! These godless thieves are attacking the Village Store. I say kill every last one of them!" and the good advisor answers "No don't kill them. They're just hungry. That's all.": never started by the game
- 500 food is taken from the storage pit; after 2 seconds the raid is over: never started by the game: the header comment says this raid destroys the storage pit, but the script only takes food
- The raid ends early if two marauders are killed during it: never started by the game
- The record's reminder becomes "Food's been stolen from the Village Store. The Marauders are to blame."; the marauders go home: never started by the game
- If two or more died in this raid, the evil advisor: "The marauding band have retreated. We've given them a good pasting!": never started by the game
- 500 food is added to the camp's store, then the band waits 40 seconds (a comment says this was lowered for testing): never started by the game: the 500 is added even when the raid was cut short before anything was taken
- If the camp has more than 100 food, the band dances and eats again as above: never started by the game
- Only if all ten marauders are still alive at this point: never started by the game
- The good advisor points at the farmer's house: "The man who asked for your help needs to speak with you, Leader.": never started by the game: nobody has asked yet, since the opening scene where he would have was cut
- It then waits until the house is clicked; the evil advisor would nag "A Silver Reward Scroll. Let's see what it's all about." every 30 seconds while the camera is within 100, but the nag hangs on a scroll that is never made: never started by the game: whether the nag ever fires with no scroll can't be told from the source
- A Norse farmer comes out of the house and walks to a spot in front of it; the camera glides to 10 above and 10 off him over 4 seconds: never started by the game
- The record is set again (success 0) and the player's alignment drops by 0.4: never started by the game: the player is marked down for having let the raids happen
- The farmer: "Please help us, massive one. We'll reward you!": never started by the game
- After 2 seconds the camera returns (3 seconds) and the farmer is released: never started by the game
- Whether or not the farmer begged, the marauders walk at 0.5 to the crèche and wait until within 15 (or all dead): never started by the game
- The good advisor points: "Now they're attacking the crèche! If I wasn't so good I'd go down there and pull out their innards!": never started by the game: said even if every marauder is dead by then
- After 3 seconds, five sparkles appear around and above the crèche for 10 seconds; after 2 more the crèche is set on fire: never started by the game
- The raid ends early if two marauders are killed during it, before the fire: never started by the game
- The record's reminder becomes "People have been taken from the Village. Those outrageous Marauders did it!"; the marauders go home: never started by the game
- If two or more died in this raid: good advisor "They took losses in that last raid. If they've got sense they'll leave us alone now.", evil advisor "It was glorious to see.": never started by the game
- From now on the band loops for ever: eat at camp while it has more than 100 food, then raid the storage pit for another 500 food and carry it home: never started by the game
- These later raids have no advisor lines and don't change the record: never started by the game
- The loop only ends when every marauder is dead; there is no timer, no failure and no other way out: never started by the game: a player who keeps them fed never finishes the challenge
- When the last marauder dies: "Sweet. The marauders are all dead.": never started by the game: the good advisor is brought out for a line voiced by the evil advisor
- If the farmer did not beg (a marauder was killed before the third raid), the record closes as a success with no reward: never started by the game
- If he did, it waits for his house to be clicked again (the same scroll-less nag as before): never started by the game
- The farmer walks out again; the camera glides to 10 above and 10 off his spot over 3 seconds; he faces the camera and prays once: never started by the game
- The player's alignment rises by 0.2 and the record closes as a success with alignment -0.2: never started by the game
- The farmer: "Oh thank you, invisible one. We will worship harder for you." then "We humbly offer you this. You know, just to say thanks.": never started by the game
- A fireball miracle seed falls from the sky beside him, with no reward camera; the reward help shows when it is clicked: never started by the game: the shared reward script, run without its camera
- After 2 seconds the camera returns over 4 seconds; the farmer is left in the script's control (a comment wonders whether to make him homeless): never started by the game
- Last of all the record is closed again with alignment 0, overwriting the alignment just set: never started by the game
- Every closing record uses the reminder "Do you want to swap your Creature with one from this person?", which belongs to the swap quests: never started by the game
- The reward only comes to a player who killed none of the band before the third raid and then all of it afterwards; killing them early, the advisors' first suggestion, gives nothing: never started by the game
- The header comment (three raids, retreat after two deaths, storage pit and crèche destroyed) is copied word for word into "Food for Thought"'s source: never started by the game: see [food_for_thought.md](#storysilver_scrollsfood_for_thought)
- Marauders also appear, unrelated, as a single villager in one of Land 5's cut scenes: never started by the game: not this quest

#### story/silver_scrolls/the_beach_temple_puzzle

From [The Beach Temple Puzzle](progress/story/silver_scrolls/the_beach_temple_puzzle.md).

- Eight of the man's lines, about belongings ruined by a flood, are in the text table but no script says them: "My collection of clocks!", "My soccer trophies!", "The Welsh dresser's toppled!", "Oh, my matchstick galleon!", "My ship in a bottle collection!", "Oh, my hip replacement!", "My CDs!", "My house of cards! That took four years!": apparently a cut flooding stage, where the temple flooded and the man lamented as he lost things; flagged differently in the text table from the lines used. Whether the engine itself ever says them is not confirmed: no script source uses them, and a search of the PC executable for their text numbers finds no plausible use
- Every other line of the quest's set (1 to 8, 17 to 19) is used

#### story/silver_scrolls/the_creature_breeder

From [The Creature Breeder](progress/story/silver_scrolls/the_creature_breeder.md).

- Land 2 and Land 5 positions are prepared too (each exactly on a Celtic house in a neutral Celtic village) but neither land's control script starts the breeder; only test launchers outside the compiled file do: never started by the game on those lands
- The comment above the breeder's swap asks people not to touch it because "it works and it was tricky to get to work": developer note; nothing to do
- The launcher for Land 1 and Land 4 finds the kennels as a house; the Land 2 and 5 entries use a plain marker: never started by the game on those lands

#### story/silver_scrolls/the_explorers

From [The Explorers](progress/story/silver_scrolls/the_explorers.md).

- A storm reward falling from the sky was the original reward and is left commented out: not in the compiled scripts
- An advisor line "It looks like the men are getting ready to set sail." (good advisor) is in the text table but no script says it: not used by the shipped scripts
- A sailor's line "How are we going to sail the ship now we don't have a full crew ?" is in the text table but no script says it: not used by the shipped scripts
- The quest's text calls the sailors missionaries throughout, though the scroll's title is "The Explorers": naming only

#### story/silver_scrolls/the_explorers_again

From [The Explorers Again](progress/story/silver_scrolls/the_explorers_again.md).

- A single-quest test launcher that sets the flag and starts it exists in the sources but is not part of the shipped game: developer test only
- A sailor's line "Eventually we landed and started this settlement in your honour." is in the text table but no script says it: never in the game
- A second sailor's thanks ("You've been so devine we thought we'd make an offering of it") was written into the film and switched off; it has no text line: never in the game
- An earlier start that waited for the camera to look at the wreck, instead of a scroll, is switched off: never in the game

#### story/silver_scrolls/the_fish_puzzle

From [The Fish Puzzle](progress/story/silver_scrolls/the_fish_puzzle.md).

- A developer test script for the creature's creed also starts this quest; it is a developer test, not reached in the shipped game: developer test only

#### story/silver_scrolls/the_heavenly_fire

From [The Heavenly Fire](progress/story/silver_scrolls/the_heavenly_fire.md).

- The challenge plays no music of its own (the capture scene before it stops the Nemesis theme): nothing to do
- A scroll over a villager's hut, pointed out by the good advisor with "Your attention is required here.", was planned and switched off: never in the game
- The challenge's lines 6 and 8 to 11 are missing from the text table: the old volley announcements were cut and replaced by the five new Land 5 lines: never in the game
- The tower and storage pit are looked up but never used, nor is the villager's hut: never in the game

#### story/silver_scrolls/the_hermit

From [The Hermit](progress/story/silver_scrolls/the_hermit.md).

- The hut branch's comments speak of the hermit giving a gesture; the shipped game gives a water miracle: comment only
- A test script starts the quest on its own; an unshipped control script had it switched off: not started by the game
- The hermit villager is also used by the fifth land's script and an unshipped waving scene, outside this quest: see [../land_5.md](progress/story/land_5.md)

#### story/silver_scrolls/the_idol

From [The Idol](progress/story/silver_scrolls/the_idol.md).

- A separate one-line test script that just runs the quest exists in the sources but is not in the shipped program: test scaffolding only (not compiled into `challenge.chl`)
- Source comments name the wrong advisor for three lines (the greeting, the maker's death line, the "worth punishing" line): comments only; the game plays what the text table gives
- The idol's scale is read twice in the compiled program for a single "four times bigger" (a compiler quirk with no visible effect): no effect a player can see
- The challenge log entry was first taken in the middle of the introduction, before the advisors spoke; that line is commented out and the entry is taken after the camera settles instead: cut
- A further camera move at the end of the introduction is commented out: cut
- A developer's temporary test ("gj temp") that blew the idol up 4 seconds after it was made, with the same bangs and flashes as the final scene, is commented out: cut test code
- The idol's text lines are numbered 01 to 20 but 07, 08, 14, 15 and 16 do not exist in the text table; of those that do, only 05 is never used: text table checked
- The special idol-builder villager kind is not used here; this maker is an Indian trader: the idol-builder villager appears on land 5

#### story/silver_scrolls/the_immersion_mushrooms

From [The Immersion Mushrooms](progress/story/silver_scrolls/the_immersion_mushrooms.md).

- The right-mushroom branch also stops a muffled child's crying sound that is never started: a leftover in the script; nothing to hear
- The evil advisor's wrong-mushroom line ends with a "stop pointing" though he never points: harmless script slip
- An earlier placing of the scroll's challenge-list entry during the film, and a final completion after the reward, are commented out: cut
- The engine has a second mushroom puzzle kind that no land uses: see [../minigames.md](progress/story/minigames.md)
- The end credits show a hippy beside a giant magic mushroom (among the credited firms is Immersion Corp.), and the fifth land's opening reuses the mushroom suspense sound: not this quest

#### story/silver_scrolls/the_japanese_traitor

From [The Japanese Traitor](progress/story/silver_scrolls/the_japanese_traitor.md).

- The creature takes no part; the talks only explain the land's creature curse: nothing to do
- The stranger was first meant to live in a hut near the forest (its spot is written in and switched off): never in the game
- A talk that started when the camera came within 75 (or 50) of him, instead of a scroll tap, is switched off: never in the game
- No line or film exists for the fourth Wonder, although the script counts it: never in the game

#### story/silver_scrolls/the_lost_flock

From [The Lost Flock](progress/story/silver_scrolls/the_lost_flock.md).

- The developers' comments call the line "You're a powerful being... not the best shepherd ever" a much ruder one; the shipped text is the polite version: text only
- A test script starts the quest on its own; an unshipped trade-show control script started it after the Pied Piper: not started by the game
- The quest exists only in the first land of the story: nothing to do beyond the story itself

#### story/silver_scrolls/the_magic_dragon

From [The Magic Dragon](progress/story/silver_scrolls/the_magic_dragon.md).

- Cut: each crusader was to idle by looking for something and looking exhausted in turn through each stage; the code is commented out (two followers keep an empty wait loop from it): not in the compiled script
- Cut: a base-camp object, a wait for the camera to be away before making the camp, a dragon murmur and a camera shake at the fourth blowhole in the leader's own spot are commented out: not in the compiled script

#### story/silver_scrolls/the_miracle_stones

The Miracle Stones: A cut stone-circle challenge: a mother kneels by her dead child beside a half circle of five singing stones and three empty stone bases. Clicking two stones that sing the same note an octave apart makes a "pair"; the first pair heals the child, the second drops food and wood, the third does nothing visible, and the fourth, which needs the three missing stones found around the land and set in their bases, brings a chimp creature for the player to swap into. It was never compiled into the game.

- Not in the compiled challenge file: it is missing from the compiler's project list, and no land's control script or challenge list starts it: never started by the game
- Only its single-quest test launcher starts it; it is not even in the developers' quest menu: never started by the game
- The third pair starts a background script for a curse on the nearby town that exists in no source file, so this file can't be built as it stands: never started by the game
- Its title "The Miracle Stones", its three reminders and every spoken line exist in the text table (under their own "singing stones 2" set): never started by the game
- Its spots fit Land 2: the mother's house is a real Indian house of Land 2's neutral Indian village (town 11), and the "cursed town" spot is that village's centre; the circle is on open ground about 140 from Land 2's shipped singing stone horseshoe: never started by the game: checked against every `Land<N>.txt`
- The three missing stones are hidden about 545 east, 860 south-west and 630 west of the circle: never started by the game
- Not reachable at all in the shipped game: never started by the game
- Land 1's "The Singing Stones" (a hippy's ring of eight holders; find the five missing stones and set them so the ring plays a rising scale; reward a food miracle dispenser) shares the idea of fetching missing stones from around the land and locking them into bases: never started by the game: see [the_singing_stones.md](progress/story/silver_scrolls/the_singing_stones.md)
- Land 2's "The Singing Stones" (a priest's horseshoe of nine stones; tap three hidden melodies for night, the dead raised or snow) shares the land, the area and the idea of playing the stones by clicking them to make magic happen, including raising the dead: never started by the game: see [the_singing_stones_land_2.md](progress/story/silver_scrolls/the_singing_stones_land_2.md)
- This version looks like a dropped alternative for Land 2's circle: matching octave pairs instead of tunes, rewards per pair, and a chimp at the end: never started by the game: no source says which came first
- It uses the same singing stone object as both shipped quests for all eight stones, and the same stone base for its empty holders: never started by the game
- Its chimp would have been the second chimp offered in the game; the only one that ships is The Rejuvenator's, given when the creature is already an ape: never started by the game: see [the_rejuvenator.md](progress/story/silver_scrolls/the_rejuvenator.md)
- Once the circle is out of view, five singing stones are made on one half of a circle of radius 17.5, each facing the centre, and they can't be moved or picked up: never started by the game
- Their notes, round the half circle: G, A, G an octave up, B, C: never started by the game: from the comments; every stone actually plays the same sound (see Sounds)
- Three empty stone bases fill the other half of the circle, the places of A, B and C an octave up: never started by the game
- A silver scroll appears about 37 west of the circle's centre; while the camera is within 100 and the scroll is in view, the evil advisor nags "Whew! As if you ain't busy enough, here's something for you to do." every 30 seconds until the scroll or the circle is clicked: never started by the game: the shared challenge notify script
- Only then are the three missing stones made at their far-off spots: never started by the game
- An Indian housewife stands 5 east of the centre; a dead boy of 9 lies near the centre, playing a dead pose: never started by the game
- Cut scene: the camera glides in 5 seconds to a low view across the circle; the mother faces the camera, kneels and prays: never started by the game
- The challenge record opens as "The Miracle Stones", success and alignment 0, reminder "We need to make two matching stone sing to heal the boy.": never started by the game
- Mother: "Holy one, these five stones are meant to bring people back from the dead." She gets up and gossips: never started by the game
- She turns to the child and mourns; the camera turns to the child over 2 seconds; mother: "Please make them work. My poor child, you see.": never started by the game
- After 2 seconds the camera returns over 3 seconds: never started by the game
- A missing stone dropped (not held, not flying) within 5 of its own base snaps onto it, faces the centre, and can no longer be moved or picked up: never started by the game
- Dropped within 5 of another empty base, it is given a little hop towards a point 10 above that base and does not lock in: never started by the game: it keeps hopping each time it lands there
- When all three are set, the good advisor: "All the stones are in place. Now we need to activate them.": never started by the game
- Clicking a stone whose pair is not yet made (a missing one only once set in its base) picks it as the first note, plays its sound and puts a sparkle 10 above it: never started by the game
- Clicking a different such stone picks the second note, plays its sound and puts a sparkle over it: never started by the game
- The pairs are G with G (the two original stones), and A, B and C each with their missing octave stone; either order counts: never started by the game: so only the G pair can be made before stones are found
- After any second note, right or wrong, both note sparkles and the choice are cleared (except after the fourth pair, which ends the puzzle): never started by the game
- A first note left without a second for about 10 seconds is cleared: never started by the game: counted in game turns, a comment assuming ten a second
- Each made pair gets a lasting sparkle 15 above both its stones within about half a second: never started by the game
- Rewards go by how many pairs are made, not by which pair: never started by the game
- Cut scene: camera to a raised view of the circle (3 seconds); good advisor: "Aha. The same note but an octave apart!" (a comment says to cut it if it makes the puzzle too obvious): never started by the game
- The camera swings round over 3 seconds while a strong heal miracle flies from the first G stone to the centre: never started by the game: always from that stone, whichever pair was made
- If the child's body is within 10 of the centre, a heal miracle hits him and he comes back to life, looks impressed, and the player's alignment rises by 0.5; otherwise it falls by 0.2: never started by the game
- If the mother is dead and her body is within 10 of the centre, she too is healed, brought back to life and set beside the centre: never started by the game
- The record moves to 0.25 (alignment 0), reminder "There are stones missing. Maybe we should find them.": never started by the game
- After 2 seconds the camera returns to the opening view; if both live, the mother faces the camera, prays and says "Eternal thanks, godly spirit.": never started by the game
- If only the child lives: "Where's my mother? I want my mother!": never started by the game: the line is voiced by the woman's voice
- If the child was outside the circle, the good advisor comes out for "Woah. Powerful stuff. Pity the kid's not in the circle.": never started by the game: a line voiced by the evil advisor
- The camera returns to where it was (3 seconds): never started by the game
- The living members of the family walk home at speed 0.3 to their house in the Indian village, about 285 north: never started by the game
- Good advisor: "Those two stones are permanently set now."; evil advisor: "I wonder what'll happen if we activate them all."; and, if any stone is still missing, good advisor: "We need the stones from around this land.": never started by the game
- Cut scene: camera to 15 above and 15 off the centre (3 seconds); five sparkles over the centre for 5 seconds: never started by the game
- A large food reward falls from the sky 5 east of the centre and a large wood reward 5 west, each showing its help when clicked: never started by the game: the shared reward help
- The camera drifts lower over 8 seconds; the record moves to 0.5 with the alignment so far, reminder "Maybe something'll happen if we match up more stones.": never started by the game
- Both advisors come out: "Wow. An honest-to-god Miracle." then "Hey! We've been granted some food! Go stones!": never started by the game: the first line is in the woman's voice although only the advisors are out
- After 2 seconds the camera returns: never started by the game
- Cut scene: camera to 15 above and 15 off the centre; magic beams run from the centre to each of the five stones and to each set missing stone, for 5 seconds: never started by the game
- After 2 seconds a beam runs from the Indian village's centre to the circle (a note asks for it in a different colour): never started by the game
- The record moves to 0.75, same reminder: never started by the game
- Evil advisor: "Eh? Nothing. Nothing at all."; good advisor: "Hmm. The pair is right, the stones are set, but no Miracle.": never started by the game
- A comment says the nearby town was to become "all 12 years old"; the curse script it starts was never written: never started by the game
- All the pair sparkles are removed: never started by the game
- Cut scene: camera to 15 above and 15 off the centre; five sparkles for 3 seconds hide the chimp's appearance: never started by the game
- After a second a chimp creature appears at the centre, turns to the camera and waves: never started by the game
- The record closes as a success with the alignment so far, with the opening reminder: never started by the game
- Good advisor, pointing at the chimp: "A Chimp. Let's have it. We must."; evil advisor: "How?"; good advisor: "Bring your Creature here and click the Chimp to swap bodies.": never started by the game
- The camera pulls back to 25 above and 25 off over 3 seconds and the cut scene ends there: never started by the game
- The chimp is then offered through the shared swap offer: never started by the game: see [creature_swaps.md](progress/story/silver_scrolls/creature_swaps.md)
- If the mother dies before reaching home: "Way to go. You killed her. Eee-vil!" and the player's alignment falls by 0.6: never started by the game
- If the healed child dies before reaching home: "You killed the little one. After healing her, too! Drat!" and alignment falls by 0.8: never started by the game: the line says "her" for the boy
- Each who reaches the house is released to village life and alignment rises by 0.1: never started by the game: see Bugs
- Before the first pair, if the child's body is more than 10 from the centre and not in the hand, the good advisor says once "Move the child back into the circle to heal him.": never started by the game
- Every stone, when clicked, plays the same placeholder: a muffled child's cry: never started by the game: comments ask for each stone's real note
- Each made pair plays a placeholder voice clip borrowed from another quest at the centre: never started by the game: a comment asks for a big chord
- The circle doesn't sing when the hand passes over it; a comment considers it but warns of the cost: never started by the game
- Reaching the house is checked every pass with no guard, so the +0.1 alignment repeats for as long as the mother or child stands at the house: never started by the game
- After the first pair the script waits for the mother's animation to finish; if she was left in her endless mourning (the child was not healed), it may never finish: never started by the game: whether an endless animation ever counts as finished can't be told from the source
- The 0.25 record ignores the alignment so far: never started by the game
- The last record goes back to the opening reminder about healing the boy: never started by the game

#### story/silver_scrolls/the_ogre

From [The Ogre](progress/story/silver_scrolls/the_ogre.md).

- A day-to-night script is started with the quest but its whole body is commented out, so it does nothing; it once darkened the time of day and raised a lightning drizzle while the camera was near the pass: cut
- An unused evil line: "Look! Here it is. You know I reckon we could fight our way up to it.": in the text table, used by no script
- Cut ways to finish: the ogre running after the creature when hit by a rock, and the quest ending when the player's influence covered the reward: commented out of the script
- A cut reward of a heal-miracle dispenser at the reward's spot: commented out
- The fourth land has a different ogre quest with its own script: not this quest
- A test script starts this quest alone with the fight-lesson flag raised: not started by the game

#### story/silver_scrolls/the_pied_piper

From [The Pied Piper](progress/story/silver_scrolls/the_pied_piper.md).

- Cut lines in the text table, never used: "You killed the piper!", "You've knocked away the rock. Here come the little brats.", "Oh this is enchanting!", "Humph.", "Nice bit of assassination there, Boss.", "And no children hurt. Wonderful!", "Scratch one piper.", "But what about the children? You haven't saved them!", "So many children are missing." — from an older design where the cave was sealed by a rock to knock away and the piper could be killed: text only
- An "anti-influence" around the piper (so villagers keep away) was written and commented out

#### story/silver_scrolls/the_plague

From [The Plague](progress/story/silver_scrolls/the_plague.md).

- Three advisor lines exist in the text table but no script says them, an argument over helping: evil "This is ridiculous. If they give us power, we can act."; good "The people are all sick, though. Let's try to help them."; evil "No. The strong will survive. That's nature. Nice and evil.": unused text; there is no plague line numbered 15

#### story/silver_scrolls/the_rejuvenator

From [The Rejuvenator](progress/story/silver_scrolls/the_rejuvenator.md).

- The opening record line was first written before the cut scene and moved inside it; a closing record line after the swap is commented out: source history only
- The text table has no lines numbered 1, 6 or 7 for this quest, and line 17 (the evil advisor's hint) exists but is never said: cut material
- The source creates a plain "female" villager; the compiled game turns that into a Norse housewife: data detail

#### story/silver_scrolls/the_riddles

From [The Riddles](progress/story/silver_scrolls/the_riddles.md).

- A tiny test script exists that runs the whole quest directly, for testing: `RunLostTreasure.txt`
- Unused clues of the earlier design, in the text table but said by no script: "Place in the ring a cunning hunter." / "And a bush of fire if you want to." / "A lump your Creature leaves behind." / "And protect with magic hand and mind.": unused text
- Unused step-by-step clues: "You need to place a cunning hunter into the ring of life." / "Now add to the ring of life, the Burning Bush." / "Next a lump of your Creature's waste products must be placed into the ring." / "Finally, protect the objects you placed into the ring with a Shield." and the hints "You may find one drinking fresh water after dark." / "You will find it between the snow and the sea."; an alternative opening "I have a legend for you. Solve it and you'll gain great power.": unused text; the wolves drinking at the water's edge at night match the first hint
- Unused progress lines: good "Excellent. Something's happening.", evil "Let's get the next clue from that woman!", good "This is getting interesting!", good "We're getting closer! There can't be much more to this.", evil "Something's happening!", good "The wolf's escaped.", good "Well that's pretty unique. I suppose.", evil "The object's no longer hot, Boss.": unused text: the shipped quest has no per-step feedback

#### story/silver_scrolls/the_sacrifice

From [The Sacrifice](progress/story/silver_scrolls/the_sacrifice.md).

- The developers' notes give the sacrifice values they planned around: a tree 250 to 1000, a decent animal 700 to 2000, a human 10,000 to 20,000, "so required is about 4K": source comment only
- The control script's comments disagree with its code: the comment on the village with id 10 says the sacrifice is "at town 2", the comment on the village with id 11 says it has "no challenge", yet the watcher is given the village with id 11, as is the developers' test script: comments only
- A running total of all offerings is kept but never used
- Every line of the quest's text (21 lines) is used; nothing is cut
- A developers' test script starts the quest on its own with the village with id 11; an unshipped land-one control script (one developer's copy) also started it: not started by the game

#### story/silver_scrolls/the_saviour

From [The Saviour](progress/story/silver_scrolls/the_saviour.md).

- A note in the land's control script says it waits until the creature is big enough, but the quest itself never checks the creature's size: a stale comment; the size it reads is never used
- A wife's line "You saved some people, but others still drowned. Call yourself a god? I'm going home." and the evil advisor's "Gutted! Ha! Fishermen. Gutted. Get it?" are never used: in the text table, used by no script

#### story/silver_scrolls/the_sculptor_cut_silver

The Sculptor (cut silver version): An early, silver-scroll version of Land 1's sculptor: a sculptor offers to carve the missing creature-gate stone if the player brings him the right rock from the old hermit's quarry, then hammers at it for twenty minutes. The shipped game replaced it with the gold-scroll story quest of the same title (its own script, in Land 1's story; see [../land_1.md](progress/story/land_1.md)); this version was never compiled.

- This file is not in the compiled challenge file: never started by the game
- The shipped Land 1 sculptor script defines scripts with the same names (the main script and the "rock returned" step, which takes one more argument there), and the shipped story calls those, never this file's: never started by the game: the compiled file only has the shipped versions
- The uncompiled early Land 1 control script asks to start a script by this file's name, but no script of that name exists (it is only a variable here), so even that early version could not start it: never started by the game
- It used a silver scroll; the shipped quest uses a gold scroll and records itself as a story quest rather than a challenge: never started by the game
- The title "The Sculptor", the reminder and most of the sculptor's lines are in the text table (shared with the shipped quest); five advisor lines and one reminder of this version are missing: never started by the game
- It calls the scroll's nag helper with one argument too many for the shipped helper: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A sculptor (a Norse farmer) is made by his house; a silver scroll appears over the house and the good advisor nags "There's someone who might need your attention, Leader.": never started by the game
- On click, a cut scene: he walks out, the camera glides in (4 seconds) and the challenge record opens, reminder "We need to get a carveable stone for the sculptor from the Hermit's Quarry over here.": never started by the game
- The sculptor: "Holy one, I hear you're looking for a Gate Stone." then "I am a sculptor. If you provide me with the right rock I'll carve one for you.": never started by the game
- The camera flies a path to the quarry: "You'll find the rock I need in the Old Hermit's Quarry. Please bring it here.": never started by the game
- Both advisors then speak five lines while the camera flies a second, 15 second path back: never started by the game: lines missing from the text table
- He then wanders around his house (radius 6): never started by the game
- Every 3 seconds, any ordinary rock within 30 of him is checked: he walks to it, says "Hmm. Actually this rock isn't right for carving. Try another." and it is removed: never started by the game
- The right rock (the quarry's gate-stone rock) put down within 30 of him, not held or flying: "Oh that's excellent. Exactly the right kind of rock to work with." the record goes to complete with full good alignment, "Thank you. Come back later and I'll have it finished.": never started by the game
- A lightning bolt miracle seed then falls from the sky about 14 from him, with the reward camera swoop: never started by the game
- He walks to the rock, faces it and hammers at it; the record is set back to half done and a 20 minute carving timer starts: never started by the game: the record goes from complete back to half, a quirk of this version
- Picking the rock up while he works: "Hey! I haven't finished working on carving the Gate Stone yet!" and the timer pauses with the time left: never started by the game
- Putting it back within 30: "Thank you. Now, if you don't mind. I've got carving to do." he walks to it and hammers again, and the timer resumes: never started by the game
- When the timer reaches its last second: the rock is replaced by the carved gate stone, "I've finished! It's ready to be placed with the others by the Gate." the record completes, he walks home and is removed: never started by the game: the check is for exactly one second left, which could be missed
- Thrown with almost no life left: the camera follows him in flight at half game speed while he screams "Aaaaaarrrrrrrggggggghhhh!" and carving stops: never started by the game
- Killed: the good advisor points at him, "What a senseless waste of human life!", the evil advisor "Come on. Let's do another!", and the record closes with full evil alignment; the quest ends: never started by the game

#### story/silver_scrolls/the_sea

From [The Sea](progress/story/silver_scrolls/the_sea.md).

- The text table holds seventeen more lines of this quest that no script uses (numbers 4 to 14 and 16 to 21; 1 to 3 don't exist), from an earlier plan where the children were in danger in the water and the woman judged the player: cut content; listed for completeness
- Unused, on killing the woman: evil "You killed the woman, Boss. Now that is a cool, calculated action. I approve."; the woman: "You disgraceful, murdering overfiend. No true god would act like that. I'm repulsed."; evil "Oh this is priceless. They can't take a bit of evil."; good "If I wasn't stuck inside your head, I'd leave and become a ghost or something.": cut content
- Unused, on success: the woman: "What a stunning performance. I'm so, so grateful. And the kids will be pleased, too."; evil "Huh. Am I wasting my time pushing the evil point of view? Are you gonna listen to me?": cut content
- Unused, on a cold result: the woman: "That was rather cold-hearted."; evil "Ooh, this girl is feisty. I like it."; good "Oh shut up and take your telling off like a man, demon."; evil "Who rattled your cage, you do-gooding nimrod?": cut content
- Unused, on failing: the woman: "Er, lordly one. You're gave it a good go. Thank you for trying.": cut content (the typo is in the text table)
- Unused, spotting the children: good "Look! Look down there! Children are in trouble in the water!"; good "We're not too good at this sort of thing. Remember the fisherman we failed to save?"; evil "Great! I get to see more people drown. Pass the popcorn!"; good "We should be able to save them. We saved the fishermen before."; good "Let's get the Creature over there to scoop them out."; evil "You mean, let's get him over there for a salty snack!": cut content; the fisherman lines refer back to an earlier fisherman rescue
- Line 15 of this quest is asked for by the cut Big Fish quest's scroll but is not in the text table: that quest belongs to the shared/unused list

#### story/silver_scrolls/the_shaolin

From [The Shaolin](progress/story/silver_scrolls/the_shaolin.md).

- The text table has five guru-quest lines no script says: evil, "He saw us! I say we leave him and come back later."; guru, "You tried to follow me before." and "Please leave me with my secrets."; "What was that?"; and "Leader, we're not managing to impress anyone in this place.": cut dialogue; nothing to play

#### story/silver_scrolls/the_singing_stones

From [The Singing Stones](progress/story/silver_scrolls/the_singing_stones.md).

- The text table has no lines 1 to 5 or 11 to 16 for this quest: cut before release: nothing to play
- A starting spot is set for stone 6, which is never used (stone 6 is one of the fixed ones)
- A first, commented-out scroll entry in the middle of the introduction (moved to its end)
- The wrong-stone notes "bad stone 2" and "bad stone 3" exist in the sound list, but all three wrong stones play "bad stone 1"
- An older version of the land control script (`LandControl1-steve.txt`, not in the shipped program) started a stone script of another name instead, whose source is not among the shipped scripts: not shipped; see the shared-and-unused quests

#### story/silver_scrolls/the_singing_stones_land_2

From [The Singing Stones (land 2)](progress/story/silver_scrolls/the_singing_stones_land_2.md).

- All six of the quest's own lines are used; no unused lines of this quest were found in the text table: checked the text table for the quest's line prefix and for tune names
- A small starter script that just runs this quest exists in the sources but is not in the land's challenge list (a test launcher): not shipped as a quest
- A cut stone-circle quest on this land, "The Miracle Stones" (eight stones, three hidden around the land, healing a boy), sits in the sources unlisted; its script is named "more singing stones", the very prefix this quest's own lines use, suggesting this quest replaced it: belongs to the shared-and-unused quests' file

#### story/silver_scrolls/the_slavers

From [The Slavers](progress/story/silver_scrolls/the_slavers.md).

- A test-menu script (start the quest straight on land 2) exists in the sources but is not in the shipped challenge file: developer test harness only; nothing to match
- Seven slaver lines exist but no script says them: "Good grief. You killed all the slave traders." (good), "You've murdered all the slaves. Dreadful." (evil), "The Slavers have gone." (good), "Hmm. Those slavers are up to something." (evil; the text keeps a stray "TA strokebeard]" stage direction), "The Slavers have kidnapped all the women from the Village!" (evil), "The Slavers are attacking the Village!" (good) and the chief's "Hey. You're our millionth customer. Have all our slaves free!": cut: never said in the shipped game
- The "all slavers dead" script is empty (comments plan cheering slaves released to the town and a "Slavers dead" line); the "killed enough" script's case for a dead chief is an empty block with a note to "do something" and attach slavers and slaves to the town: cut: the shipped game does nothing there
- The circus finale has a note to make the slavers do tricks with the animals; the camp builder's comment hoped for better cages made in the editor: cut
- The warning is compiled into the shipped challenge file, but nothing starts it: no land control script, no other script and the file's own header run it (it has no start-up line), so it never appears in the game: searched every script source and the compiled program's start-up list; only its own definition exists
- It was set at the Indian village (town id 10) by the shore: a bronze (tips) scroll by a hut (2821, 3052), about 80 from the village centre and just east of the Baywatch quest's hut: cut
- Its notice would have sent the evil advisor out pointing with "Your godly attention is required here, Leader." (a line the text table gives to the good advisor) until the scroll or hut was clicked: cut; the shared scroll-notice script
- An Indian fisherman would come out of the hut and walk to a spot 14 in front; the camera flies to him (position 4 seconds, focus 2), and once he arrives he turns to the camera: cut
- He says: "A band of foreigners have taken some of our womenfolk!" / "They kidnapped them when we were out hunting!" / "We tried to track them but they're long gone now." / "Three women were taken. Please, holiest, look out for them.": cut; the source's comments carry an older draft ("Strange foreigners have kidnapped our wives!" and so on)
- The camera returns over 3 seconds, but a typo moves the camera's position twice (to the old position, then to the old focus point) and never restores its focus; the fisherman walks home and is let go: cut; the same in the compiled program
- No log entry, reward or link to the Slavers quest: a closing comment only hopes to check whether "the Indian Slave women" are returned once the Slavers quest has begun. The Slavers' slaves are Greek, three of them women, and the quest's own introduction already has a villager warn of the missing people: cut

#### story/silver_scrolls/the_spiritual_healer

From [The Spiritual Healer](progress/story/silver_scrolls/the_spiritual_healer.md).

- It is a background event, not a silver scroll: no scroll stands anywhere, nothing has to be clicked, and no record, reminder or alignment is written to the challenge log: checked in the source and the compiled program: no highlight, snapshot or title
- The village is Greek: the land's map script makes town id 9 a Greek village; the control script's comment calls it Norse, and the villagers made at the end are Norse ones: naming only
- No music is played
- The script reads its village by id itself and ignores the village the watcher passes it
- His spoken lines read slightly differently from the developers' comments (e.g. "You kill me, you kill them all."): comments only
- The text table holds only the lines numbered 15 to 24 for him, all used; lines 1 to 14 are missing from the table and from every script, so an earlier version's lines were cut entirely
- There is no scroll title for it in the text table; the nearest unused-looking title, "The Rejuvenator", belongs to the ape-swap scroll: checked: that title is used by the ape-swap script
- A developers' test script starts the event on its own: not started by the game

#### story/silver_scrolls/the_treacherous_path

From [The Treacherous Path](progress/story/silver_scrolls/the_treacherous_path.md).

- Morg was once to cast enlarge and strength miracles on himself before fighting; the lines are commented out: cut; the growth is done by scaling instead
- A fourth tree by the path is looked up but never set alight; a brother's reward spot and reward object are declared but never used; an earlier wolf-pack spot is commented out: cut

#### story/silver_scrolls/throwing_stones

From [Throwing Stones](progress/story/silver_scrolls/throwing_stones.md).

- A counter of rocks put back at the pile is kept but never read
- An earlier version had three boulders to knock off (each with its own advisor line and the reminder "See if you can hit that pillar…" until all three fell); it was commented out and its lines are gone from the text table
- Only one fisherman is made, though the script is written to make several watchers spread around the spot

#### scripts/creature_mind_scripts

From [Creature mind scripts](progress/scripts/creature_mind_scripts.md).

- The four older files (`CreatureDestroyOtherCreatures`, `CreatureDestroyTowns`, `CreatureImpressTowns`, `CreatureProtectTowns`, 54 to 130 KB) are in a format the shipped game can't load, and nothing names them: left over from development; our reader rejects them the same way

#### scripts/dance_scripts

From [Dance scripts](progress/scripts/dance_scripts.md).

- The developers' dance editing (a dance-for-editing state and an editor state): a developer tool; the test land for it (`dance.txt`) loads a landscape that isn't shipped
- Villagers praying facing inwards: file `DancePrayInwards`, 120 beats, no set length, any number of dancers, starts on its own: the file isn't shipped (only `DancePrayingInwards.dat` is); the game makes the dance all the same, with one group and no key frames, and so does our tree
- Villagers dancing around an object narrower than 20: file `DanceAroundObjectDiameterLess20`, 90 beats, length 900 (units unconfirmed), 14 to 20 dancers: never chosen: objects 10 or wider get the "20 or wider" dance (see [../town/artefacts.md](progress/town/artefacts.md))
- `Creature1` (656 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `CreatureImpressVillagers` (2,729 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `CreaturePlayingGame` (8,123 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `DancePrayingInwards.dat` (396 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `FourDancers` (3,808 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `LineDancing1` (2,628 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `NewCelebration1` (7,628 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `NewCelebration2` (11,896 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `NewEightDancers` (7,840 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `NewFourDancers` (5,056 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `OnlyDanceThatWorks` (5,912 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `TwoDancers` (1,876 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `Worship1` (6,140 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `Worship2` (9,724 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `Worship3` (684 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `funeral1` (5,916 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `marraige1` (8,061 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `totem1` (4,408 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do
- `totem2` (3,870 bytes) is shipped but no dance in the table uses it: left over from development; nothing to do

#### scripts/info_scripts

From [Info scripts and tables](progress/scripts/info_scripts.md).

- Two of the narrators used (the guide and the monk) aren't among the narrator numbers the script declares: data quirk: our reader gives them an undeclared narrator (`helptext::k_NarratorUndeclared`); which voice the game gives them is not known

#### scripts/land_script_commands

From [Land script commands](progress/scripts/land_script_commands.md).

- `BRUSH_SIZE`: Sets the editor's brush size; not used by the shipped scripts: an editor setting, empty in openblack
- `MULTIPLAYER_DEBUG`: Turns on network debugging; not used by the shipped scripts: a developer switch
- One miracle in *Death Comes To Those That Wait* is misspelt (`EXPLSION_PU1`); the game finds no such miracle for that town: data error in the shipped file; the miracle is not found and nothing is given, as in the game
- `OUTPUT_VILLAGERS`: Writes the villagers out (debugging); not used by the shipped map script: developer output
- `OUTPUT_TOWN`: Writes the towns out (debugging); not used by the shipped map script: developer output
- `OUTPUT_CREATURES`: Writes the creatures out (debugging); not used by the shipped map script: developer output
- `OUTPUT_COLLIDE`: Writes collisions out (debugging); not used by the shipped map script: developer output
- `OUTPUT_ALLOC`: Writes memory use out (debugging); not used by the shipped map script: developer output
- `SAVE_FOR_NET_DEBUG`: Saves for network debugging; not used by the shipped map script: developer output
- `LOAD_FOR_NET_DEBUG`: Loads for network debugging; not used by the shipped map script: developer output
- `OUTPUT_RAND`: Writes the random numbers out (debugging); not used by the shipped map script: developer output

#### scripts/map_and_demo_scripts

From [Map, demo and test scripts](progress/scripts/map_and_demo_scripts.md).

- The demo's own challenges: the shipped `challenge.chl` is the full game's; the demo's scripts aren't shipped
- Loads a developers' landscape (`DEVDATA\Landscape\old.led`) that isn't shipped, so neither the game nor openblack can open it: the landscape is missing from the game
- Builds five Celtic towns with 43 buildings, 168 villagers, 19 cows, 65 trees and two temples and two worship sites (the player's and a rival's), and switches on a second computer player with an old-style creature: for reference: what the test land would build; every command it uses is covered in [land_script_commands.md](progress/scripts/land_script_commands.md)
- Loads a dance test landscape (`dance.led`) that isn't shipped, and builds nothing else: the landscape is missing from the game

#### rival_gods/ai

From [How a computer god decides](progress/rival_gods/ai.md).

- The world aim has nothing under it: nothing to do
- Nice and nasty messages, taunts, scares and gifts to a player are in the tree but were never made: they do nothing in the game

#### rival_gods/challenge_scripts

From [Script control of computer gods](progress/rival_gods/challenge_scripts.md).

- A list of every need's name ships for script writers' help: a text list in the game's script sources, not read by the game
- A step-by-step battle plan for Nemesis: every 30 seconds, by which towns the player holds and whether their belief is rising, it impresses, destroys or defends a town or attacks the player's creature, retuning his weights each time: the script ships but nothing runs it
- Debug scripts start single story scripts on their own: developer tools in the script sources

#### rival_gods/creatures

From [The computer gods' creatures](progress/rival_gods/creatures.md).

- Four larger template minds (destroying other creatures, destroying towns, impressing towns, protecting towns) ship with the game but no script loads them, and the game refuses their old layout: unused; our reader rejects their layout too: they are version 17, with no species row where a mind has one, so it answers that they are not mind files (`MindResult::ErrNotAMindFile`, `components/creaturemind/include/MindFile.h`)
- Exported creatures the player saved also sit beside them; they are the player's, not a god's: see [../creature/saves_and_files.md](progress/creature/saves_and_files.md)
- The explosion miracle was left out of their lessons: commented out in the game's scripts

#### rival_gods/nemesis

From [Nemesis](progress/rival_gods/nemesis.md).

- He is never a player on the second, third or fourth land; his attacks there are scripted from the vortex or from points in the sky: there is no Nemesis player to judge; see [khazar.md](progress/rival_gods/khazar.md) and [../story/land_4.md](progress/story/land_4.md)
- A script for a step-by-step battle plan (impress, destroy, defend, attack the creature) exists but nothing runs it: unused in the shipped game; see [challenge_scripts.md](progress/rival_gods/challenge_scripts.md)

#### rival_gods/skirmish_opponents

From [Skirmish opponents](progress/rival_gods/skirmish_opponents.md).

- Messages, taunts and gifts to other players are in the mind but were never made: see [ai.md](progress/rival_gods/ai.md)

#### easter_eggs/developer_jokes

From [Developer jokes](progress/easter_eggs/developer_jokes.md).

- If the program's linked lists ever went wrong it would stop with "Peter: Damn Richard won the bet" (a node with no node before it), among other "Peter:" messages: only shown on an internal error, never in normal play; nothing to port
- Other developer-signed error messages: "JONTY-When is a flock not a flock?", "JONTY- Something is seriously wrong with my code", "Jonty-Thing should be in script! OH CALL ME!!!!", "No thing - BAD MAN!", "Script offsets exceeded - Blimey!", "Jeremy: failed miserably save has - tell me.", "Richard: no subactions?", "Wierd - this bitstring (11b) is reserved.": internal errors only
- The creature's actions include "kiss friend's arse", "tell creature to sod off", "watch telly" and "fart", and the challenge language has a command to get a creature's "arse position" (where its poo comes from): names inside the program; the behaviours are in [secret_behaviours.md](progress/easter_eggs/secret_behaviours.md) and [unused_content.md](progress/easter_eggs/unused_content.md)
- A key layout named "Daniel's Key Config mofo, dont mess": see [hidden_keys_and_cheats.md](progress/easter_eggs/hidden_keys_and_cheats.md)
- A message left in the program for testers: "This is a developer beta patch. There will be no support from EA regarding this patch for Black & White. ... please fill in the bugreport.txt file included with the patch and email it to [a Lionhead address]": whether any build shows it is unconfirmed

#### easter_eggs/hidden_content

From [Hidden content](progress/easter_eggs/hidden_content.md).

- Four of the twenty-odd phone box recordings are never played (numbers 3, 12, 13 and 14 of the set): they ship in the script sound bank; see [unused_content.md](progress/easter_eggs/unused_content.md)

#### easter_eggs/hidden_keys_and_cheats

From [Hidden keys and cheats](progress/easter_eggs/hidden_keys_and_cheats.md).

- There are no typed cheat codes: the game keeps a buffer of typed keys, but it only replays them, one by one, into the normal key handling each frame; nothing matches sequences: checked in the key-buffer handling; nothing to port
- Jeremy's menu, H for its help page: K "Switch QuickClick Interface", C "Cheat", I "SlowDown Game", O "SpeedUp Game", M "Advance Night Time", F "Generating Footpaths", R "Free Move in Slow Motion", W "Divers (Help My Current Test)", Z "Go As Fast As Possible": unreachable in the release; each key prints a "Jeremy - ..." message
- What Jeremy's keys actually do in the release code: R toggles a free-movement flag, C sets it on, Z toggles "as fast as possible", and W (despite its label) toggles a spell cheat for the local player; I, O, M and F only print their message, because the routine they pass on to is empty; K does nothing: unreachable
- Tim's menu: "O - Force an Out of Sync" (forces a multiplayer desynchronisation for testing): only its help text is left; unreachable
- George's menu: Shift+P reloads the weather system, W shows weather information: unreachable
- Tom's menu: F generates the footpaths of every land in turn (lands 1 to 5, the three multiplayer maps and the Two, Three and Four Gods playgrounds), W turns on the wall-hugging (path-finding) debug view: unreachable
- Nine named key layouts: "Default Key Config", "Jeremy Key Config", "Mark Webley Key Config", "Giles Key Config", "Jean-Claude Key Config", "Daniel's Key Config mofo, dont mess", "George Backer Conifg", "Tim's Conifg" and "TBL's Conifg" (sic): the table of names is never read
- The interface's own cheat routine is empty in the released program: nothing to port
- Creature cheats: learn everything, learn ordinary things, go to the next stage of growing up, make it fight, clear its help timers, and agree to every request: present in the program but no key or command reaches them in the release (unconfirmed for every build); openblack's debug spawner does similar things, see [../creature/saves_and_files.md](progress/creature/saves_and_files.md)
- Player, worship site and temple cheats, and a cheat that hands a town to a player: present but unreachable (unconfirmed for every build); see [../worship/prayer_power.md](progress/worship/prayer_power.md) and [../town/belief_and_conversion.md](progress/town/belief_and_conversion.md)
- The computer gods' "find any old ..." helpers let them pick any town, field, tree, villager, creature or miracle anywhere, without having to know about it: part of the computer god, not a player cheat; see [../rival_gods/ai.md](progress/rival_gods/ai.md) (unconfirmed which of them the rival gods use)
- In the internet lobby, a player whose name cannot be read is listed as "Cheater": the lobby service is gone; see [../multiplayer/](progress/multiplayer/)
- A hidden setting, "LeetLamer", in the game's setup registry key is read each time the list of internet game channels is built and passed on with every channel: what it changes is unconfirmed; the lobby service is gone

#### easter_eggs/launch_switches_and_files

From [Launch switches and stray files](progress/easter_eggs/launch_switches_and_files.md).

- `FORCEINETCONN` and `NOINETCONN` force, or skip, the start-up check for an internet connection: only the long-gone online extras use the connection; see [../pc_integration/](progress/pc_integration/)
- `EDITOR` and `LAND` are recognised but nothing in the released program uses them: left over from the developers' build
- The separate Creature Upload utility (`CreatureUpload.exe`) sent the profile's creature to Lionhead's creature site, where it could be viewed at a page of its own name: the site is gone; see [../pc_integration/](progress/pc_integration/)
- The program still knows the paths of several developer files: `savedmap.txt` (and `..\newtestbed\savedmap.txt`), `MemoryLeaks.txt`, a saved copy of `Data\PhysicsConstants.txt`, out-of-sync logs and a physics log: developer leftovers; `MemoryLeaks.txt` even ships in the game folder; which of them a player can trigger is unconfirmed

#### easter_eggs/unused_content

From [Unused content](progress/easter_eggs/unused_content.md).

- "Tell a friend a joke" is fully built (it walks up fluttering its eyelids, talks, and its friend reacts) but is missing from the game's table of actions, so it can never be chosen: not in the game as it plays
- The Creature Ladder: fifteen fights on plinths against copies of your own creature in every shape, in order sheep, chimp, cow, ape, zebra, horse, mandrill, tortoise, wolf, gorilla, leopard, bear, polar bear, tiger and lion: not in the shipped challenges
- The Marauders: a pack of ten raids the village three times, first driving off a flock of cattle to their den, then wrecking the storage pit, then the crèche; they retreat from a raid once two are killed, and leaving food at their camp keeps them away for a while (advisor: "That food should keep the Marauders out of town for a bit longer."): not shipped
- The Caretaker: a good and an evil version of an adventure with a following of chickens, marauders and an "avatar": not shipped; the source is only an outline
- The Chimp Posse: three chimp creatures guard their toys (a die and a teddy) and quarrel with Nemesis' creature over a toy ball ("Nemesis' Creature looks mighty pissed."): not shipped
- The Big Whale: a whale has swallowed a fisherman's son, and a rock thrown at it makes it spit him out: not shipped; unfinished
- Food for Thought (poisoned and magic food for a flock), Raiding Lions, Landslide, Storm Brewing, the Spitting Totem (a totem that spits objects out), Villager Catch, The Meeting, Rebuild Village, Release the Creature, Free Fall, Make Ball, Bug Flock and Throw It: not shipped; Food for Thought is described in [../resources/poison_and_mushrooms.md](progress/resources/poison_and_mushrooms.md); Villager Catch: [../story/gold_scrolls/villager_catch.md](#storygold_scrollsvillager_catch); Release the Creature: [../story/gold_scrolls/release_the_creature.md](#storygold_scrollsrelease_the_creature)
- The swap-to scripts for the horse, leopard, lion, turtle, wolf and sheep (only the ape, brown bear and cow swaps are compiled; the breeder uses its own swapping): not shipped; see the secret creatures in [hidden_content.md](progress/easter_eggs/hidden_content.md)
- A series of creature-training lessons (learn to eat, play and poo, the leashes one by one, drinking, thirst, meeting the guide, seeing its home, helping and impressing a town) replaced by the single shipped creature-development script: not shipped; see [../creature/lessons_and_help.md](progress/creature/lessons_and_help.md)
- Test scripts: "Hello, world!" (its header: "This classic \"Hello, world!\" script re-defines the gaming experience."), a test that makes a Norse housewife at the hand, a storm-over-the-cows night scene, black cloud, food store, scrap and "did you know" tests, creature and creature-animation tests, and another developer's copy of the Land 1 control script: not shipped
- A "BAFTA" control script that runs the old versions of the Lost Brother, the Pied Piper and Choose Your Creature, apparently for an awards demonstration: not shipped; its Choose Your Creature draft: [../story/gold_scrolls/choose_your_creature_bafta_draft.md](#storygold_scrollschoose_your_creature_bafta_draft)
- The Madness: Nemesis takes over the player's creature, makes it strong and of the opposite alignment and sends it to throw fireballs at a village's store and houses, then it is confined for a minute and a half until the madness wears off: not shipped; the fifth land's curse does the same story beat: [../story/gold_scrolls/the_madness.md](#storygold_scrollsthe_madness)
- The Ally Speaks: a cut scene, once meant to be offered from a gold scroll at the player's temple, in which the camera flies a set path while the advisors talk about another god's temple; its lines were never written: not shipped: [../story/gold_scrolls/the_ally_speaks.md](#storygold_scrollsthe_ally_speaks)
- The Lost Brother once had three more endings, all commented out: both brother and sister killed, both brought home safe, and the brother "left for dead" when his health, draining while he was lost, ran out: the advisors' lines for them exist; the shipped challenge has none of it
- The hidden phone box's land-skipping cheat (see [hidden_content.md](progress/easter_eggs/hidden_content.md)): replaced by "Sorry, the boss got the cheat removed"
- In the unfinished Cup Final, the away side (the Celts) cheats near the end by bringing on an extra substitute: not shipped
- The creature's "watch telly" action has no telly: its builder runs the poo's agenda, and no desire lists it (the "fart" is the poo desire's second action, with puffs of its own; see creature/idle_behaviour above). openblack gives a forced WatchTelly the poo's agenda, as the original (2026-10-22, 764e131fa); nothing else to port
- The developers' key menus and nine named key layouts: see [hidden_keys_and_cheats.md](progress/easter_eggs/hidden_keys_and_cheats.md)
- `Scripts/dance.txt` is a land for editing dances, but its landscape (`dance.led`) does not ship
- `Scripts/comp.txt` is a computer-player test land loading a landscape from a developer folder that does not ship
- `Scripts/demo2.txt` sets up Land 3 with the player in the Japanese town, apparently for a demo: (unconfirmed which demo)
- The playground "Construct" (`Scripts/Playgrounds/construct.txt`, on `construct.lnd`), announced as "Testing landscape": one Norse house, two villagers, a temple and a row of objects: not a game file: it is openblack's own test land, added to openblack's test data in 2021. It sits with the playgrounds only when copied into the install's playground folder, and then the skirmish box lists it as "construct" like any other land there; see [../multiplayer/maps/construct.md](progress/multiplayer/maps/construct.md)
- Four of the hidden phone box's recordings are never played (numbers 3, 12, 13 and 14): in the script sound bank
- Dance files named "Test" and "OnlyDanceThatWorks" in `Scripts/Dance`: (unconfirmed whether any dance loads them)
- A "cheat box" model (a plain box 180 × 400 × 180, sitting entirely below the ground) is in the model list; there is no bowling pin model, and the toy bowling ball and skittles are shipped toys used on the tutorial land: (unconfirmed whether the cheat box is used anywhere); the shipped toys and the cut bowling games: [../nature/toys.md](progress/nature/toys.md)
- The script sources reference text for cut challenges (25 lines for the marauders, 17 for whack-a-villager, the chimps' names): whether the shipped text still holds them is unconfirmed

### Engine

#### engine/asset_formats

From [Asset formats](progress/engine/asset_formats.md).

- Loose wave files (`.wav`): our wave decoder reads them (`src/Audio/Codec/WaveFile.cpp`), but the game has none to read. Our wiki differs: the original plays no loose wave files, every sound is in a bank ([audio](../formats/media-files.md#sas-and-animation-tables))
- The script library and language plug-ins (`Plug Ins/*.dll`): openblack builds the script VM in

#### engine/file_system_and_packs

From [File system and packs](progress/engine/file_system_and_packs.md).

- Player-made data overrides (mods): the game has none. openblack's old mod support was removed (2026-10-06); what stays is the minimal mod loader library (`src/ModLoader`), and only with it and its mods folder read, a switched-on mod's files replace the game's files of the same path (data mods), a 🎯 chosen difference of 2026-10-22 ([data mods](mod-loader.md#data-mods-a-mods-files-replace-the-games))

#### engine/platforms_and_performance

From [Platforms and performance](progress/engine/platforms_and_performance.md).

- The disc must be in the drive and its copy protection pass: openblack reads an existing install
- The minimum machine of 2001 (a 350 MHz processor, 64 MB of memory, an 8 MB 3D card): modern machines only
- The keyboard is read on its own thread and its keys handed to the loop under a lock: openblack reads keys from SDL's events on the main thread
- The game manages its own memory heaps and writes a leak report to the install folder on exit: openblack uses the standard allocator and owns memory through RAII

#### engine/saving_and_loading

From [Saving and loading](progress/engine/saving_and_loading.md).

- The creature's details are exported to a web page in the profile for the online creature service: the online service no longer exists
- Creatures shared with Creature Isle: expansion not supported

#### engine/script_vm

From [Script virtual machine](progress/engine/script_vm.md).

- Creature Isle's version of the program format: expansion not supported

#### rendering/display_and_frame_rate

From [Display and frame rate](progress/rendering/display_and_frame_rate.md).

- 16 or 32 bit colour: modern displays are always 32 bit
- Anti-aliasing: the game has none, and openblack draws none in play or in the checks: four-sample multisampling of the back buffer is a debug switch only (the debug bar's View menu, off by default; a06adda2d), a 🎯 chosen difference of 2026-10-21 ([the frame](../rendering/frame.md#comparison-with-openblack)). `FrameBuffer` (`src/Graphics/FrameBuffer.cpp`) multisamples an off-screen target only when asked, and none asks
- Gamma or brightness setting: no gamma or brightness setting in openblack. Our wiki differs: the original has no gamma, post-processing or fog setting in its frame ([parity](../rendering/frame.md))

#### rendering/level_of_detail

From [Level of detail](progress/rendering/level_of_detail.md).

- Objects beyond a distance stop being drawn: openblack draws everything. Our wiki differs: the original never fades or hides models by distance and has no far plane ([parity](../rendering/frame.md))

#### rendering/light_beams

From [Beams and lights of worship](progress/rendering/light_beams.md).

- Coloured lights in red, green, yellow, blue or white follow the dancers of a dance: the game keeps them but never draws them: the routine that would light the land under them (`DanceLight*.raw`, 6 by 6) is never called in any PC version

#### rendering/sprites_and_billboards

From [Sprites and billboards](progress/rendering/sprites_and_billboards.md).

- Distant trees drawn as flat stand-ins: the game draws its trees as models (unconfirmed that it has no stand-ins)

#### audio/music

From [Music](progress/audio/music.md).

- The creature's mood shapes the music: Our wiki differs: the original has no mood-driven music; what looks like a music mood controller is network packets ([audio](../audio/music.md#intro-trailer-outro-videos-and-menu))
- Music from the CD's audio tracks: the game's own banks are used

#### audio/sound_effects

From [Sound effects](progress/audio/sound_effects.md).

- Crowds impressed, or losing belief: not in the original: the in-game bank's crowd samples (H_ImpressedGroup, H_LoseBeliefGroup) are played by nothing in the exe or its scripts, and the impressed villagers' states play no sound ([world-sounds.md](../audio/world-sounds.md#villagers-buildings-and-trees-own-sounds)); what is heard is the spirits' belief remarks, see [voices_and_speech.md](progress/audio/voices_and_speech.md)

#### audio/voices_and_speech

From [Voices and speech](progress/audio/voices_and_speech.md).

- The spirits warn that the creature is under attack or that people are injured, and remark on other villages: not in the original: their three functions are in the v1.42 exe but nothing calls them, jumps to them or holds their address ([voices](../audio/voices.md#gguidance)); `guidance::WarnCreatureUnderAttack`, `WarnInjuredPeople` and `RemarkOtherVillages` stay uncalled as there

#### video/bink_playback

From [Bink video playback](progress/video/bink_playback.md).

- Videos are looked for on the CD first when the game runs from the CD: openblack reads the install folder

#### video/bink_videos

From [Bink videos](progress/video/bink_videos.md).

- No other videos ship with the game; Creature Isle is not supported: a search of the game's files found no expansion videos
- An option of the game's options for its films: there is none; the options' only video setting is the detail level, whose change the game tells with HELP_TEXT_DIALOG_VIDEOCHANGE ([the detail change message](../video/playback.md#the-detail-change-message), tracked with the options in [Options](progress/interface/options.md)); the films are asked for only by the command line's PREINTROVIDEO and the scripts

#### pc_integration/online_services

From [Online services and PC extras](progress/pc_integration/online_services.md).

- Each online step is logged, prefixed "[INET]", to `inetlog.txt` in the game folder: a log of the original's online steps
- A command-line switch forces the game to think it is connected (unconfirmed): a switch named FORCEINETCONN is in the game; its effect was not traced (unconfirmed)
- The online features need an account registered at the game's web site, `http://www.bwgame.com/register/`: the site is gone
- The game logs in at `login.bwgame.com` with the account's name and password: see [../multiplayer/network_play.md](progress/multiplayer/network_play.md)
- The account's login name and password are kept in the online player profile, the password encrypted
- The installer offers to register online: from the readme
- A separate program, the Creature Upload Utility (`CreatureUpload.exe`), sends the file to Lionhead's server: the server is gone
- The utility asks for the profile, user name and password, checks the account ("Validating user account, may take a while…"), then sends: its text is in `creatureupload.cfg`
- When sent, it says the creature can be seen at `http://www.bwcreatures.com/<name>`: the site is gone
- Its errors: could not connect, could not send or get data, could not open the creature file, timed out, user not validated, user not found, wrong password, no profile ("You need to register online"), no password entered: from `creatureupload.cfg`
- Sending a creature to a friend by e-mail: no such feature was found in the game (unconfirmed for the upload utility)
- A creature's mind is packed for sending when an online game starts: see [../multiplayer/network_play.md](progress/multiplayer/network_play.md)
- In a clan game (from patch 1.2), the clan's creature is downloaded from the game's servers at the start and uploaded at the end, instead of the team leader's: from the patch 1.2 readme; see [../multiplayer/multiplayer_rules.md](progress/multiplayer/multiplayer_rules.md)
- If the upload at the end fails, the end-of-game box says so, and the creature is uploaded next time
- A global list of maps, `GlobalList.txt`, is fetched from the game's servers: the servers are gone
- Maps and their thumbnails missing on the player's PC are downloaded into `Online Maps` and `Online Maps/Thumbs`: from the game's storage server (`storage.bwgame.com` or `www.bwgame.com`, unconfirmed which for which)
- If a map cannot be downloaded, the player is told
- Newly made maps keep their thumbnails in `Online Maps/Creation`: the map-making side is not part of the game's player features (unconfirmed)
- Internet games are found in a GameSpy lobby, through GameSpy's main server: GameSpy closed in 2014; see [../multiplayer/network_play.md](progress/multiplayer/network_play.md)
- The player's GameSpy registration is read from GameSpy's own settings on the PC
- The lobby has a main room and game channels: create or join a room, list the players in it, see players join and leave
- The host can ban a player from the channel, lock it, and allow invitations
- The game's own lobby and message servers, and a ping server, were also used: the ping server's side is done in bwgame-service: see [../multiplayer/gathering_box_and_ping_server.md](#multiplayergathering_box_and_ping_server)
- Internet games earn ranking points for players and clans, shown on the web site: see [../multiplayer/multiplayer_rules.md](progress/multiplayer/multiplayer_rules.md)
- A debug line shows the current effect's name: a developer display

#### pc_integration/real_clock

From [The real clock and calendar](progress/pc_integration/real_clock.md).

- If that does not match either, it tries the name Windows was registered to, from the system's setup information: read from the Windows registry; Windows-only
- None of the shipped challenge scripts use them: searched all the game's script sources; only mods would see the difference
- No other date changes anything: no Christmas, Halloween, New Year, Easter, birthday or release-date surprise: every read of the date in the game was traced; only the first of April and the real-night hours are acted on
- The Christmas music and the "White Christmas" singing-stones song play from a script on any date: not date-based; see [../audio/music.md](progress/audio/music.md) and [../nature/one_shot_features.md](progress/nature/one_shot_features.md)
- The in-game calendar (days, months, years and seasons) is the land's own, set by its script, not the real date: see [../weather/climates.md](progress/weather/climates.md)
- The e-mail feature notes the start-up time and ignores older mail: see [villager_names_from_contacts.md](progress/pc_integration/villager_names_from_contacts.md)
- The real weather queries send the computer's date as day.month.year: see [real_weather.md](progress/pc_integration/real_weather.md)
- A config text file named after the detail level is given a line with the date, time and detail level (unconfirmed when): a developer log, not a player feature

#### pc_integration/real_weather

From [Real weather](progress/pc_integration/real_weather.md).

- Its settings live under the game's setup key in the registry: whether to use it, the server's name, the server's port and the address of the query page: value names: UseInternetWeatherSystem, WeatherSystemServerName, WeatherSystemServerPort, WeatherSystemServerURL
- If the settings are missing, the game writes the defaults: use it = 1, server `weather.bwgame.com`, port 80, page `/query/`: the server is gone
- The query is only made when the "use" value reads 0: the game writes 1 as the default and then only runs when it reads 0, so it looks inverted; what the setup program wrote for "on" is not confirmed (unconfirmed)
- The server name and page may each be up to 200 characters
- It needs the current online player profile's login name and password; with no login name it does nothing: the password is kept encrypted in the profile and decrypted for the query; the accounts are gone
- The player's place is not typed into the game: the game asks the web site for the place stored with the player's account: the account (registered at the game's web site) held a country, an area and a city
- The first request is a web page fetch from the weather server with the action "query user data", the user's name, password and today's date: the date is the computer's local date written day.month.year with two-digit day and month and a four-digit year
- The reply starts with a status line that must be 1; anything else is a failure and the reply is thrown away
- The next three lines are the country, the area and the city: the country is a two-letter code, the area and city up to 150 characters each
- The second request asks with the action "query weather", the country, the area, the city and today's date: plain web page fetch over port 80 with the game's own web client
- The reply is lines of colon-separated fields, one line per day
- Each line gives a date (three numbers), then six numbers: the temperature, the visibility, the weather type and three more: the meanings of the three others are not confirmed (unconfirmed); fields after the ninth are ignored
- A line whose first field is empty is skipped
- The first day line in the reply becomes the player's weather: confirmed in the game's code during the bwgame.com service work (bwgame-service's `docs/protocol.md`); an earlier reading said the last line
- A developer key shows the weather got: country, area, city, type, temperature and visibility, or that the system is not loaded: developer keys of the original, not a player feature
- A developer key reloads the presets, and another switches the real weather off and on, putting the land's weather back: as above
- A developer key steps through the nine weather pictures in a test message: as above
- Everything it did depends on the service, reached only through it (checked in v1.42, `LHInetWeather::InitForGame` 0x5E98B0, called once from `GGame::Init` 0x54FF18): it gives up in a multiplayer game, without `Scripts/weatherinfo.lhw` (fn_005E9350), with the registry's "use" value not 0, or with no online profile login name (0x5E9B30); then both web queries must succeed before the weather is taken (0x5E9C33, 0x5E9C43) and the turn's update is switched on (+0xCF = 1, 0x5E9C82, its only writer outside the developer keys). The files the game ships (`Scripts/weatherinfo.lhw`, `Scripts/country.lhw`) are only read on that path
- The real weather is set up once as a game starts, never in a multiplayer game: as above
- It needs the weather preset file `Scripts/weatherinfo.lhw`; without it nothing happens: as above
- With no internet connection found at start-up, no query is made: as above
- There is no in-game option screen for it; it is set up outside the game: as above
- Country codes are named from the game's list of 242 countries, one "code=Name" per line (for example de=Germany, gb=Great Britain, uk=United Kingdom, us=United States): `Scripts/country.lhw`, read only for the online place
- The days fetched are kept in a list and written to `weather.dat` in the player's profile folder: written after a fetch, never read back (the reader 0x5E91C0 is unreachable)
- The weather is fetched only once per game start, not refreshed while playing: as above
- Once a turn, while it is on and outside multiplayer, the game checks whether new real weather is waiting: the turn's update (0x5E9CA0) runs only after a successful fetch
- The first time, the land's own background weather is kept so it can be put back; the background temperature becomes the real temperature and the overcast the real visibility: only after a fetch
- The weather type then picks one of 13 presets (types 0 to 12) from `weatherinfo.lhw`, each record 112 bytes; a preset can set, add to or take away from the overcast, snow cover, snowfall, rain and the two winds, each capped at 100, and can make a storm: only after a fetch. The types: 0 and 1 nothing; 2 a storm with rain 40; 3 snowfall 50 and snow cover 50; 4 a storm with no rain; 5 rain 100; 6 rain 50 and 10 more overcast; 7 rain, snowfall and snow cover 30; 8 a storm with no rain or snow; 9 rain 40; 10 snowfall and snow cover 100, wind 1 each way and 100 more overcast; 11 snowfall and snow cover 100; 12 overcast set to 2
- A message joins the game's message list naming the place, "City (Country)" or "City (Area,Country)", with one of nine weather pictures chosen from the type (1 → 0; 8 and 12 → 2; 9 → 3; 2 and 6 → 4; 3 → 5; 11 → 6; 7 → 7; 5 and 10 → 8; others → 1): only after a fetch
- Turning it off puts the kept background weather back and removes the storm it made: only after a fetch (the off switch is a developer key)
- A replacement weather source (a public weather service, or the player's own settings): not in the original; openblack has no network code
- Reading the 13 presets from `weatherinfo.lhw`: only on the online path, as above

#### pc_integration/villager_names_from_contacts

From [Villagers named after the player's contacts](progress/pc_integration/villager_names_from_contacts.md).

- Outlook Express is not supported, only full Outlook or POP3: as the readme states; openblack has no mail reading at all
- Outlook must be in "Internet Only" mode; in groupware (Exchange) mode it may refuse to give the names or mail: a limit of Outlook itself, from the readme
- The player must have registered on the game's web site and logged in to the online game at least once: the readme lists it as required; the game only starts its mail system when the online check succeeds and at least one online player profile exists; the online service is gone
- With Outlook, the names come from Outlook's contacts through a small helper library shipped with the game (`outlookdll.dll`), which drives Outlook itself: uses Outlook's own automation; Windows and Outlook only
- On start-up the game notes the time; only mail received after it is ever shown, so old mail left on the server is not repeated each session: relies on the mail headers' times; the readme notes mail with unreadable times or odd time zones is dropped
- Mail older than the game's start-up time, or whose time cannot be read, is not shown, and a line saying so is logged
- With Outlook, all folders holding new mail are scanned, which can take minutes for big folders: from the readme

## openblack

None of this is ported as behaviour of the game, by that rule; where the original has no X, openblack has none either.
The rule's exceptions are openblack's debug controls, off by default, and the 🎯 chosen differences
([the list](reconstruction.md#what-is-deliberately-different--chosen-differences)); the lines above that touch one
say so. Where openblack keeps code for such behaviour that never runs or never shows in play (such as the sea's
drift, three spirit remarks and the loose wave reader), its line says so too. One line still names something
openblack does in play that the original does not: it refuses to hold a sleeping creature
([creature/learning_from_feedback](#creaturelearning_from_feedback)).
The rows that point to a missing engine subsystem (saved games, multiplayer) stay pending in their files.

## Pending

Nothing.
