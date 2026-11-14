# Not in the original

## Summary

The behaviour listed here is **not part of the game as it plays**: written for it but never run, cut before
release, served by online services that are gone, a developer tool, or a guess an earlier tracker made that the
original did not bear out. By the user's rule of 2026-10-21 it is not progress, so it has no row in the
[progress files](progress/README.md); this page keeps what was found, so that nobody adds the row back. Before
adding a row, look here first. What the original does only with a force-feedback device is not listed here: it keeps
its rows with the `device` status, out of the score (user, 2026-10-27).

## The original

Grouped by the progress domain it was read for. Each line is the behaviour, then why the game does not do it.

### World

#### terrain/footpaths

From [Footpaths and roads](progress/terrain/footpaths.md).

- Footpaths are drawn for debugging as lines: openblack-only (`RenderingSystemCommon.cpp`, the Footpaths switch of the debug bar's View menu)
- A villager without a farm becoming a fisherman and walking to a fish farm's spot along the footpaths: the overload `Villager::VillagerBecomesFisherman()` 0x75B510 (its footpath walk at 0x75B540) has no caller, and no pointer to it is in the exe (checked 2026-11-07: `xref.py callers` and `refs` 0, no dword 0x0075B510 in the file); the fisherman's footpath walks come from the overload with a farm 0x75B560, which openblack ports ([routes](../beings/routes.md#pending))

#### terrain/land_height

From [Land height and mountains](progress/terrain/land_height.md).

- openblack's flat test land, with a lake, shallows, a bank and patches of sand and snow: openblack-only, the testbed's land (`src/3D/FlatLand.h`), a debug tool; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)

#### terrain/rivers

From [Rivers and lakes](progress/terrain/rivers.md).

- Rivers are drawn for debugging as lines between their points: openblack-only (`RenderingSystemCommon::PrepareDraw`, its streams option: the Streams switch of the debug bar's View menu)

#### sky/mist

From [Mist and haze](progress/sky/mist.md).

- Creatures notice mists as objects in the world: Not in the original: a mist is kept only on the game's list of mists, never linked into a map cell, and a creature looks at the things linked into the map cells about it; a mist has a belief kind of its own only for one a script hands it ([haze-and-mist.md](../world/haze-and-mist.md#map-mist-create_mist), [mind.md](../creature/mind.md))

#### sky/sun_and_moon

From [Sun and moon](progress/sky/sun_and_moon.md).

- On openblack's flat test land the sun and moon behave as on any land: openblack-only, the testbed's land (`src/3D/FlatLand.h`); see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)

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

- The weather debug window shows the climates and storms, forces storms and strikes lightning: openblack-only, `src/Debug/Weather.cpp`; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)
- The player gives their country and town, from the game's country list: the place came from the player's account on Lionhead's web site (`LHInetWeather::GetLocationDataForUser` 0x5E8410, an HTTP query), which is gone; see [pc_integration/real_weather](#pc_integrationreal_weather)
- Outside multiplayer, the game asks a weather server for the day's weather where the player lives: `LHInetWeather::QueryWeather` 0x5E88B0 over HTTP; the service is gone and openblack has no network code
- The real weather is turned into a preset that sets the land's rain, snow, temperature and wind: only after both queries succeed (`InitForGame` 0x5E98B0 returns at 0x5E9C33 / 0x5E9C43 otherwise), so never without the service
- The last weather fetched is kept on disk for when there is no connection: the reply is written to `weather.dat` (fn_005E9130, `wb`, from the reply's reader fn_005E8C60), but the game never reads it back: its reader (0x5E91C0, `rb`) has no caller and no pointer to it anywhere in the exe

#### weather/storms

From [Storms and thunder](progress/weather/storms.md).

- A storm can be forced over the whole island, and storms ended, from the debug window: openblack-only, `src/Debug/Weather.cpp`; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)

#### nature/fireflies

From [Fireflies](progress/nature/fireflies.md).

- The land editor writes each firefly into the land's script as a placement at its spot: openblack has no land editor; its in-game editor (Debug > Editor, a debug tool) places things in the running world and writes no land script

#### nature/plants_and_flowers

From [Plants and flowers](progress/nature/plants_and_flowers.md).

- The creature eats plants and stomps on flowers: Not in the original (checked 2026-10-26): the `Flowers` class has no
  code of its own but its creation, its 3D type, its save type and its debug text (`Flowers::CallVirtualFunctionsForCreation`
  0x527A30, `SaveObject` 0x527A80), no code casts a thing to `Flowers`, and its info row adds only a growth speed to the
  feature's; to the creature it is a feature among the others, which it may look over or act on as one, never food
  and never trodden down. openblack has no such code either.

#### nature/toys

From [Toys](progress/nature/toys.md).

- Black & White's model list has no separate bowling pin or toy-only bowling model: its skittle and toy bowling ball are the only bowling models (the bowling lane end and its model belong to Creature Isle): nothing to make
- The "cheat box" model is a plain box 180 wide, 180 deep and 400 tall, all of it below the ground, with eight corners: not used by any shipped land; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- A bowling game: ten villagers stand as pins in a triangle and a cow is the ball; each knocked-over villager scores, the game slows to half speed while anything is down, and all ten plus the cow make a strike ("You got a strike!"): a script source never compiled into the game's challenges; see [../story/minigames.md](progress/story/minigames.md)
- Cow bowling, the same game with its own wording: same; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- A bowling lane end, a bowling model and a skittle knock sound: only in Creature Isle's tables
- The Chimp Posse's die, teddy and ball, made at half size: never started by the game; see [../story/silver_scrolls/chimp_posse.md](#storysilver_scrollschimp_posse)
- The "cheat box" model: see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)

#### nature/trees

From [Trees](progress/nature/trees.md).

- Villagers sleeping under trees or sheltering by them: the villager state table (`src/Enums.h`, the original's list of states) has sleeping only at home and in a tent (from the worship site too), with no state under or by a tree; the row was an earlier tracker's guess, already marked as not found in the original, and openblack has no such code
- Footprints worn round trees on busy routes: the worn paths and roads are part of the land's painted texture ([footpaths](progress/terrain/footpaths.md)) and nothing wears them by use; also an earlier guess, already marked as not found in the original

#### nature/rocks_splitting_and_heat

From [Rocks splitting and heat](progress/nature/rocks_splitting_and_heat.md).

- The creature setting something alight with a burning thing it can lift (row 283 SetFireToObject, builder 0x4BF140) and its own "start a fire" action (row 73 StartFire, builder 0x4A41D0, which only ends the action as failed: bw1-decomp `CreatureAgenda::ConstructSubActionsForStartFire`): no desire or compassion-for-town table of `Scripts/info.dat` lists row 73 or 283, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is one of them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, no shipped script forces them (`refs/chl`), and their builders are referenced only from the action table's initialiser (read 2026-11-08) ([building/building_by_creature](#buildingbuilding_by_creature)).

#### resources/poison_and_mushrooms

From [Poison and mushrooms](progress/resources/poison_and_mushrooms.md).

- The unused "Food for Thought" challenge ("The Nomads") checks whether the player hands hungry nomads poisoned food, and kills them all if so: not shipped; see [../easter_eggs/unused_content.md](progress/easter_eggs/unused_content.md)
- The game's text still names a "Poisoned Food Miracle" ("Poisoned Food. Boss, this will cause a whole…" (`HELP_TEXT_QUESTION_MAGIC_TYPE_FOOD_PU_ONE_01`)) left over from before the extreme food miracle made speed-up food: leftover text, never shown for a working miracle
- The advisors' remark about a mushroom, a magic mushroom or a toadstool held in the hand ("That's gross. But not as gross as something…" (`HELP_TEXT_HAND_FUNGUS_01`), the evil advisor's "Special Mushroom."; the in-hand message sets, `HELP_SYSTEM_MESSAGE_SET_IN_HAND_*` 107-149, animals and seeds too): their only runner, `HelpSystem::ProcessObjectInHand` 0x5C9360, which reads the held thing's set from its info row (`Object::GetHandHelpMessageSet` 0x6385C0, info +0x60, and `GetHandHelpCondition` 0x6385D0), has no caller and no pointer to it in the frozen v1.42 exe (`xref.py callers`/`refs`; `HelpSystem::Process` 0x5C8FE0 calls the spirits, tooltips, bubble, conditions and banter only), the two getters are called only from it (vt +0x650 and +0x654: 3 and 1 sites, all inside it), and bw1-decomp's 1.20 has no caller either (`HelpSystemProcess.cpp:111`); so the sets' table rows stay as data and nothing says them (read 2026-11-14; see also [help_system](progress/interface/help_system.md#what-triggers-help))

#### resources/stores_and_piles

From [Stores and piles](progress/resources/stores_and_piles.md).

- Only food and wood exist; there is no ore: the game has no other resource
- A town's temporary pots placed by the land scripts: CREATE_TOWN_TEMPORARY_POTS is in no shipped land script (see engine/script_vm)
- A pot of info.dat's row 8 ("Food Pile"): nothing in the exe makes it and no shipped land script creates one; its
  model, the worship site's grain heap, lies about 52 m from its origin, so such a pot is drawn that far from where
  it stands. Only openblack's debug placings and testbed scenes make it ([row 8](../world/objects.md#row-8-food-pile))

#### physics/object_dynamics

From [Object dynamics](progress/physics/object_dynamics.md).

- A villager clearing ground pushes the nearest obstacle within 2 m with a force of its weight × 9.81 (the clear-area search before building, state 185 ARRIVE_AT_PUSH_OBJECT): the search keeps only objects whose IsPushable is true, and every Object vtable of the frozen exe (134) points that slot (+0x814) at `Object::IsPushable` 0x402AE0, `xor eax, eax; ret`, so `Villager::CheckForClearArea` 0x7590A0 always returns 0 and `Object::PushObject` 0x639640 (its one call, 0x75917E) never runs ([physics.md](../physics/physics.md#manager), [villager-jobs.md](../beings/villager-jobs.md)). openblack keeps `DynamicsSystem::PushObject` with its test (`ParticleCarriedObjectsTest.TakeAndReleaseRoundTrip`) and no caller. The football's kick is a separate function, a row of [football.md](progress/town/football.md)

### Beings

#### creature/creature_tattoos

From [Creature tattoos](progress/creature/creature_tattoos.md).

- No challenge script of the five lands reads or changes tattoos: nothing to do
- Creature Isle's tattoo types (the Brotherhood, the super gods, fighters) for its scripts: Creature Isle; the values are listed in `src/ScriptHeaders/ScriptEnums.h` only
- Uploading a creature to the online creature database sends its tattoos with it: the online creature service no longer exists (already listed: "Creatures can be uploaded and downloaded online")
- The tattoo editor names each place on the body (back, head, chest, bottom, arms, hands): not in the original (checked 2026-11-05): the editor's functions (`TattooEditor::Init` 0x542510, `UpdateC3D` 0x542C50, `ControlCallback` 0x543440) read only the drag help, OK, Cancel and the network warning texts; the place names are only openblack's cave readout ([tattoos.md](../creature/tattoos.md#the-colour-of-the-symbols))
- A sound plays as a tattoo is put on, and another as one is lifted off: not in the original (checked 2026-11-05): the drop (`ControlCallback` 0x543440) and the lift (`UpdateC3D` 0x542C50) call no sound; the dialog's one click noise (`RussClickNoise` 0x4082F0) plays only for a press and release over the same control ([tattoos.md](../creature/tattoos.md#the-colour-of-the-symbols))
- Lifting a tattoo off sets the colour pickers to its colour: not in the original (checked 2026-11-05): the lift (`UpdateC3D` 0x542C50, from 0x5430C1) tints the sixteen symbols with the tattoo's colour and leaves the pickers as they were ([tattoos.md](../creature/tattoos.md#the-colour-of-the-symbols))

#### creature/decision_making

From [Decision making](progress/creature/decision_making.md).

- Multiplayer locked selection of a creature across the network: network play is not supported; see [../multiplayer/](progress/multiplayer/)
- A creature in a script's flock or dance, played to by its state as a member (FLOCK_ATTACH's `AttachToId` 0x6EF63A, DANCE_CREATE 0x6F02F4 and FLOCK_DISBAND 0x6EFE84 hand any `Living` to the per-thing script state 0x6F82E0, so a creature member would play its script play): no shipped script attaches a creature to a flock or a dance (`refs/chl`, 2026-11-02: the attaches are villagers, the Crusaders, and FreeTheCreature's wolf, made by `create 6 2`, an animal), so what such a creature does besides the play is not read; openblack's script containers take only villagers and animals ([script-control.md](../creature/script-control.md#openblack))
- When free, the creature plans every desire at once before falling back on idling — the original plans its desires in passes, two a turn (`NextGoals`, plans-and-needs.md); openblack's every-desire planning was never called (`PlanCreature`'s `everyDesire` was passed true nowhere), so the row was a guess the exe does not bear out; the switch was removed on 2026-10-22 (7a52bd21f).
- A script resets the count of how many times a creature has done an action, or reads it through the second getter (CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION, CREATURE_GET_NUM_TIMES_ACTION_PERFORMED): `GScript::CreatureInitialiseNumTimesPerformedAction` 0x6F42B0 and `GScript::CreatureGetNumTimesPerformedAction` 0x6F4340 are reached only through the native table (checked 2026-11-07: `xref.py callers` 0 calls, its one reference is its entry in the script native table, written by the table's static initialiser at 0x7015C4 and 0x701603; no statement in `refs/chl` and none in the coverage tool's 514 shipped challenge scripts); the scripts read the count through GET_ACTION_COUNT ([Decision making](progress/creature/decision_making.md))
- A script teaches a creature everything but some actions (CREATURE_LEARN_EVERYTHING_EXCLUDING): `GScript::CreatureLearnAllExcluding` 0x6F4EF0 is reached only through the native table (checked 2026-11-07: `xref.py callers` 0 calls, its one reference is its entry in the script native table, written by the table's static initialiser at 0x704D57; no statement in `refs/chl` and none in the coverage tool's 514 shipped challenge scripts)
- Messing up an action for lack of skill (a throw, fishing, a dance with villagers, making a fire, impressing villagers, raising a totem): the skill at an action is read by nothing that changes play ([beliefs_and_opinions](#creaturebeliefs_and_opinions)), and the only mess-up that runs is a miracle tried before it has been seen enough, which fizzles by its sightings, not its skill ([creature casting](progress/creature/creature_casting.md#which-miracles-it-may-try)); the help telling of the other six is never raised ([lessons_and_help](#creaturelessons_and_help); read 2026-11-14)

#### creature/desires

From [Desires](progress/creature/desires.md).

- Being content, being dissatisfied, laziness, curiosity, wanting to be sick, illness and wanting to look around fed by what the creature does or sees: sources 4, 8, 23, 20, 29, 46 and 59 have no reader in the source table (bw1-decomp `CreatureMentalDesireSource.cpp:39`; the exe's table at 0xC8D478, 0x20 a source, filled by its static initialiser), and none of the exe's 21 `CreatureDesires::ChangeSource` 0x4DE730 calls nor its one `SetSource` 0x4DE6C0 call outside `MakeAllSourcesMaximum` passes them: the fixed ones push 18 and 9 (a damaging effect 0x478908/0x478919, a blow 0x479B4C/0x479B61, a slap 0x4E0AEC/0x4E0AF6), 12 (a toy seen 0x4D7F43, a stroke 0x4E0B30), 60 (0x4D7F68), 3 (a town 0x4D8059), 54 (0x4E0A64, 0x4EA8AD), 38 and 2 (a stroke 0x4E0B3A, 0x4E0B44), 19 (0x4F3229) and 48 (0x4F3B7C, 0x50A010, 0x50A17A) and set 43 (0x4E08F4); `UpdateSourcesFromBelief` 0x4DE800 pushes only 1, 3 and 7; the noticed deed (0x4EA898) the first source of its desire; and `EmpathiseWithPlayer` 0x4D2800 is reached only from `GPlayer::MakeCreatureEmpathiseWithPlayer` 0x64B5B0, which nothing calls. They change only by their starting value, their fade, the mind file and `MakeAllSourcesMaximum` 0x4DE780 (a desire made dominant by `SetCheatDominantDesire` 0x4DCE20 or the mood and need spells, CREATURE_SET_DESIRE_VALUE at 1), which openblack does (`creature_desires::MakeSourcesFull`) (read 2026-11-14)
- Sadness from losing a fight or a friend: the sadness source (48) is pushed only by a death come across (0x4F3B7C, by 1), a fizzled miracle (0x50A010) and 0x50A17A in sub_50A020, which nothing calls or points to; otherwise it changes only as the line above says (read 2026-11-14)
- A script turns a desire on and makes it as strong as it gets: not in the original (checked 2026-11-05): the code that does it (0x6F3B00, which turns the desire on and sets it at its maximum with all its sources full through 0x4F6950 and `CreatureDesires::MakeDesireMax` 0x4DC970) lies after `GScript::CreatureTurnOffAllDesires` 0x6F3A90 but nothing in the exe refers to its address, so no script native reaches it; the script natives that set desires are CREATURE_SET_DESIRE_VALUE, _ACTIVATED and _MAXIMUM ([script-control.md](../creature/script-control.md))
- A script asks whether a creature's desire is a given one (CREATURE_DESIRE_IS): `GScript::DesireIs` 0x6FA2C0 is reached only through the native table (checked 2026-11-07: `xref.py callers` 0 calls, its one reference is its entry in the script native table, written by the table's static initialiser at 0x701A26; no statement in `refs/chl` and none in the coverage tool's 514 shipped challenge scripts)
- The strongest physical desire (hunger, thirst, tiredness, poo) found for emergencies or anything else: `CreatureDesires::FindStrongestPhysicalDesireValue` 0x4DC880 (bw1-decomp CreatureMentalDesire.cpp:346) is called only by `Creature::LeashForceCreatureToActOnHandObject` 0x4CECF0, which has no caller and no reference, and by `Object::TellCreatureWhatToDoWhenAttachedToMe` 0x4CEE80, reached only through 0x4CEE70 from the tied leash's tug 0x4CEAB8, which only a packet nothing sends reaches (checked 2026-11-07: `xref.py callers` and `refs`; see the creature/leash lines); the emergencies a creature looks at are low life and fire ([Desires](progress/creature/desires.md))

#### creature/development_phases

From [Development phases](progress/creature/development_phases.md).

- The testbed can set a creature's stage: openblack-only debug tool (the testbed's "set stage" command, `src/Debug/TestbedScenarioRunner.cpp`); the debug spawner sets the stage too (next line)
- The stage shown and changed in the debug spawner: openblack-only debug tool (Debug > Windows > Creature Spawner, "Set stage"); the game's stage moves are scripts, packets, the mind file and the computer creatures
- A stage moving on by itself once its criteria and time are met: the stage tests' dispatcher 0x4C59A0 has no caller, no reference and no pointer in the exe's bytes, and the duration table 0xC843B0 is read only by the stage tests 0x4C5AB0, 0x4C5B50 and 0x4C5BB0, reached only through that dispatcher (read 2026-11-05)
- A young creature that can't pick things up or run until it has learnt to: none of the 27 functions that read the development phase is a pick-up or locomotion test (read 2026-11-05)
- First lessons given to the computer gods' creatures: 0x65BCE0 (every desire off, stages 0 to 13, four actions learnt) has no caller, reference or pointer; a computer creature gets its mind file and stage 13 (0x65BA40) (read 2026-11-05)
- The stages of meeting its guide (5), the guide explaining the history (7) and helping a town (11) as stages a creature lives in: no shipped script sets them (`refs/chl`: the 18 stage settings set 0 to 4, 6, 8, 9, 10, 12 and 13, CreatureDevelopment.txt and CreatureGuide.txt among them), and of the exe's 13 calls of `Creature::MoveToDevelopmentPhase` 0x4C59C0 (`xref.py callers`, read in the frozen exe 2026-11-07) each sets 13 or 0 (the five in `GPacket::ProcessPacket`: 13 at 0x63C5A0, 0x63CDDB and 0x63DA82, 0 at 0x63C5F7) or the script's stage (SET_CREATURE_DEV_STAGE, 0x6F4894), or steps through 0 to 12 at once with nothing between (the packet's loop at 0x63D9EE and `GGame::SetUnusedCitadelComputerPlayers` 0x555B62, each ending at 12; 0x65BCE0, which has no caller, 0 to 13), so no creature stays at 5, 7 or 11

#### creature/growth_and_size

From [Growth and size](progress/creature/growth_and_size.md).

- The creature's size as a value in the Creature Cave's attributes scroll: not in the original (checked 2026-11-05): `CreatureRoom::MakeCreatureScrollText` 0x788A10 adds, through `TempleRoom::AddText` 0x79A910 (whose ids are the game's text numbers less 4), the title, name, age, health, alignment, energy, strength, fatness, exhaustion, dehydration, warmth, illness, spells received, amount of poo and the seven tallies; there is no size text among the game's scroll texts (`InfoScript1.txt` 1162..1184), and the scroll leaves out the itchiness (1172) and development stage (1181) texts it has ([progress row](progress/creature/growth_and_size.md#size-limits-and-how-it-is-shown))
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
- Watching villagers play or eat stirring its wish to play or eat: sources 13 and 15 have no getter in the source table (bw1-decomp `CreatureMentalDesireSource.cpp:49`), none of the 21 `ChangeSource`/`SetSource` calls passes them, and `UpdateSourcesFromBelief` 0x4DE800 feeds only sources 1, 3 and 7; only `MakeAllSourcesMaximum` 0x4DE780 and the mind file write them (read 2026-11-05)
- Seeing an action done updating its desires as if it had done it: `ConsiderLearningAction` 0x4E2380 only counts sightings; only its god's deeds stir sources (0x4EA898, 0x4EA8AD) (read 2026-11-05)

#### creature/lessons_and_help

From [Lessons and creature help](progress/creature/lessons_and_help.md).

- The creature help telling of a missed throw, and of fishing, dancing with villagers, making a fire, impressing villagers or raising a totem gone wrong (MessedUpAction 0 and 2 to 6): not in the original (checked 2026-11-05): the help kind (0x27) is raised only at the fizzle's two sites (0x4F7B83, 0x4F7FBA, parameter 1) and in sub_50A020 (0x50A196, parameter mind +0x21F0), which nothing calls, jumps to or holds the address of (no call site, no dword 0x50A020 anywhere in the exe), and nothing writes mind +0x21F0; the texts stay unused ([progress rows](progress/creature/lessons_and_help.md#things-it-messed-up))
- The creature's thoughts, the last things it learnt, kept in its mind: not in the original's play (checked 2026-11-05): `CreatureMental::UpdateLessonText` 0x4E1670 writes "I've learnt to ..." lines into mind +0x16D04 (256 bytes a line) for its lessons, and their only reader is the debug dump `Creature::DisplayStatistics` ("Last Lesson: %s", 0x476C0D); openblack keeps them for its debug readout only (`creature_mind_model::Think`, `src/Debug/CreatureSpawnerMind.cpp`) ([progress row](progress/creature/lessons_and_help.md#signs-the-creature-gives)); the learning page's row "Each lesson is put into words as the creature's thought" removed 2026-11-14 for the same reason (the advisor's spoken lessons stay on [lessons_and_help.md](progress/creature/lessons_and_help.md))
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
- The prompt on how to take the leash off (Unleash): its condition (0x508EF0) always answers no, and no instruction of the exe stores to its known flag (Creature +0x1A4; a sweep of every function's stores at that offset finds only CreatureReceiveSpell's constructor and FelledTree::Create, 2026-11-14): never told, never marked; openblack's condition is false as well (`creature_help::TutorialApplies`, test `CreatureHelp.PromptsTakeTurnsOldestFirst`); its progress row removed 2026-11-14 (this bullet already stands in the section; only the date and the store check are new)
- A creature help message held back because the creature's help level is below its kind's: `SendCandidateHelpScript` 0x4C9FE0 drops a message when the creature's level (help state +0x94, Creature +0x21C) is below its kind's (table 0x9D9CA8, levels 1 to 4), but the help state's constructor 0x4C9C30 sets that level to 10, and no instruction of the exe stores to Creature +0x21C (a sweep of every function's stores at that offset finds only GGame::GoInsideCitadel's stack, CreatureRoom and PathCreator, 2026-11-14; xref field Creature 0x21C: the one read at 0x4C9FF8), so the drop never happens; openblack's `creature_help::Decide` doesn't read the kinds' levels, which comes to the same thing (checked 2026-11-14)
- The creature help naming the source behind its current desire (CurrentSource, kind 0x1D, text 401 + the dominant source): raised only by `Creature::ConsiderGivingHelpOnCurrentAction` 0x4F14E0 (at 0x4F1522), and xref callers finds 0 calls, jumps, vtable slots or data pointers to it; of the help request's 46 call sites, every other one passes another kind or a register holding a stacked, feedback or stage kind (CurrentSource doesn't stack): never raised (checked 2026-11-14)
- The creature help telling that nice or nasty music is playing (Miscellaneous 0 at 0x47EBB9, 1 at 0x47EEC2), and music changing its mood or its mood the music: raised only by `MakeMyCreatureReactToNiceMusic` 0x47E760 / `MakeMyCreatureReactToNastyMusic` 0x47EC10. xref callers: their only caller is `MakeMyCreatureReactToMusic` 0x47E060, whose only caller is `Creature::UpdateMoodsBasedOnMusic` 0x47B3F0, packet 0x58's handler. That packet comes from the music-mood add-on (`CreatureMusicMoodUpdateCallback` 0x47B310: callers none, reached as the add-on's callback; `GatheringBox::MusicMoodActive` needs the BWFeatureKey add-ons, single player and the MP3 player library). The chosen football difference doesn't turn it on. Not run in the original's play (checked 2026-11-14; see the creature/idle_behaviour bullet 'Music moving its mood', read 2026-11-05); its three progress rows removed

#### creature/saves_and_files

From [Creature saves and files](progress/creature/saves_and_files.md).

- The creature upload tool sends a creature file to the game's website under the player's account: the service no longer exists
- Selecting another player's creature waits for the network to agree: covered under network play
- The game fixing or refusing a loaded creature mind that fails its check: not in the original (checked 2026-11-14). `Creature::CheckValidation` 0x4E8330 reads the file's two encrypted names (the name it was saved as and the profile) and, for the local player's creature, compares the profile with `LHNetGetCurrentUsedProfile`. All three callers drop its answer and load the mind anyway: `ResolveLoad` 0x4E6CCE, `LoadFullyQualified` 0x4E82B8 (from version 0x15 only) and `LoadMindIntoPreexistingCreature` 0x4E8467 each go straight to `CreatureMental::LoadMind` 0x4E84A0 without testing eax (bw1-decomp CreatureMentalSaveAndLoad.cpp:405, :737, :779 the same). The only routine that would delete a bad creature's files (`CheckThatMindAndBodyLoadedCorrectly` 0x47DF50 to `DeleteMyCreatureFiles` 0x47E000) has no caller (xref callers: 0, no pointer in the exe). openblack's reader reads the two names (creaturemind `EncryptedName`, MindFile.cpp) and goes on, as the original ([progress row](progress/creature/saves_and_files.md))

#### creature/species_choice

From [Species and choosing a creature](progress/creature/species_choice.md).

- Chicken and crocodile, listed by the game but with no bodies in its data: never playable in the five lands
- Species added by Creature Isle: Creature Isle
- The hand's tooltip naming the creature's species: over a creature the tooltip is the single text 0xE85, sent by `ToolTips::SubmitToolTips` in `GInterface::CalculateCanSelectLock` 0x5D7190 (read 2026-11-05)

#### villager/daily_routine

From [Villager daily routine](progress/villager/daily_routine.md).

- Housewives fetch food from the storage pit, make dinner, serve it and clear it away: the housewife's states are TODO rows. Our wiki differs: the housewife's day (fetching food, cooking, serving and clearing dinner, housework, gossip) is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births)); see [villager/jobs](#villagerjobs)
- Villagers sit down to dinner at home, waiting for it when the housewife is late: `SITS_DOWN_TO_DINNER` and `WAIT_FOR_DINNER` are TODO rows. Our wiki differs: the housewife's meal call, which makes villagers wait for dinner, is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))
- Idle villagers noticing interesting things nearby (animals, trees, rocks, pots, the ball, fields, abodes, other villagers) and going to interact: the CHECK_INTERACT_WITH_* states 170-181 are reached only from a disciple's landing inspection (FindCloseObjectsForInteract 0x756E80 is called only by Villager::Landed and Villager::InHand; SetupInspectObject 0x76AAB0 only by Landed; docs/wiki/beings/villager-hand-and-death.md, LANDED); row removed from villager/daily_routine.md.
- Homeless villagers eating their dinner outside: `HomelessEatDinner` 0x75C0F0 (state 145) is empty, its only reference is the state table (xref), nothing in the exe sets state 145 and the shipped scripts set only states 4, 15, 27, 199 and 244 (read 2026-11-05)
- A villager lying weak on the ground: `WeakOnGround` 0x768A10 (state 198) is the same case, only the state table refers to it and nothing sets 198 (read 2026-11-05)

#### villager/disciples

From [Disciples](progress/villager/disciples.md).

- Protection disciple: in the table but never made in the game

#### villager/jobs

From [Villager jobs](progress/villager/jobs.md).

- Housewives fetch food from the storage pit, gossip round it, and cook dinner at home: the housewife's states are TODO rows. Our wiki differs: the housewife's day (states 100 to 109, with the storage pit, the gossip and the dinner) is dead code in this version of the original ([villagers](../beings/villager-life.md#pregnancy-and-births))
- Housewives do housework when nothing else needs doing: `HOUSEWIFE_DOES_HOUSEWORK` is a TODO row; part of the housewife's day, dead code in the original (row above)
- Shepherds fetch strays: SHEPHERD_FETCH_STRAY (98) and SHEPHERD_MOVE_FLOCK_TO_FOOD (93) return 1 and do nothing (`Villager::ShepherdFetchStray` 0x769460, `ShepherdMoveFlockToFood` 0x7690D0), and no code enters them; the shepherd's wait for its flock is in ([villagers](../beings/villager-jobs.md#shepherds))
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
- Villagers stopping to gossip: GOSSIPS (169) is entered only from `CheckInteractWithVillager` 0x757760 (state 179), whose only source is `GetDiscipleInteractState` 0x756E20, with no caller, vtable or data pointer (xref) (read 2026-11-05)

#### villager/special_villagers

From [Special villagers](progress/villager/special_villagers.md).

- openblack has a debug overlay that labels villagers with numbers and states: `Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`, which labels a named villager with its name; a debug tool with its own switch (Villager Names > Overlay), apart from the game's names display, see [debug windows](#debugdebug_windows)
- New e-mail is announced by a named villager in its bubble ("Hey! I've sent you an e-mail about ..." (`HELP_TEXT_DIALOG_ADDITION_11`) when the sender is that villager's contact, otherwise "Hey! ... has sent you an e-mail about ..." (`HELP_TEXT_DIALOG_ADDITION_67`)): the game's mail link is gone; see [../pc_integration/villager_names_from_contacts.md](progress/pc_integration/villager_names_from_contacts.md)
- A villager marked to always show its name shows it while the names display is off: `SpecialVillager::DrawVillagerInfo` 0x71F240 reads the mark (status bit 0x100, byte +0xB5 & 1, 0x71F28C), but no writer of it is found in the exe, so no name shows by it (read 2026-11-05; [the villagers' names](../interface/screen-and-readouts.md#the-villagers-names))
- A script can ask for a named villager of a given town: the only caller of the special villager's creation (0x71F1A0) is the plain villager creation (0x74FC13), which always passes no special row (0x74FC0D); no script command makes or asks for a named villager (xref callers)
- A named villager speaks in a speech bubble over its head (one speaker at a time; 250 by 175, text size 19, up to 1023 characters; 1.5 above the head, closing after 6 s or when the villager dies, is deleted or hidden; shown instead of its name): the bubble's only caller with a text is the gathering box's incoming message (GatheringInterface, the e-mail and ping-server messages, gone); the others close it (ToBeDeleted, Draw) and one has no caller (0x71F090) (xref callers of 0x71EF70)

#### villager/tools_and_carried_items

From [Villager tools and carried items](progress/villager/tools_and_carried_items.md).

- Kicks in football each play the kick sound: the villagers' kicks (`Villager::FootballAttackerShootProcess` 0x75D2D0 and the other kick steps, bw1-decomp `VillagerFootball.cpp`) and `Ball::KickBallAtDestination` 0x435C40 call no sound; a ball's knocks play its collision sound ([football](progress/town/football.md) "A ball's knocks play the collision sound", read 2026-11-05; moved 2026-11-10)
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

- The vulture: its kind (17) makes nothing, as the animal factory's switch has no case for it (fn_00419E00, with
  the goat, the zebra, the citadel dove and bat and the puzzle animals), so its banking constants (2.0 s, 0.5 rad,
  [animals](../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)) are never used, and no land script
  asks for one (kinds 0-16 only, checked 2026-10-26). openblack makes none either (`AnimalArchetype`'s class of kind
  17 is none), and leaves it out of the birds.
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
- In the cup final, a player whose health runs out is replaced by a sub, with a cry, the camera following him and the advisor's line ("Hard tackle. Not a foul, though, I say." (`HELP_TEXT_SOCCER_MATCH_08`)): as above
- The cup final lasts 300 s or until one side loses all seven. With 30 s left the first Norse sub is set on fire and killed, so the Celts "cheat": as above. No winner is declared; everyone is sent home
- The sound list names a referee's long and short whistle, a huge crowd and a goal sound, but no recordings by those names ship: no football music or chants exist
- The game counts no football statistics: the score lives only in the pitch and its debug text
- The match's score shown as "Home (%d) v Away (%d)" (0xBEA110) and the play state as "Football State: %s" (0xBEA128): `Football::Process` 0x533E40 hands them to `GDebug::SetMessage` 0x511DA0 (0x533F1D, 0x533F43), the developer text that is written only while its debug channel is on (0x511DB1); besides it the two tally bytes (+0x214 home, +0x215 away) are only zeroed (`SetToZero` 0x531CE0), counted (`ProcessBall` 0x531F40 / 0x531F5D), saved and loaded (`xref.py field Football 0x214` / `0x215`, read 2026-11-08), so no player ever sees the score ([football.md](progress/town/football.md))
- A celebrating player's own clip (one of four by a draw of 100: impressed, won, won again, unimpressed) is only the Creature Isle build's: the function is in the exe (0x423DA0), but state 76 FOOTBALL_WON_GOAL's animation slot in the villager state table is empty (+0x2B20 in its static init 0x59D020), nothing calls it or holds its address, and bw1-decomp's GStates.cpp fills that slot only under `VERSION_BW1WCI`. The won goal takes its table clip with no draw; see [buildings.md](../world/buildings.md#the-football-pitch)
- A paused match (the pitch's play state 2, "Paused" in its debug names) never happens: the play state is only ever set to 0 (the constructor, MakeFunctional, StopFootballGame, Process), 1 (Process, and SetPlayState 0x534860 from its one call 0x75F36E) or 3 (GoalHasBeenScored 0x531940), so the watchers' unimpressed clip for it (0xE8 in 0x423C80) is never chosen. openblack's play state has no paused value; see [buildings.md](../world/buildings.md#the-football-pitch)
- The creature kicks or throws the ball at the goal as an attacker, clears it as a defender or stomps on it (actions
  220 AttackerThrowBallAtGoal, 221 AttackerKickBallAtGoal, 223 DefenderClearBall, 222 DefenderStompOnBall), and
  celebrates its side's goal or commiserates the other's (226 CelebrateGoal, 227 CommiserateGoal, "creatures of
  even-numbered players are on the home side"): no row of the desire action table nor of the town table lists them
  (info.dat, read 2026-11-03: Play lists only 224 and 225 of the eight), and no shipped script forces one; openblack
  has no executor for them (test `CreatureKindActions.ACreatureJoinsAMatchByThrowingItsBallAtTheGoalOrFoulingTheOtherSide`).
  The two it does are in [Football](progress/town/football.md#the-player-and-the-creature)
- Script commands for football: GET_FOOTBALL_PITCH (212), STOP_ALL_GAMES (213), ATTACH_TO_GAME (214),
  DETACH_FROM_GAME (215), DETACH_UNDEFINED_FROM_GAME (216), SET_ONLY_FOR_SCRIPTS (217), START_MATCH_WITH_REFEREE (218)
  and GAME_TEAM_SIZE (219): the shipped challenge script (`Scripts\Quests\challenge.chl`) calls none of them
  (`tools/checks/story/script_coverage.py`, 2026-11-03; no "football" string in it either); only the uncompiled cup
  final would. With them go what only they fill: the pitch kept for scripts, a match started with a chosen referee,
  and the cup final's slots on the pitch (+0xC8, +0xF0, +0x140). The natives stay openblack's stubs
- A debug view shows the match state and the score: developer only; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)
- An older pitch kind (with its own ball and place table) remains, but every villager hook it relies on is empty: no table or script makes one
- A "Miraculous soccer ball" miracle was cut: a creature-lesson line and a miracle-icon model remain: the shipped miracle list has none
- A match impressing its watchers through football's own impressive kind (8): the ball and football classes (Ball, Football, PBall, PFootball) keep `GameThingWithPos::GetImpressiveType` 0x56FF70 (type 27), the creature's three football actions carry 24 in info.dat, and a match's only belief is `StopFootballGame`'s end gift (0x531FD0); the magic infos were not scanned for an 8 (read 2026-11-05)

#### town/town_desires

From [Town desires](progress/town/town_desires.md).

- The flags hang still: their meshes are static, with no ripple (raffclar PR #275): the row once said they ripple in the wind; vanilla's flag meshes have no animation
- `SET_TOWN_DESIRE_BOOST` with a negative desire: the original writes the boost before the town's desire array (its test is only desire < 17, signed, 0x6FE6C3..0x6FE6EF), but no shipped script passes one: the ten calls in the challenge scripts (Land 1's setup, Follow Us, Land 4's start and setup) give desires 0, 5, 6, 8, 9 and 16 (`refs/chl/*.txt`, read 2026-11-04). openblack writes nothing for it, and never meets it

#### building/abodes

From [Abodes](progress/building/abodes.md).

- Abodes casting a shadow at night by lantern light: an abode answers that it casts one (`Abode::IsCastShadowAtNight`
  0x401720), but nothing ever asks any object (read 2026-11-14, [rendering/shadows](#renderingshadows))

- Low-detail versions of buildings far away: openblack always draws the most detailed meshes. Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../rendering/meshes-and-lod.md#villager-blobs-object-reflections-and-lod)); See [../rendering/](progress/rendering/)

- The abode working out the food its family needs for dinner, and whether it holds enough (`Abode::CalculateFoodNeededForDinner` 0x4045D0, `Abode::IsEnoughFoodForDinner` 0x404600): their only callers are the housewife's states (`CheckNeededForHouseWork`, `HousewifeGotoStoragePit`, `HousewifeArrivesAtStoragePit`, `HousewifeMakeDinner`), which nothing enters (no code sets states 100 to 109 but those states themselves, no disciple starts in them, no state row of info.dat names them and no shipped script sets them; checked 2026-11-08), so the housewife's day is dead code ([villagers](../beings/villager-life.md#pregnancy-and-births), [villager/jobs](#villagerjobs)).

#### building/civic_buildings

From [Civic buildings](progress/building/civic_buildings.md).

- Walls (planned wall sections): no land or playground script uses the command
- Creature pens: no land or playground script uses the command

#### building/workshop_and_scaffolds

From [Workshop and scaffolds](progress/building/workshop_and_scaffolds.md).

- An "old scaffold" object exists in the tables but nothing places it: no land or script uses it
- Villagers never carry scaffolds: the game leaves that empty: See [../villager/tools_and_carried_items.md](progress/villager/tools_and_carried_items.md)

#### building/fields_and_fish_farms

From [Fields and fish farms](progress/building/fields_and_fish_farms.md).

- Sounds of the fish shoals with their own pitch: the shoals make no sound of their own; the farm's turn (`FishFarm::Process` 0x52D130), its creation (0x52CC10) and the fish's drawing (`FishRush::Draw` 0x824DA0, `Fish::Draw` 0x824720, `Fish::ComputePos` 0x8248E0) call no sound function (xref callees, read 2026-11-05); the sounds over the fish are the hand's, its splash and its pick-up loop rising in pitch (`ProcessInInteractFish`, done on [the hand's sounds](progress/hand/hand_sounds.md))

#### building/construction

From [Construction](progress/building/construction.md).

- The player's scaffolds counting as part of a building's wood: the wood a site still needs is (1 − its percent built, or 1 − its life once built) × the building's info wood × its scale over the player's factor, less the wood in its pile (`BuildingSite::GetWoodNeededToBuild` 0x43C5F0, `GetWoodValue` 0x43C0C0, `GetResource` 0x43C5B0), with no scaffold term; a scaffold joining its site (`Scaffold::TryToBuildPlannedBuilding` 0x6E9080 → `BuildingSite::AddScaffold`) adds no wood; a scaffold's wood worth (`Scaffold::GetDefaultResource` 0x6E9D30, its scaffold value × its info wood) is the scaffold's own resource as a thing, which none of those functions reads (read 2026-11-05)

#### building/building_by_creature

From [Building by the creature](progress/building/building_by_creature.md).

- A creature dropping a scaffold where the town has room makes no building site: `Scaffold::EndPhysics` 0x6E8780 builds only on a drop whose physics carries FROM_HAND (0x6E87D8), which a creature's let-go never has (`or [po +0x1D8], 4` only without a thrower, 0x63710E; [physics impacts](../physics/physics-impacts.md)); the other callers of `Scaffold::BuildBuilding` 0x6E9250 are `ForceBuildBuilding` 0x6E8860 (a destroyed abode, the scripts) and `Scaffold::Combine` 0x6E9CEF, none a creature's action (xref callers, read 2026-11-05)
- The creature stealing another town's scaffold and carrying it home (row 254 StealScaffolding, builder 0x4B9280), and the creature setting a thing alight by carrying a burning thing to it (row 283 SetFireToObject, builder 0x4BF140): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 254 or 283, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is one of them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, no shipped script forces them (`refs/chl`), and their builders are referenced only from the action table's initialiser (read 2026-11-08). A scaffold out of its player's influence is still stolen by rows 264 and 265 ([town_actions.md](progress/creature/town_actions.md)), and a creature still burns buildings with fireballs ([building_by_creature.md](progress/building/building_by_creature.md)).

#### worship/belief

From [Belief](progress/worship/belief.md).

- Challenge scripts set a town's belief scale: `SET_OBJECT_BELIEF_SCALE` (native 428, `GScript::TownBalanceBeliefScale` 0x6FF840: the scale popped first, then the thing; a town's +0x5DC takes it, anything else is a script error and nothing) is called nowhere in the shipped `Scripts/Quests/challenge.chl` (lhvmtool's code listing has no `CALL 428`): never run; openblack's native is an empty stub. The land script's `SET_TOWN_BALANCE_BELIEF_SCALE` sets the same scale and is ported
- Hurting a town lowering its belief in the attacker: `GBelief::ReduceBelief` 0x437FD0 has one caller, the desire fold; `AddToBelief` 0x437EB0 gets a non-negative amount from `UpdateHowImpressed` 0x7634C0, `Spell::GetImpressiveValue` 0x721630 being a product of positive factors: an attack impresses and takes no belief (read 2026-11-05)

#### worship/worshippers

From [Worshippers and dancing](progress/worship/worshippers.md).

- Coloured lights follow the dancers: the game keeps them but never draws them; see [../rendering/light_beams.md](progress/rendering/light_beams.md)
- Dancers rest at the altar to get their strength back: the altar rest's states 61-64 exist, but nothing in the game sends a villager to 61 ([worship](../magic/worship.md#going-to-the-site))
- Worshippers ask to go home when tired or hungry: `Villager::CheckRequestGoHome` 0x76C8D0 has no caller, so nobody asks; the row is rewritten to what the original does (any worshipper goes home once fewer are needed)
- Villagers who are impressed enough by a creature worship it, dancing round it: no such villager behaviour was found; the creature itself may dance with villagers to impress them (`CreatureAgenda::ConstructSubActionsForDanceImpressivelyWithVillagers` 0x49C960); the row is rewritten to that
- A dance taking a few turns to get going before it counts: `StartWorshippingAtWorshipSite` puts the worshipper in its dance group at once (`FindDanceGroup` 0x597400, then `AddVillagerToWorshipSite` 0x76C3F0) before it walks to its place, and the site's end of turn counts every dancer (read 2026-11-05)

#### animal/fish

From [Fish and fish farms](progress/animal/fish.md).

- A town's desire to fish follows how full its farms are: a farm's score counts only its fishermen, and fishing never touches the farm's stock (`Villager::Fishing` 0x75B6A0; `fish_farms::Score`); beings/villager-jobs.md#fishermen.
- Stoning a fish farm damages it: `FishFarm::CanBeDestroyedByStoning` 0x52C530 answers 0 (bw1-decomp FishFarm.cpp). (The burning part is kept as an inferred row.)
- A fish farm is drawn and repaired as the town's: `FishFarm::Draw` 0x52C830 is empty (what shows is its shoal, sea pages) and `FishFarm::NeedsRepair` 0x52C580 answers 0; creature/plans-and-needs.md (mending).

#### animal/livestock

From [Livestock](progress/animal/livestock.md).

- Livestock drink: no thirst and no drinking state (as above); the only link to water is a shepherd moving its flock's home to its abode's nearest water (`Villager::ShepherdMoveFlockToWater` 0x768CC0, a villager state), kept in the shepherd row.
- A breeder disciple breeds a town's livestock: the original's breeder disciple breeds villagers only (`Villager::SetupBreederDisciple` 0x769EE0 reads pregnancy and the town's child places; its partners are villagers of the other sex, through `GoAndHaveSexWith` 0x7672C0, [villagers](../beings/villager-life.md#love-outside-the-home-and-the-breeder)); animals are born of their own flock. The villagers' breeder is [disciples](progress/villager/disciples.md) "Breeder disciple" (checked 2026-11-10)
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
- A worship site giving reward chests of its own: `Reward::Create` 0x6E58C0 has three callers, the scripts' generic `CREATE` (fn_006F11A0), `CREATE_REWARD` (`GScript::CreateReward` 0x6F2C14) and `CREATE_REWARD_IN_TOWN` (`GScript::CreateRewardInTown` 0x6F2DB2), and `Reward`'s only constructor is called from `Create` alone (xref callers, 2026-11-04): every chest comes from a script, which openblack runs ([rewards](progress/story/rewards.md))

#### villager/reactions

From [Villager reactions](progress/villager/reactions.md).

- Villagers approach the hand when it is near: no code of the frozen exe sets APPROACH_HAND_REACTION (158) (no push 0x9E before an AddReaction, SetTopState or walk call), and the villager's hand pick-up reaction (16) set-up is the Living's empty one (0x4170D0, `ret 8`, Villager vtable +0x9E0)
- Villagers react to something put in the storage pit by the hand, and to a hand using a totem: the villager's set-ups for reactions 22 and 17 are the Living's empty ones (0x417120 at Villager vt +0x9F4, 0x4170E0 at +0x9E4)
- Villagers react to an object crushed nearby: reaction 18 is made, but the villager's set-up is the Living's empty one (0x4170F0 at Villager vt +0x9E8); checked again 2026-11-08: its priority for every villager is the Living's, which answers 0 (0x474990 at Villager and SpecialVillager vt +0xA78), and the set-up is the empty one for Villager, SpecialVillager, PieceVillager and PuzzleVillager alike, so no villager takes it up whatever the reaction table says; its progress row is gone from [impact_damage.md](progress/physics/impact_damage.md)
- Villagers get out of the way of a falling tree: no call of `Reaction::CreateReaction` 0x6E3D70 makes reaction 27 (the 45 callers read in the exe; a felled tree makes 12, the wood reaction)
- Villagers crowd round to watch something (CROWD_REACTION 214): no call makes reaction 29
- Villagers are confused at shocking sights: no call makes reaction 26 ([villager-reactions](../beings/villager-reactions.md#fainting-and-confused-reactions-25-and-26))
- A creature looks over something it can't make sense of: its confused reaction's set-up (a curious look at the thing, `Creature::SetupReactToConfused`) is written, but no call of `Reaction::CreateReaction` makes reaction 26, the same as for villagers; the reaction's row is gone from [creature reactions](progress/creature/reactions.md)
- Villagers inspect an object new to them and tell others about it: no code enters INSPECT_OBJECT_REACTION (9) for a villager (132 is set only from 9's own chain); INITIALISE_TELL_OTHERS_ABOUT_OBJECT (134) is never set, and TELL_OTHERS_ABOUT_INTERESTING_OBJECT 0x764650 and the look-around 0x764660 are 16-byte functions that return 0
- A villager is scared stiff by a frightening creature: no code sets MAKE_SCARED_STIFF (88); the inspecting clip's scared-stiff pose (one draw in three for women and children) is the creature reaction's, already counted
- Villagers fight a villager of another town: the fight reaction (19) is made only by creature-fight arenas, and its states 143 / 144 walk to a ring round the arena and watch; no villager state fights
- Villagers celebrate (dance) when their town is won over: `GPlayer::ClaimTown` makes reaction 31 from the town centre when there is one, and `Villager::ReactToTownCelebrationPriority` 0x767180 answers non-zero only when the initiator is the villager's own town; with no centre the town branch of the set-up only looks at the town, so the town-celebration dance is never joined (the reactions row now says this)
- A stealthy reaction, which skips at random a cell whose first thing is another living thing as it spreads: the spread's stealth branch (`SpreadReaction` 0x6E3E10: with the reaction's flag +0x20 set, `GameRand(1000) > stealthRandomChance` skips the cell, otherwise the flag is cleared) never runs, as no creator of a reaction sets the flag: the only function that sets it, 0x6E4840, is never called and no pointer to it is in the exe ([reactions](../magic/influence-and-alignment.md#reactions)); openblack's reaction keeps the flag, off when made, and leaves the branch out (`src/ECS/Effects/Reactions.h`)

#### creature/learning_from_feedback

From [Learning from feedback](progress/creature/learning_from_feedback.md).

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
- Run to something (row 122 RunToObject, builder 0x4A6EA0: the run speed, then to within 1.5 heights of the thing, smiling): no desire, town or mimic table lists it and no shipped script makes a creature do it; the only call that forces it is at 0x4F7831, in a function (0x4F7820) that nothing calls and no pointer in the exe names (`xref.py refs` 0, and no dword 0x004F7820 in the file; read 2026-11-08). The creature runs when its other plans ask, under their rows.
- Sleep at a given place (row 85 SleepAtPos, builder 0x4A59C0: the walk home of sleeping at home without its yawn first): no desire, town or mimic table lists it, no call of the exe forces it and no shipped script makes a creature do it (read 2026-11-08); sleeping at home keeps its row.
- Eats the grain from fields only when the time is ripe: a guess; the field's rule is growth and food both above 0 (Field::IsFieldWithFoodInIt 0x4E4930, covered by the field row), and puzzle grain can never be eaten (PuzzleGrain 0x561920 answers 0, oracle 2026-10-18; plans-and-needs.md#what-a-creature-can-eat).
- Showing the player a thing (row 24 ShowPlayerAnObject, builder 0x49D5E0): no desire or compassion-for-town table of `Scripts/info.dat` lists row 24, no mimic row notices or copies it, no constant or register-chosen action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is it (the register sites read: the obstacle's removal, the leash's point and thing orders, the nasty miracle's flee or look, the gift's object-in-hand actions, the worshippers' food, the computer gods' orders and the repeats of a plan already made; the leash's thing search goes through the desire table), and no shipped script forces it (`refs/chl`); each builder is reached only from the action table's initialiser (read 2026-11-08). openblack's plan actions for them and their scenes were removed. Throwing a thing at the camera (row 69) is the original's.
- Destroy whoever attacked a town (row 53 DestroyAggressor): its builder in the v1.42 exe (`CreatureAgenda::ConstructSubActionsForDestroyAggressor` 0x4A1510, 0x30 bytes) only casts its activity object to a town and returns 0, adding no step (xref: callers none, reached only from the action table's initialiser; callees `__RTDynamicCast` alone; bw1-decomp CreatureAction.cpp:1542 the same), so a creature that plans it does nothing; no shipped script names it (refs/chl, read 2026-11-14). The row's test that the town is under attack (`IsActivityObjectATownUnderAttack` 0x4D2C30, callers none) stays with town_actions.md's attack row.
- Put out a fire by stamping on it: no creature action or sub-action of the original stamps a fire out. Its one dousing agenda, row 37 PutOutFire (builder 0x49E470: go near, step back two heights, face it, a cast, then the PUT_OUT_FIRE sub-state, which applies the water pail's effect five times over, bw1-decomp CreatureAction.cpp:1066 and CreatureSubActions.cpp:1893), is itself never run (xref: callers none, the action table only; see creature/creature_casting above), and no shipped script makes a creature put out a fire (refs/chl, read 2026-11-14). A fire put out with the water miracle keeps its row.

#### creature/marks

From [Wounds, burns and blood](progress/creature/marks.md).

- The debug spawner can add wounds, burns and blood: an openblack debug control (src/Debug/CreatureSpawnerAppearance.cpp), not a behaviour of the game
- Healing goes fifty times faster in some state of the creature: never in the game. The day's healing reads 50 counts a turn instead of 1 while a flag of the creature's body is set (`LH3DCreature::UpdateTime` 0x481FC0), but that flag is written only once, to 0, as the body is made (`LH3DCreature::Init` 0x4801BD); a scan of the exe's code finds no other access to it (2026-11-04). openblack's unused fast rate is removed

#### creature/idle_behaviour

From [Idle behaviour](progress/creature/idle_behaviour.md).

- Watches the telly: the action's builder only runs the poo's agenda (bw1-decomp `ConstructSubActionsForWatchTelly`, 0x4B04B0, 16 bytes), and no desire's row of the desire action table nor the town table lists action 176 (info.dat, read 2026-10-21): nothing in the game chooses it. openblack has it as a forced-only action with the poo's agenda, which its planner never chooses (764e131fa; test `CreatureFart.WatchingTheTellyIsOnlyEverForcedAndIsAPoo`). Note: the page's line "the creature's watch telly and fart actions exist but are empty" is wrong for the fart, which is the poo desire's second action and runs the poo's agenda with its own puffs (creature/plans-and-needs.md#the-poo-desires-fart)
- It looks down a cliff: the action's builder is empty and returns failure (`ConstructSubActionsForLookDownCliff` 0x4B18E0, 16 bytes, `return 1`; `ConstructSubActionsFromActionAgenda` 0x4D36A0 answers success only for 0), and no desire lists action 195; openblack has no executor for it, so a forced one fails too (da83d404f; test `CreatureFart.LookingDownACliffIsNeverBuilt`; also listed for easter_eggs/secret_behaviours)
- With a need and the means at hand, it sees to the need before idling: openblack's own idle choice (the "old idle policy" stand-in, removed with its switch on 2026-10-22, e51b27ed1); in the game a creature with no plan does nothing, and needs are weighed by the planner like any desire (decision_making.md)
- Replaying its last shown desire on request: no action in the action table (bw1-decomp `CreatureAction.cpp:105` onwards), no script native and no command does it; `ConstructSubActionsForCommunicateState` (`CreatureAction.cpp:982`) only faces the camera (read 2026-11-05)
- Seeing important things far away and turning to them: its looks are its plan actions and its reactions, each within the reaction kind's `MaxReactionDistance` (`StandardNumGameTurnsToReactFunction`, `CreatureReaction.cpp:113`); nothing turns it to far things (read 2026-11-05)
- Music moving its mood, dancing to music, its feelings picking the music: only with the add-ons behind `BWFeatureKey`; `CreatureMusicMood` (`GGame::Init` 0x54F559 to 0xD06438, read only by `GatheringBox::MusicMoodActive`, which also needs single player and the MP3 player library) tags the MP3 player's song with the creature's recent feedback (0x47B310) and makes it react through packet 0x58, 0x47B3F0 and 0x47E060; `CreatureDanceLineIn` (0x54F56D) needs the sound card's line-in (0xD01964, read in `StateAction`); without the key none of it runs, and the game's own music is never picked by its feelings (read 2026-11-05)
- Watching the player, going to a hill to sit, and watching the telly (rows 284 WatchPlayer, 26 GoToHillAndSit and 176 WatchTelly, builders 0x4BF440, 0x49D8D0 and 0x4B04B0): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 26, 176 or 284, no mimic row notices or copies them, no constant or register-chosen action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is them (the register sites read: the obstacle's removal, the leash's point and thing orders, the nasty miracle's flee or look, the gift's object-in-hand actions, the worshippers' food, the computer gods' orders and the repeats of a plan already made; the leash's thing search goes through the desire table), and no shipped script forces them (`refs/chl`); each builder is reached only from the action table's initialiser (read 2026-11-08). openblack's plan actions for them and their scenes were removed. Going to a hill to look round or to walk along its ridge (rows 25 and 27) is listed by the curiosity desire.

#### creature/animation

From [Creature animation](progress/creature/animation.md).

- The debug spawner plays any animation, face or gesture: a developer tool (src/Debug/CreatureSpawner.cpp), openblack only; it stays as a debug control, not a progress row
- A creature jumping to the side (clip 9, `C_MOVE_R_JUMP_L`), standing relaxed (clip 119, `C_EXTRA_STANDS_RELAXED`) and turning its head from hate to love (clip 79, `C_LOOK_HATE_LOVE`): nothing of the original plays them (read 2026-11-09). No `push 9`, `0x77` or `0x4F`, nor a register loaded with one, comes before `Morphable::GetAnim` 0x619650 or any of `LH3DCreature`'s Start*Action calls (static, concurrent, individual, keep, remove, dance, death, facial, blended, fight, fight recoil, fighting) or the agendas' `AddSubAction`, `AddMainSubAction`, `AddOrder` anywhere in the code (the only 9s are the hand's own clip, in `ObtainRequiredHandPosition`, `HandStateMultiPickUp`, `HandStateTotem` and `HandStateTug`); the desires' manifest animations (`CreatureDesireTableEntry::Table` 0xC87850, +0x1C, built by `_$E211` 0x4DB5F0) are 52, 54..59, 61, 63, 64, 66, 67, 70 and 72; the dance files play 52, 55, 56, 60, 61, 65, 67, 70 and 72 ([pose-and-animation](../creature/pose-and-animation.md)); and no shipped script plays them (`refs/chl`). The clips stay in the species' files and the debug spawner plays them.
- A creature plays its "play" action (68, `play_action`) by itself: no action, desire, dance or table of the original plays it, and no shipped script asks for it ([the spec file's animation numbers](../creature/pose-and-animation.md#the-spec-files-animation-numbers)); a script's play of it plays it as any other
- Breathing that quickens fast and calms slowly: the target period +0x4990 is written only by `SetSize` 0x480636 and `StopMoving` 0x484280, both with the standard period, and `SetSize` writes the current period +0x498C too (0x48063C), so `ProcessBreath` 0x4907C0 never eases anything (read 2026-11-05)

#### creature/beliefs_and_opinions

From [Beliefs and opinions](progress/creature/beliefs_and_opinions.md).

- The creature's decision trees read as text in the game's own mind viewer: a developer tool (the exe's belief Dump functions, e.g. `CreatureBeliefAboutContext::Dump` 0x4C7BE0), not seen in play; openblack's debug spawner shows the trees as a debug control
- Weighing a thing's edibility, damage, danger, interest or impressiveness, a nice or nasty purpose, or how abusive or neglectful the player is: the whole `Creature::How*` family (0x4C3B30..0x4C4310, e.g. `HowImpressedIsObject` 0x4C3EC0, `HowAbusiveIsPlayer` 0x4C3F40, `HowNeglectfulIsPlayer` 0x4C3F60, `HowMuchDangerNearObject` 0x4C3FF0) has no call and no pointer (xref and a byte scan); usefulness is weighed per desire (`CreatureMental::GetObjectToActOnUsefulness`, bw1-decomp `CreatureMentalBeliefs.cpp:402`), with no nice or nasty split (read 2026-11-05)
- A skill at each action that changes what it does: the exe works one out (`CreatureLearning::GetSkillAtAction` 0x4E22E0: the times the action was carried out, as an unsigned count, over the action table's full-skill count, info.dat +0xC4, kept to 0..1), but its five readers change nothing: the four `Creature::HowMuchFriendNeedsToKnowFireball` / `Lightning` / `MagicFood` / `MagicWood` (0x4C40A0..0x4C4220) are of the `How*` family above, with no call and no dword of their addresses in the exe, and `CalculateErrorDisplacement` 0x475FC0 feeds only an angle the cast drops ([creature_casting](#creaturecreature_casting)); its writer `SetSkillAtAction` 0x4E2350 is unreachable too. What rises with practice is the count itself, ported and read by the scripts ([progress row](progress/creature/beliefs_and_opinions.md); read 2026-11-14)
- A script teaches it a distinction about a kind of thing (CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT): `GScript::CreatureLearnDistinction` 0x6F3B80 is reached only through the native table (checked 2026-11-07: `xref.py callers` 0 calls, its one reference is its entry in the script native table, written by the table's static initialiser at 0x7012AF; no statement in `refs/chl` and none in the coverage tool's 514 shipped challenge scripts)

#### creature/friends_and_other_creatures

From [Friends and other creatures](progress/creature/friends_and_other_creatures.md).

- How much more life and strength an opponent has, and how many dangerous miracles it knows, weighing on whether to take it on: not in the original (checked 2026-11-05): `Creature::HowMuchStrongerIsOpponentThanMe` 0x4C3BF0, `HowMuchMoreLifeHasOpponentThanMe` 0x4C3C70 and `HowManyDangerousSpellsOpponentKnows` 0x4C3D00 have no call site, and no dword of their addresses is anywhere in the exe, so no table or pointer reaches them; code never run ([progress row](progress/creature/friends_and_other_creatures.md#noticing-and-judging-other-creatures))
- Eating with a friend: the exe's builder `ConstructSubActionsForEatWithFriend` 0x4B1980 only ends the action unsuccessfully ("failed to construct subactions", 0x9D961C), so it never runs
- Showing a friend an object: `ConstructSubActionsForShowFriendObject` 0x4A7D20 is the same failing stub; only showing its home (0x4A7D40) and its citadel (0x4A8410) are built
- The opposing creature in a challenge set by script: OPPOSING_CREATURE only reads the species opposite a god's creature (`GScript::GetOpposingCreatureType` 0x6F5AC0), and no shipped script calls it ([../scripts/challenge_natives_creature.md](progress/scripts/challenge_natives_creature.md))
- A friend doing something drawing the creature to join in: joint actions are started by the asker (0x4F64D0's answer, AddOrder), and `FollowFriendAround` 0x4A2330 only walks near the friend and faces it, five times (read 2026-11-05)
- Eating fish with a friend, getting a friend to fetch food from a field or a fish, and ordering a friend around as an action of its own (rows 271 EatFishWithFriend, 270 GetFriendToGiveMeFoodFromField, 272 GetFriendToGiveMeFish and 257 OrderFriendAround, builders 0x4BC8C0, 0x4BBD80, 0x4BCCA0 and 0x4B9AD0, each reached only from the action table's initialiser): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 257, 270, 271 or 272, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, and no shipped script forces them (`refs/chl`) (read 2026-11-08). Eating from a field with a friend (row 269) is listed by the friendship desire, and a joint action's orders are still obeyed ([friends_and_other_creatures.md](progress/creature/friends_and_other_creatures.md)).
- Showing a friend its home (row 131 ShowFriendHome, builder `ShowFriendMyHome` 0x4A7D40): the planner allows it only with `HasCreatureBuiltHisHome` 0x4D3010, that is `HasFinishedBuildingHome` 0x479480, Creature +0x1210 > 6. +0x1210 is written only by the two constructors (0x473D00, 0x47481C, to zero) and raised only by stage 1 of the AddToHome sub-action (sub_5060D0, `inc` at 0x50612D), which only `ConstructSubActionsForBuildHome` 0x4A5340 queues; BuildHome's own test `IsHomeUnderConstruction` 0x49A7A0 needs +0x11FC, written only by `Initialise` (0x4745A8, to zero) and by the CREATE_FOOT_PRINT sub-action (0x49, its stage sub_5062C0 at 0x5062D6), and no code queues sub-action 0x49 (no `push 0x49` before `AddSubAction`, `AddMainSubAction` or `AddOrder` anywhere in the code, where the same scan finds the 14 pushes of 0x5A and BuildHome's 0x45). So the count stays 0 and the planner never chooses it; no shipped script forces action 131 (`refs/chl`), and no `push 0x83` comes before `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 (read 2026-11-09). Showing a friend its citadel (row 132) is listed and runs ([friends_and_other_creatures.md](progress/creature/friends_and_other_creatures.md)).
- Giving another creature food or a thing as an action of its own (rows 135 GiveFoodToCreature and 68 GiveToCreature, both built by 0x4A37B0): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 68 or 135, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, and no shipped script forces them (`refs/chl`) (read 2026-11-08). The builder still runs inside giving a friend a toy (row 281 GiveFriendToy, 0x4BEEB0, listed by the friendship desire).
- Going to the beach with a friend (row 204 GoToBeachWithFriend, builder 0x4B35E0, reached only from the action table's initialiser): no desire or compassion-for-town table of `Scripts/info.dat` lists row 204, no mimic row notices or copies it, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is row 204, the computer gods' forced creature actions push none, and no shipped script forces it (`refs/chl`); the forcing calls with an action held in a register give other actions: the emergency step 0x47D29D row 238, `Creature::SetupReactToNastyMagic` 0x4F32C2 rows 156 and 157, `Creature::ForceActionCastSpellOnObject` 0x4F8934 the spell's own casting action from the magic action table (0x47C5F0), the kept gift 0x50A731 rows 275 to 278, and `ForceCreatureToPerformAction` 0x654DED the computer gods' constant actions (read 2026-11-08; the 204s pushed at 0x502057 and 0x50212A are a villager's state, the one at 0x4CCAC1 a learning context's). Sitting on top of a hill with a friend (row 141) is listed by the friendship desire.

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

- Scripts tie a creature's leash to an object (`ATTACH_OBJECT_LEASH_TO_OBJECT`, native 185) or put it back in the hand
  (`ATTACH_OBJECT_LEASH_TO_HAND`, native 186): no shipped script calls either (the decompiled challenge files,
  `refs/chl`, read 2026-11-03), so in the original neither runs. The scripts' leash natives that do run are the
  take-off (`DETACH_OBJECT_LEASH`, 20 calls in 7 files, CreatureDevelopment 1, CreatureGuide 3, SwapCreatures 4,
  creatureguardian 5, land4ogre 1, thebigfight 1, thehermit 5, every one on `MyCreature`), `SET_LEASH_WORKS` (on
  `MyCreature`, and in FreeTheCreature on `LethysCreature`, Lethys's only creature), the leash tests and the leash
  draw. openblack keeps the two natives, ported as the original's ([leash.md](../creature/leash.md#what-openblack-does-with-the-leash-natives)),
  and their differences from the original (the leash to the hand refused for a creature its owner does not lead, one
  another player holds or the neutral player's, and one leash state a creature rather than a player) are never
  reached: every player of the shipped game has one creature, so the player's leash status and the creature's leash
  are the same leash

- Sent with the leash to a point near it after taking something into its hand, the creature puts the thing down there
  (marking the put-down prompt, +0x1B4), or onto the sea throws it into the sea: in `Creature::ForceActOnMapCoord`
  0x4C6540 these two paths (0x4C6B..0x4C6C, the put-down at 0x4C6C01, and the sea's `THROW_IN_THE_SEA` behind its Play test at 0x4C6D0E)
  are on the empty hand's branch, taken only when the creature held nothing at the first read of its hand (0x4C65E5),
  and they need it to hold something at the second read. Nothing between the two reads can put a thing in its hand:
  the calls between are `IsDestinationValid`, `SpiralCheckForValidPoint`, `SendCandidateHelpScript` (which queues a
  help script), `IsFighting`, `EndAnyAnimsRapidly`, the sounds, `StartConcurrentAction` and `GetDistanceInMetres`,
  and the game runs a turn on one thread; so they never run (read 2026-11-03, [leash.md](../creature/leash.md#sent-to-a-point-of-the-land))
- A wait after putting a leash on before another: `GLeashStatus::SetOn` 0x5E6F70 keeps no time and no wait is found on the put-on path (read 2026-11-05)
- Without a temple, not free to roam away from home: the kept area (`Creature::UpdateLeash` 0x4CE5D2 onwards, the young creature's home rule) tests the leash, the development phase and the land number, never a temple; openblack's `leash::FreeOfHome` / `LeashSystem::FreeOfHome` feed only the spawner readout (read 2026-11-05)
- A leashed creature acting on the hand's held object: `Creature::LeashForceCreatureToActOnHandObject` 0x4CECF0 has no caller and no reference (xref); the tug's path (0x4CEE70 to 0x4CEE80) is reached only from a packet nothing sends (read 2026-11-05)

#### creature/creature_cave

From [The Creature Cave](progress/creature/creature_cave.md).

- A fight scroll of its own in the Creature Cave: not in the original (checked 2026-11-05): the room's "fight" scroll callback (`CreatureRoom::CreatureScrollFightSet` 0x789540) shows the actions learnt scroll (scroll 3, help script CitadelCreatureRoomActionsLearntScroll), and the room makes only four scroll texts (creature, actions learnt, personality, magic); the fights' tallies end the creature scroll (`TempleRoom::AddText` 0x79A910, texts 0x492..0x498) ([progress row](progress/creature/creature_cave.md#trophies))
- Embers drifting up from the Creature Cave's fire: not in the original (checked 2026-11-05): the room makes only four fire sprites (`CreatureRoom::InitEngine` 0x787968, `S_Fire.raw`, colour 0x80FF8040), its two smokes (0x787F49, 0x7880BD, the waterfall's spray and the fire's smoke) and the mists; neither `CreatureRoom::Draw` 0x786840 nor `InitEngine` makes any other particle ([progress row](progress/creature/creature_cave.md#getting-there-and-moving-about))
- The actions learnt scroll listing the strongest opinions each way, what it has learnt to do and not to do: not in the original (checked 2026-11-05): `CreatureRoom::MakeActionsLearntScrollText` 0x788C10 writes only its title and whether each of five skills is known; the lists are the ImGui cave screen's only (`creature_cave::LessonsOf`, `src/Gui/CreatureCaveScreen.cpp`) ([progress row](progress/creature/creature_cave.md#the-scrolls))
- A lesson browser in the Creature Cave stepping through the actions it knows, telling why, on what and how: not in the original (checked 2026-11-05): the room's functions are its draw, update, belts and medals, four scroll texts and their callbacks, the screenshot and the availability test (`CreatureRoom` 0x7865E0..0x789B30), and its helps name only the four scrolls, the plinths and the dummies; nothing in openblack either ([progress row](progress/creature/creature_cave.md#the-scrolls))
- The creature turns to face the hand as the hand moves about the cave: the exe's room turns its copy only to the tattoo editor's eye as the editor opens and back to the fire once it closes (`CreatureRoom::Draw` 0x786C86..0x786F75, [tattoos.md#the-copys-turns-and-looks](../creature/tattoos.md#the-copys-turns-and-looks)); replaced by a done row
- The creature looks at the hand with a static pose, at random mirrored, when the hand is in front of it: the room's only random flag is the rest's (sleep or sit) at its start, and the only look at the pointer is the first second and a half of a press with the editor open (same page); replaced by done rows
- A picture of the creature can be taken from the cave and saved: `CreatureRoom::TakeScreenShot` 0x789A70 writes `creatureshot_%i.jpg`, which only the creature web page export for the online creature service reads (fn_00578330, `%s\creatureshot_%d.jpg`); the service no longer exists (already listed: "The creature's details are exported to a web page in the profile for the online creature service")
- The debug spawner shows and sets every value of the creature's body: an openblack developer tool (`src/Debug/CreatureSpawnerBody.cpp`), the original has no such window; it stays as a debug control, with no progress row.
- Without a temple, as on the testbed, F5 shows the cave's screen on its own, and F5 or Escape closes it: openblack-only, for the testbed's land (`src/Gui/CreatureCaveScreen.cpp`); the original's room keys only take the player into the temple (`GGame::DoAction` calls `GoInsideCitadel` 0x553E10) and its cave is a room of the temple (`CreatureRoom`), so with no temple there is no cave
- A likes scroll in the Creature Cave naming and showing the thing the creature likes most: not in the original (checked 2026-11-14). The room's "likes" scroll callback (`CreatureRoom::CreatureScrollLikesSet` 0x7896D0, second of the four Set callbacks the constructor gives `TempleRoom::LoadOptionData`, table 0xC27838) only sets the shown scroll to 2 ([0xDE1000]) and starts its help CitadelCreatureRoomLikesScroll. `CreatureRoom::Draw` makes scroll 2's text with `MakePersonalityScrollText` 0x788D20 (jump tables 0x7870FC and 0x78710C: 1 creature, 2 personality, 3 actions learnt, 4 magic), and that text names no liked thing (the attention line, a perceived desire's line end, the creatures it knows and four beliefs). xref callers of CreatureScrollLikesSet: none, only its data pointer at 0xC27840 in the room's callback table; it is the personality scroll's callback ([progress row](progress/creature/creature_cave.md#the-scrolls))
- An empty Creature Cave with no creature yet, its scrolls saying so: not in the original (checked 2026-11-14). The room's availability test (`CreatureRoom::IsAvailable` 0x789B30, the room's vtable slot 0x0, no direct callers) answers no while the local player has no creature (g_game+0xA64 + the player index x 0xA60), in a skirmish as well as in the story, so the cave is never entered without one. `MakeCreatureScrollText` 0x788A10 has no branch for a missing creature. A creature kept out by a script (SET_CREATURE_IN_TEMPLE, used by the shipped FreeTheCreature, SetupLand3 and lethysvortex scripts in refs/chl) is the scripted switch's row, not this one ([progress row](progress/creature/creature_cave.md#the-creature-in-the-cave))

#### creature/town_actions

From [Town actions](progress/creature/town_actions.md).

- Bringing an object to a town and leaving it there (row 36 BringToTown, builder 0x49E460, which builds nothing: bw1-decomp `CreatureAgenda::ConstructSubActionsForBringToTown` returns at once), and stealing another player's animal or villager for its own town (rows 311 StealAnimal and 312 StealVillager, builders 0x4C1A10 and 0x4C1CD0), each reached only from the action table's initialiser: no desire or compassion-for-town table of `Scripts/info.dat` lists rows 36, 311 or 312, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, and no shipped script forces them (`refs/chl`) (read 2026-11-08). Stealing a scaffold, a toy or a one-off miracle for its town or temple still runs (rows 264 and 265).
- Shaking fruit from a tree for the storage pit, putting a pile of magic wood in it, and frightening or kissing a villager in play (rows 30 GiveFruitFromTreeToStoragePit, 33 GiveMagicWoodToStoragePit, 296 PlayfullyFrightenVillagers and 301 PlayfullyKissVillager, builders 0x4AD820, 0x4AE020, 0x4C04D0 and 0x4C0E30): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 30, 33, 296 or 301, no mimic row notices or copies them, no constant or register-chosen action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is them (the register sites read: the obstacle's removal, the leash's point and thing orders, the nasty miracle's flee or look, the gift's object-in-hand actions, the worshippers' food, the computer gods' orders and the repeats of a plan already made; the leash's thing search goes through the desire table), and no shipped script forces them (`refs/chl`); each builder is reached only from the action table's initialiser (read 2026-11-08). openblack's plan actions for them and their scenes were removed. Magic food for the store (row 31) is in the town table and playing with a villager (row 299) in the play desire's.
- Making a pot in its hand from a thing, or from nothing (the sub-steps CreateHandObjectFromObject 0x505400 and CreatedHandObjectExNihilo 0x508130; the old row put the first under the creature-inspect step 0x505380 by mistake, and that step makes no pot): neither has a caller (xref callers: none; each is reached only through the sub-state table filled by `_$E224` 0x4F8D80, whose entries are 0x90 apart, so their names at `SubStateTable` +0x1E60 and +0x4380 make them steps 0x36 and 0x78, the same numbers as bw1-decomp's enum). In the frozen exe's C (`gx.py grep` over every `AddSubAction`/`AddMainSubAction`/`AddOrder`), step 0x78 is pushed only by GetFriendToGiveMeFoodFromField 0x4BBD80 and GetFriendToGiveMeFish 0x4BCCA0. Those are rows 270 and 272, already not in the original ([friends_and_other_creatures](#creaturefriends_and_other_creatures)). Nothing pushes step 0x36, and bw1-decomp has no use of CREATURE_SUB_STATE_ACTIONS_CREATE_HAND_OBJECT_FROM_OBJECT. Scripts cannot push sub-steps, so no shipped script (`refs/chl`) reaches either one (read 2026-11-14). The shared pot maker `Creature::CreateHandObject` 0x47ACD0 is still reached from its third caller, the fish from the sea, which is ported (the plan action GiveFishToStoragePit, [town_actions.md](progress/creature/town_actions.md)).
- Stealing a miracle's seed (row 310 StealSpellSeed, builder 0x4C1860; xref callers: none, the action table only; xref note #6985): the evidence is already the creature/creature_casting bullet on rows 253 and 310 (the stealing desire 39's own info.dat table lists only row 265, no other desire, town or mimic table lists it, nothing in the exe forces it, and no shipped script makes a creature do it, `refs/chl`; read 2026-11-08, checked again 2026-11-14). A one-off globe lying on the ground is still taken to the temple by row 265 ([town_actions.md](progress/creature/town_actions.md)).

#### creature/face_eyes_hair

From [Face, eyes and hair](progress/creature/face_eyes_hair.md).

- Mood faces happy, in pain, irritable and lonely: the exe has the four face functions (SetFaceForMoodHappy 0x4C8600, InPain 0x4C8500, Irritable 0x4C8570, Lonely 0x4C85C0) but nothing calls or references them (xref refs: 0 each); docs/wiki/creature/pose-and-animation.md#the-faces-a-creature-pulls ("no agenda uses them")
- The hair hidden altogether: openblack-only debug switch (CreatureHairSystem::SetShown, the debug spawner's "Show hair"); no such switch in the original

#### creature/reactions

From [Reactions](progress/creature/reactions.md).

- A reaction to the hand coming near the creature, looking at it or running from it: not in the original (checked 2026-11-05): the creature's reaction kinds about the hand are its picking something up, using a totem and putting things in a storage pit (`Creature::SetupReactToHandPickUp`, `SetupReactToHandUsingTotem`, `SetupReactToHandPuttingStuffInStoragePit`); running from a near hand is the plan action RunAwayFromPlayer, chosen only with the nearest hand within reach and an attitude to the player below 0 (`Creature::IsPlayerNearbyAndFrightening`, bw1-decomp `CreatureMentalActionTests.cpp:152`), whose rows are on [desires](progress/creature/desires.md) and [object actions](progress/creature/object_actions.md) ([progress row](progress/creature/reactions.md#the-players-hand))
- Its priority for each kind of reaction depends on the creature (how frightened or curious it is): a guess the exe does not bear out; the creature's priorities are the reaction table's priority, by distance for a nasty miracle, by the facts of the kind (bw1-decomp CreatureReaction.cpp, docs/wiki/creature/mimicry.md#reactions-to-miracles); fear and curiosity only choose what it does once it has taken a reaction up
- Only the nearest few creatures cast shadows (appearance): every creature body makes its own projected shadow (`CreateDynamicShadow`, LH3DCreature 0x47F543) on the one shadow list with no count or distance cap; the row was written from expectation ([shadows](../rendering/shadows-and-reflections.md#projected-shadows-shadowinfo))
- Scripts can wipe a creature's mind (saves_and_files): `CLEAR_ACTOR_MIND` (0x6FE750) never touches a creature's mind; it tests for a town, ignores the answer and deletes a villager's script memory ([vanilla_bugs/creature.md](progress/vanilla_bugs/creature.md))
- The Creature Cave can take a picture of the creature for the creature's web page (saves_and_files; the page itself is written, see [Launch switches and stray files](progress/easter_eggs/launch_switches_and_files.md#files-the-game-writes-about-the-creature)): the pictures are the `creature.lhp` archive's `creatureshot_%d.jpg`, for the online service ([players-creature.md](../creature/players-creature.md#the-creatures-files))
- The game can print a creature's information and statistics (saves_and_files): `Creature::DisplayStatistics` 0x476070 is a debug dump, not the status panel ([fights.md](../creature/fights.md)): developer output
- A land script command lists all creatures (saves_and_files): `OUTPUT_CREATURES` is developer output, not used by the shipped map script (already listed under [scripts/land_script_commands](#scriptsland_script_commands))
- Creatures far from the camera are drawn with simpler meshes (appearance): in the v1.42 exe every model draws one level of detail, with no switch by distance ([meshes-and-lod](../rendering/meshes-and-lod.md), [visible-blocks-and-lod](../rendering/visible-blocks-and-lod.md#the-model-level-of-detail-in-v142))
- Scripts can read or set the ALIGNMENT property of a thing that is not a creature (appearance): the original's `GET_PROPERTY` gives such a thing its player's alignment (`GetPlayer` vt+0x1C, `GPlayer::GetAlignmentValue` 0x64D6A0; 0 with none) and its `SET_PROPERTY` writes a reward's +0x78 (read by nothing but `Reward::Save` / `Load`; anything else "UNEXPECTED Alignment change" 0xC20A0C), but every property 18 in the shipped `Scripts/Quests/challenge.chl` (47 uses, 2026-10-28: refs/chl SetupComputerCreatures, CreatureCurse, landcontrol5, thebigfight, land4ogre, BlindWoman, creatureguardian, creaturemirrorfight) is on a creature: the player's, a computer god's, Nemesis's mirror, the guardian, the ogre or one a script makes with `create_creature_from_creature` / `create 12`; a creature's is ported ([players-creature.md](../creature/players-creature.md))
- A planned creature pen is shown as a plan before the citadel is built (home_and_pen): the pen is the temple mesh's pen point; the civic creature pen (`CREATE_CREATURE_PEN`) is used by no land or playground script (already listed under building/civic_buildings) and the shipped lands make no pen as a thing of its own ([plans-and-needs.md](../creature/plans-and-needs.md))
- The pen's size and look come from the game's pen tables (home_and_pen): same evidence; the pen tables belong to the unused civic creature pen
- Scripts can call the player's creature home (home_and_pen): `CALL_PLAYER_CREATURE` (native 197, 0x6F3D40) only hands the player's creature to the script; no native calls it home ([players-creature.md](../creature/players-creature.md#the-natives-land-1-reaches-on-it))
- The creature creates its own home where it chooses (home_and_pen): the exe's CreateHome action is a stub that fails at once (`ConstructSubActionsForCreateHome` 0x4A5320 calls only `FinishActionUnsuccessfully`)
- Being impressed by what its own player, its player's other creature or another god's creature does: on taking up a reaction the game adds 4 times the initiator's impressive value to the mind's tally of how impressed it is by its player (mind +0x18C64), or, for another player's creature, 12 times it to how impressive it finds that creature (the attitude's +8; `Creature::UpdateHowImpressed` 0x4F2780, `CreatureMental::UpdateHowImpressiveIsCreature` 0x4C4F10), but nothing reads either tally: the only access to mind +0x18C64 in the exe is that add (every function scanned, 2026-11-08), and the attitude's impressiveness is read only by its own update and its fade (bw1-decomp CreatureAttitudeToCreature.cpp; no caller of `GetAttitudeToCreature` 0x4C4C80 reads +8), so the creature acts no differently. openblack keeps the attitude's figure, fading, for the debug window only. The miracles' row "A creature watching its own player's miracle is impressed by it" was this tally, and is removed (2026-11-10).

#### creature/creature_casting

From [Creature casting](progress/creature/creature_casting.md).

- It draws the miracle's gesture in the air before casting — the constructs add the gesture sub-action (0x58) only when `GMagicInfo[magic]` +0x2C is not 0, and no miracle of the game's tables has one (casting.md, the constructs and Pending): written but never run.
- It casts warming or cooling spells on a friend, or cures its illness — rows 136 CastWarmingSpellOnCreature, 137 CastCoolingSpellOnCreature and 138 CureIllnessOnCreature have magic 0 in info.dat (read 2026-11-08 for 138) and `CanCreatureCastSpell` 0x4D2940 refuses magic 0, so the planner and the mimic copy never weigh them (casting.md, "The teleport, warming and cooling rows" [V]); no call of the exe forces them (every call of `ForceActivityAndForceAction` 0x4C4450 and `CreaturePlan::CreaturePlan` 0x4F1230 scanned for its action argument, 2026-11-08) and no shipped script makes a creature do them (refs/chl: every `force` names another action). Forced anyway, the helpful construct would cast magic 0, nothing.
- It casts teleport to travel to something — row 182 CastTeleport serves compassion (info.dat desire 1), whose own table never lists it and is not planned, and row 216 CastTeleportAndUseIt has magic 0 and desire 40, past the 40 desires; no desire, town or mimic table of info.dat lists either, no call of the exe forces them and no shipped script makes a creature do them (read 2026-11-08). Only row 196 ExploreAndCastTeleport, which wanderlust lists, keeps its row.
- It swaps minds with another creature of its own accord — no desire, town or mimic table lists row 187 SwapMindWithOtherCreature; only the swap itself forces it on the two creatures (`Creature::SwapMinds`, plans made at 0x47C29F and 0x47C2DE, read 2026-11-08), which keeps its rows ([secret_behaviours.md](progress/easter_eggs/secret_behaviours.md)).
- It puts out a fire by dousing it (row 37 PutOutFire, builder 0x49E470) — no desire, town or mimic table lists it, no call of the exe forces it and no shipped script makes a creature do it (read 2026-11-08; [fire.md](../world/fire.md) found the same); the fire is put out with the water miracle (row 238), which keeps its row.
- A fizzled try shows a light bulb over its head: the original's fizzle show plays the embarrassed and sad animations and adds no light bulb; its light bulb comes only from learning by sight ([casting.md](../creature/casting.md#the-fizzle-shown)).
- The debug tools and testbed can tell a creature to cast any miracle at something — a developer tool of openblack's (the testbed's `TellCast`), not the original's behaviour; the code stays as a debug control.
- Full skill at a miracle given by a script or anything else: the only caller of `SetSkillAtAction` 0x4E2350 is the block at 0x4C5E60 (full skill to a random one of eight actions), which has no reference; no native sets a skill, and `ScriptLearnEverything` 0x4F7080 only adds knowledge (read 2026-11-05)
- An unskilled creature's aim at a thing turned aside: `Creature::CastSpellOnObject` 0x4F7970 does turn a point by the skill (`CalculateErrorDisplacement` 0x475FC0, the angle raised by trunc((1 - skill) x 56) of the 2048 steps to a turn, skill = `GetSkillAtAction` 0x4E22E0 of the action under way), but uses it only for an angle it drops (`fstp st(0)` at 0x4F7C33); the cast goes at the object and its own position (0x4F7D9E, 0x4F7DBC), and no random draw is taken, so no aim is turned and no row is kept (read 2026-11-14; [casting.md](../creature/casting.md#casting-at-an-object))
- It steals miracles and miracle seeds (rows 253 StealSpell and 310 SteaSpellSeed, builders 0x4B8E50 and 0x4C1860): both rows serve desire 39, the wish to steal, whose own table in info.dat lists only row 265 StealObjectAndPutByCitadel; no other desire, town or mimic table lists either, no call of the exe forces them (every call of `ForceActivityAndForceAction` 0x4C4450 and `CreaturePlan::CreaturePlan` 0x4F1230 scanned for its action argument) and no shipped script makes a creature do them (`refs/chl`), read 2026-11-08. A lying one-off globe is still taken to its temple by row 265 ([town_actions.md](progress/creature/town_actions.md)).
- Catching a fireball and throwing it back (row 255 CatchFireballAndThrowBack): its builder 0x4B9430 only ends the action as failed and builds nothing (bw1-decomp `CreatureAgenda::ConstructSubActionsForCatchFireball`, the same five instructions in this exe), and no desire, town or mimic table lists the row, no call of the exe forces it and no shipped script makes a creature do it (read 2026-11-08). A thrown fireball is offered to a creature to catch like anything else that flies ([feeding_and_thrown_things.md](progress/creature/feeding_and_thrown_things.md))

#### creature/creature_mode

From [Creature Mode and the status panel](progress/creature/creature_mode.md).

- The follow camera following a fast-turning creature more loosely: `CameraModeFollow::Update` 0x44C160 (bw1-decomp `CameraModeFollow.cpp:268`) eases with fixed start and end zoom times and has no turn-rate term (read 2026-11-05)

#### creature/physiology

From [Creature physiology](progress/creature/physiology.md).

- Fire warming and water cooling the creature: its warmth (physical +0x10) is written by the constructor (0x4EF420) and the load, and in play only by the body's turn from `GClimate::GetTemp` less the species' liked temperature (0x4EF864..0x4EF928); fire is not in `GetTemp` (read 2026-11-05)

#### creature/home_and_pen

From [Home and pen](progress/creature/home_and_pen.md).

- Free to roam only within reach of home: `LeashedOrTooFarFromHome` 0x4D2E40 (140 m) is only the row test of SleepOnTheSpot and SleepByObject (its address taken at 0x493BEC and 0x49A126); openblack's `FreeOfHome` is only the debug readout (read 2026-11-05)
- Nothing scary near home making home safe: `NothingScareyNearHome` 0x4E4490 has no caller, reference or pointer (read 2026-11-05)
- The creature building its own home and knowing its state: HomeExists (+0x11FC) is written only by `Initialise` (0x4745A8, 0), so `IsHomeUnderConstruction` 0x49A7A0 is always false and `HasNoHome` always true, +0x1210 is written only in the constructors, and `CreateHome` 0x4A5320 fails at once (read 2026-11-05)
- Going out to look for food: `ConstructSubActionsForGoOutAndLookForFood` 0x4AF640 (row 166) only calls FinishActionUnsuccessfully (read 2026-11-05)
- The creature building a home of its own (home_and_pen), row removed 2026-11-14: the desire to build a home only plans CreateHome, and its builder `ConstructSubActionsForCreateHome` 0x4A5320 only calls `FinishActionUnsuccessfully("failed to construct subactions")` (bw1-decomp CreatureAction.cpp:2205, the same 0x20 bytes in this exe). `xref.py callers 0x4A5320`: 0 calls; its address is taken only by the action table's initialiser (0x494F3C). No native builds a creature's home, so no shipped script can (refs/chl). The building and the home's state are already listed above (HomeExists, `IsHomeUnderConstruction`, `HasNoHome`), and the desire itself keeps its row on [desires.md](progress/creature/desires.md)

#### creature/locomotion

From [Creature locomotion](progress/creature/locomotion.md).

- The creature uses the land's footpaths: not in the original (checked 2026-11-05): the only call of a thing's footpath use (vt+0x80, `UseFootpathIfNecessary`) is `Living::SetupMoveToOnFootpath` 0x5EDD20 (0x5EDD7D), and every caller of that is a villager's (37 call sites in `Villager`, the town celebration's reaction and a villager dance helper sub_5020A0); no creature walk asks for a footpath ([progress row](progress/creature/locomotion.md#routes))

#### creature/appearance

From [Creature appearance](progress/creature/appearance.md).

- A fizzed-out creature casts no shadow: not in the original (checked 2026-11-05): the body's projected shadow is made once by `CreateDynamicShadow` (the creature's constructors, 0x47F543 and 0x47F828) and each frame `LH3DComplexObject::PrepareShadow` 0x814FD0 drops it only for a hidden object (+0xAC, 0x814FE4) and fades it only by distance (`GetAlphaForShadow` 0x874600); neither reads the drawn fizz (+0xA0), so a fizzing creature casts its shadow, as openblack's does (`src/Graphics/ShadowList.cpp`) ([progress row](progress/creature/appearance.md#drawing))

### Player and interface

#### camera/saved_modes

From [Saved games](../engine/saved-games.md#the-saves-in-play).

- The fly-and-click overview views (`CameraModeFlyAndClick`: the camera's, the island's and the temple's, at a
  distance of 2200 (a quarter of it for the temple) and a pitch of π/8, a land grip flying to the point): its constructor 0x44AD60 has no caller, and its vtable
  is written there alone, so the mode is never made (bw1-decomp `CameraModeFlyAndClick.cpp`, matched, read 2026-11-10).
  openblack has none
- The heading camera mode (`CameraModeFollowHeading`, saved as 0x48) and the free camera mode (`CameraModeFree`, 0x65):
  made only by a load of a world file that has one (fn_00441650 from `GCamera::Load`); `CameraModeFollowHeading::Create`
  0x44D840 has no caller and no reference, and neither constructor (0x44D8A0, 0x44DF90) has another caller, so no game
  in play has one to save. openblack does not make them; a load of one gives the player's camera, with one log line

#### hand/creature_contact

From [Creature contact](progress/hand/creature_contact.md).

- Punching the creature: the punch cycle is in the hand's animations but the game never plays it
- Tickling as a separate act: the game has no tickle apart from stroking; its tickle cycle is what the hand plays while stroking
- Scripts putting the hand on a creature to stroke or slap it: no native does it. The hold starts only from the
  action press (`GInterface` 0x5D1330 → `StartTapOrLockedSelect` 0x5D1A00, its one caller, at 0x5D1486); the script
  natives about the hand read it (`GET_HAND_POSITION`, `GET_HAND_STATE`, `GET_OBJECT_HAND_IS_OVER`,
  `IS_LOCKED_INTERACTION`, `GET_INTERACTION_MAGNITUDE`, `CREATURE_INTERACTING_WITH`, `IN_CREATURE_HAND`) or play
  recorded input (`PLAY_HAND_DEMO`), which goes through the same press. openblack's testbed commands that put the
  hand on a creature (`CreatureHandSystemInterface::Stroke` / `Slap`, held by a command) are debug controls
- The hand drawn 0.3 of the creature's height back towards the camera when far from its middle: the original moves
  the creature's look point there (`HandStateCreature::Update` 0x5B22E1..0x5B23AB, `SetLookPoint`), and places the
  hand along the mouse ray at its own distance (0x5B23C4); now the creature's look row

#### hand/hand_animations

From [Hand animations](progress/hand/hand_animations.md).

- The two spare slots of the standard set: empty in the game's file
- The spare slots of the reward and punishment sets: empty in the game's file
- The spare slot of the camera set: empty in the game's file
- The spare slot of the special holds: empty in the game's file
- Point (`Cpoint`, clip 3), Punch (`Cpunch`, clip 36), Beckon (`Cbeckon`, clip 58) and the hold after the horn (`Cphile`, clip 63): loaded with the hand but never played by the original. Every clip the hand plays goes through `Morphable::GetAnim` from the hand's states, and their clips are: the normal state's 0 Cwiggle, 12 Ccan_pickup, 15 Chold_fingers, 24 Cstroke and the camera poses ([the hand's clip](../hand/animation-and-placement.md#the-hands-clip)); the camera state's 0, 42 Cgrip, 45 Crotate, 48 Cpitch, 51 Czoom (0x5B07C6) and their lean layers (+1, +2); the citadel's 0..2 (0x5B10F1..0x5B1245); `ObtainRequiredHandPosition`'s own 0 (0x5B4780, its clip register zeroed on every path into the call: 0x5B4118, 0x5B4198, 0x5B45DA, its two jump tables 0x5B568C and 0x5B56A8 followed); the creature state's 0, 27 Ctickle, 33 Cslap (0x5B29DE, [states](../hand/states.md)); the holding state's 6, 0, 60 and 9 (0x5B4F2A, 0x5B5128, 0x5B51BB, 0x5B53BC), the multi pick-up's and the totem's 9 (0x5B5AF4, 0x5B7B8E), the tug's 6, 0, 60 and 9 (0x5B8A5D..0x5B8BF5), and the knock's 57 Ctap_house, the play state's only clip (`StartFixedPosAnimation`'s only caller, `Abode::InterfaceTap` 0x4068BB). Read 2026-10-31 from every `GetAnim` call in the hand's code (the xref callers of 0x619650). Nothing else picks a hand clip: the other callers of `GetAnim` 0x619650 and of `GetSetAnim` 0x619690 are the creatures' (`LH3DCreature`), the developer's animation editor's (`AnimEdit::PrssKey` 0x51E144 and its helpers 0x51CCFB, fn_0051E500, fn_0051EB90 and fn_0051E430, reached only from `AnimEdit::ClickFunction`) and `Morphable`'s own, and `CHand`'s only one, `LoadBinary`'s five `GetSetAnim` (0x46D52B..0x46D55F), loads the clip sets; `Morphable::SetAnimTime` 0x617EB0 (vt +0x0) is not called on the hand (the vt +0x0 calls of `CHand`'s code, 0x46BE5B, 0x46C764, 0x46ED30 and 0x46EDA8, are its states' Enter, and 0x46D2EA a file's); the hand demos replay input into these same states. No script native plays a hand clip: the hand's natives (GET_HAND_POSITION, IN_CREATURE_HAND, GET_OBJECT_HAND_IS_OVER, ATTACH_OBJECT_LEASH_TO_HAND, PLAY_HAND_DEMO, IS_PLAYING_HAND_DEMO, HAND_DEMO_TRIGGER, GET_HAND_STATE, SET_HAND_DEMO_KEYS) read the hand or drive its demos, OVERRIDE_STATE_ANIMATION 0x6F9F50 refuses what is not a `Living`, SET_ANIMATION_MODIFY 0x6F5570 what is not a creature, and PLAY_GESTURE 0x6FC090 only lays a gesture (`FakeGestureOnLandscape`); the decompiled challenge.chl (`refs/chl`, 98 scripts) uses the hand only through its demos ("start hand demonstration", "wait until hand demonstration trigger" / "played") and its tests (hand position, hand state, the thing in the hand), and the land scripts (Land1..5, LandT) name no hand. The four rows left the hand animations page
- Horn (`Chorn`, clip 60), the horn-shaped hold: never shown by the original. It is the pose of the grain hold (hold type 3) in the holding state (0x5B51B9) and the tug state (0x5B8B52), but no object is ever held so: the hand's hold type (CHand +0x490C) is written only from the held object's `GetHoldType` (vt +0x58C, at 0x46DCED and 0x46E537), whose implementations give 1, 5, 6 or 7 (Object, DeadTree and Tree, MobileObject, FieldCrop, Poo, Animal, Villager, `MobileStatic` 0x6091B0's 1 or 6) or, for a spell seed (`SpellSeed::GetHoldType` 0x728680), 2 until it is ready and then its info's hold type, which in the frozen `Scripts\info.dat` is 1, 2 or 6 for all 30 seeds (record stride 384 from STORM at 15664, the hold type at +320: none is 3). openblack's grain hold (`hand_place::HoldClipOf` gives it the side pose) is unreachable the same way. The row left the hand animations page (2026-10-31)

#### hand/hand_effects_and_glows

From [The hand's effects and glows](progress/hand/hand_effects_and_glows.md).

- The hand flashing in its player's colour on a recognised gesture: the gesture's effect writes the player's colour,
  fading, into the hand object's specular each turn, but every frame the hand's own draw writes the land's colour and
  specular at the hand over it before the hand is queued, and nothing runs between that and the draw, so the flash
  changes no pixel ([the hand's pulse](../magic/hand-casting.md#the-hands-pulse)); openblack writes no flash and draws
  the same hand
- The fireball held in the hand rolling its flames with the hand's smoothed movement on screen: the held ball draws nothing of its own (`MagicFireBall::Draw` 0x682F20 is empty), and its in-hand effect's hand rule only places the atoms at the gesture's point (`UR_FollowLocalHand::ModifyAtomCore` 0x69A6A0: `GetCurrentGesturePosn`, `GlobalToLocal`); the gesture's angular velocity is read only by the side-spin and initial-spin rules (`PSysManager::GetCurrentGestureAngularVel` 0x673640, callers 0x69E376 and 0x69E504), which turn a cast ball's velocity, not a held one's flames (read 2026-11-05)

#### hand/hand_in_scripts

From [Hand in scripts](progress/hand/hand_in_scripts.md).

- Scripts asking how many buttons the mouse has (`NUM_MOUSE_BUTTONS`, CHL 207) and what the creature is interacting with (`CREATURE_INTERACTING_WITH`, CHL 318): no shipped script calls either. `tools/checks/story/script_coverage.py` (2026-11-07), reading the natives every statement of the compiled `Scripts/Quests/challenge.chl` calls (33 563 statements, all matched to `refs/chl`), finds 327 natives used and neither of these; openblack's natives stay stubs. The row "Scripts read what the creature is interacting with" and the button count of "Scripts ask whether the mouse has a wheel" were removed from [Hand in scripts](progress/hand/hand_in_scripts.md) (2026-11-07)
- The interface level also makes the tooltip of a held object come from elsewhere: not in the original. The
  level's native (`GScript::SetInterfaceInteraction` 0x70B220) keeps the level (the script's +0xA4), then sets the
  interface flags, the camera features, the control map's two switches and the hand's reach, and writes nothing
  of the tooltips; the kept level is read back only by the script's reset, save and load (`GScript::Reset`
  0x6EB2D0, `Save` 0x6EBEA0, `Load` 0x6EC270, which sets the level again), so no tooltip reads it. The tooltips
  read only the camera features and interface flags the level sets, which openblack's level sets too
  (`help::interface_interaction::Set`)

#### hand/hand_sounds

From [Hand sounds](progress/hand/hand_sounds.md).

- A deed of the hand that impresses a village setting off a group cheer, and one that loses belief a group groan: not in the original: the in-game bank's crowd samples (H_ImpressedGroup_01..03, H_LoseBeliefGroup_01..03) are played by nothing in the exe or its scripts ([world-sounds.md](../audio/world-sounds.md#villagers-buildings-and-trees-own-sounds), read 2026-10-21); the hand's deeds are heard through the spirits' belief remarks ([voices](../audio/voices.md)). The two rows left the hand sounds page (2026-10-31)
- A villager dying in the hand or by its throw setting off death cries: not in the original as a sound of the hand's: what the hand's kill brings is the advisors' killing-people remark, now that row ([hand sounds](progress/hand/hand_sounds.md))
- Guidance sounds follow a village totem's raising and lowering: not in the original: the guidance's start only remembers the share the hand took hold at, and its end, when the share let go is higher, only asks whether that guidance may play and reads the player's alignment class, then plays nothing ([the village totem](../magic/village-totem.md#the-original)); `audio::guidance::StartTotemRaiseSound` and `EndTotemRaiseSound` do the same and play nothing

#### hand/other_players_hands

From [Other players' hands](progress/hand/other_players_hands.md).

- A computer god's hand raises and lowers village totems: not in the original: the computer mind's score for raising a totem in a town is always none, so the action is never chosen ([the computer minds](../hand/computer-minds.md#scoring-a-node)); our score is none too
- A hand's record of the last thing it picked up having any effect: not in the original. The interface status keeps it (`GInterfaceStatus` +0x120, `LastPickedUpObject`), set as a thing goes into the hand (fn_005DC330, from `GInterface::PlaceObjectInMagicHand`), and it is read only by its own upkeep: the turn's `GInterfaceStatus::Process` 0x5DC4E0 forgets it once the thing is no longer available, and the next put-in-hand clears the record of the status that held it last (`Object::GetInterfaceStatusWhoLastPickedMeUp` 0x63A6A0, whose only caller is fn_005DC330, 0x5DC3DA); `xref.py field GInterfaceStatus 0x120` lists no other access, `GInterfaceStatus::Save` 0x5DCA50 does not write it, and bw1-decomp's only other reader is that lookup (`Object.cpp:2120-2134`); the twin mark of the last thing dropped, by contrast, is read by seven places (`Object::GetInterfaceStatusWhoLastDroppedMe` 0x63A710 from a creature's copying of a landing thing, a living thing's drowning, a scaffold's building, a town's artifact and three more), which is why openblack keeps that one. What the put-in-hand records beside it, the most influential player at the thing's place (+0x128), is kept by openblack ([objects](../world/objects.md)). So the two rows' "the last thing picked up" parts were dropped (2026-11-13), their shares kept
- A computer god's hand estimating how long its steps will take before it starts them: not in the original as behaviour. When an action is done the action state notes the turn (+0x3C) and the sum of each queued step's own estimate (+0x40) (0x6507D0; its callers are the action's run 0x656170, 0x658410, 0x658490, 0x658590 and `GComputerPlayer::ForceComputerPlayerToMoveToPointAndPause`), and the two are read only by `PlayerActionState::Save` and `Load` (`xref.py field PlayerActionState 0x3c` / `0x40`: `SetToZero`, `Save`, `Load`) and by 0x650850, the share of the estimate gone by, which no call, jump or pointer reaches (`xref.py refs 0x650850`: none) ([the computer minds](../hand/computer-minds.md#running-a-chosen-node)). openblack's step that would make it (`HandActions::TimeSteps`) does nothing, and its save writes the two words as they were loaded, or 0 for a new game; neither exe reads them back into play. The row left the other players' hands page (2026-11-14)

#### hand/picking_up

From [Picking up](progress/hand/picking_up.md).

- A creature dragged on the leash by a thing let go as the thing is taken or thrown: the original's
  `Object::RemoveDraggingCreatureByLeash` 0x636E70 (called by `Object::InitialisePhysicsFromHand` 0x636F1E and
  `Object::InitialisePhysics` 0x637491) clears the leash's attached object only for a thing with the dragged-by-leash
  flag (+0x24 bit 0x800), and nothing in the exe sets that flag: no `or` of 0x800 into +0x24 (or of 8 into +0x25),
  no bitfield write of bit 11 there (the Ghidra export's only writes to it are this function's clearing), so the
  function never lets go. openblack has no such release either

- A field crop taken into the hand (a handful of the hand food's food value, the crop deleted: `FieldCrop::InterfaceSetInMagicHand` 0x6083E0): no field crop is ever made in play. A `FieldCrop` is made only by `MobileObject::Create` 0x607000 for a mobile object row whose kind (+0x104) is 15, and only row 15 ("MobileCrop") is (info.dat, the 32 mobile object rows from file 0x3F47A, 0x104 bytes a row, kind at +0xF4); `FieldCrop::Create` 0x607E40 has no caller. The callers of `MobileObject::Create` pass row 5 (the creature's poo, `LH3DCreature::DoStaticAction` 0x48784A), row 31 (the cauldron, fn_006DC220 0x6DC251) or the script's own row (fn_006F11A0, the challenge scripts' create, and sub_7165CA, the land script's CREATE_MOBILEOBJECT). The shipped land scripts (Land1..5, LandT, the playgrounds and the other `Scripts/*.txt`: 3428 CREATE_MOBILEOBJECT lines) use rows 0..14, 16..19, 23, 24 and 31, never 15; the challenge scripts (`refs/chl`, 98 files) create mobile objects only of rows 21 (the creed) and 31 (the cauldron). A save only restores what play made (`GameOSFile::LoadInstance` 0x55A74A). openblack has no field crop class.

#### hand/totem

From [Totem](progress/hand/totem.md).

- A sound as the hand takes hold of a totem and as it lets go: not in the original: see hand/hand_sounds above; the totem's own rising sound and bell come from the worship share being set after the let go, as ours
- Computer gods raising totems: see hand/other_players_hands above

#### hand/pointing_and_tooltips

From [Pointing and tooltips](progress/hand/pointing_and_tooltips.md).

- Over a leash post: "Leash" and the leash's kind (Aggression, Compassion, Learning): The original never shows it: a post is no locked select, so the hand over one is Over Object and shows "Tap" ([tooltips](../interface/tooltips.md#state-13-can-select-lock)), as ours does
- Over a thing that can be asked: «Preguntar» 0xE7C "Ask": the original's state 24 gives it only for a query icon (`IsQueryIcon`, vtable slot 0x1FC), which no class overrides (every vtable's slot is 0x401B50, `xor eax, eax`), so it is never shown
- Showing a tooltip triggers the help system's matching help event: each builder passes a help event with its text (0x22 / 0x23 with a highlight's, 41 / 40 / 47 with the leash's), but `SubmitToolTips` 0x5C9A70 never reads that argument (no read in 0x5C9A70..0x5C9C60) and nothing reads it back, so no event is raised; the focus-leash `HelpProfile::Trigger(0x2F)` of «Traer Criatura» is a call of its own, ported
- Over a signpost «Poste» 0xEF2 "Signpost", over a scroll «Pergamino» "Scroll" on the land: the original never shows either. 0xEF2's only use is `ScriptHighlight::GetOverwriteTapToolTip` 0x70AE10 (`and ecx, 0xEF2` at 0x70AE1F), which gives it only for a highlight info of OBJECT_TYPE 1, and every row of that info is 35 ([script highlights](../interface/script-highlights.md)), so it gives 0; 0xEE2 «Pergamino» is only the temple rooms' tooltip (`TempleRoom::ToolTipText` [0xC2A1C4], set at 0x7842F8, 0x789BD2 `FindProperTextOffset`, 0x792078 and 0x79F1E5 `WorldRoom::WorldScrollGet`, read by `CalculateTooltipsInsideCitadel` 0x78D9B5), and 0xE84 is pushed only as a debug allocation's line number (0x4B3995). A whole-code immediate scan of the frozen exe (capstone over .text, 2026-10-31) finds no other use (read 2026-10-31). The row left the pointing and tooltips page
- «Acercar (%s)» 0xEE6 "Zoom In (…)" naming the place a key zooms to: not in the original as the hand's: 0xEE6 is the temple save game room's picture tooltip, "Zoom In (the save's name)" for a slot with a save (fn_00790C00 0x790DFA sets `TempleRoom::ToolTipText` to it; `GInterface::CalculateOverObject` 0x5CA096 fills the name in from 0xE05EF4); its only other immediate is a debug allocation's line number (0x4B489B). The save room's tooltip waits with the saved games. The row left the pointing and tooltips page (2026-10-31)
- «Doble clic» 0xE78 "Double-click" where double clicking would fly there, and «Repetir Milagro Anterior.» 0xEF5 "Repeat Previous Miracle.": the original never shows them: no instruction of the frozen exe has 0xE78 or 0xEF5 as an operand (the whole-code immediate scan above), the tooltip builders pass their texts as immediates to `SubmitToolTips` 0x5C9A70 or `ForceToolTips` 0x5C9C60, and the only data holding them are the help text tables listing every text id in order (0x90BAE0, 0x94D8D8, 0x9620B0). The two rows left the pointing and tooltips page (2026-10-31)

#### gesture/creature_gestures

From [Creature gestures](progress/gesture/creature_gestures.md).

- The game's tables have gestures for zooming to the creature and for ending a gift to it, both left empty: Not used by the original game
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

#### miracles/creature_spells

From [Creature spells](progress/miracles/creature_spells.md).

- A special animation of the creature receiving a spell: the original plays none. The spells on a creature are run only by its spell state machine (fn_004F48C0, the creature's turn), which per spell calls its row of the handler table at 0xBE0380 (filled by crt_xc_fn_CreatureReceiveSpell_004F3CD0: the start, the ease, the finish's start, the finish and each turn's), plays the spell's sound (`SamplePlayAnimEffect`, the sound only) and asks for its help messages; the handlers, 0x4F5430..0x4F5E30, call no animation of the creature: they set its size targets, strength, fizz, alignment, desires and leash, and the freeze's start only ends its action and stops it (`FinishActionUnsuccessfully`, `LH3DCreature::StopMoving`, 0x4F543C..0x4F545A); a whole-range scan of their calls, 2026-10-31. openblack plays none either ([the creature's spells](../creature/body.md)). The row left the creature spells page

#### miracles/dispensers_and_seeds

From [Dispensers and one-off seeds](progress/miracles/dispensers_and_seeds.md).

- The disk is hidden while the dispenser is being built: openblack dispensers are never under construction
- In the testbed and debug tools a seed can be summoned straight into the hand as if from an icon: Debug and testbed tools don't count here (this file's rule); openblack's are `OPENBLACK_TEST_SEED` and the debug menus

#### miracles/fireball

From [Fireball](progress/miracles/fireball.md).

- A creature catching an enemy fireball and throwing it back: not in the original. The creature mind's catch-fireball action has its name (CatchFireballAndThrowBack, 0x9D8374) but its sub-actions are never made: `CreatureAgenda::ConstructSubActionsForCatchFireball` 0x4B9430 only ends the action unsuccessfully ("failed to construct subactions"). The ball itself can't be caught by the creature's general catch either: `MagicFireBall`'s `CanBecomeAPhysicsObject` 0x682A70, `CanBePickedUpByCreature` 0x682A80 and `CanBeThrownByCreature` 0x682AB0 all give 0, so it never flies as a physics object for `Creature::CheckAllCreaturesForCatching` 0x47CBD0 and a creature may not hold or throw it. Only a god's hand catches a ball ([fireball](../magic/fireball-and-lightning.md)). Read 2026-10-31; the row left the fireball page

- The water miracle putting out a fireball (on the fireball page, and on the water page as a fireball rolling in the water): the original's ball is never in the map cells (`MagicFireBall::InsertMapObject` 0x682D10 is a bare `ret`), and the water reaches things only through them: a drop's event (burn −4000 in 1 m) and its `ApplyWaterSpell` walk the cells round the drop (`SpellWater::Process` 0x724ED0, `ApplyEffectToMapPos` 0x525274..0x5253BF, [the water](../magic/food-wood-water-heal.md)); the fireball list (g_game +0x205C9C) is read only by the ball's own code (0x682956, 0x682B9E, 0x682CB2), and its fire's cooling by rain (vt +0x5EC) only by the fires' turn (fn_0072EFB0), with the weather's rain, not the miracle's drops. openblack's water cools fires through the map cells too and the ball never enters them. Read 2026-10-31; the row left the fireball page

#### miracles/forest

From [Forest](progress/miracles/forest.md).

- The flight no longer locks the camera and the player's movement (user-reported defect): the original's flight never locks them: a movement key that would move the camera, or a land grip, gives it back before the flight ends ([forest](../magic/forest.md#what-is-seen-the-sf_forest-effect)). openblack flies it too (the forest's camera particle, `src/Particles/Rules/Forest.cpp`), and the original's way of giving the camera back is tracked in [the caster's camera](progress/miracles/forest.md#the-casters-camera)

#### miracles/lightning

From [Lightning](progress/miracles/lightning.md).

- Villagers struck by the bolt shown as skeletons: the original never does it. A villager's skeleton look is its Living status bit 0x40, written only by `Living::SetSkeleton` 0x417000 and `Villager::SetSkeleton` 0x7562C0 (the bit from their argument), which only SET_SKELETON calls (vt +0x4C0, 0x6FDBC6 and 0x6FDC66) and the villager's creation passes on (0x74FB0C); every other writer of the status word keeps the bit (`SetDying` ors in 0x31, `EndPhysics` its land type, `SetPoisoned` its own bit; a whole-code scan of the writes at +0xB4). The bolt reaches a villager through its effect values ([the bolt](../magic/fireball-and-lightning.md)), which set no skeleton. openblack sets the bit only through SET_SKELETON too (`ecs::villager::SetSkeleton`, from `src/ECS/LivingSkeleton.cpp`). Read 2026-10-31; the row left the lightning page

#### miracles/miracle_core

From [Miracle effects and reactions](progress/miracles/miracle_core.md).

- A history of alignment deeds is kept for statistics: Our tree keeps none. Our wiki differs: the history only feeds a debug overlay that retail never shows, so nothing in the game reads it ([alignment](../magic/influence-and-alignment.md#player-alignment))
- Each miracle marking the map cell it stands in for the minimap, the marks fading while the minimap shows: the original keeps the 64 by 64 grid at 0xD9C370, marked by fn_00721570 (from `Spell::ProcessMaintainRequest` 0x720604 and `SpellCreature::Process` 0x7223E1) and lowered by 0x20 a turn by fn_007215C0 (from `Spell::ProcessSpells` 0x720301) only while g_game +0x205A28 is 1, the player being inside the temple (`GGame::GoInsideCitadel` 0x553E10 sets it, `GAudio::IsInsideCitadel` 0x4282F0 reads it); the grid's only reader, the cell getter 0x721540, has no caller and its address is held nowhere in the image, and no other instruction refers to the grid's 0x1000 bytes (xref refs over 0xD9C370..0xD9D370 and a scan of the image for every pointer into it, read 2026-11-08), so no mark is ever seen ([spell grid](../magic/spell-core.md#spell-grid))

- A creature caster's count of the things its miracles destroy: the original raises a creature's own count by one when an effect it applied takes a thing's last life (`Object::ApplyEffect` 0x637A54..0x637A73, Creature +0x11C0), but nothing reads it: the image's only other access is its clearing in `Creature::Initialise` (0x4745D2; a scan of `.text` for every `[reg+0x11C0]`, read 2026-11-09), and neither `Creature::Save` 0x4E4EA0 nor `Load` 0x4E5FF0 keeps it, so it changes nothing ([spell core](../magic/spell-core.md#life-cycle))

#### miracles/tornado

From [Tornado](progress/miracles/tornado.md).

- A field's crops lifted by the tornado: the original lifts none, as a field's crops are not things: they are a count in the field (`Field::PlantCrop` 0x5291A0, from `Villager::FarmerPlantsCrop`, only adds one). The tornado lifts things from the map cells round it ([the tornado](../magic/storm-and-tornado.md#the-original)); the class of crop things, `FieldCrop`, is made only by `MobileObject::Create` 0x607000 for a mobile object row of type 15 (0x607053), whose callers make a row of their own (a creature's static action, the cauldron at 0x6DC220) or a script's (the CHL create fn_006F11A0, the land script's sub_7165CA), and no shipped script names a crop (the decompiled challenge.chl in `refs/chl` and the land scripts Land1..5 and LandT). Read 2026-10-31; the row left the tornado page
- The creature help telling the player that the creature has fainted (in the tornado's faint, or any): never heard in the original. `Creature::Faint` 0x476FA0 first forces the faint's plan, action 9 (`CreaturePlan(0x18, 9, …)` 0x477012, `ForceActivityAndForceAction` 0x477022), then asks for the help 0x25 MISCELLANEOUS_STACKED with 3 CREATURE_HAS_FAINTED (`SendCandidateHelpScript` 0x47704D, its last argument 0). `SendCandidateHelpScript` 0x4C9FE0, for a type between 0x22 and 0x28 and that last argument 0, returns without a message while the current plan's action (mind +0xF60, [the current action](../creature/hits-and-catch.md)) is 9, 0x11, 0x4E or 0x55 (the jump table at 0x4CA0C0), and the faint has just made it 9. openblack: `CreatureHelpSystem::Fainting` raises only the body's problem, and not this message, as the original. The row left the tornado page

#### miracles/water

From [Water](progress/miracles/water.md).

- The water putting out a fireball rolling in it: see [miracles/fireball](#miraclesfireball); the row left the water page
- Old rings keep drifting with the wind the way a reused ring slot did in the game: Not in the original: nothing ever sets a ring's drift, and a reused slot keeps nothing that is drawn ([water.md](../world/water.md#water-rings)); test `WaterRingSystem.RingsStayWhereTheyWereMade`

#### miracles/wood

From [Wood miracle](progress/miracles/wood.md).

- There is no extreme version: the game has none

#### camera/camera_help

From [Camera help](progress/camera/camera_help.md).

- A script making the player's camera turn only about a fixed point (`SET_FIXED_CAM_ROTATION`): the native and the
  player camera's turn about the point are in the game, but none of the 514 shipped scripts calls the native (the
  compiled `challenge.chl` read by `tools/checks/story/script_coverage.py`, 2026-11-03: 31510 native calls of 327
  natives, this one not among them), and the camera's setter is called only from the native (`xref.py vtcalls`: no
  other call reaches the player camera's slot); a player never sees it, and openblack keeps only the native's slot,
  which logs that it is not implemented ([zones and fixed rotation](../camera/player-camera.md#zones-and-fixed-rotation))

#### camera/camera_limits

From [Camera limits and collision](progress/camera/camera_limits.md).

- The player's camera left free of the map's disc in the editor mode: the player camera's update skips the disc while
  the developers' editor mode is on (bw1-decomp's player camera update; the flag read at 0x45FEAE), but in v1.42 that
  flag is always 0: its only two writers clear it (`xref.py writers`: 0x54CCEB, 0x54E9E7) and its only setter is
  never called ([the editor mode](../engine/game-clock.md#the-speed-indicator)); openblack holds the camera inside the
  disc always

#### camera/camera_paths

From [Camera paths](progress/camera/camera_paths.md).

- The near plane brought right in while a camera path has the camera: the path writes its close plane as it starts,
  but the camera's update writes the height's near plane over it every frame before any draw
  ([camera](../camera/game-camera.md#camera)); openblack's took it, and no longer does

#### camera/follow_cameras

From [Following cameras](progress/camera/follow_cameras.md).

- The editor's orbit and follow cameras: openblack-only, `src/Camera/EditorCameraModel.cpp`; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)

#### camera/script_camera

From [The script camera](progress/camera/script_camera.md).

- A script camera that looks at or follows a computer god's hand (`SET_FOCUS_FOLLOW_COMPUTER_PLAYER`,
  `SET_POSITION_FOLLOW_COMPUTER_PLAYER`): the natives and the camera's hand branch are in the game, but none of the 514
  shipped scripts calls either native (the compiled `challenge.chl` read by `tools/checks/story/script_coverage.py`,
  2026-11-03: 31510 native calls of 327 natives, neither of these among them), and the camera's two setters are
  called only from the natives ([following a computer player](../scripts/script-commands.md#the-camera)); a
  player never sees it, and openblack leaves both natives as stubs
- The developers' camera editor (its tracks, cameras and scripts edited, listed, sorted and saved, with its own
  draw and keys; bw1-decomp `src/Black/CameraEditor.cpp`, read 2026-11-12): its maker has no caller, and its input runs
  only under the developers' editor mode, which is always off in v1.42 ([the editor mode](../camera/player-camera.md));
  a player never sees it. openblack keeps only what play reaches from that file: the script camera's tracks read from
  `Data/camera.edt` and the camera numbers the scripts convert

#### camera/temple_camera

From [Temple camera](progress/camera/temple_camera.md).

- The camera flying up to the temple's entrance and in (the row "Entering the temple, the camera flies up to the
  entrance and in", an earlier guess): `Temple::InitEngine` 0x793C60 sets the temple's fade to 1.2f (0x3F99999A, at
  0x793C8A) with a target of 0 and starts the active room's intro with `TriggerIntroCamera(0, 0, 0)`, so its camera
  keeps nothing of the island's and starts on the room's own path, under the fade; `GGame::GoInsideCitadel` 0x553E10
  makes no camera call ([coming in](../interface/temple-interior.md#the-temple-interior)). openblack cuts inside the
  same way (`TempleInterior::Activate`: `FadeIntoRoom`, `StartIntro(room, false)`; test
  `TempleCamera.ComingIntoTheTempleCutsToTheStartOfTheRoomsPath`)

#### camera/world_camera_controls

From [World camera controls](progress/camera/world_camera_controls.md).

- A setting to speed the movement keys up or down: openblack-only (editor camera speed), `src/Camera/KeyboardMoveSpeed.h`; see [openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal)
- The player camera's count of how long the mouse and its buttons were left alone (the row "The camera counts how long
  the mouse and its buttons have been left alone"): bw1-decomp's `CameraModeNew3::Update` adds the frame's time to it
  while no button is down and zeroes it at a mouse move of more than 2 pixels or a button, and `Reinitialise` zeroes it,
  but nothing reads it: in the v1.42 exe its field (+0x2E8) is written at 0x458B37, 0x45BDE3, 0x45BE5B and 0x45BE63
  and read only at 0x45BE55, its own increment (`xref.py field CameraModeNew3 0x2e8`, 2026-11-13), and no other
  function reads a camera's +0x2E8 (`gx.py grep`); a player never sees it, and openblack
  keeps none

#### camera/temple_camera

From [Temple camera](progress/camera/temple_camera.md).

- A camera flight up to the temple's entrance and in: entering (`GGame::GoInsideCitadel` 0x553E10, from a tap on the entrance `CitadelEntrance::InterfaceTap`, the actions and `ENTER_EXIT_CITADEL`) moves no outside camera; `Temple::InitEngine` 0x793C60 starts the room's camera with `TriggerIntroCamera(0, 0, 0)`, no blend from the outside camera (the blend of `InnerCamera::TriggerIntro` 0x7957C0 from the world camera's position is never asked for there), and sets the fade from 1.2 to 0, so the view cuts inside under the fade (read 2026-11-05)

#### interface/help_system

From [Help system](progress/interface/help_system.md).

- The creature help telling that a desire is wanted more or less after a reward or a punishment (CREATURE_HELP_TYPE_SHOW_LESSON_INCREASE_DESIRE and _DECREASE_DESIRE, kinds 2 and 3): their texts are in the help file, but no caller of `Creature::SendCandidateHelpScript` 0x4C9FE0 passes them (the feedback's lessons pass 0, 1, 4 and 5, `CreatureMental::LearnLessonsFromFeedback` 0x4E0FC0; the re-send of stacked messages, fn_004CA670, only sends kinds already stacked)
- The creature help's per-kind help level: the creature's level is 10 and never changes, above every kind's 1 to 4, so the test never drops a message

#### interface/help_system

From [Help system](progress/interface/help_system.md).

- Four of the creature help's raisers never run: the leash pulling the creature away from what it does (`Creature::LeashMakesCreatureStop` 0x4CEC00, "pulled away", reached only from `Creature::LeashMakesCreatureMoveToHand` 0x4CE8E0, which only the tug's packet calls, and nothing sends it); `Object::TellCreatureWhatToDoWhenAttachedToMe` 0x4CEE80 (from the same packet's `LeashForceCreatureToActOnAttachedObject` 0x4CEE70); `Creature::LeashForceCreatureToActOnHandObject` 0x4CECF0 (no caller); and `Creature::ForceActOnMapCoord` 0x4C6540's put-down within 5 m, which tests for something held inside its branch for an empty hand. openblack has none of them (`creature_help::k_PulledAway` is declared for the first and raised nowhere)

#### interface/main_menu

From [Main menu](progress/interface/main_menu.md).

- The menu's lists scrolling with the mouse wheel: the original's list control (`SetupList`: `AutoScroll` 0x409DD0,
  `KeyDown` 0x409EB0, `Drag` 0x40A110, `MouseDown` 0x40A370, `Draw` 0x40A5C0) scrolls by its bar and the keys only,
  and `SetupBox::DrawAll` 0x408340 reads no wheel; the wheel's delta (`LHMouse`'s accumulator, TheSystem +0x2C0) is
  read only by the cameras, the bindable actions, the key binding box and `CMouse::ProcessButtons` 0x61A150, which
  hands it to the bindable actions (xref readers 0xE85300), and the window's WM_MOUSEWHEEL case does nothing.
  openblack's menu wheel (`GameMenu::Wheel` to `List::Wheel`) is removed; the menu still takes the wheel while it is
  open, and the debug windows get theirs through ImGui first (`src/Debug/Gui.cpp`)
- Controls showing a tooltip when the pointer rests on them: no control is ever given one. A control's tooltip (`SetupControl` +0x224, null from its constructor) is set only by its `SetToolTip` slots 0 and 4 (0x409210, 0x4092F0), and no call site in the front end's or the boxes' code uses either slot on a control; the front end hands the null tooltip of the control under the pointer to the pointer's text each frame, so `FrontEnd::DrawMouse` 0x53C0F0 never draws it (read 2026-11-14: [the dialogs' controls' tooltips](../interface/tooltips.md#the-dialogs-controls-tooltips)). openblack's pointer draws no text (`DialogPainter::DrawPointer`). The former row is removed

#### interface/on_screen_text

From [On-screen text](progress/interface/on_screen_text.md).

- A challenge's title shown as the challenge begins: not in the original. No function or string of the exe puts a
  title up (its only strings with "title" are a new dance group's "Untitled %d", another "Untitled" and the save box's
  prompt), and none of the scripts decompiled in `refs/chl` (98 files) writes one; a challenge's title is only the text its snapshot keeps
  (`GScript::SnapShot` 0x6F97B0), which the temple's challenge room shows
  ([the challenge room](../interface/temple-interior.md)). A land's title is the land information box's, which a
  playground land's feature script fills (the row on game messages)

#### interface/options

From [Options](progress/interface/options.md).

- Screen resolution and the 3D card are picked outside the game, in its setup: openblack takes `--width`, `--height` and `--backend-type` on the command line
- Left handed swapping which mouse button picks up and which acts: the box only mirrors the hand. The profile's left-handed value (`Profile+0x81C`) is read only by `PlayerProfile::SetCurrentProfile` 0x66B7A0 and `CHand::UpdateLeftRightFromPlayer` 0x46E5F0, which set the hand's side; the hand's side (+0x484C, `isLeftHand`) is read only by `CHand::ToggleLeftRight` 0x46C260 (the mesh's winding) and `CHand::GetScaleMatrix` 0x46D080 (its mirror). The buttons stay as the key bindings set them, as in openblack (read 2026-10-31; the former options row, removed)

#### interface/profiles

From [Profiles](progress/interface/profiles.md).

- Creatures can be uploaded and downloaded online: the service no longer exists
- A picture of the creature kept with the profile: the creature's photos (`creatureshot_%i.jpg` in the profile's folder, `CreatureRoom::TakeScreenShot` 0x789A70 from fn_0047BC50) are read only by the creature web page export for the online creature service (fn_00578330), which no longer exists (already listed under creature/creature_cave; read 2026-11-05)

#### interface/scrolls_and_signs

From [Scrolls and signs](progress/interface/scrolls_and_signs.md).

- The temple's world room hiding or showing every challenge's scroll: its challenges button (`WorldRoom::WorldButtonDisplayChallengesCheckedSet` 0x79ED20 / `UncheckedSet` 0x79EDA0) only sets `DisplayChallenges`, which `WorldRoom::DrawAdditional` reads to draw the challenges' markers on the room's own map (`DrawChallenges` 0x79E030); the scrolls in the world are never hidden by it (their hidden flag +0x5C has no writer in play). openblack's button changes the room's map the same way (`src/3D/TempleToggles.cpp`). Read 2026-10-31 in bw1-decomp `WorldRoom.cpp`; the former row, removed
- Signposts that show their words when the hand is over them: the game's only signs are the did-you-know signs (MSH_O_INFO_SIGN, the highlight row 1), read by tapping. The highlight's tooltip override (`ScriptHighlight::GetOverwriteTapToolTip` 0x70AE10, 0xEF2 for an info of OBJECT_TYPE 1) never applies, as every row's type is 35, and no other object is a signpost. Read 2026-10-31; the former row, removed

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
- If the database can't be reached the player is told "Could not connect to database. Your creature and/or…" (`HELP_TEXT_COULD_NOT_CONNECT`): the server is gone

#### interface/tooltips

From [Tooltips](progress/interface/tooltips.md).

- The mouse picture's blink kind 2 (the button lit twice for 0.16 s in each 0.96 s, and the wheel's frames chosen from a cycle of 12 eightieths): written but never run, every caller asks for the key, the always lit button or the plain blink; see [../interface/tooltips.md](../interface/tooltips.md) ("animType 2 and 3 are never passed"). openblack's `Renderer::DrawKeyOrMouseMouse` had kind 2's blink branch, which nothing of ours passed either; it is removed (types 0 and 1 draw as before)
- Tooltips on the dialogs' controls: never shown, no control is given a tooltip; see [interface/main_menu](#interfacemain_menu) and [the dialogs' controls' tooltips](../interface/tooltips.md#the-dialogs-controls-tooltips). The former row is removed

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

#### multiplayer/maps/construct

Its progress page was removed (2026-10-26).

- The playground "Construct" (`Scripts/Playgrounds/construct.txt`, 32 lines, on `Data/Landscape/construct.lnd`), announced as "Testing landscape": a flat plain with one Norse house, a forester and a housewife, the player's temple and worship site, and a weeping stone with its reward stone: not the original's: it is openblack's own test land, added to openblack's test data in 2021 (now in the openblack-assets repository), on which the continuous-integration workflow takes its screenshot; neither file is in any folder of the game (`_gamedata/frozen`, checked 2026-10-26). Copied into the playground folder, the game's skirmish box would list it as "construct" like any other script there. Its progress page and its rows in the playground scripts, skirmish and land script commands pages were removed (2026-10-26)

#### multiplayer/maps/death_comes_to_those_that_wait

Its progress page was removed (2026-10-26).

- *Death Comes To Those That Wait* (`Scripts/Playgrounds/Death Comes To Those That Wait.txt`, 5,162 lines, on `Data/Landscape/Multi_Player/DCTTTW.lnd`), titled "Death Comes To Those That Wait -- By Fat Omen": a fan-made four-corner land by "Fat Omen", built on Four Gods (the same land size, the same corner temples and home towns for players three and four, and the same spot for the computer creatures): not the original's: neither the script nor `DCTTTW.lnd` is in any folder of the game (`_gamedata/frozen`, checked 2026-10-26). Its progress page and its rows in the playground scripts, skirmish, skirmish opponents, land script commands, fireflies and vanilla bug pages were removed (2026-10-26)
- What those rows had found in its script, kept here: its 119 named villagers are written with a misspelt command (`CREATE_SPECIAL_TOWNVILLAGER`), which a reader that skips unknown commands, as the game's does, never makes (openblack's reader logs and skips them; test `LandScriptReader.MisspeltSpecialTownVillagerIsSkipped`); town 21 is given a misspelt miracle (`EXPLSION_PU1`), which is not found, so nothing is given; one `FIRE_FLY_SPELL_REWARD_PROB` line lacks its closing bracket (the game reads such a line, openblack's reader skips it); two towns share the number 15 (a Greek town and the Tibetan town in the middle); a horse is given to "player eight", a name the game does not have; its 86 fireflies lie in two lists that partly repeat Three Gods' spots (only 35 on one of its trees), and its last firefly reward table is Land 2's

#### multiplayer/maps/firestorm

Not behaviour missing from the original: Firestorm is a copy of the game's own three-player online map, `Online Maps/mpm_3p_2.map` (an 8-byte header and a land script; checked in `_gamedata/frozen`, 2026-10-26). Its page, [Firestorm](progress/multiplayer/maps/firestorm.md), and its sections of the playground scripts page stay, parked with online and counted in no score (2026-10-26; [the grouping rules](progress/README.md#how-the-dashboard-groups-the-rows)). No `Scripts/Playgrounds/Firestorm.txt` exists in the game; the copy openblack's earlier checks read under that name had only its landscape path and some villagers' ages changed.

- Firestorm as a skirmish land: "No story script runs on a skirmish land" and "With no computer gods playing, a skirmish here has no opponent to beat; the skirmish's own rule decides when it ends": the skirmish box lists only `.\scripts\playgrounds\*.txt` (`SkirmishGameBox::InitControls` 0x544C5C), and the game has no `Scripts/Playgrounds/Firestorm.txt` (checked in `_gamedata/frozen`, 2026-11-02), so Firestorm is never a skirmish; it is played only as the online map `mpm_3p_2` (`Online Maps/localmaps.txt` line 5) The skirmish page's two rows about it go the same way (2026-11-03): "Firestorm: three temples, but no computer god is switched on, so the other two temples stand idle (as far as found)" and "Whether a skirmish can end without a winner (Firestorm's idle temples)": Firestorm is not offered by the skirmish box at all.
- Firestorm in the skirmish menu (the [playground scripts](progress/scripts/playground_scripts.md)' "It can be chosen from the game's skirmish menu"): the skirmish box lists only `.\scripts\playgrounds\*.txt`, as above, and Firestorm is an online map (moved 2026-11-07)

#### multiplayer/maps/island_wars

Its progress page was removed (2026-10-26).

- *Island Wars* (`Scripts/Playgrounds/Island Wars.txt`, 1,145 lines, on `mpm_4p_2.lnd`): a fan's rework, made with a community map editor (the script opens with a note saying so), of the game's four-player online map `mpm_4p_2`, with three computer gods and their creatures added (a leopard, a gorilla and a brown bear, all from Nemesis's mind file), the temples moved after their towns and the two small Greek towns' miracles reshuffled: not the original's: the reworked script is in no folder of the game (`_gamedata/frozen`, checked 2026-10-26; the game's own `Online Maps/mpm_4p_2.map` is there). Its progress page and its rows in the playground scripts, skirmish, skirmish opponents, rival creatures, Nemesis and creature mind script pages were removed (2026-10-26)
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

- Fire! Fire! I'm on Fire! (Land 3): its five endings never play. They wait for `NumberOfFanaticsFinished == TotalFanatics`, and `TotalFanatics` (refs/chl/FreeTheCreature.txt:5) is never written by any shipped script, so it stays 0, while each fisherman's end raises the other count first (:106620, :106645). So the last fisherman's cut scene, the lines "Thank you, Mighty One. You saved us all." (`HELP_TEXT_L3_LETHYS_FIREBALLS_06`), "Thank you. At least we all didn't die." (`HELP_TEXT_L3_LETHYS_FIREBALLS_08`), the good advisor's "Well, you tried to save them…" (`HELP_TEXT_L3_LETHYS_FIREBALLS_09`) and the evil advisor's "You let 'em fry…", and the story-log results (complete with alignment +1, 0 or −0.8) never happen. openblack runs the same scripts, so the same holds there: no code change.

#### story/advisors

From [Advisors and characters](progress/story/advisors.md).

- The audio CD left in the drive: the game's own disc check
- Multiplayer games have their own banter set of taunts: openblack has no multiplayer (the `MultiBanter` script exists, but there is no multiplayer game)

#### story/scroll_notice

From the gold and silver scroll pages ([gold scrolls](progress/story/gold_scrolls.md),
[silver scrolls](progress/story/silver_scrolls.md)), [Advisors and characters](progress/story/advisors.md) and
[Portals per land](progress/story/portals_per_land.md).

The scroll notice: while a quest's scroll waits to be clicked, the shared notice script would, at most every 30
seconds and only when no film is running, eject the good or the evil advisor, point it at the scroll and say the
quest's line ("Your godly attention is required here, Leader." (`HELP_TEXT_GENERAL_CHALLENGE_START_06`), "There's a Silver Reward Scroll down here, Boss." (`HELP_TEXT_GENERAL_CHALLENGE_START_01`),
and the others). Its test is "camera position near [scroll] radius NotifyDistance and scroll viewed"
(refs/chl/ChallengeNotify.txt:8; the same test in the copies at refs/chl/ChallengeNotify.txt:36, :154,
refs/chl/FinalGoldScroll.txt:116, refs/chl/setupland2.txt:17 and refs/chl/singingstonecircle.txt:150).

- The advisor never steps out to point at a waiting scroll: `NotifyDistance` is only declared
  (refs/chl/ChallengeNotify.txt:2) and nothing assigns it. No script source sets it, the compiled
  `Scripts/Quests/challenge.chl` reads it 8 times (PUSHF) and never stores it (no POPF), the compiled test is
  "distance LT NotifyDistance", and the name is not in `runblack.exe`. So it stays 0, a distance is never below 0,
  and the notice never plays. Only the wait for the click is left (the loop's `until ... clicked`, then the scroll is
  made active, refs/chl/ChallengeNotify.txt:26-28).
- The scripts concerned: ChallengeHighlightNotify, ChallengeHighlightNotifyWithEndVariable and
  DidYouKnowHighlightNotify (ChallengeNotify; the last is never called), ChallengeHighlightNotifyL2FinalScroll
  (FinalGoldScroll), CheckForKhazarChallengeHighlightNotify (SetupLand2) and SingingStonesNotify (SingingStoneCircle). The ogre's
  notice ChallengeNotifyOgre also ends on the creature coming within NotifyDistance (refs/chl/ChallengeNotify.txt:84),
  and is never run anyway (story/gold_scrolls/the_defending_ogres, below).
- The quests concerned (their notice rows left the progress pages on 2026-10-27): gold scrolls Choose Your Creature,
  Destroy It, Impress Village, Khazar's Fireball and Shield Challenges, Leave Through the Vortex on Lands 2, 3 and 4,
  The Heartbroken Man, The Lost Brother, The Sculptor, The Totem Puzzle, The Workshop, Undead Village, Worship Site and
  The Defending Ogres' scroll; silver scrolls Stanley the Wolf, Swap to Brown Bear, The Beach Temple Puzzle, The
  Creature Breeder, The Explorers, The Fish Puzzle, The Greedy Farmer, The Hermit, The Idol, The Immersion Mushrooms,
  The Lost Flock, The Magic Dragon, The Ogre, The Pied Piper, The Plague, The Rejuvenator, The Riddles, The
  Sacrifice, The Saviour (both of its scrolls), The Sea, The Shaolin, The Singing Stones (both lands), The Slavers and
  The Treacherous Path; Land 1's did-you-know scroll by the village store (refs/chl/followus.txt:757); and the
  tutorial island's lesson scrolls (refs/chl/LandControlT.txt:62 and five more).
- Notices that pass their own distance are not affected and keep their rows: Throwing Stones' (its own radius,
  refs/chl/throwingstones.txt:160), the Creature's Learning's and the Creature Guide's (CreatureDevelopment, radius
  given by the caller, refs/chl/CreatureDevelopment.txt:62), the creature swaps' (radius 100,
  refs/chl/SwapCreatures.txt:45), The Explorers' gate hint (refs/chl/TheMissionaries.txt:31) and the reminders that
  repeat on a timer (QuestHighlightNotify, QuestReminder).
- openblack runs the same compiled scripts, so the global is 0 there too and the notice never plays, as in the
  original: no code change. The quest checks agree: the advisor stepping out from the notice script
  (`SPIRIT_EJECT` in its task) is not found in the quest runs of 2026-10-27 (The Hermit, The Saviour, The Lost
  Flock, The Pied Piper, The Ogre).

#### story/creature_guide

From [Creature Guide](progress/story/creature_guide.md).

- The guide exists only in the first land of the story; not in skirmish, multiplayer or the Creature Isle: nothing to do beyond the story itself
- The development stages "meeting its guide", "the guide explains the history" and "helping a town" are never set by any shipped script (the guide's chapters jump from making friends to miracles, impressing, fighting and full maturity): data quirk
- An older, unshipped set of lessons: a horse guide explaining its past on the mountain (Nemesis's temple, its loneliness), teaching miracles with fireball and heal seeds, helping and impressing a town, a friendship scene where it shrinks itself with a miracle: in the source but not in the program
- An unshipped storm brewing scene and older stand-alone storm, meeting and fight scripts: not in the program
- A shipped but never started script for taking over the land's villages, where the advisors guess the big creature wants them to be the Aztec village's god: only started by an unshipped control script
- Test scripts that run the whole guide story or the storm alone: not started by the game
- A cut line in a later land says the Creed lies in the body of the one the player knew as the Guide: commented out of the fourth land's script
- The Yes and No help sounds meant to play while the player brings the creature to the guide (within 30, within 20, or moving away) never play: their loop runs only while the creature is more than 45 from the guide, but it starts right after a loop that has already waited for the creature to come within 45 (`refs/chl/CreatureGuide.txt:1216`, after the loop at `:1203`), and the moving-away test compares with a starting distance that is never set

#### story/creature_isle

Creature Isle: The expansion: a new island where the creature, not the player, is the hero, with the Brotherhood of Creatures and Tyke. openblack targets the original game only.

- The Creature Isle land and its challenges: the expansion is not supported
- Tyke, the young creature who follows the player's: the expansion is not supported
- The Brotherhood's trials: the expansion is not supported
- The expansion's extra creature spells: the expansion is not supported (its spells are commented out in `src/Enums.h`)

#### story/cutscenes

From [Cut scenes and films](progress/story/cutscenes.md).

- An animal or a creature walking a camera track for a scene (`WALK_PATH` on one): the original's script command would start the same living walk it gives a villager, but no shipped script gives one a track. The command is called 4 times in all (refs/chl/BlindWoman.txt:481 on the blind woman, refs/chl/followus.txt:315 and :316 on the two sharks, :576 on the father), never on an animal or a creature; openblack walks the villagers and the sharks, and reports the rest as not implemented ([Walking a camera track](../beings/villager-speed-and-scripts.md#walking-a-camera-track))

#### story/ending

From [The ending](progress/story/ending.md).

- Nemesis's mirror fight against the player's creature's opposite self: in the scripts but never started; [gold_scrolls/so_this_is_a_fight_to_the_death.md](progress/story/gold_scrolls/so_this_is_a_fight_to_the_death.md)
- A script steering Nemesis's battle tactics: in the scripts but never started (checked 2026-11-08: its file holds two scripts, the tactics loop and the per-target action it starts; no instruction of the compiled `challenge.chl` runs the loop, it is not on the program's auto-start list, no statement of the decompiled scripts names it but its own, refs/chl/ControlNemesisBattleStrategy.txt:72, and the frozen exe holds no reference to either name (`xref.py str` finds none, where it finds the story's control script the game starts by name); the per-target action is started only by the loop, refs/chl/ControlNemesisBattleStrategy.txt:110 to :173. Its row is gone from [challenge_scripts.md](progress/scripts/challenge_scripts.md))

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
- A gold scroll is put at the creature gate, 14 up; the evil advisor points it out when the camera comes near: "There's a Silver Reward Scroll down here, Boss." (`HELP_TEXT_GENERAL_CHALLENGE_START_01`) (the shipped line, wrong scroll colour and all): never started by the game
- It never opens a story-log entry: it only updates one (to 0.75, reminder "We need to choose a Creature!" (`HELP_TEXT_REMINDER_54`)), so the log entry would have to come from elsewhere: never started by the game
- Two helper scripts for the gate-stone hunt (the advisor spotting the second gate stone, the trainer's reminder that the stone isn't carved) are in the file but never run; a "start russ" note marks where the hunt was cut out: never started by the game
- The three creatures are made in the valley; the trainer is made at her hut, walks to her door and to a talking spot, and turns to the camera: never started by the game
- Trainer: "The Gate Stones are together. Pass through and…" (`HELP_TEXT_NEWEST_GET_CREATURE_29`) (a comment holds an earlier joke line): never started by the game
- The camera moves to the plinth; the gate-stone sound plays, the camera shakes (amplitude 0.1 for 7 seconds within 20) and the plinth opens with the stones sinking: never started by the game
- The camera looks at the gate chain; the gates open with the bolt sound; the trainer beckons; the gate-open sound plays as the camera pulls back: never started by the game
- The camera flies to the valley along the prepared path with the creature-choosing music; the creatures turn to the camera: never started by the game
- The advisors talk as the creatures act up: good "Look at them. Just look at them. These…" (`HELP_TEXT_NEWEST_GET_CREATURE_05`); evil "They certainly are. But not quite as big…" (`HELP_TEXT_NEWEST_GET_CREATURE_06`); good "Not yet, maybe. But they can become the…" (`HELP_TEXT_NEWEST_GET_CREATURE_07`); evil "Now that I'd like to see. We must…" (`HELP_TEXT_NEWEST_GET_CREATURE_08`); good "Any. They're all special." (`HELP_TEXT_NEWEST_GET_CREATURE_09`); evil "If rather small at the moment." (`HELP_TEXT_NEWEST_GET_CREATURE_10`); good "Shh.": never started by the game
- The music stops and the camera is kept to the valley: never started by the game
- Holding the hand over a creature makes it beg to be picked; the first time over each, the advisors pitch it: cow (good "We could have the cow. A strong and noble beast." (`HELP_TEXT_NEWEST_GET_CREATURE_12`) evil "What? Not the fierce, lethal tiger? Click the…" (`HELP_TEXT_NEWEST_GET_CREATURE_13`)), ape (good "Hmm. How about the ape. Intelligent and quick to learn?" (`HELP_TEXT_NEWEST_GET_CREATURE_14`)), tiger (evil "I'm up for the tiger. Look at those claws." (`HELP_TEXT_NEWEST_GET_CREATURE_15`)): never started by the game
- Each creature turns to the camera again every 5 seconds: never started by the game
- Clicking a creature once asks "Are you sure you want this Creature? …" (`HELP_TEXT_EXTRA_TRAINER_05` or `HELP_TEXT_CREATURE_SWAP_CONFIRM_03`) (good); clicking the same one again chooses it; clicking another resets the count: never started by the game
- On choosing: a 10-second close-up, evil "We've chosen a Creature." (`HELP_TEXT_NEWEST_GET_CREATURE_16`), a 3-second fade to black, the creature looks at the camera, becomes the player's creature, is moved to its pen and given it as home; the other two are deleted: never started by the game
- The creature development scripts are started and the scroll is marked complete with the reminder "We should speak to the sculptor about the Gate Stones." (`HELP_TEXT_NEW_REMINDER_09`) (a leftover from the full version's order): never started by the game

#### story/gold_scrolls/destroy_it

From [Destroy it!](progress/story/gold_scrolls/destroy_it.md).

- The creature takes no part in this scroll: nothing in the script touches it
- Two flags meant to say whether the left and right towns are needed are set but read only by the fly-over itself: nothing else uses them

#### story/gold_scrolls/fire_fire_im_on_fire

From [Fire! Fire! I'm on Fire!](progress/story/gold_scrolls/fire_fire_im_on_fire.md).

- A test switch skips the 60-second wait: only the test launcher uses it

#### story/gold_scrolls/i_have_a_surprise_for_you

From [I have a surprise for you.](progress/story/gold_scrolls/i_have_a_surprise_for_you.md).

- The "village taken back" lines can never play: the check for Nemesis retaking a village tests the same condition as the check for the player owning it, so it is never reached, and the player's "retaken the town" lines never play either because a village is never marked as lost. Unreachable lines: good "Excellent! We've retaken the town!" (`HELP_TEXT_NEW_L5_VILLAGE1_03`), Nemesis "Ha! That is how it should be!" (`HELP_TEXT_NEW_L5_VILLAGE1_04`), evil "We're the best. We are! Way to go!" (`HELP_TEXT_NEW_L5_VILLAGE2_14`), Nemesis "Ha. That was too easy to be a Challenge!" (`HELP_TEXT_NEW_L5_VILLAGE2_15`), evil "Ha! Maybe Nemesis isn't as powerful as he…" (`HELP_TEXT_NEW_L5_VILLAGE3_31`) (which would have pointed at the Greek village while speaking of the Tibetan one) and Nemesis "Welcome back, my worshippers. Do not stray again." (`HELP_TEXT_NEW_L5_VILLAGE3_32`): never reachable in the game
- Nemesis's "You'll find he won't be of much use…" (`HELP_TEXT_NEW_L5_INTRO_11`) is commented out of the opening film: never in the game
- The advisors' "Where's our Creature going?" and "Where's he off to now?" (`HELP_TEXT_NEW_L5_CREATURE_CURSE_02`) were cut from the nightly lines ("the creature doesn't really move too much into Nemesis's land"): never in the game
- Nemesis's "I have one more surprise for you." (`HELP_TEXT_NEW_L5_CREATURE_CURSE_07`) has no script that says it: never in the game
- An earlier curse script is compiled into the game but nothing starts it: it struck every 60 seconds at night, took 0.1 strength (Aztec), 0.1 fatness (Greek, not height) and 0.2 alignment (Tibetan) per strike, played a first film ("Nemesis curse has harmed our Creature!" (`HELP_TEXT_NEW_L5_CREATURE_CURSE_08`)) and later "Our Creature is suffering further from Nemesis' curse!" (`HELP_TEXT_NEW_L5_CREATURE_CURSE_09`) then "He's heading towards Nemesis himself!" (`HELP_TEXT_NEW_L5_CREATURE_CURSE_10`) or, if leashed, "The dark of night must be the key to this!" (`HELP_TEXT_NEW_L5_CREATURE_CURSE_11`), kept the creature's gains while cursed so they could be added back, and ended only when the player owned Nemesis's village: never started by the game (checked 2026-11-08: no instruction of the compiled `challenge.chl` runs it, it is not on the program's auto-start list, no statement of the decompiled scripts names it but its own, refs/chl/CreatureCurse.txt:172, and the frozen exe holds no reference to its name (`xref.py str`); the script that leads the creature towards Nemesis's town, refs/chl/CreatureCurse.txt:154, is started only by it, :274, so it never runs either)
- Its helper that walks the creature to Nemesis's village and waits until it is leashed or within 100 is also unused: never started by the game

#### story/gold_scrolls/impress_village

From [Impress Village](progress/story/gold_scrolls/impress_village.md).

- Two spare influence demo scripts exist in the hand demo file; one has its demo commented out, the other plays a second influence recording (`influence2.hnd`); nothing in the game runs them: never in the shipped game
- An earlier place for the 0 log entry, after the gesture demo, is commented out: commented out
- A wood check on the store (2,800 wood as well as food) is commented out: commented out
- A note to clear the repeat-miracle icon and remove the leash "for confusion" is left as a to-do: a to-do note in the source

#### story/gold_scrolls/khazars_fireball_challenge

From [Khazar's Fireball Challenge](progress/story/gold_scrolls/khazars_fireball_challenge.md).

- Unused lines in the text table: Khazar's "Here are some Fireball Miracle seeds." (`HELP_TEXT_NEW_LAND_2_FIREBALL_05`) and "Using these fireballs.", the evil advisor's "Oh Boss. You didn't hit anything at all." (`HELP_TEXT_FIREBALL_CHALLENGE_EXTRA_02`), and two entries marked "NOT USED": in the text table but never spoken by the shipped script

#### story/gold_scrolls/khazars_shield_challenge

From [Khazar's Shield Challenge](progress/story/gold_scrolls/khazars_shield_challenge.md).

- The hand demo's line "Make sure you join up with the start position." (`HELP_TEXT_HAND_DEMO_SHIELD_03`) is cut; the script's comment says the speech was too slow for the demo: in the text table, commented out in the demo
- Unused retry lines in the text table: "I give you more Seeds to practice with." (`HELP_TEXT_NEW_LAND_2_SHIELD_12`), "You were unsuccessful. Try again." (`HELP_TEXT_NEW_LAND_2_SHIELD_13`), "Try recasting the Shield Miracle." (`HELP_TEXT_NEW_LAND_2_SHIELD_14`) and "Draw a circle with the Action Button pressed…" (`HELP_TEXT_NEW_LAND_2_SHIELD_23`), which fit the multi-attempt loop the shipped script never gives; two more entries are marked "NOT USED": never spoken by the shipped script
- A commented-out timer that would have dropped the boulders after 30 seconds whatever the player did: left in the source as comments
- A commented-out final log update at the end: commented out

#### story/gold_scrolls/leave_through_the_vortex_land_1

From [Leave Through the Vortex (Land 1)](progress/story/gold_scrolls/leave_through_the_vortex_land_1.md).

- The prepared "Click the Action Button on the Scroll and…" (`HELP_TEXT_GENERAL_LEAVE_THROUGH_VORTEX_01`) line is never said (the reminder's own first notice is commented out): cut use of an existing line
- If the storm stops existing before the scroll is clicked, the storm's script would end, the land's control script finish and the story load the second land at once, skipping the rest of the quest: the storm never stops existing while its script runs (read 2026-11-14): the storm's script holds it in a variable to its end (refs/chl/CreatureGuide.txt:23561, :24198, the `while Storm exists` loop at :25374), and a weather thing whose storm has run out (its life is set to 2000000 at :24213) is deleted only when no script holds it (`WeatherThing::Process` 0x774230 asks `IsInScript`, vt +0x448, which `ScriptManage::IncreaseReferenceCount` 0x70D5F0 sets with the first reference), so `exists` stays true; no other script names it, and the loop ends only when the quest stops the guide's scripts (refs/chl/leavethroughvortexl1.txt:48795). openblack keeps a held weather thing the same (`weather_thing::Process`, [weather](../world/weather.md#chl-weather-objects))

#### story/gold_scrolls/leave_through_the_vortex_land_2

From [Leave through the vortex (Land 2)](progress/story/gold_scrolls/leave_through_the_vortex_land_2.md).

- The player has no creature here: Lethys has taken it: the script doesn't touch a creature

#### story/gold_scrolls/lethys_has_taken_our_creature

From [Lethys has taken our Creature!](progress/story/gold_scrolls/lethys_has_taken_our_creature.md).

- A script that would ring Lethys's temple with five lightning storms (time 80, no wind, heavy forks) is written but its start is commented out: never started by the game (checked 2026-11-08: no instruction of the compiled `challenge.chl` runs it, it is not on the program's auto-start list, no statement of the decompiled scripts names it but its own, refs/chl/lethysvortex.txt:25, and the frozen exe holds no reference to its name, so no kept run shows it; `tools/checks/story/rows_seen.py` lists such scripts)
- A high camera spot is set up but never used; the good advisor's "No!" is in the text table but unused: unused
- Pointing the advisors at the creature is commented out: commented out

#### story/gold_scrolls/nemesis_no

From [Nemesis. No!](progress/story/gold_scrolls/nemesis_no.md).

- Lines written but not played: Nemesis "Did you think you could hide the Creed from me?" (`HELP_TEXT_KHAZAR_DIES_14`), "Creature, you have something for me." (`HELP_TEXT_KHAZAR_DIES_08`), "Lethys, retrieve the Creed for me." (`HELP_TEXT_KHAZAR_DIES_15`); the good advisor "That must have been the Creed the Guide…" (`HELP_TEXT_KHAZAR_DIES_19`) and the evil advisor "And we lost it." (`HELP_TEXT_KHAZAR_DIES_20`); good "Maybe we haven't lost it forever, Demon." (`HELP_TEXT_KHAZAR_DIES_21`) and evil "I hope you're right, Beardy." (`HELP_TEXT_KHAZAR_DIES_22`): commented out in the source
- Lines in the text table never used by any script: Nemesis "I will be the only god." (`HELP_TEXT_KHAZAR_DIES_03` or `HELP_TEXT_KHAZAR_DIES_13`) (an earlier copy), Khazar "I'm losing my…" (`HELP_TEXT_KHAZAR_DIES_06`), good "Oh no! Khazar's Creature!" (`HELP_TEXT_KHAZAR_DIES_09`), Nemesis "Lethys, use your Creature to bring the Creed to me." (`HELP_TEXT_KHAZAR_DIES_10`), evil "He's taken the Creed from Khazar's Creature!" (`HELP_TEXT_KHAZAR_DIES_11`), good "So this is what the Guide meant!" (`HELP_TEXT_KHAZAR_DIES_12`): never used by any script
- Dropping the Creed as an object for Lethys's creature to pick up, and freezing Khazar's creature with a creature spell, were tried and replaced by the glow moving from one creature to the other: commented out in the source

#### story/gold_scrolls/nemesis_shielded_village

From [Nemesis's Shielded Village](progress/story/gold_scrolls/nemesis_shielded_village.md).

- The shield's creature check at the centre stone does nothing: the reaction it was meant to start (the creature running away) is commented out: never in the game
- The control script still holds a comment that the shield script replaced the Tibetan village's old lines, though its capture film is still there: nothing to do
- The third worshipper's own line "No one shall harm us or test our faith." (`HELP_TEXT_NEW_L5_VILLAGE3_04`) was dropped as a bad recording (it is filed under the good advisor's voice): never in the game
- A fuller version had shouts when worshippers gave up or were hit ("Argh! I'm giving up!" (`HELP_TEXT_NEW_L5_VILLAGE3_10`), "Ow! I give up!" (`HELP_TEXT_NEW_L5_VILLAGE3_12`), "I've had enough. This is too dangerous." (`HELP_TEXT_NEW_L5_VILLAGE3_14`), screams), advisors' comments on each ("Perfect. He survived. Let's move quickly before he recovers." (`HELP_TEXT_NEW_L5_VILLAGE3_16`), "I spy a dead guy" (`HELP_TEXT_NEW_L5_VILLAGE3_18`), "He's toast.", "You iced him, Boss." (`HELP_TEXT_NEW_L5_VILLAGE3_20`), "You got him, but the guy ran off." (`HELP_TEXT_NEW_L5_VILLAGE3_21`), "He'll be back sooner or later to reactivate…" (`HELP_TEXT_NEW_L5_VILLAGE3_22`)) and "One of the rays has been reactivated!" (`HELP_TEXT_NEW_L5_VILLAGE3_23`); no script says them: never in the game
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
- An earlier mirror fight ("Creature Mirror Fight") is compiled into the game but nothing starts it (only its single-quest test launcher; checked 2026-11-08: no RUN of it in `challenge.chl`, not on the auto-start list, named by no statement but its own, refs/chl/creaturemirrorfight.txt:1, and `xref.py str` finds no reference in the exe; its progress row is gone from [challenge_scripts.md](progress/scripts/challenge_scripts.md)): Nemesis's hand flies around the player's creature, he says "Your Creature has proved itself many times. But…" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_01`), makes a copy of the creature next to it with the opposite alignment, only anger as a desire (at half) and fully grown, and says "This is your Creature if you'd been the…" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_02`) and "Watch and weep as it shows its superior…" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_03`): never started by the game
- In it the advisors cling to the screen edges and say "We'll give it a pummelling!" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_04`) (evil) and "Er, yes. I hope so." (`HELP_TEXT_CREATURE_MIRROR_FIGHT_05`) (good); the fight starts when the creatures are within 50; winning: "We did it! Did you see that? Oh…" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_08`) and the copy is deleted; losing: "We're losing. Stop and let our Creature heal." (`HELP_TEXT_CREATURE_MIRROR_FIGHT_06`) and "Come on! Crush him!" (`HELP_TEXT_CREATURE_MIRROR_FIGHT_07`): never started by the game
- A cut speech by Nemesis over a monsoon storm at 4:30 pm ("My dream was to be the only god." (`HELP_TEXT_NEMESIS_SPEAKS2_01`), "To have the whole of Eden worship me." (`HELP_TEXT_NEMESIS_SPEAKS2_02`), "But you stand in opposition." (`HELP_TEXT_NEMESIS_SPEAKS2_03`), "So this is a fight to the death" (`HELP_TEXT_NEMESIS_SPEAKS2_04`)) is not compiled into the game; its last line is used as this quest's log title: not in the shipped `challenge.chl`
- A script for Nemesis's battle tactics is compiled but never started: see [ending.md](progress/story/ending.md)
- Two of the fight's scripts are compiled but started by nothing: one that waits and then turns the camera on Nemesis's creature, and the one that would push the creature's alignment to an extreme (checked 2026-11-08: no instruction of the compiled `challenge.chl` runs either, neither is on the program's auto-start list, no statement of the decompiled scripts names them but their own, refs/chl/thebigfight.txt:90 and :315, and the frozen exe holds no reference to their names; `tools/checks/story/rows_seen.py` lists such scripts)
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
- A compiled but never started good-and-evil leash lesson: back at the pen the trainer shows the fluffy Leash of Compassion and the spiky Leash of Aggression, a dying farmer appears, the player leads the creature to him on either leash; "That's it! I can break the curse now!" (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_LEASH_GOOD_AND_EVIL_08`), "I have nothing more to teach you." (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_LEASH_GOOD_AND_EVIL_10`), "He's all yours. Now it's up to you." (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_LEASH_GOOD_AND_EVIL_11`) and she walks off to join the village (scroll at 0.92): nothing runs it
- A compiled script that runs the five lessons in a row, started only by a commented-out line of the land's control script and an unshipped test launcher: nothing runs it
- Older, shorter drafts of the five lessons as separate files, with placeholder lines typed into the script and a man as trainer: not in the game's program
- An unshipped lesson where the creature needs to relieve itself and is led to a spot ("Er, Mighty One? Your Creature needs to relieve himself." (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_LEARN_TO_POO_01`), advisors' bowel jokes), and a leash version of it: not in the game's program
- An unshipped thirst lesson: nothing to drink at home, so the creature is leashed and dragged to water ("I think your Creature is thirsty, powerful one." (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_THIRSTY_01`)), and a leash version of it: not in the game's program
- An unshipped anger lesson: villagers crowd the creature, it gets angry, and stroking or slapping changes how easily it angers ("Remember, punishment and reward is the path to…" (`HELP_TEXT_NEW_DEVELOPMENT_STAGE_ANGRY_11`)): not in the game's program
- Unshipped leash drafts: tying the leash to an object (with the three leashes explained), tying it to a walking villager so the creature follows and learns from them, pulling to a tree and picking up a cow: not in the game's program
- A misnamed "learn to play" file that holds a draft of the guide's help-a-town lesson: the guide's unshipped lessons are listed in [creature_guide.md](progress/story/creature_guide.md)
- Killing or losing the creature during the lessons, which would leave every step that waits on it waiting for ever: the game can do neither there (read 2026-11-14): harm that takes the last of a creature's life faints it, or gives it all its life back when it cannot die (`Creature::DestroyedByEffect` 0x476F70, `Creature::ReduceLife` 0x47DD00, bw1-decomp Creature.cpp), and a fainted creature is carried home and gets up ([fighting](progress/creature/fighting.md)); the only change of the player's creature on Land 1 is the Lost Flock's swap, whose quest waits for a flag the land's control script sets only after the last lesson (refs/chl/thelostflock.txt:59632, refs/chl/LandControl1.txt:68081..68083)

#### story/gold_scrolls/the_defending_ogres

From [The Defending Ogres](progress/story/gold_scrolls/the_defending_ogres.md).

- A notify script written for this scroll (an advisor nagging until the lair is clicked or the creature comes within 100) is never used by the game: written but never run
- A test launcher for the ogre alone (it sets up the land, logs the scroll as a silver challenge, puts up the lair's scroll and runs Sleg) exists in the source but is not in the shipped scripts: not compiled into `challenge.chl`
- Curing a gremlin back into a villager (turning it human and adding it to the home village) was planned but is commented out, so the "You've cured all the cursed Villagers." (`HELP_TEXT_NEW_L4_OGRE_12`) / "They've recovered fully." (`HELP_TEXT_NEW_L4_OGRE_13`) and "All those little ogres have been dealt with." (`HELP_TEXT_NEW_L4_OGRE_14`) / "You haven't cured all the cursed Villagers." (`HELP_TEXT_NEW_L4_OGRE_15`) lines can never play: the count of cured gremlins never rises
- An unused gold-scroll notify for the ogre (above) and the ogre-only test launcher: unused or not compiled

#### story/gold_scrolls/the_heartbroken_man

From [The Heartbroken Man](progress/story/gold_scrolls/the_heartbroken_man.md).

- An early draft of the lines survives in the source comments: Adam was "Knobbo", Keiko his "girlfriend", and the woman was simply his friend and neighbour: comments only
- A test launcher (not compiled into the game) runs this quest on its own by setting the start flag: never in the shipped `challenge.chl`

#### story/gold_scrolls/the_lost_brother

From [The Lost Brother](progress/story/gold_scrolls/the_lost_brother.md).

- A cut ending where both siblings arrive home together: "We offer thanks, O mighty spirit. Here. The Gate Stone." (`HELP_TEXT_LOST_BROTHER_14`) and the good advisor: "A Gate Stone! Let's give it to the…" (`HELP_TEXT_LOST_BROTHER_10`): commented out of the script; the lines are in the text table
- A cut scene for both siblings killed: "The siblings are dead. You gotta trash the…" (`HELP_TEXT_LOST_BROTHER_08`): commented out
- A cut 60-second countdown and slow health loss, ending with the brother dying of his fever: "The lost one must be at death's door…" (`HELP_TEXT_LOST_BROTHER_16`) / "Maybe we've got better things to do. You…" (`HELP_TEXT_LOST_BROTHER_17`): commented out
- A cut scene for flying the brother to his sister: only a commented-out call; no such script
- An older copy of the quest run by an unshipped demo control script: not in the game's program

#### story/gold_scrolls/the_madness

The Madness: A cut gold-scroll story chapter in which Nemesis takes over the player's creature: it is made strong, turned to the opposite alignment and sent to throw fireballs at a village's store and houses while the advisors look on, then confined for a minute and a half until the madness wears off. It was never compiled into the game; the shipped fifth land does the same story beat as the curse ([i_have_a_surprise_for_you.md](progress/story/gold_scrolls/i_have_a_surprise_for_you.md)).

- The script is not in the list of files compiled into the shipped `challenge.chl`, so the game does not contain it: never started by the game: only its own test launcher (also uncompiled) runs it
- Its challenge name still has an entry in the compiler's list of challenges, so it was a planned story chapter: never started by the game
- Its title "The Madness" and the reminder "Your Creature is still out of control!" (`HELP_TEXT_REMINDER_07`) are in the shipped text table, and so are all eleven lines of its dialogue (Nemesis's four and the advisors' seven): never started by the game: the text survived the cut; no shipped script uses the title
- Its markers fit the fifth land: the village it attacks is the Japanese village there (its centre is about 3 units from the land's town 7, a neutral town at the start), and its camera points are around that village: never started by the game; no other land has a town within 230 units
- The shipped fifth land tells the same story differently: Nemesis curses the creature's strength, size and alignment at the start and the player lifts it in three steps: see [i_have_a_surprise_for_you.md](progress/story/gold_scrolls/i_have_a_surprise_for_you.md)
- There is no scroll to click: the chapter would start as soon as whatever started it ran the script: never started by the game
- The creature's leash is switched off and the creature is walked to a spot beside the village; the script waits until it is within 150 of it: never started by the game
- The scroll is logged in the story log as a gold quest titled "The Madness", at nothing done, with the reminder "Your Creature is still out of control!" (`HELP_TEXT_REMINDER_07`) (good advisor): never started by the game
- The creature is given a Strength miracle that never runs out, and is taught everything: never started by the game
- The time of day is set to 18:30 and game time is stopped: never started by the game
- The camera locks onto the creature and follows it, the lens widening to 35 over 8 seconds: never started by the game
- Nemesis: "You are close to your Creature." (`HELP_TEXT_CREATURE_RUNS_AMOK_12`) then "After all this time, you think you know him." (`HELP_TEXT_CREATURE_RUNS_AMOK_13`): never started by the game
- The camera swings round to a point east of the village over 5 seconds, aimed at the creature's chest (80% of its height), then rises 20 over 9 seconds: never started by the game
- Nemesis: "But I am stronger than you will ever be." (`HELP_TEXT_CREATURE_RUNS_AMOK_01`); when the creature reaches its spot (within 5): "And now he obeys only me." (`HELP_TEXT_CREATURE_RUNS_AMOK_02`): never started by the game
- The creature looks into the camera; two seconds later the camera cuts in close to it by the store and rises slowly: never started by the game
- A target marker effect is put on the creature for 30 seconds; it is made to want only to be angry, plays its angry animation twice, and the camera shakes (amplitude 0.5 for 2 seconds within 200): never started by the game
- The creature's alignment is flipped to its mirror (a good creature becomes as evil as it was good, and the other way round), in steps of 0.025: never started by the game
- The creature is allowed to attack its own side's town, looks at the store and throws a fireball at it at full energy, then plays angry again: never started by the game; the store is looked up by its position, about 50 from the village centre
- Both advisors step out. Good: "He's out of control! Oh this is terrible!" (`HELP_TEXT_CREATURE_RUNS_AMOK_03`) Evil: "We've got to prevent him from doing too…" (`HELP_TEXT_CREATURE_RUNS_AMOK_04`): never started by the game
- The creature is moved to the village centre; the camera pulls back above the village. Good: "We've got to get him back, too. He's…" (`HELP_TEXT_CREATURE_RUNS_AMOK_05`): never started by the game
- It throws a fireball at a house; the cut scene ends but the advisors keep talking. Evil: "Restrain him first!" It throws a fireball at a second house. Evil: "That's it. We must keep him harmless until…" (`HELP_TEXT_CREATURE_RUNS_AMOK_07`) Both advisors go home: never started by the game
- The creature is let go, still wanting only anger, and kept within 90 of the first house for 90 seconds; the player can only limit the damage it does: never started by the game; there is no success or failure test: the chapter simply waits
- After 90 seconds the leash is switched back on, the Strength miracle is removed, the confinement and the right to attack its own town are taken away: never started by the game
- Good: "The Creature did some damage when he was…" (`HELP_TEXT_CREATURE_RUNS_AMOK_08`) Evil: "I know. Let's get it cleaned up. As…" (`HELP_TEXT_CREATURE_RUNS_AMOK_09`) Good: "Cleanliness is next to godliness." (`HELP_TEXT_CREATURE_RUNS_AMOK_10`) Evil: "Doh!": never started by the game
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

- A second line, "It's beautiful and it will be extremely useful later." (`HELP_TEXT_CITADEL_GUIDE_02`), is in the text table but commented out of the scene: cut line
- No creature is involved (it hasn't been chosen yet): no creature is involved
- Commented-out camera moves straight onto the gold scroll and back to the temple: cut from the scene
- An older draft of the temple's opening (rotating the camera, carrying wood and food to the builders, the kinds of scroll): not in the shipped program: [../silver_scrolls/see_the_citadel.md](#storysilver_scrollssee_the_citadel)
- The good advisor's "Press the Escape key to leave the Temple." (`HELP_TEXT_CITADEL_GUIDE_13`) after a minute inside, then every 30 seconds: in
  the script, but its timer counts game turns and going in pauses the game offline (0x553F83; the turn's step 0x54E507
  is skipped while paused), so it stands at 60 until the player leaves and the line is never said (2026-10-28, the
  page's "The reminder to leave never comes in the original either")

#### story/gold_scrolls/the_totem_puzzle

From [The Totem Puzzle](progress/story/gold_scrolls/the_totem_puzzle.md).

- A first version of this quest used the game's built-in totem puzzle (the third totem layout) at the stone and waited for it to be solved; it is left commented out: never in the shipped game
- A line from the elder, "The rewards are surely worth the effort. But…" (`HELP_TEXT_NEW_L4_METEORITES_47`), is in the text table but cut from the scene: cut from the scene
- A test launcher, "Simon Says" (not compiled into the game), runs this quest on its own: never in the shipped `challenge.chl`

#### story/gold_scrolls/the_workshop

From [The Workshop](progress/story/gold_scrolls/the_workshop.md).

- A second info sign on combining scaffolds into buildings (one to seven scaffolds) is commented out; its text is still in the text table: never in the shipped game
- Earlier places for the 0 and 0.1 log entries, after the films, are commented out: never in the shipped game
- A comment "need a highlight to say about the homeless" marks where the second scroll was added: a source comment only

#### story/gold_scrolls/undead_village

From [Undead Village](progress/story/gold_scrolls/undead_village.md).

- A line "And they know the location of the object you desire." (`HELP_TEXT_NEW_L4_METEORITES_09`) is recorded but taken out of the scene and marked as no longer needed: never played
- The quest's "finished" entry is marked in the script as possibly belonging elsewhere, but it stays at the moment the curse lifts: a note only; no behaviour

#### story/gold_scrolls/villager_catch

Villager Catch: A cut gold-scroll mini-game in which a hostile god flings ten villagers out of a town, one at a time, and the player must catch each in the hand before it lands. It was never compiled into the game and its lines were never written into the text table; it has no in-game title, so this file uses the script's own name.

- The script is not among the files compiled into the shipped `challenge.chl`: never started by the game: only its own (uncompiled) test launcher runs it
- Its challenge name keeps an entry in the compiler's list of challenges: never started by the game
- None of its 35 lines are in the shipped text table; the source's comments give the intended words of the main ones, and four "Nemesis comment required" notes show his lines were never written: never started by the game
- It sets a "villager catch completed" flag "that the control script uses" (a comment credits another designer), but no land's control script, shipped or cut, reads it: never started by the game
- Its town marker is 67 from the player's home town on the second land (the Norse town); the source calls it Nemesis's town; no other land has a town within 230: never started by the game; which land it was meant for is not certain
- A gold scroll is put over a point just north of the town as soon as the script starts: never started by the game
- The evil advisor's scroll prompt ("Whew! As if you ain't busy enough, here's…" (`HELP_TEXT_GENERAL_CHALLENGE_START_09`)) is commented out, so nothing points the scroll out: never started by the game
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
- "You have lost the game. Click YES to…" (`HELP_TEXT_WIN_GAME_01` or `HELP_TEXT_LOSE_MULTIPLAYER_GAME_01`) (twice) and the same for "the Skirmish Game": nothing in the 1.2 program shows them; the end box uses "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`) with Watch Game and Leave Game buttons
- "You have lost the game. The winner was %s." (`HELP_TEXT_WIN_GAME_02`), "Game winner: %s" and "All the players lost. It's a draw." (`HELP_TEXT_WIN_GAME_06`): nothing in the 1.2 program shows them
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
- A football match impressing those watching through its own impressiveness kind: see town/football above (read 2026-11-05)

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
- The good advisor points at the chimp; the record opens as "Chimp Posse", success and alignment 0, reminder "It looks like these chimps have lost a toy." (`HELP_TEXT_REMINDER_50`): never started by the game
- Good advisor (comment's wording): "Oooh look at that. A group of chimps. Aaah sweet.": never started by the game: not in the text table
- After 2 seconds the camera moves to the ball over 4 seconds; the evil advisor points at it: "Ahh diddums. They seem to have lost their ball in the neighbours garden.": never started by the game: not in the text table
- Nemesis's creature is put about 80 from the ball and made to pick it up; the camera follows him, rising to 60 above over 12 seconds: never started by the game
- With the ball in hand he walks off towards the big fight arena, about 320 east: never started by the game
- "Uh Oh. Getting the ball back for the chimps has just got harder." After 4 seconds and the line, the camera returns over 4 seconds: never started by the game: not in the text table
- When Nemesis's creature is within 20 of the arena, he puts the ball down and is let go (once): never started by the game
- Any toy that no longer exists is made again at the chimps' home: never started by the game
- The first time the player's creature comes within 50 of the home while the chimps aren't friendly and it isn't carrying the ball, the evil advisor points at it: "Woah. The Chimps aren't very friendly.": never started by the game: not in the text table
- A toy counts as home when it is within 50 of the home, not in the hand and not in Nemesis's creature's hand: never started by the game
- The ball home for the first time moves the record to 0.5, reminder "These Chimps seem to be friendly towards our Creature." (`HELP_TEXT_REMINDER_56`): never started by the game
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
- No title, reminder or dialogue: the only line it uses is the shared "Your attention is required here." (`HELP_TEXT_GENERAL_CHALLENGE_START_02`): never started by the game
- Its arena sits on open ground at sea level; the "citadel heart" it faces is not near any shipped land's citadel (the closest is Land 4's, about 150 away), so it can't be placed on a shipped land with confidence: never started by the game: checked against every `Land<N>.txt`
- It uses real script commands of the shipped language (making a creature from a creature, stone creature icon features), so it would compile: never started by the game
- Not reachable at all in the shipped game: never started by the game
- Fifteen stone creature icons are placed on a half circle of radius 80 round the arena, 12 degrees apart, each facing the next; they can't be destroyed: never started by the game
- From one end to the middle: sheep, cow, zebra, ape, wolf, leopard, polar bear, lion; from the other end back to the middle: ape, ape, horse, tortoise, ape, bear, tiger; sizes grow from 1 at the ends to 2.4 for the lion in the middle: never started by the game: the chimp, mandrill and gorilla statues use the ape's icon, there being no icon for them
- The comments list the rungs in a different order (sheep, chimp, cow, ape...) from the one the statues and levels actually use: never started by the game
- The rung waits until the player's creature is within 100 of the arena, then a silver scroll appears over it and the good advisor nags ("Your attention is required here." (`HELP_TEXT_GENERAL_CHALLENGE_START_02`)) until it is clicked: never started by the game
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
- Swap To Horse: out of view, a horse creature and an Indian trader appear at a fixed spot; a silver scroll and the evil advisor's "Hey. There's a job for you. Wanna do it?" (`HELP_TEXT_GENERAL_CHALLENGE_START_03`); on click, a cut scene where the trader walks up and speaks four lines, the horse steps forward, both advisors come out for five more lines; then the field offer: never started by the game: all nine lines are missing from the text table; its challenge record lines were commented out because they "don't compile"; the spot is not near any shipped land's town
- Swap To Leopard: the challenge record opens, a leopard creature appears out of view under a silver scroll ("There's a Silver Reward Scroll down here, Boss." (`HELP_TEXT_GENERAL_CHALLENGE_START_01`)), then a cut scene of twelve advisor lines and the field offer; the record closes at once as a success: never started by the game: all twelve lines missing; the leopard became a breeder creature
- Swap To Lion: a tamer and a wild lion (an animal, not a creature) by a flock; the good advisor's "Your attention is required here." (`HELP_TEXT_GENERAL_CHALLENGE_START_02`); the lion hunts nearby cattle; if the player carries it away from the tamer and puts it down alive, it turns into a lion creature after a cut scene and is offered (record closes good); if it dies, the advisors comment and the record closes evil: never started by the game: all sixteen lines missing; a debug line "Picked up Lion" would have shown while it was held; meant to be placed outside the player's influence
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
- A silver scroll appears over a villager's hut; the good advisor nags "We've got something to do. Let's see what…" (`HELP_TEXT_GENERAL_CHALLENGE_START_08`) until it is clicked: never started by the game
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
- An Aztec fisherman is made on the hillside, with a silver scroll over him; the good advisor nags with an evil advisor's line ("Whew! As if you ain't busy enough, here's…" (`HELP_TEXT_GENERAL_CHALLENGE_START_09`)) until clicked: never started by the game: the line belongs to the evil advisor's voice
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
- (row moved 2026-11-06) "If he were dead when the puzzle is solved, the good advisor says "Hey. You've completed the puzzle." and "It's sad that you murdered the man who asked…" (`HELP_TEXT_NEW_L5_LION_PUZZLE_10`), and there is no reward at all": the branch is in the compiled script (the reward only if his life is above 0, refs/chl/lionpuzzle.txt:133458), but it can never run: the script makes the man indestructible as it creates him (:133118) and never clears it, and the engine refuses a life at or below 0.01 for an indestructible thing wherever its life is set (openblack ports the same rule, `life::SetLife` in src/ECS/Life.cpp); the same reason makes the life watch of the row before it unreachable; dead branch

#### story/silver_scrolls/swap_to_brown_bear

From [Swap To Brown Bear](progress/story/silver_scrolls/swap_to_brown_bear.md).

- The source's to-do notes say the trail was meant to lead "through a forest to reach the brown bear" and the camera was meant to "zoom to bear"; the shipped version does both only roughly as described above: development notes, nothing to do
- The text table has no lines numbered 5, 8 to 11 or 16 for this quest, so lines were cut; the source's comments quote older wordings of three lines ("I don't think the owner of that house is gonna be too pleased.", "Urgh. The smell still persists...", "The smell has improved drastically..."): the shipped wordings are the ones above
- The first pile's pointing hint can never play: the step counter is set past it just before the trail loop starts, the same in the compiled program: dead branch

#### story/silver_scrolls/swap_to_cow

Swap To Cow: A herding challenge written for Land 4: a lame herder asks for the player's creature to drive ten frightened cattle into a pen by the player's home village, and a trader there pays with a strength miracle seed, or with a cow creature to swap for when the player's creature is not already a cow. It is compiled into the shipped game but nothing ever starts it.

- The script is compiled into the shipped challenge file (it is in Land 4's file list): never started by the game: compiled, but no script in the compiled file runs it; Land 4's control script starts the ogre, the fish puzzle, the breeder and others, never this
- Only a single-quest test launcher, which is not compiled, ever runs it: never started by the game
- A note at its top says it was to be triggered after the ogre fight on Land 4: never started by the game
- Checked again 2026-11-08: no instruction of the compiled `challenge.chl` runs it and it is not on the auto-start list; no statement of the decompiled scripts names it but its own (refs/chl/swaptocow.txt:61); the swap offers start the shared swap with a species (`SwapCreatures(Zebra)`, `SwapCreatures(MagicCow)` and so on), never a swap challenge by number, and the land scripts start only Swap To Ape and Swap To Brown Bear by name; `xref.py str SwapToCow` finds no reference in the frozen exe. Its progress row is gone from [challenge_scripts.md](progress/scripts/challenge_scripts.md)
- The herder's house is a real Japanese house in Land 4's neutral Japanese village; the pen is about 190 from the player's Norse home village; the trader's hut position matches no building on Land 4: never started by the game: checked against `Land4.txt`
- The silver scroll over the herder's house and its advisor nag ("Whew! As if you ain't busy enough, here's…" (`HELP_TEXT_GENERAL_CHALLENGE_START_09`)) are commented out, so even if it were run it would start straight with the cut scene, with no scroll: never started by the game: the compiled version has no highlight at all
- Its title "Swap To Cow" and reminder "Let's get these cows back to the Village." (`HELP_TEXT_REMINDER_25`) exist in the text table, as do all its dialogue lines: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A cut scene: a Greek farmer comes out of the house and walks a few steps; the camera glides to him over 3 seconds: never started by the game
- Out of view, eight short Celtic fence pieces are set in a ring of radius 10 around the pen spot, each turned to face it: never started by the game
- A still flock is made at the pen (inner 5, outer 10) and a moving flock at the cattle's start (inner 15, outer 20); ten cows are made at the start, scattered up to 10 either way, and join the moving flock: never started by the game
- The herder turns to the camera, prays, and says: "Mighty one. I need to herd my cattle…" (`HELP_TEXT_SWAP_TO_COW_13`) then "But I have injured my ankle and cannot…" (`HELP_TEXT_SWAP_TO_COW_14`): never started by the game
- The camera cuts to the cattle; the challenge record opens; the herder: "But as my beasts are afraid of your…" (`HELP_TEXT_SWAP_TO_COW_15`): never started by the game
- The camera cuts to the pen: "You could get him to shepherd the cattle…" (`HELP_TEXT_SWAP_TO_COW_16`) and "Please try and chase as many into the…" (`HELP_TEXT_SWAP_TO_COW_17`): never started by the game
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
- If the player's creature is already a cow, the trader thanks by result and gives a creature strength miracle seed beside himself with a sparkle: all ten ("Thank you, lordly one. You got the herd here safely." (`HELP_TEXT_SWAP_TO_COW_18`)), more penned than lost ("You got most of the cattle here safely." (`HELP_TEXT_SWAP_TO_COW_20`)), at least one penned ("Well, some cows made it, Mighty One." (`HELP_TEXT_SWAP_TO_COW_21`)), each followed by "Here's an offering to show my gratitude." (`HELP_TEXT_SWAP_TO_COW_19`): never started by the game
- If all ten died, a cow owner gets nothing: "None of the cows made it. I shall…" (`HELP_TEXT_SWAP_TO_COW_22`): never started by the game
- Otherwise a cow creature appears near the trader's hut, looks at the camera and waves; the lines are "A billion thanks. All the cattle got to…" (`HELP_TEXT_SWAP_TO_COW_23`) / "A hundred thanks. You managed to get most…" (`HELP_TEXT_SWAP_TO_COW_25`) / "Thanks, Holy One. A few cows did make…" (`HELP_TEXT_SWAP_TO_COW_26`) each followed by "Take this Miraculous cow as an offering from…" (`HELP_TEXT_SWAP_TO_COW_24`): never started by the game
- Even with every cow dead, a non-cow owner still gets the cow: "Lordly spirit, none of my herd survived. I…" (`HELP_TEXT_SWAP_TO_COW_27`) "Here. Take this special beast." (`HELP_TEXT_SWAP_TO_COW_28`) "We might have to eat it otherwise." (`HELP_TEXT_SWAP_TO_COW_29`): never started by the game
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
- The challenge record opens as "The Attackers", success and alignment 0, reminder "Cattle are missing and those Marauders are responsible." (`HELP_TEXT_REMINDER_17`): never started by the game
- A cut opening scene is commented out: the Norse farmer would have come out of his house, pointed towards the camp and said two lines (neither is in the text table), with the camera following the marauders: never started by the game
- All through the quest, every 2 seconds while any marauder lives: any food store within 50 of the camp that the script didn't make is emptied into the camp's store: never started by the game
- Any cow within 40 of the camp that the script didn't make is killed and adds 500 food to the camp's store: never started by the game
- Either way, the good advisor says "This food should keep the Marauders quiet for a while." (`HELP_TEXT_MARAUDERS_13`), at most once every 30 seconds: never started by the game
- If the camp's store has gone, a new one is made at the camp: never started by the game
- When the marauders are within 30 of the herd, the herd moves onto them: never started by the game
- Once the camera is within 100 of the marauders and looking at them, the good advisor points at them: "These folk are stealing our cattle! Something must be done!" (`HELP_TEXT_MARAUDERS_03`): never started by the game
- The marauders head home at speed 0.5, the herd following at 0.4; the record's reminder is set again to the cattle line: never started by the game
- Every 4 seconds while the herd keeps up (within 30) the marauders are sent on again; if the herd falls behind they stop and wait until it is within 10: never started by the game
- Every 5 seconds the script looks for a cow more than 75 from the herd; one found before the marauders get home is taken out of the herd and walks back to the herd's starting spot, joining a rescued herd there (inner 5, outer 30): never started by the game: this is how the player saves cattle, by carrying or scaring a cow away from the herd
- When the marauders are within 30 of the camp, the herd is led in; one cow at a time walks to the camp's store at 0.4 and, once within 20, or dead, or picked up, or thrown, is killed and fades away, adding 100 food to the camp's store: never started by the game: a comment says 300 food per cow; a cow the player is holding is killed in the hand
- The raid is called off at once if two marauders are killed (fewer than nine left) or the herd is empty: the marauders go home and the herd goes back to its starting spot: never started by the game
- After a raid, if the camp's store holds more than 100 food, the marauders stay at camp: one is put at the camp and the band dances round it (a 30-second dance): never started by the game
- Meanwhile one marauder at a time walks to a spot by the store, faces it, eats 100 food and rejoins the band, then waits 30 seconds: never started by the game: so each 100 food buys a little over 30 seconds of peace
- When the store is down to 100 or less, the dance stops and the next raid begins: never started by the game
- The marauders walk at 0.5 to a spot beside the village storage pit and wait until within 15 of it: never started by the game
- Once the camera is within 100 and the pit is in view, the evil advisor points: "Argh! These godless thieves are attacking the Village…" (`HELP_TEXT_MARAUDERS_05`) and the good advisor answers "No don't kill them. They're just hungry. That's…" (`HELP_TEXT_MARAUDERS_15`): never started by the game
- 500 food is taken from the storage pit; after 2 seconds the raid is over: never started by the game: the header comment says this raid destroys the storage pit, but the script only takes food
- The raid ends early if two marauders are killed during it: never started by the game
- The record's reminder becomes "Food's been stolen from the Village Store. The…" (`HELP_TEXT_REMINDER_43`); the marauders go home: never started by the game
- If two or more died in this raid, the evil advisor: "The marauding band have retreated. We've given them…" (`HELP_TEXT_MARAUDERS_04`): never started by the game
- 500 food is added to the camp's store, then the band waits 40 seconds (a comment says this was lowered for testing): never started by the game: the 500 is added even when the raid was cut short before anything was taken
- If the camp has more than 100 food, the band dances and eats again as above: never started by the game
- Only if all ten marauders are still alive at this point: never started by the game
- The good advisor points at the farmer's house: "The man who asked for your help needs…" (`HELP_TEXT_MARAUDERS_14`): never started by the game: nobody has asked yet, since the opening scene where he would have was cut
- It then waits until the house is clicked; the evil advisor would nag "A Silver Reward Scroll. Let's see what it's…" (`HELP_TEXT_GENERAL_CHALLENGE_START_07`) every 30 seconds while the camera is within 100, but the nag hangs on a scroll that is never made: never started by the game: whether the nag ever fires with no scroll can't be told from the source
- A Norse farmer comes out of the house and walks to a spot in front of it; the camera glides to 10 above and 10 off him over 4 seconds: never started by the game
- The record is set again (success 0) and the player's alignment drops by 0.4: never started by the game: the player is marked down for having let the raids happen
- The farmer: "Please help us, massive one. We'll reward you!" (`HELP_TEXT_MARAUDERS_12`): never started by the game
- After 2 seconds the camera returns (3 seconds) and the farmer is released: never started by the game
- Whether or not the farmer begged, the marauders walk at 0.5 to the crèche and wait until within 15 (or all dead): never started by the game
- The good advisor points: "Now they're attacking the crèche! If I wasn't…" (`HELP_TEXT_MARAUDERS_06`): never started by the game: said even if every marauder is dead by then
- After 3 seconds, five sparkles appear around and above the crèche for 10 seconds; after 2 more the crèche is set on fire: never started by the game
- The raid ends early if two marauders are killed during it, before the fire: never started by the game
- The record's reminder becomes "People have been taken from the Village. Those…" (`HELP_TEXT_REMINDER_44`); the marauders go home: never started by the game
- If two or more died in this raid: good advisor "They took losses in that last raid. If…" (`HELP_TEXT_MARAUDERS_07`), evil advisor "It was glorious to see." (`HELP_TEXT_MARAUDERS_08`): never started by the game
- From now on the band loops for ever: eat at camp while it has more than 100 food, then raid the storage pit for another 500 food and carry it home: never started by the game
- These later raids have no advisor lines and don't change the record: never started by the game
- The loop only ends when every marauder is dead; there is no timer, no failure and no other way out: never started by the game: a player who keeps them fed never finishes the challenge
- When the last marauder dies: "Sweet. The marauders are all dead." (`HELP_TEXT_MARAUDERS_11`): never started by the game: the good advisor is brought out for a line voiced by the evil advisor
- If the farmer did not beg (a marauder was killed before the third raid), the record closes as a success with no reward: never started by the game
- If he did, it waits for his house to be clicked again (the same scroll-less nag as before): never started by the game
- The farmer walks out again; the camera glides to 10 above and 10 off his spot over 3 seconds; he faces the camera and prays once: never started by the game
- The player's alignment rises by 0.2 and the record closes as a success with alignment -0.2: never started by the game
- The farmer: "Oh thank you, invisible one. We will worship…" (`HELP_TEXT_MARAUDERS_09`) then "We humbly offer you this. You know, just…" (`HELP_TEXT_MARAUDERS_10`): never started by the game
- A fireball miracle seed falls from the sky beside him, with no reward camera; the reward help shows when it is clicked: never started by the game: the shared reward script, run without its camera
- After 2 seconds the camera returns over 4 seconds; the farmer is left in the script's control (a comment wonders whether to make him homeless): never started by the game
- Last of all the record is closed again with alignment 0, overwriting the alignment just set: never started by the game
- Every closing record uses the reminder "Do you want to swap your Creature with…" (`HELP_TEXT_REMINDER_40`), which belongs to the swap quests: never started by the game
- The reward only comes to a player who killed none of the band before the third raid and then all of it afterwards; killing them early, the advisors' first suggestion, gives nothing: never started by the game
- The header comment (three raids, retreat after two deaths, storage pit and crèche destroyed) is copied word for word into "Food for Thought"'s source: never started by the game: see [food_for_thought.md](#storysilver_scrollsfood_for_thought)
- Marauders also appear, unrelated, as a single villager in one of Land 5's cut scenes: never started by the game: not this quest

#### story/silver_scrolls/the_beach_temple_puzzle

From [The Beach Temple Puzzle](progress/story/silver_scrolls/the_beach_temple_puzzle.md).

- Eight of the man's lines, about belongings ruined by a flood, are in the text table but no script says them: "My collection of clocks!" (`HELP_TEXT_BEACH_PUZZLE_09`), "My soccer trophies!", "The Welsh dresser's toppled!" (`HELP_TEXT_BEACH_PUZZLE_11`), "Oh, my matchstick galleon!" (`HELP_TEXT_BEACH_PUZZLE_12`), "My ship in a bottle collection!" (`HELP_TEXT_BEACH_PUZZLE_13`), "Oh, my hip replacement!" (`HELP_TEXT_BEACH_PUZZLE_14`), "My CDs!", "My house of cards! That took four years!" (`HELP_TEXT_BEACH_PUZZLE_16`): apparently a cut flooding stage, where the temple flooded and the man lamented as he lost things; flagged differently in the text table from the lines used. Whether the engine itself ever says them is not confirmed: no script source uses them, and a search of the PC executable for their text numbers finds no plausible use
- Every other line of the quest's set (1 to 8, 17 to 19) is used
- The comments beside the dialogue are older drafts that differ from the spoken text: comments only
- A timer primed 31 seconds back in the scroll script and two marked spots in the main script (a start spot and a spot for the man) are set up but never used: no effect a player can see

#### story/silver_scrolls/the_creature_breeder

From [The Creature Breeder](progress/story/silver_scrolls/the_creature_breeder.md).

- Land 2 and Land 5 positions are prepared too (each exactly on a Celtic house in a neutral Celtic village) but neither land's control script starts the breeder; only test launchers outside the compiled file do: never started by the game on those lands
- The comment above the breeder's swap asks people not to touch it because "it works and it was tricky to get to work": developer note; nothing to do
- The launcher for Land 1 and Land 4 finds the kennels as a house; the Land 2 and 5 entries use a plain marker: never started by the game on those lands
- The breeder's "nothing to show" answer, after which he walks back, is never reached: its unlock test always answers yes (`creature_swap::k_GameUnlocks`, test `CreatureSwap.TheGameHasEveryBonusCreatureUnlocked`), and at least four of the five special creatures always differ from the player's species, so something is always shown
- The breeder's one-creature greeting (a two-subject camera on the creature and the breeder, "Hello. I have this special Creature for you." (`HELP_TEXT_CREATURE_BREEDER_02`), refs/chl/CreatureBreeder.txt 14657..14665) is never reached, for the same reason: every special creature is unlocked, so at least four are shown and the several-creatures line is said instead (read 2026-11-14)

#### story/silver_scrolls/the_explorers

From [The Explorers](progress/story/silver_scrolls/the_explorers.md).

- A storm reward falling from the sky was the original reward and is left commented out: not in the compiled scripts
- An advisor line "It looks like the men are getting ready…" (`HELP_TEXT_PICKUP_MISSIONARIES_01`) (good advisor) is in the text table but no script says it: not used by the shipped scripts
- A sailor's line "How are we going to sail the ship…" (`HELP_TEXT_MISSIONARIES_EXTRA_01`) is in the text table but no script says it: not used by the shipped scripts
- The quest's text calls the sailors missionaries throughout, though the scroll's title is "The Explorers": naming only

#### story/silver_scrolls/the_explorers_again

From [The Explorers Again](progress/story/silver_scrolls/the_explorers_again.md).

- A single-quest test launcher that sets the flag and starts it exists in the sources but is not part of the shipped game: developer test only
- A sailor's line "Eventually we landed and started this settlement in your honour." (`HELP_TEXT_NEW_L5_MISSIONARIES_11`) is in the text table but no script says it: never in the game
- A second sailor's thanks ("You've been so devine we thought we'd make an offering of it") was written into the film and switched off; it has no text line: never in the game
- An earlier start that waited for the camera to look at the wreck, instead of a scroll, is switched off: never in the game

#### story/silver_scrolls/the_fish_puzzle

From [The Fish Puzzle](progress/story/silver_scrolls/the_fish_puzzle.md).

- A developer test script for the creature's creed also starts this quest; it is a developer test, not reached in the shipped game: developer test only

#### story/silver_scrolls/the_greedy_farmer

From [The Greedy Farmer](progress/story/silver_scrolls/the_greedy_farmer.md).

- A cut start condition: the source's comments plan the children to appear only when the town's storage pit runs low, and the scroll only after a first stolen cow; the storage pit is looked up and a food and a starving counter are set up but never used, and the wait for the first stolen cow is commented out: not in the compiled script's behaviour
- The hideout house is looked up but never used, the slaughtered-cow count is kept but never read, and a local alignment total and a debug size are unused or commented out: no effect a player can see
- A wander state for the children and a rule that a cow counts as saved only well away from the gang are commented out: not in the compiled script
- A stand-alone test challenge that only runs the quest is in the sources but not in the compiled `challenge.chl`: developer test only
- Text-table lines no script says: an alternative advisor exchange about someone praying and the hungry children, four more lines for the boys that suggest a cut ending, and two filler lines under the quest's prefix: unused text
- The source's comments quote other wordings than the lines the game plays, plan the herd-gone ending to ask for a click (none is asked for), and call the children villagers though the script makes them at the hideout: comments only

#### story/silver_scrolls/the_heavenly_fire

From [The Heavenly Fire](progress/story/silver_scrolls/the_heavenly_fire.md).

- The challenge plays no music of its own (the capture scene before it stops the Nemesis theme): nothing to do
- A scroll over a villager's hut, pointed out by the good advisor with "Your attention is required here." (`HELP_TEXT_GENERAL_CHALLENGE_START_02`), was planned and switched off: never in the game
- The challenge's lines 6 and 8 to 11 are missing from the text table: the old volley announcements were cut and replaced by the five new Land 5 lines: never in the game
- The tower and storage pit are looked up but never used, nor is the villager's hut: never in the game

#### story/silver_scrolls/the_hermit

From [The Hermit](progress/story/silver_scrolls/the_hermit.md).

- The hut branch's comments speak of the hermit giving a gesture; the shipped game gives a water miracle: comment only
- A test script starts the quest on its own; an unshipped control script had it switched off: not started by the game
- The hermit villager is also used by the fifth land's script and an unshipped waving scene, outside this quest: see [../land_5.md](progress/story/land_5.md)
- Killing him after he was impressed but before the seed is found (or on his walk into his hut afterwards) plays the death scene, closes the record at -0.2 and updates it on his death: the script tests for his death there, but he cannot die in that span. The quest made him and holds him, so the script controls him, and before the impressed scene it makes him impossible to pick up, to move or to destroy, giving that back only after he has gone into his hut. Nothing lowers an indestructible villager's health to nothing, every death or removal that does not wait for that (drowning, being eaten, a sacrifice, the tornado, old age, the explosion's blast) is refused to an indestructible, unmovable or script-controlled one, and the way out of the land is far from him: not reachable in the game; see [Death](../beings/villager-hand-and-death.md#death) (indestructible villagers) and `refs/chl/thehermit.txt` (lines 57200-57226, 56167-56197)

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
- The closing line for a dead maker carries a developer note asking whether the line is wanted, and its comment expects a gleeful line instead: developer note; the game plays the text table
- Two evil advisor lines at the end of the introduction and the advisors going home afterwards are commented out: not in the compiled script
- A ring of anti-influence of radius 50 round the idol, removed when it explodes, is commented out (all three lines): not in the compiled script

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
- A silver scroll appears about 37 west of the circle's centre; while the camera is within 100 and the scroll is in view, the evil advisor nags "Whew! As if you ain't busy enough, here's…" (`HELP_TEXT_GENERAL_CHALLENGE_START_09`) every 30 seconds until the scroll or the circle is clicked: never started by the game: the shared challenge notify script
- Only then are the three missing stones made at their far-off spots: never started by the game
- An Indian housewife stands 5 east of the centre; a dead boy of 9 lies near the centre, playing a dead pose: never started by the game
- Cut scene: the camera glides in 5 seconds to a low view across the circle; the mother faces the camera, kneels and prays: never started by the game
- The challenge record opens as "The Miracle Stones", success and alignment 0, reminder "We need to make two matching stone sing…" (`HELP_TEXT_REMINDER_19`): never started by the game
- Mother: "Holy one, these five stones are meant to…" (`HELP_TEXT_SINGING_STONES2_01`) She gets up and gossips: never started by the game
- She turns to the child and mourns; the camera turns to the child over 2 seconds; mother: "Please make them work. My poor child, you see." (`HELP_TEXT_SINGING_STONES2_02`): never started by the game
- After 2 seconds the camera returns over 3 seconds: never started by the game
- A missing stone dropped (not held, not flying) within 5 of its own base snaps onto it, faces the centre, and can no longer be moved or picked up: never started by the game
- Dropped within 5 of another empty base, it is given a little hop towards a point 10 above that base and does not lock in: never started by the game: it keeps hopping each time it lands there
- When all three are set, the good advisor: "All the stones are in place. Now we…" (`HELP_TEXT_SINGING_STONES2_04`): never started by the game
- Clicking a stone whose pair is not yet made (a missing one only once set in its base) picks it as the first note, plays its sound and puts a sparkle 10 above it: never started by the game
- Clicking a different such stone picks the second note, plays its sound and puts a sparkle over it: never started by the game
- The pairs are G with G (the two original stones), and A, B and C each with their missing octave stone; either order counts: never started by the game: so only the G pair can be made before stones are found
- After any second note, right or wrong, both note sparkles and the choice are cleared (except after the fourth pair, which ends the puzzle): never started by the game
- A first note left without a second for about 10 seconds is cleared: never started by the game: counted in game turns, a comment assuming ten a second
- Each made pair gets a lasting sparkle 15 above both its stones within about half a second: never started by the game
- Rewards go by how many pairs are made, not by which pair: never started by the game
- Cut scene: camera to a raised view of the circle (3 seconds); good advisor: "Aha. The same note but an octave apart!" (`HELP_TEXT_SINGING_STONES2_05`) (a comment says to cut it if it makes the puzzle too obvious): never started by the game
- The camera swings round over 3 seconds while a strong heal miracle flies from the first G stone to the centre: never started by the game: always from that stone, whichever pair was made
- If the child's body is within 10 of the centre, a heal miracle hits him and he comes back to life, looks impressed, and the player's alignment rises by 0.5; otherwise it falls by 0.2: never started by the game
- If the mother is dead and her body is within 10 of the centre, she too is healed, brought back to life and set beside the centre: never started by the game
- The record moves to 0.25 (alignment 0), reminder "There are stones missing. Maybe we should find them." (`HELP_TEXT_REMINDER_45`): never started by the game
- After 2 seconds the camera returns to the opening view; if both live, the mother faces the camera, prays and says "Eternal thanks, godly spirit." (`HELP_TEXT_SINGING_STONES2_06`): never started by the game
- If only the child lives: "Where's my mother? I want my mother!" (`HELP_TEXT_SINGING_STONES2_07`): never started by the game: the line is voiced by the woman's voice
- If the child was outside the circle, the good advisor comes out for "Woah. Powerful stuff. Pity the kid's not in…" (`HELP_TEXT_SINGING_STONES2_08`): never started by the game: a line voiced by the evil advisor
- The camera returns to where it was (3 seconds): never started by the game
- The living members of the family walk home at speed 0.3 to their house in the Indian village, about 285 north: never started by the game
- Good advisor: "Those two stones are permanently set now." (`HELP_TEXT_SINGING_STONES2_09`); evil advisor: "I wonder what'll happen if we activate them all." (`HELP_TEXT_SINGING_STONES2_10`); and, if any stone is still missing, good advisor: "We need the stones from around this land." (`HELP_TEXT_SINGING_STONES2_03`): never started by the game
- Cut scene: camera to 15 above and 15 off the centre (3 seconds); five sparkles over the centre for 5 seconds: never started by the game
- A large food reward falls from the sky 5 east of the centre and a large wood reward 5 west, each showing its help when clicked: never started by the game: the shared reward help
- The camera drifts lower over 8 seconds; the record moves to 0.5 with the alignment so far, reminder "Maybe something'll happen if we match up more stones." (`HELP_TEXT_REMINDER_46`): never started by the game
- Both advisors come out: "Wow. An honest-to-god Miracle." (`HELP_TEXT_SINGING_STONES2_22`) then "Hey! We've been granted some food! Go stones!" (`HELP_TEXT_SINGING_STONES2_14`): never started by the game: the first line is in the woman's voice although only the advisors are out
- After 2 seconds the camera returns: never started by the game
- Cut scene: camera to 15 above and 15 off the centre; magic beams run from the centre to each of the five stones and to each set missing stone, for 5 seconds: never started by the game
- After 2 seconds a beam runs from the Indian village's centre to the circle (a note asks for it in a different colour): never started by the game
- The record moves to 0.75, same reminder: never started by the game
- Evil advisor: "Eh? Nothing. Nothing at all." (`HELP_TEXT_SINGING_STONES2_23`); good advisor: "Hmm. The pair is right, the stones are…" (`HELP_TEXT_SINGING_STONES2_24`): never started by the game
- A comment says the nearby town was to become "all 12 years old"; the curse script it starts was never written: never started by the game
- All the pair sparkles are removed: never started by the game
- Cut scene: camera to 15 above and 15 off the centre; five sparkles for 3 seconds hide the chimp's appearance: never started by the game
- After a second a chimp creature appears at the centre, turns to the camera and waves: never started by the game
- The record closes as a success with the alignment so far, with the opening reminder: never started by the game
- Good advisor, pointing at the chimp: "A Chimp. Let's have it. We must." (`HELP_TEXT_SINGING_STONES2_25`); evil advisor: "How?"; good advisor: "Bring your Creature here and click the Chimp…" (`HELP_TEXT_SINGING_STONES2_27`): never started by the game
- The camera pulls back to 25 above and 25 off over 3 seconds and the cut scene ends there: never started by the game
- The chimp is then offered through the shared swap offer: never started by the game: see [creature_swaps.md](progress/story/silver_scrolls/creature_swaps.md)
- If the mother dies before reaching home: "Way to go. You killed her. Eee-vil!" (`HELP_TEXT_SINGING_STONES2_11`) and the player's alignment falls by 0.6: never started by the game
- If the healed child dies before reaching home: "You killed the little one. After healing her, too! Drat!" (`HELP_TEXT_SINGING_STONES2_12`) and alignment falls by 0.8: never started by the game: the line says "her" for the boy
- Each who reaches the house is released to village life and alignment rises by 0.1: never started by the game: see Bugs
- Before the first pair, if the child's body is more than 10 from the centre and not in the hand, the good advisor says once "Move the child back into the circle to heal him." (`HELP_TEXT_SINGING_STONES2_13`): never started by the game
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
- An unused evil line: "Look! Here it is. You know I reckon…" (`HELP_TEXT_CREATURE_GUARDIAN_02`): in the text table, used by no script
- Cut ways to finish: the ogre running after the creature when hit by a rock, and the quest ending when the player's influence covered the reward: commented out of the script
- A cut reward of a heal-miracle dispenser at the reward's spot: commented out
- The fourth land has a different ogre quest with its own script: not this quest
- A test script starts this quest alone with the fight-lesson flag raised: not started by the game

#### story/silver_scrolls/the_pied_piper

From [The Pied Piper](progress/story/silver_scrolls/the_pied_piper.md).

- Cut lines in the text table, never used: "You killed the piper!" (`HELP_TEXT_PIED_PIPER_07`), "You've knocked away the rock. Here come the…" (`HELP_TEXT_PIED_PIPER_12`), "Oh this is enchanting!" (`HELP_TEXT_PIED_PIPER_13`), "Humph.", "Nice bit of assassination there, Boss." (`HELP_TEXT_PIED_PIPER_15`), "And no children hurt. Wonderful!" (`HELP_TEXT_PIED_PIPER_16`), "Scratch one piper.", "But what about the children? You haven't saved them!" (`HELP_TEXT_PIED_PIPER_18`), "So many children are missing." (`HELP_TEXT_PIED_PIPER_21`) — from an older design where the cave was sealed by a rock to knock away and the piper could be killed: text only
- An "anti-influence" around the piper (so villagers keep away) was written and commented out

#### story/silver_scrolls/the_plague

From [The Plague](progress/story/silver_scrolls/the_plague.md).

- Three advisor lines exist in the text table but no script says them, an argument over helping: evil "This is ridiculous. If they give us power…" (`HELP_TEXT_PLAGUE_03`); good "The people are all sick, though. Let's try…" (`HELP_TEXT_PLAGUE_04`); evil "No. The strong will survive. That's nature. Nice…" (`HELP_TEXT_PLAGUE_05`): unused text; there is no plague line numbered 15
- Several variables are set up and never used (five first-victim slots, a dead person, Lethys's position, a commented-out rock and a global for the intro being triggered): leftovers of an earlier design, not behaviour
- The source's comments quote earlier wordings of two shipped lines: comments only; the game plays the text table
- If everyone in the village has died, the good advisor steps out, points at the spot and says "Take people from your other Village. Everyone here has died." (`HELP_TEXT_PLAGUE_16`) and "But you'll need to cure this Plague, though, Leader." (`HELP_TEXT_PLAGUE_17`); never in the original: the failure ending makes the trader at the hut and lets him go before the main loop's empty-village test (refs/chl's plague script :91212, :91316), and a villager let go with no town joins the nearest town of his tribe within 200 m (`Villager::ReleaseFromScript` 0x7531D0, `VagrantStart` 0x76A8D0, [villager-life.md](../beings/villager-life.md)), so the village is never empty there; its town (id 10) is not one Land 2 makes uninhabitable (`SET_TOWN_UNINHABITABLE` 6, 7, 8 and 14), the only writer of that flag besides the town's making and loading (checked 2026-11-05)

#### story/silver_scrolls/the_rejuvenator

From [The Rejuvenator](progress/story/silver_scrolls/the_rejuvenator.md).

- The opening record line was first written before the cut scene and moved inside it; a closing record line after the swap is commented out: source history only
- The text table has no lines numbered 1, 6 or 7 for this quest, and line 17 (the evil advisor's hint) exists but is never said: cut material
- The source creates a plain "female" villager; the compiled game turns that into a Norse housewife: data detail

#### story/silver_scrolls/the_riddles

From [The Riddles](progress/story/silver_scrolls/the_riddles.md).

- A tiny test script exists that runs the whole quest directly, for testing: `RunLostTreasure.txt`
- Unused clues of the earlier design, in the text table but said by no script: "Place in the ring a cunning hunter." (`HELP_TEXT_LOST_TREASURE_26`) / "And a bush of fire if you want to." (`HELP_TEXT_LOST_TREASURE_27`) / "A lump your Creature leaves behind." (`HELP_TEXT_LOST_TREASURE_28`) / "And protect with magic hand and mind." (`HELP_TEXT_LOST_TREASURE_29`): unused text
- Unused step-by-step clues: "You need to place a cunning hunter into…" (`HELP_TEXT_LOST_TREASURE_30`) / "Now add to the ring of life, the Burning Bush." (`HELP_TEXT_LOST_TREASURE_31`) / "Next a lump of your Creature's waste products…" (`HELP_TEXT_LOST_TREASURE_32`) / "Finally, protect the objects you placed into the…" (`HELP_TEXT_LOST_TREASURE_33`) and the hints "You may find one drinking fresh water after dark." (`HELP_TEXT_LOST_TREASURE_34`) / "You will find it between the snow and the sea." (`HELP_TEXT_LOST_TREASURE_35`); an alternative opening "I have a legend for you. Solve it…" (`HELP_TEXT_LOST_TREASURE_36`): unused text; the wolves drinking at the water's edge at night match the first hint
- Unused progress lines: good "Excellent. Something's happening." (`HELP_TEXT_LOST_TREASURE_16`), evil "Let's get the next clue from that woman!" (`HELP_TEXT_LOST_TREASURE_17`), good "This is getting interesting!" (`HELP_TEXT_LOST_TREASURE_18`), good "We're getting closer! There can't be much more…" (`HELP_TEXT_LOST_TREASURE_19`), evil "Something's happening!", good "The wolf's escaped." (`HELP_TEXT_LOST_TREASURE_43`), good "Well that's pretty unique. I suppose." (`HELP_TEXT_LOST_TREASURE_44`), evil "The object's no longer hot, Boss." (`HELP_TEXT_LOST_TREASURE_45`): unused text: the shipped quest has no per-step feedback
- The two shield variables are named the wrong way round (the one named for the magic shield holds the physical one): source naming only, no effect on play
- Unused variables in the main script (a tiger, a chosen one, a clue number, a clue on the ground, a fourth artefact, effect timers) and a hot object in the ring's watch point to a longer, step-by-step design: never used
- A turn-around animation for Annika, an earlier camera focus and a second snapshot after the scene are commented out: not in the compiled script
- The source's comments quote earlier wordings of Annika's opening, of her knock-on-my-door line and of the damaged-hut line: comments only

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
- A wife's line "You saved some people, but others still drowned…" (`HELP_TEXT_DROWNINGFISHERMEN_07`) and the evil advisor's "Gutted! Ha! Fishermen. Gutted. Get it?" (`HELP_TEXT_DROWNINGFISHERMEN_10`) are never used: in the text table, used by no script
- The all-drowned ending, "At times like this I wish we had…" (`HELP_TEXT_DROWNINGFISHERMEN_08`) (text 1500) with the advisors' "I feel gutted." / "Give it a rest." (`HELP_TEXT_DROWNINGFISHERMEN_11` or `HELP_TEXT_ZEBRA_HIT_BY_MAN_01`), never plays: the men drown at fixed times before the five minutes end, the last with one second left (his wait of 1 against the timer, refs/chl/creaturesavingpeople.txt:43581), and each drowning man first plays his drowning clip to its end (:43203, :43211) before he is counted as drowned (:43214). That clip, M_P_Into_Dead_Drowned (283 in Data\AllAnims.anm), lasts 6000 ms (its header's period; 61 keys), and a clip's time advances by the game ms of each frame and stops at its end ([Playback](../beings/animation.md#playback)). The quest's count is made one second after the timer runs out (:43843, :43847, :43850), while the last man is still dying, so he is counted as murdered and the scene says the none-saved, some-killed lines (1507, 1508). Measured in openblack: the clip starts at turn 3369 and ends at 3430, the count is at 3389 (refs/story/evidence/a2q_saviour_drowned.log, lines 444571, 446714, 451416; run a2q_10_st1 says 1507 and 1508 with five drowned)

#### story/silver_scrolls/the_sculptor_cut_silver

The Sculptor (cut silver version): An early, silver-scroll version of Land 1's sculptor: a sculptor offers to carve the missing creature-gate stone if the player brings him the right rock from the old hermit's quarry, then hammers at it for twenty minutes. The shipped game replaced it with the gold-scroll story quest of the same title (its own script, in Land 1's story; see [../land_1.md](progress/story/land_1.md)); this version was never compiled.

- This file is not in the compiled challenge file: never started by the game
- The shipped Land 1 sculptor script defines scripts with the same names (the main script and the "rock returned" step, which takes one more argument there), and the shipped story calls those, never this file's: never started by the game: the compiled file only has the shipped versions
- The uncompiled early Land 1 control script asks to start a script by this file's name, but no script of that name exists (it is only a variable here), so even that early version could not start it: never started by the game
- It used a silver scroll; the shipped quest uses a gold scroll and records itself as a story quest rather than a challenge: never started by the game
- The title "The Sculptor", the reminder and most of the sculptor's lines are in the text table (shared with the shipped quest); five advisor lines and one reminder of this version are missing: never started by the game
- It calls the scroll's nag helper with one argument too many for the shipped helper: never started by the game
- Not reachable at all in the shipped game: never started by the game
- A sculptor (a Norse farmer) is made by his house; a silver scroll appears over the house and the good advisor nags "There's someone who might need your attention, Leader." (`HELP_TEXT_CREATURE_RETRIEVE_01`): never started by the game
- On click, a cut scene: he walks out, the camera glides in (4 seconds) and the challenge record opens, reminder "We need to get a carveable stone for…" (`HELP_TEXT_REMINDER_06`): never started by the game
- The sculptor: "Holy one, I hear you're looking for a…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_02`) then "I am a sculptor. If you provide me…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_03`): never started by the game
- The camera flies a path to the quarry: "You'll find the rock I need in the…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_04`): never started by the game
- Both advisors then speak five lines while the camera flies a second, 15 second path back: never started by the game: lines missing from the text table
- He then wanders around his house (radius 6): never started by the game
- Every 3 seconds, any ordinary rock within 30 of him is checked: he walks to it, says "Hmm. Actually this rock isn't right for carving…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_10`) and it is removed: never started by the game
- The right rock (the quarry's gate-stone rock) put down within 30 of him, not held or flying: "Oh that's excellent. Exactly the right kind of…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_05`) the record goes to complete with full good alignment, "Thank you. Come back later and I'll have…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_06`): never started by the game
- A lightning bolt miracle seed then falls from the sky about 14 from him, with the reward camera swoop: never started by the game
- He walks to the rock, faces it and hammers at it; the record is set back to half done and a 20 minute carving timer starts: never started by the game: the record goes from complete back to half, a quirk of this version
- Picking the rock up while he works: "Hey! I haven't finished working on carving the…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_07`) and the timer pauses with the time left: never started by the game
- Putting it back within 30: "Thank you. Now, if you don't mind. I've…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_08`) he walks to it and hammers again, and the timer resumes: never started by the game
- When the timer reaches its last second: the rock is replaced by the carved gate stone, "I've finished! It's ready to be placed with…" (`HELP_TEXT_NEW_CREATURE_RETRIEVE_09`) the record completes, he walks home and is removed: never started by the game: the check is for exactly one second left, which could be missed
- Thrown with almost no life left: the camera follows him in flight at half game speed while he screams "Aaaaaarrrrrrrggggggghhhh!" and carving stops: never started by the game
- Killed: the good advisor points at him, "What a senseless waste of human life!" (`HELP_TEXT_KILLING_GA_01`), the evil advisor "Come on. Let's do another!" (`HELP_TEXT_KILLING_EA_14`), and the record closes with full evil alignment; the quest ends: never started by the game

#### story/silver_scrolls/the_sea

From [The Sea](progress/story/silver_scrolls/the_sea.md).

- The text table holds seventeen more lines of this quest that no script uses (numbers 4 to 14 and 16 to 21; 1 to 3 don't exist), from an earlier plan where the children were in danger in the water and the woman judged the player: cut content; listed for completeness
- Unused, on killing the woman: evil "You killed the woman, Boss. Now that is…" (`HELP_TEXT_BAYWATCH_04`); the woman: "You disgraceful, murdering overfiend. No true god would…" (`HELP_TEXT_BAYWATCH_05`); evil "Oh this is priceless. They can't take a…" (`HELP_TEXT_BAYWATCH_06`); good "If I wasn't stuck inside your head, I'd…" (`HELP_TEXT_BAYWATCH_07`): cut content
- Unused, on success: the woman: "What a stunning performance. I'm so, so grateful…" (`HELP_TEXT_BAYWATCH_08`); evil "Huh. Am I wasting my time pushing the…" (`HELP_TEXT_BAYWATCH_09`): cut content
- Unused, on a cold result: the woman: "That was rather cold-hearted." (`HELP_TEXT_BAYWATCH_10`); evil "Ooh, this girl is feisty. I like it." (`HELP_TEXT_BAYWATCH_11`); good "Oh shut up and take your telling off…" (`HELP_TEXT_BAYWATCH_12`); evil "Who rattled your cage, you do-gooding nimrod?" (`HELP_TEXT_BAYWATCH_13`): cut content
- Unused, on failing: the woman: "Er, lordly one. You're gave it a good…" (`HELP_TEXT_BAYWATCH_14`): cut content (the typo is in the text table)
- Unused, spotting the children: good "Look! Look down there! Children are in trouble…" (`HELP_TEXT_BAYWATCH_16`); good "We're not too good at this sort of…" (`HELP_TEXT_BAYWATCH_17`); evil "Great! I get to see more people drown…" (`HELP_TEXT_BAYWATCH_18`); good "We should be able to save them. We…" (`HELP_TEXT_BAYWATCH_19`); good "Let's get the Creature over there to scoop…" (`HELP_TEXT_BAYWATCH_20`); evil "You mean, let's get him over there for…" (`HELP_TEXT_BAYWATCH_21`): cut content; the fisherman lines refer back to an earlier fisherman rescue
- Line 15 of this quest is asked for by the cut Big Fish quest's scroll but is not in the text table: that quest belongs to the shared/unused list
- A testing script that sets up land 2 and runs this quest at once is in the sources but not in the shipped program: developer test only

#### story/silver_scrolls/the_shaolin

From [The Shaolin](progress/story/silver_scrolls/the_shaolin.md).

- The text table has five guru-quest lines no script says: evil, "He saw us! I say we leave him…" (`HELP_TEXT_L3_MONK_15`); guru, "You tried to follow me before." (`HELP_TEXT_L3_MONK_17`) and "Please leave me with my secrets." (`HELP_TEXT_L3_MONK_18`); "What was that?"; and "Leader, we're not managing to impress anyone in…" (`HELP_TEXT_L3_MONK_22`): cut dialogue; nothing to play

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
- Seven slaver lines exist but no script says them: "Good grief. You killed all the slave traders." (`HELP_TEXT_SLAVERS_12`) (good), "You've murdered all the slaves. Dreadful." (`HELP_TEXT_SLAVERS_14`) (evil), "The Slavers have gone." (`HELP_TEXT_SLAVERS_15`) (good), "Hmm. Those slavers are up to something." (evil; the text keeps a stray "TA strokebeard]" stage direction), "The Slavers have kidnapped all the women from the Village!" (`HELP_TEXT_SLAVERS_17`) (evil), "The Slavers are attacking the Village!" (`HELP_TEXT_SLAVERS_18`) (good) and the chief's "Hey. You're our millionth customer. Have all our…" (`HELP_TEXT_SLAVERS_24`): cut: never said in the shipped game
- The "all slavers dead" script is empty (comments plan cheering slaves released to the town and a "Slavers dead" line); the "killed enough" script's case for a dead chief is an empty block with a note to "do something" and attach slavers and slaves to the town: cut: the shipped game does nothing there
- The circus finale has a note to make the slavers do tricks with the animals; the camp builder's comment hoped for better cages made in the editor: cut
- The raid's script drives the circus animals' flock to the village and disbands it there, but every paid animal is held in its script state from its trade on, and a flock's move makes none of them walk, so they stay at the pen and the flock is never disbanded: never happens in the shipped game ([A flock's move sets fields, not a walk](../scripts/script-commands.md#a-flocks-move-sets-fields-not-a-walk))
- The warning is compiled into the shipped challenge file, but nothing starts it: no land control script, no other script and the file's own header run it (it has no start-up line), so it never appears in the game: searched every script source and the compiled program's start-up list; only its own definition exists
- It was set at the Indian village (town id 10) by the shore: a bronze (tips) scroll by a hut (2821, 3052), about 80 from the village centre and just east of the Baywatch quest's hut: cut
- Its notice would have sent the evil advisor out pointing with "Your godly attention is required here, Leader." (`HELP_TEXT_GENERAL_CHALLENGE_START_06`) (a line the text table gives to the good advisor) until the scroll or hut was clicked: cut; the shared scroll-notice script
- An Indian fisherman would come out of the hut and walk to a spot 14 in front; the camera flies to him (position 4 seconds, focus 2), and once he arrives he turns to the camera: cut
- He says: "A band of foreigners have taken some of our womenfolk!" (`HELP_TEXT_KIDNAPPED_WOMEN_01`) / "They kidnapped them when we were out hunting!" (`HELP_TEXT_KIDNAPPED_WOMEN_02`) / "We tried to track them but they're long…" (`HELP_TEXT_KIDNAPPED_WOMEN_03`) / "Three women were taken. Please, holiest, look out for them." (`HELP_TEXT_KIDNAPPED_WOMEN_04`): cut; the source's comments carry an older draft ("Strange foreigners have kidnapped our wives!" and so on)
- The camera returns over 3 seconds, but a typo moves the camera's position twice (to the old position, then to the old focus point) and never restores its focus; the fisherman walks home and is let go: cut; the same in the compiled program
- No log entry, reward or link to the Slavers quest: a closing comment only hopes to check whether "the Indian Slave women" are returned once the Slavers quest has begun. The Slavers' slaves are Greek, three of them women, and the quest's own introduction already has a villager warn of the missing people: cut
- Comments disagree with the code: they call the slaves Celts (the script makes Greeks), say the den script makes both animal flocks (it makes one, of lions), and the land's control script calls the town Norse (it is Greek): source comments only

#### story/silver_scrolls/the_spiritual_healer

From [The Spiritual Healer](progress/story/silver_scrolls/the_spiritual_healer.md).

- It is a background event, not a silver scroll: no scroll stands anywhere, nothing has to be clicked, and no record, reminder or alignment is written to the challenge log: checked in the source and the compiled program: no highlight, snapshot or title
- The village is Greek: the land's map script makes town id 9 a Greek village; the control script's comment calls it Norse, and the villagers made at the end are Norse ones: naming only
- No music is played
- The script reads its village by id itself and ignores the village the watcher passes it
- His spoken lines read slightly differently from the developers' comments (e.g. "You kill me, you kill them all." (`HELP_TEXT_SPIRITUALHEALER_20`)): comments only
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
- An earlier version had three boulders to knock off (each with its own advisor line and the reminder "See if you can hit that pillar…" (`HELP_TEXT_REMINDER_36`) until all three fell); it was commented out and its lines are gone from the text table
- Only one fisherman is made, though the script is written to make several watchers spread around the spot

#### scripts/challenge_natives_advisors_and_text

From [Challenge natives: advisors and text](progress/scripts/challenge_natives_advisors_and_text.md).

- GET_FIRST_HELP and GET_LAST_HELP of an object that is not a spell dispenser (its own info's HelpStartEnum, +0x28's +0x50 in `GScript::GetFirstHelp` 0x6FD0A0 and `GetLastHelp` 0x6FD180): no shipped script asks for it, the only calls (Reward.txt 3616, 3621) pass a spell dispenser; openblack gives nought there
- Shows the shape of a gesture at a position: *animate gesture ‹value› at ‹value› radius ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)

#### scripts/creature_mind_scripts

From [Creature mind scripts](progress/scripts/creature_mind_scripts.md).

- The four older files (`CreatureDestroyOtherCreatures`, `CreatureDestroyTowns`, `CreatureImpressTowns`, `CreatureProtectTowns`, 54 to 130 KB) are in a format the shipped game can't load, and nothing names them: left over from development; our reader rejects them the same way

#### scripts/dance_scripts

From [Dance scripts](progress/scripts/dance_scripts.md).

- The developers' dance editing (a dance-for-editing state and an editor state): a developer tool; the test land for it (`dance.txt`) loads a landscape that isn't shipped
- Villagers praying facing inwards: file `DancePrayInwards`, 120 beats, no set length, any number of dancers, starts on its own: the file isn't shipped (only `DancePrayingInwards.dat` is); the game makes the dance all the same, with one group and no key frames, and so does our tree
- Villagers dancing around an object narrower than 20: file `DanceAroundObjectDiameterLess20`, 90 beats, length 900 (units unconfirmed), 14 to 20 dancers: never chosen: objects 10 or wider get the "20 or wider" dance (see [../town/artefacts.md](progress/town/artefacts.md))
- Children dancing together in town: file `ChildrenDancing` (row 2, the meeting dance), 120 beats, length 12000: never made: checked 2026-11-08: the dance's constructor (0x50B6A0) has nine callers, and none passes this row: the script's `DANCE_CREATE` takes the script's number, and the shipped scripts pass only 14 (followus.txt:453) and 6 (piedpiper.txt:433); a finished building 4 or 5 by its width (0x76604E / 0x76605A); a town artefact 15, 16 or 18 by its width (`FixedObject::GetArtifactDanceInfo` 0x50CA20); a worship site 19 and on (0x77AF37); the creature's dances the types of its actions, 1 and 9 to 13 ([dances](../beings/dances.md)); the town celebration 6; and the playtime dance (0x66C890), whose one maker (0x66C350) has no caller and no pointer to it, so it is only ever read back by a saved game's loading (0x432263). Its progress row is gone from [dance_scripts.md](progress/scripts/dance_scripts.md)
- A housewives' dance in town: file `Test` (row 3, the housewife dance), 120 beats, length 240: never made: checked 2026-11-08: the dance's constructor (0x50B6A0) has nine callers, and none passes this row: the script's `DANCE_CREATE` takes the script's number, and the shipped scripts pass only 14 (followus.txt:453) and 6 (piedpiper.txt:433); a finished building 4 or 5 by its width (0x76604E / 0x76605A); a town artefact 15, 16 or 18 by its width (`FixedObject::GetArtifactDanceInfo` 0x50CA20); a worship site 19 and on (0x77AF37); the creature's dances the types of its actions, 1 and 9 to 13 ([dances](../beings/dances.md)); the town celebration 6; and the playtime dance (0x66C890), whose one maker (0x66C350) has no caller and no pointer to it, so it is only ever read back by a saved game's loading (0x432263). Its progress row is gone from [dance_scripts.md](progress/scripts/dance_scripts.md)
- The town's celebration around its village centre: file `DanceAroundTownCentre` (row 8), 120 beats, length 50000: never made: the town celebration's set-up makes it only for a reaction started from a town centre, and its priority takes only a reaction started by the villager's own town, so that branch is never reached ([the town celebration](../beings/villager-reactions.md)); no other caller of the dance's constructor passes row 8 (checked 2026-11-08, the callers as for row 2 above). Its progress row is gone from [dance_scripts.md](progress/scripts/dance_scripts.md)
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
- Coloured dance lights (red, green, yellow, blue, white) light the creature's dance: the game makes and moves them (only `CreatureDanceWithLightsAndCamera` has them) but the routine that draws them has no caller, and the path groups they count for hold no dancers; see [Dances](../beings/dances.md#the-dance-lights-actions-2023)
- The map script can load a tribe's dance: LOAD_TRIBE_DANCE is in no line of the game's Scripts folder (map.txt is one LOAD_FEATURE_SCRIPT line; checked 2026-11-05)

#### scripts/challenge_natives_objects_and_world

From [Challenge natives: objects and world](progress/scripts/challenge_natives_objects_and_world.md) and
[Hand in scripts](progress/hand/hand_in_scripts.md).

- Naming what a script creates after the task that made it: the ten creating natives (`CREATE`, `CREATE_HIGHLIGHT`,
  `CREATE_TIMER`, `CREATE_MIST`, `CREATE_FLOCK`, the rewards, `CREATE_WITH_ANGLE_AND_SCALE`, the random villager) ask
  the VM for the running task's name and hand it to `GameThing::SetScriptNameOfCreate` 0x56FA70, which in the v1.42
  exe is only `ret 4`, and the name kept at `ScriptDLL` +0x24 is never read: a debug-build hook emptied in the
  release, with no effect in the game; openblack keeps no such name
  ([naming a create](../scripts/map-script-reader.md#the-natives))
- `GET_OBJECT_DROPPED` (native 88): gives the thing a creature last put down or threw; no shipped script calls it:
  never run. openblack ports it from the v1.42 exe for the scripts of a data mod (`CreatureDroppedObject`; the
  original's writers and messages are on
  [the creature's pose and animation](../creature/pose-and-animation.md#movement-body-over-time-spells-fights-throws))
- `CLEAR_DROPPED_BY_OBJECT` (native 89): forgets that thing; no shipped script calls it: never run. openblack ports it
  with GET_OBJECT_DROPPED
- `RELEASE_FROM_SCRIPT` on a ball (script type 28, `Ball` of `ObjectType`): `ReleaseScriptThingIntoTheGame` 0x70F600
  sets the ball's +0x2FC flag (the case at 0x70F6A4), but no shipped script ever holds a ball. No `CREATE` or
  `CREATE_WITH_ANGLE_AND_SCALE` in the 98 scripts of `challenge.chl` makes type 28 (they make 3 4 5 6 8 12 15 16 20 22
  23 24 25 26 30 31 39 41, and 3 8 20 32 33 34 36 with an angle and scale); the one create of a variable type
  (`BringBackTheDead`, the Singing Stones) remakes what `GET_DEAD_LIVING` found, a dead villager or animal only; the
  `get 28 total event` lines (HandDemos) read a hand event's count, not an object; no script variable taken from the
  hand, a click or a creature's hand (`GET_OBJECT_HELD`, `GET_OBJECT_CLICKED`, `get held by`) is ever controlled or
  released. The land files (Land1 to Land5, LandT, the Playgrounds) place no ball, and a ball is otherwise made only
  by a football pitch (`Football::CreateBall` 0x5313E0), which needs the Soccer Add-On's switch, off in the original
  without it ([football](progress/town/football.md#the-add-on-and-its-switch)), or by a network packet
  (`Ball::Create` 0x435B90's three callers). openblack has no ball and does not port the flag
- A town asked for an animal by the script's container finds (`CALL_IN`, `CALL_IN_NEAR`, `CALL_IN_NOT_NEAR`, type 6
  in a town): `Town::FindAnimal` 0x73E6E0 walks the town's flocks (+0xF08) for the first animal that passes the
  filter. None of the 24 `CALL_IN`, 3 `CALL_IN_NEAR` and one `CALL_IN_NOT_NEAR` calls of the 98 scripts of
  `challenge.chl` (`refs/chl`) asks a town for an animal: those asking a town ask for villagers (4, 5), and the one
  type 6 (`greedyfarmer.txt:389`) asks a flock; `Town::FindLiving` 0x73E720, its other caller, has none. Never run; openblack
  looks for no animal in a town ([villager speed and scripts](../beings/villager-speed-and-scripts.md#the-original)).
  The town's animal list (+0x984) is not this: the town's deletion reads it, and openblack keeps it
- A building site for a totem statue, a field or a fish farm a script sets below whole: after `SET_PROPERTY`
  `BUILT_PERCENTAGE` (0x70EC69) the worker gives any `MultiMapFixed` with a town (vt +0x48) that is not in the
  town's building list a site (`Town::AddBuildingSite` 0x73B8E0); a totem statue (`TotemStatue::GetTown` 0x738480),
  a field (`Field::GetTown` 0x528960) and a fish farm (`FishFarm::GetTown` 0x52C450) answer one. No shipped script
  sets theirs. The 29 sets of property 22 in the 98 scripts of `challenge.chl` (`refs/chl`, the form `22 of <thing>
  = <v>` or `+=`) are on things found as type 2 (abode), 18 (citadel) or 36 (spell dispenser), or created as a
  feature (TheMissionaries' dry dock); none is found or made as type 40 (totem statue), and a fish farm has no script
  type (`GameThingWithPos::GetScriptObjectType` 0x570200, 0), so no find reaches one. A field answers type 2
  (`Abode::GetScriptObjectType` 0x406810), but each type-2 find that is set below whole takes the nearest abode
  of a building standing at its point: Land 4's workshop, houses and town centre (0.0 to 0.6 m from a planned abode
  of `Land4.txt` that `BUILD_BUILDING` has just begun; the nearest field 16.5 m off, outside the radius of 10), Land
  5's four wonders (1.0 to 4.3 m), Shaolin's wonder (0 m, radius 5), and Land 2's town centre (beginland2:104,
  radius 50), which `GiveScaffold`, run to its end just before, has Khazar build with the scaffold at that very
  point (`LearnWorshipping` finds it there later at radius 10), while the town's nearest field is 32 m off. Never
  run; openblack gives these three no site (the abodes' is ported:
  [script commands](../scripts/script-commands.md#properties-and-flags))
- Stops villagers reacting to an object (unconfirmed): *detach reaction ‹object›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives where an object is heading: *destination of ‹obj›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives the slowest speed in a flock: *get slowest speed in ‹flock›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives the arena (unused in the shipped scripts) (unconfirmed): *get the arena (no statement in the language)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Lets an object out of its area (unconfirmed): *release a confined object (no statement in the language)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives the flock an object belongs to: *get ‹member› flock* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Whether a flock is within its limits: *‹object› within flock limits* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Starts an object over: *restart ‹object›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Reads the other properties no shipped script reads (moving, running and default speed, death, thing type, the x and z position, fight power); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Clearing an actor's mind (CLEAR_ACTOR_MIND, 346): no statement of the script language compiles to it and no shipped script calls it, so it never runs; read in the exe (`GScript::ClearActorMind` 0x6FE750, read 2026-11-05) it says "Thing not valid!" for a thing that is not a town, then on any thing deletes the object at +0xB0 (a villager's script wander or animation, one of a town's desires on a town); see [the vanilla bugs](progress/vanilla_bugs/creature.md). openblack's native only logs the message
- Starts the cut scenes' script graphics object: *start jc special 3*: no shipped script passes 3; the 10 calls pass 0, 1, 2, 4, 5, 14, 15 and 18 (refs/chl/followus.txt) and 6 (refs/chl/TheMissionaries.txt:837), each run with no "not implemented" line in kept runs (checked 2026-11-07)
- Reads or sets the alignment of anything but a creature: every alignment read and set in the shipped scripts (refs/chl/CreatureCurse.txt, SetupComputerCreatures.txt, BlindWoman.txt, creatureguardian.txt, creaturemirrorfight.txt, land4ogre.txt, landcontrol5.txt, thebigfight.txt) is on a creature: the player's, a rival god's, or one made from the player's creature or as a creature; no kept run logs "not implemented" for it (checked 2026-11-07)

#### scripts/challenge_natives_game_flow

From [Challenge natives: game flow, time and scripts](progress/scripts/challenge_natives_game_flow.md).

- `DEV_FUNCTION` 4 (`LOAD_MY_CREATURE`): the v1.42 exe's switch (`GScript::DevFunction` 0x6FC130, jump table 0x6FC4E4)
  sends it to `GGame::SetPacket(0x4E)` (0x6FC270), the packet that makes or loads the sender's creature at its
  citadel in a multiplayer or skirmish game. No shipped script calls it: every `run ‹n› developer function` of
  `challenge.chl` is a constant, and they are 1, 2, 3, 5, 6, 7, 8, 9, 11 and 12 (`refs/chl`, 23 calls; `grep -rhoi
  "run [^ ]* developer function" refs/chl`). openblack logs "not implemented" for it. 10 (`CREATURE_HELP_ON`) is
  called by none either, and does nothing in the original's switch and in openblack
- Starts the countdown clock shown on screen: *start the countdown clock (no statement used)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Hides the countdown clock: *hide the countdown clock (no statement used)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Shows the countdown clock again: *show the countdown clock (no statement used)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Stops every script except those named: *stop all scripts excluding ‹script names›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives the number of buttons on the player's mouse: *number of mouse buttons* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Turns the intro building sequence on or off (unconfirmed): *enable/disable intro building* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Takes the player into the temple or back out: *enter/exit temple* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)

#### scripts/info_scripts

From [Info scripts and tables](progress/scripts/info_scripts.md).

- Two of the narrators used (the guide and the monk) aren't among the narrator numbers the script declares: data quirk: our reader gives them an undeclared narrator (`helptext::k_NarratorUndeclared`); which voice the game gives them is not known

#### scripts/land_script_commands

From [Land script commands](progress/scripts/land_script_commands.md).

- Planned features: `CREATE_FEATURE` with N4 ≠ 0 and `CREATE_NEW_FEATURE` with N5 ≠ 0 make a `PlannedFeature`
  (0x527440, a `PlannedMultiMapFixed`, never drawn) as the last created object
  ([map-script-objects](../scripts/map-script-objects.md)); no script of the game's `Scripts` folder has such a line (its
  1289 `CREATE_NEW_FEATURE` lines all end in 0, and none is `CREATE_FEATURE`; checked 2026-10-26), so the original's game never makes one. openblack makes nothing for such a
  line and writes none as the last created object, where the original's would be the plan: nothing the original does
  is added.
- `BRUSH_SIZE`: Sets the editor's brush size; not used by the shipped scripts: an editor setting, empty in openblack
- `MULTIPLAYER_DEBUG`: Turns on network debugging; not used by the shipped scripts: a developer switch
- `SET_A_TOWNS_INFLUENCE_MULTIPLIER`: not a command of the original's land table (105 commands at 0xC20F00), so its 76 lines in 10 of the playgrounds' scripts (none in LandT, checked 2026-11-07) run nothing ([map-script-reader](../scripts/map-script-reader.md#how-a-line-is-read)); openblack bound it to a stub that only logged, removed 2026-10-26 (test `LandScriptReader.SetATownsInfluenceMultiplierIsNoCommand`)
- `CREATE_BASE_WITH_ANGLE`: not a command of the original's land table either (no such name anywhere in the frozen exe), so the one line of it in the shipped data, `4- The Japanese God.txt`, runs nothing in the original as in openblack, whose reader skips it as any unknown word ([map-script-reader](../scripts/map-script-reader.md#pending))
- `CREATE_TOWN_TEMPORARY_POTS`: Gives a town its temporary food and wood pots (the map script's case 0x27, 0x716B58); no land, playground or other script of the game's data uses it (only the exe's command name has it): never run; openblack binds it to an empty command, and the towns' own temporary pots, which a villager needing a store makes, are ported ([storehouse](progress/town/storehouse.md))
- `OUTPUT_VILLAGERS`: Writes the villagers out (debugging); not used by the shipped map script: developer output
- `OUTPUT_TOWN`: Writes the towns out (debugging); not used by the shipped map script: developer output
- `OUTPUT_CREATURES`: Writes the creatures out (debugging); not used by the shipped map script: developer output
- `OUTPUT_COLLIDE`: Writes collisions out (debugging); not used by the shipped map script: developer output
- `OUTPUT_ALLOC`: Writes memory use out (debugging); not used by the shipped map script: developer output
- `SAVE_FOR_NET_DEBUG`: Saves for network debugging; not used by the shipped map script: developer output
- `LOAD_FOR_NET_DEBUG`: Loads for network debugging; not used by the shipped map script: developer output
- `OUTPUT_RAND`: Writes the random numbers out (debugging); not used by the shipped map script: developer output
- `CREATE_VILLAGER` (a villager at a place, going to a place): no shipped script uses it, and the original reads the villager's kind from its second argument, which is a position, so it never names a villager and never makes one ([map-script-reader](../scripts/map-script-reader.md#villagers-from-the-map-script)); openblack makes nothing for it, the same
- `COUNTRY_CHANGE` (the land's country changed round a place): no shipped script uses it, and the feature script's jump table sends it to its default case, so it does nothing in the exe ([land_height](progress/terrain/land_height.md)); openblack's handler is empty too
- `HEIGHT_CHANGE` (the land's height changed round a place): no shipped script uses it, and the same jump table sends it to its default case ([land_height](progress/terrain/land_height.md)); openblack's handler is empty too
- `CREATE_AREA`: Marks an area of the land with a radius; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `EDIT_LEVEL`: Opens the land in the game's editor; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `FLY_BY_FILE`: Names a camera fly-by for the land; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `SET_LAND_BALANCE`: Sets a balance number for one player; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `TOWN_NEEDS_POS`: Where a town's needs are shown; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_BASE`: Creates a base; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_CREATURE_PEN`: Creates a creature pen; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_FURNITURE`: Creates furniture; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_PITCH`: Creates a football pitch; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_PLANNED_WALL_SECTION`: Plans a piece of wall; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_WALL_SECTION`: Creates a piece of wall; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CREATE_PATH`: Creates a path; not used by the shipped scripts; no line of the game's Scripts folder has it (checked 2026-11-05)
- `SET_NO_PLAYERS`: Sets the number of players; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_TRIBE_DANCE`: Loads a tribe's dance; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `SET_DATE`: Sets the date; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `SET_TIME`: Sets the time; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `SET_TURNS_PER_YEAR`: Sets how many game turns make a year; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `PAUSE_GAME`: Pauses the game; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_LANDSCAPE`: Loads a landscape; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_GAME_SCRIPT`: Loads the compiled challenge script; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_RAW_GAME_SCRIPT`: Loads challenge script source; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_LANGUAGE`: Loads the language; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_CHALLENGES`: Loads the challenges; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `LOAD_OLD_CREATURE`: Loads the player's old creature; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `CONFIG`: Reads the configuration; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- `KEYCONFIG`: Reads the key configuration; not used by the shipped map script; no line of the game's Scripts folder has it (checked 2026-11-05)
- The script reader's `IF` and `ENDIF` lines: no line of the game's Scripts folder or its online maps has them (checked 2026-11-07); openblack does not port them ([map-script-reader](../scripts/map-script-reader.md))
- `SET_GAME_TICK_TIME`: Sets how long a game turn lasts; not used by the shipped map script; no line of the game's Scripts folder or its online maps has it, only the exe's command name (checked 2026-11-07)
- `CREATE_CREATURE` in the map script: Creates a creature; not used by the shipped map script (`map.txt` is one `LOAD_FEATURE_SCRIPT` line; checked 2026-11-07); the land scripts' own command of that name is another row

#### scripts/map_and_demo_scripts

From [Map, demo and test scripts](progress/scripts/map_and_demo_scripts.md).

- The demo's own challenges: the shipped `challenge.chl` is the full game's; the demo's scripts aren't shipped
- Loads a developers' landscape (`DEVDATA\Landscape\old.led`) that isn't shipped, so neither the game nor openblack can open it: the landscape is missing from the game
- Builds five Celtic towns with 43 buildings, 168 villagers, 19 cows, 65 trees and two temples and two worship sites (the player's and a rival's), and switches on a second computer player with an old-style creature: for reference: what the test land would build; every command it uses is covered in [land_script_commands.md](progress/scripts/land_script_commands.md)
- Loads a dance test landscape (`dance.led`) that isn't shipped, and builds nothing else: the landscape is missing from the game

#### scripts/land2_script

From [Land 2 script](progress/scripts/land2_script.md).

- Starting on this land runs this land's challenges; the original starts its land control script only on Land 1 (`GGame::OnNewGame` 0x5538E0, its land test at 0x553942; [land-change-and-saving.md](../scripts/land-change-and-saving.md)), as openblack does (`new_game_choice::StartsTheStory`, test `NewGameChoice.TheStoryStartsOnlyWithItsProgramReadAndOnLand1`) (checked 2026-11-05)

#### scripts/land3_script

From [Land 3 script](progress/scripts/land3_script.md).

- Starting on this land runs this land's challenges; the original starts its land control script only on Land 1 (`GGame::OnNewGame` 0x5538E0, its land test at 0x553942; [land-change-and-saving.md](../scripts/land-change-and-saving.md)), as openblack does (`new_game_choice::StartsTheStory`, test `NewGameChoice.TheStoryStartsOnlyWithItsProgramReadAndOnLand1`) (checked 2026-11-05)

#### scripts/land4_script

From [Land 4 script](progress/scripts/land4_script.md).

- Starting on this land runs this land's challenges; the original starts its land control script only on Land 1 (`GGame::OnNewGame` 0x5538E0, its land test at 0x553942; [land-change-and-saving.md](../scripts/land-change-and-saving.md)), as openblack does (`new_game_choice::StartsTheStory`, test `NewGameChoice.TheStoryStartsOnlyWithItsProgramReadAndOnLand1`) (checked 2026-11-05)

#### scripts/land5_script

From [Land 5 script](progress/scripts/land5_script.md).

- Starting on this land runs this land's challenges; the original starts its land control script only on Land 1 (`GGame::OnNewGame` 0x5538E0, its land test at 0x553942; [land-change-and-saving.md](../scripts/land-change-and-saving.md)), as openblack does (`new_game_choice::StartsTheStory`, test `NewGameChoice.TheStoryStartsOnlyWithItsProgramReadAndOnLand1`) (checked 2026-11-05)

#### scripts/landT_script

From [Tutorial land script](progress/scripts/landT_script.md).

- The tutorial can be chosen from the game's menu; the exe's only `.\scripts\landT.txt` is pushed by the F2 box's answer (0x54C98B), so no menu leads there (xref, checked 2026-11-05)
- A watcher for the Island Keeper's up-and-down-the-mountain walk, which would have the advisor call the player back when the camera loses the Keeper, is compiled but started by nothing: only the round-the-mountain lesson runs a watcher, its own, twice (refs/chl/LandControlT.txt:564 and :696, both in that lesson's script), and the up-and-down lesson runs none (checked 2026-11-08: no instruction of the compiled `challenge.chl` runs it, it is not on the auto-start list, no statement of the decompiled scripts names it but its own, refs/chl/LandControlT.txt:306, and `xref.py str` finds no reference to its name in the frozen exe; `tools/checks/story/rows_seen.py` lists such scripts)


#### scripts/challenge_natives_camera

From [Challenge natives: camera, cut scenes and fades](progress/scripts/challenge_natives_camera.md).

- Gives how far away things are still drawn (unconfirmed): *get inclusion distance* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Sets one of the player's camera bookmarks to a position: *set bookmark ‹value› to ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Locks the camera's turning about a point: *enable/disable fixed camera rotation at ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)


#### scripts/challenge_natives_creature

From [Challenge natives: creatures](progress/scripts/challenge_natives_creature.md).

- Teaches a creature whether an action is good to do with a kind of object: *teach a distinction about an action (no statement in the language)* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Resets the count of how many times a creature has done an action: *initialise number of ‹value› for ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Gives how many times a creature has done an action: *get number of ‹value› for ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Whether a creature's strongest desire is a given one: *desire of ‹value› is ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Lets a creature attack its own player's town or not: *enable/disable ‹value› attack own town* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Teaches a creature everything except one action: *teach ‹value› all excluding ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Whether a creature is fighting on its own: *‹creature› is auto fighting* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05; native 391 in none of the 31510 native calls of the shipped program, 2026-11-08)
- A script's own arena for a fight, found near a place or made there: `GET_ARENA` (native 211, `GScript::GetArena` 0x6F4AF0, which reuses or makes an arena as the creatures' fight start does) is in none of the 31510 native calls of the shipped program (`Scripts/Quests/challenge.chl`, read 2026-11-08); openblack's native stays a stub. The fight a script forces keeps its row ([fighting.md](progress/creature/fighting.md) "Scripts can start a fight")
- Ties a creature's leash to the player's hand: *attach ‹creature› leash to hand* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)


#### scripts/challenge_natives_magic_and_weather

From [Challenge natives: miracles, influence and weather](progress/scripts/challenge_natives_magic_and_weather.md).

- Whether an object was cast by another: *‹spell instance› cast by ‹caster›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)


#### scripts/challenge_natives_towns_and_players

From [Challenge natives: towns, players and rival gods](progress/scripts/challenge_natives_towns_and_players.md).

- Holds back one of a rival god's behaviours: *set computer player ‹value› suppression ‹value› ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Finds a building of a kind in a town, built at least so far: *get building ‹value› in ‹value› min built ‹value› [excluding scripted]* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Sets a player's land balance: *set ‹value› land balance ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)
- Adds food or wood to a building site (unconfirmed): *add for building ‹value› to ‹value›* (not called by the shipped scripts); the coverage tool finds no call of it in the 514 shipped challenge scripts (checked 2026-11-05)


#### rival_gods/ai

From [How a computer god decides](progress/rival_gods/ai.md).

- The world aim has nothing under it: nothing to do
- The god watching what other gods do and keeping it to copy later ("look and learn"): not in the original: checked 2026-11-08: none of the 126 rows of the mind's tree does it ([the rows](../hand/computer-minds.md#the-rows)), no symbol or string of the frozen exe names it (`xref.py func`/`str` for learn, lesson, remember, copy and watch find only the creature's learning and the help's player watching), and `Scripts/comp.txt` names nothing of the kind; every action of the tree the original runs is ported. Its progress row is gone from [ai.md](progress/rival_gods/ai.md)
- The god checking each turn that the lessons and miracles it remembers still exist: not in the original beyond the cleaning of its list of things seen (the row "It keeps a list of things it has seen", done): checked 2026-11-08: none of the 126 rows of the mind's tree does it ([the rows](../hand/computer-minds.md#the-rows)), no symbol or string of the frozen exe names it (`xref.py func`/`str` for learn, lesson, remember, copy and watch find only the creature's learning and the help's player watching), and `Scripts/comp.txt` names nothing of the kind; every action of the tree the original runs is ported. Its progress row is gone from [ai.md](progress/rival_gods/ai.md)
- A town's food or wood casting leaves casting a miracle at a place (fireballs, lightning, storms, a tornado, the shields): the leaves' shared action has the branch, but no score of the three leaves gives them such an icon (they pick the food or the wood icon) and the one script that names one of them (Land 2's LearnGestures, `refs/chl/LearnGestures.txt:165`) gives the wood icon; openblack's action gives no step for them; see [the minds](../hand/computer-minds.md) (ShallCastFoodSpell)
- A miracle cast at a place by a god's hand from inside its influence (fireballs and lightning, through the hand's
  move that stops at the influence's edge and a cast toward the place): the shared cast at a place has the branch, but
  none of the six actions that use it is given such an icon (their scores pick wood, food, heal and the shields); see
  [the cast at a place](../hand/computer-minds.md#openblack); openblack's gives no step there
- A god throwing a thing at a town it wants to destroy: the leaf DestroytownWithObject has no score, so its node takes
  the thing above it, the town DestroyTown chose, and its action asks for an object there and fails
  ([DestroytownWithObject](../hand/computer-minds.md#openblack)); openblack has no such action
- A god making its creature drink, sleep, or move somewhere warmer or cooler: those four rows of the mind start at
  weight 0, and no shipped script sets them (the story's 25 personality calls name ExpandInfluence,
  ReactToAggressiveCreature, TakeOverTown, DestroyTown, DefendTown, AttackCreatureWithSpells and DefeatPlayer; no
  script loads a personality file or sets a suppression), so their worth is always 0 and they are never chosen
  ([the rows](../hand/computer-minds.md#the-rows)); openblack's actions for them are not ported
- A god reacting to its ill creature: the need's score tests what the game never sets, so it is 0 and the action
  under it never runs ([the rows](../hand/computer-minds.md#the-rows))
- A god raising its town's totem: the leaf's score is 0 in the game ([the rows](../hand/computer-minds.md#the-rows))
- A god using its creature as a diversion against a player who casts miracles or throws things at it: the leaf
  UseCreatureAsDiverseionAgainstPlayer is scored and chosen, but its action does nothing and fails at once, so the node
  is only held back ([the action](../hand/computer-minds.md#openblack)); openblack's action fails the same
- A god carrying a thing near a burning or damaged building of its own (the own action of FindBurningAbode and
  FindDamagedAbode, 0x668210): a node with a score is its own choice only until a child scores, and its worth is its
  best child's times its own, so it is chosen through its parent only when a child was, and then that child's action
  runs; both nodes' children have actions of their own (the water miracle, the creature's help), and no shipped script
  forces these nodes ([weighing a node](../hand/computer-minds.md#weighing-a-node)); openblack has no such action
- A god's shield over a building of its own that has none: the leaf under FindUnprotectedAbode is CastShieldToProtect,
  whose score (the one DefendtownWithSpells has) asks for a town above the leaf's parent and finds the building, so it
  scores 0 and is never chosen ([the shields](../hand/computer-minds.md#openblack)); openblack's score answers 0 the
  same (`ScoreDefendtownWithSpells`, `src/ECS/ComputerPlayerScores.cpp`)
- A computer hand untying its player's leash before it casts a miracle at an object, so that its creature can watch: the cast's start has the test, but it asks for the creature's free time to be below the same free time scaled by what is left for the creature to learn of the miracle (from 0 to 1), which never holds, so the leash is never untied; openblack's cast does not have it; see [the computer players' hands](../hand/computer-hands.md#the-miracle-steps) (CastSpellAtObject)
- Nice and nasty messages, taunts, scares and gifts to a player are in the tree but were never made: they do nothing in the game
- Dealing with other players (giving them things and talking to them, each weighted a quarter): both needs score 0 in the game, so nothing under them is ever weighed or done; the throw at a player under them among it (its action is not ported); openblack's score rules answer 0 the same (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`): a player never sees it

- A god judging how much aggressive magic each player casts at it: the row ReactToPlayerCastingSpells scores 0 in the
  game (0x665030), so its child is never weighed ([PlayerInteract and ReactToPlayer](../hand/computer-minds.md#playerinteract-and-reacttoplayer));
  each player's last cast and casts by type are kept (`PlayerMagic::lastCast`, `castCount`) and no god reads them, as
  in the original

#### rival_gods/challenge_scripts

From [Script control of computer gods](progress/rival_gods/challenge_scripts.md).

- A list of every need's name ships for script writers' help: a text list in the game's script sources, not read by the game
- A step-by-step battle plan for Nemesis: every 30 seconds, by which towns the player holds and whether their belief is rising, it impresses, destroys or defends a town or attacks the player's creature, retuning his weights each time: the script ships but nothing runs it
- Debug scripts start single story scripts on their own: developer tools in the script sources
- The story forcing a god to win, destroy or defend a town, or to send its creature to help one: these forced actions appear only in Nemesis's battle-plan script (`refs/chl/ControlNemesisBattleStrategy.txt:23`, `:49`), which nothing runs; the story only forces a pick-up-and-drop and a scaffold placement, and queues a wood miracle
- The camera's focus or position following a god's hand: no shipped script calls the two natives that do it (see
  [camera/script_camera](#camerascript_camera))

#### rival_gods/creatures

From [The computer gods' creatures](progress/rival_gods/creatures.md).

- Four larger template minds (destroying other creatures, destroying towns, impressing towns, protecting towns) ship with the game but no script loads them, and the game refuses their old layout: unused; our reader rejects their layout too: they are version 17, with no species row where a mind has one, so it answers that they are not mind files (`MindResult::ErrNotAMindFile`, `components/creaturemind/include/MindFile.h`)
- Exported creatures the player saved also sit beside them; they are the player's, not a god's: see [../creature/saves_and_files.md](progress/creature/saves_and_files.md)
- A mirror creature of the player's species, opposite in alignment, angry and fully grown, made from the player's
  creature for a fight: the script that makes it (`CreatureMirrorFight`, `refs/chl/creaturemirrorfight.txt`) is run
  by no script in `refs/chl`, is not on the compiled program's auto-start list (one script there), and its name is in
  the frozen `challenge.chl` only once, in its own definition, and nowhere in the exe, so nothing starts it (read
  2026-11-14); the native it uses, a creature made from another, is ported and used by other scripts ([a creature made
  from another](../creature/players-creature.md#a-creature-made-from-another))
- The explosion miracle was left out of their lessons: commented out in the game's scripts
- Scripts forcing a god's creature to help a town: only Nemesis's battle-plan script forces it (`refs/chl/ControlNemesisBattleStrategy.txt:23`, `:49`), and nothing runs that script; see rival_gods/challenge_scripts
- A god stroking or slapping its creature, or casting a miracle on it for a laugh: the three rows under playing with
  the creature (SlapCreature, StrokeCreature, CastSpellOnMyCreatureForALaugh) start at weight 0, and no shipped script
  sets or forces them (the story's personality calls name ExpandInfluence, ReactToAggressiveCreature, TakeOverTown,
  DestroyTown, DefendTown, AttackCreatureWithSpells and DefeatPlayer; its forced actions are a pick-up and drop, the
  scaffold placements, TakeOverTown, DestroyTown, DefendTown and GetCreaturetoHelp; read from `refs/chl` 2026-11-07),
  so a score times 0 is 0, never above the best, and the cast's child is never weighed
  ([the rows](../hand/computer-minds.md#the-rows), [weighing a node](../hand/computer-minds.md#weighing-a-node));
  openblack's actions for them are not ported
- A god tying its creature's leash to a moving thing for play (the row AttachLeashtoMovingObject, under playing with
  the creature, its action 0x65AEC0): the row starts at weight 0 like the slap and the stroke, and no shipped script
  sets or forces it (no personality call or forced action in `refs/chl` names it, read 2026-11-08), so it is never
  chosen ([the rows](../hand/computer-minds.md#the-rows), row 39); the leash the god does use is tied by its attack,
  destroy, impress and help actions, which openblack ports ([the creatures](progress/rival_gods/creatures.md))

- A god weighing its creature's own wish to learn a miracle: the only reader of how many miracles the creature has
  left to learn from a computer player (`CreatureMental::GetNumMagicActionsLeftToLearnWhichCanBeTaughtByComputerPlayer`
  0x4E2A20) has no caller (`xref.py callers`, 2026-11-08); the miracle lessons are weighed by the counts of lessons the
  mind's own actions raise ([the rows](../hand/computer-minds.md#the-rows), TeachCreature*Magic)
- A miracle on its own creature for a laugh: see the stroke, slap and laugh rows above (weight 0, never chosen); its
  row on [the gods' miracles](progress/rival_gods/magic.md) is removed (2026-11-08)

#### rival_gods/lethys

From [Lethys](progress/rival_gods/lethys.md).

- Five monsoon storm clouds gathering over Lethys's temple on the second land (80 seconds, still and out of the wind):
  the challenge program has the script that makes them (`refs/chl/lethysvortex.txt:83766`), but no script runs it: its
  name appears in no other statement of the decompiled challenge scripts (`refs/chl`, 2026-11-03), and Lethys's exit
  film next to it in the same file never calls it; a player never sees these clouds

#### rival_gods/nemesis

From [Nemesis](progress/rival_gods/nemesis.md).

- He is never a player on the second, third or fourth land; his attacks there are scripted from the vortex or from points in the sky: there is no Nemesis player to judge; see [khazar.md](progress/rival_gods/khazar.md) and [../story/land_4.md](progress/story/land_4.md)
- A script for a step-by-step battle plan (impress, destroy, defend, attack the creature) exists but nothing runs it: unused in the shipped game; see [challenge_scripts.md](progress/rival_gods/challenge_scripts.md)

#### rival_gods/skirmish_opponents

From [Skirmish opponents](progress/rival_gods/skirmish_opponents.md).

- Messages, taunts and gifts to other players are in the mind but were never made: see [ai.md](progress/rival_gods/ai.md)

#### easter_eggs/developer_jokes

From [Developer jokes](progress/easter_eggs/developer_jokes.md).

- Villagers grumble aloud with fourth-wall lines ("I wonder if I'll get a part in the sequel?", "I had to audition for this. Can you believe it?", "I could have been someone. I could have been in Dungeon Keeper.", "I know Peter Molyneux personally.", "Lord, won't you buy me a Mercedes Benz?", "It feels like someone's watching me."): the lines are the villagers' banter texts 1 to 400 of the shipped text table (`HELP_TEXT_VILLAGER_MALE_BANTER_SET1_01` to `_SET20_10`, then `HELP_TEXT_VILLAGER_FEMALE_BANTER_SET1_01` to `_SET20_10`), but nothing makes a villager show or say them: read 2026-11-07, the only reader of any of them in the frozen exe is `GGame::SetUnusedCitadelComputerPlayers` 0x555A80, which names the creature of a network game's temple the computer takes over with text `GameRand(99)` (0 falls back to the table's first, invalid-text line; 1 to 98 are the men's first grumbles; [skirmish opponents](progress/rival_gods/skirmish_opponents.md)); no constant text offset of the table below text 401 is read anywhere (gx.py over every `HelpTextDatabase.array +` access), no message set (the advisors' banter sets among them) starts below text 2813, and no shipped script names or says a text below 401 (`refs/chl`: no literal, range or variable). The villagers' babble is sound only (the `VillagersBanter.sad` bank, [voices_and_speech.md](progress/audio/voices_and_speech.md))
- If the program's linked lists ever went wrong it would stop with "Peter: Damn Richard won the bet" (a node with no node before it), among other "Peter:" messages: only shown on an internal error, never in normal play; nothing to port
- Other developer-signed error messages: "JONTY-When is a flock not a flock?", "JONTY- Something is seriously wrong with my code", "Jonty-Thing should be in script! OH CALL ME!!!!", "No thing - BAD MAN!", "Script offsets exceeded - Blimey!", "Jeremy: failed miserably save has - tell me.", "Richard: no subactions?", "Wierd - this bitstring (11b) is reserved.": internal errors only
- The creature's actions include "kiss friend's arse", "tell creature to sod off", "watch telly" and "fart", and the challenge language has a command to get a creature's "arse position" (where its poo comes from): names inside the program; the behaviours are in [secret_behaviours.md](progress/easter_eggs/secret_behaviours.md) and [unused_content.md](progress/easter_eggs/unused_content.md)
- A key layout named "Daniel's Key Config mofo, dont mess": see [hidden_keys_and_cheats.md](progress/easter_eggs/hidden_keys_and_cheats.md)
- A message left in the program for testers: "This is a developer beta patch. There will be no support from EA regarding this patch for Black & White. ... please fill in the bugreport.txt file included with the patch and email it to [a Lionhead address]": whether any build shows it is unconfirmed
- A Lionhead person's birthday and Christmas lines among the hidden-script texts ("Hey Boss! Today is a special day." (`HELP_TEXT_HIDDEN_SCRIPTS_07`), "Yes. It's someone very important's birthday." (`HELP_TEXT_HIDDEN_SCRIPTS_08`), "So many happy returns to them!" (`HELP_TEXT_HIDDEN_SCRIPTS_10`), "Happy Christmas!"; texts 6483, 6484, 6486 and 6487 of the shipped text table, `HELP_TEXT_HIDDEN_SCRIPTS_07`, `_08`, `_10` and `_11`), and the man who wants to be thrown's "I can see your mouse from here." (`HELP_TEXT_THROW_BLOKE_45`) (text 1984, `HELP_TEXT_THROW_BLOKE_45`; his script says only texts 1940 to 1969 of his set): nothing in the frozen exe reaches these texts: (1) no shipped script says them, alone, in a `constant from .. to ..` range or as a value held in a variable (searched in all 98 decompiled script files in `refs/chl`); (2) no help message set covers them: the 152 sets of `g_helpMessageSets` 0xBF0D60 (start, end, kind; read by `HelpSystem::GetStartAndEndTextForSet` 0x5C8AE0) end at text 3646 at most; (3) the program's code names a text by its number (a compare with the text count, then `HelpTextDatabase.array + 12 × number`, as `CDBox::Init` 0x543D80 does), and neither the numbers nor their 12-byte offsets appear in the code; the 0x7C0 (1984) immediates the code holds are offsets, loop bounds and source lines; (4) the only data holding the numbers is the per-text speech table (rows of text, bank, sample from 0x900D40, 6975 rows, with its per-language copies), read by text number (`MacAdjustHelpID` 0x5C3780), which every text has; (5) no date reaches them: the real clock is read only through the C library's `time` 0x7C79FD (the only user of the GetLocalTime and GetSystemTime imports), and of its callers only the script natives `GScript::GetRealTime`, `GetRealDay`, `GetRealWeekDay`, `GetRealMonth`, `GetRealYear` (0x710F4F to 0x71110F, which no shipped script calls), the night voices (`GSpookyVoices::IsNight` 0x72E3B0, hours only), the first-of-April footprints (`LH3DWetFeet::Add` 0x81F419), the saved game's date (`TempleSaveGame::UpdateDateAndTime` 0x78F79B) and a start-up log line (fn_0046AF20, `asctime`) break the time into a date; the others (`GGame::Init`, `GGame::Close`, `GameStats::Init`, fn_005776E0's seed, `MusicMoodController::UpdateOnGameTurn`) use it as a count of seconds ([pc_integration/real_clock](#pc_integrationreal_clock)); of the same hidden-script set, "One of the Lionhead people." (`HELP_TEXT_HIDDEN_SCRIPTS_09`) (6485) is said by the phone box ([hidden_content.md](progress/easter_eggs/hidden_content.md#the-hidden-phone-box-land-1)) and "Bah, humbug!" (6488) by the evil advisor at the Land 2 sacrifice ([the sacrifice](progress/story/silver_scrolls/the_sacrifice.md))

#### easter_eggs/hidden_content

From [Hidden content](progress/easter_eggs/hidden_content.md).

- Four of the twenty-odd phone box recordings are never played (numbers 3, 12, 13 and 14 of the set): they ship in the script sound bank; see [unused_content.md](progress/easter_eggs/unused_content.md)
- The secret creatures' unlock code, kept in two values of the setup registry key `Software\Lionhead Studios Ltd\Black & White\BWSetup` named "TimerValue" and "TimerCheckSum": still read at start-up (a static initialiser, fn_007118A0, reads `TimerCheckSum` at 0x71190C and `TimerValue`), but the frozen exe's unlock test fn_00711D90 is patched to `B8 01 00 00 00 C3` and NOPs (0x711D90 to 0x711DBF), so every creature is unlocked and the code decides nothing ([the unlock](../creature/players-creature.md#the-bonus-creatures-unlock))
- The code checked at start-up against a number worked out from Windows' product ID, so a code works only on the computer it was made for: the same patch leaves the check's result unused
- The code holding one switch per creature, so any mix of the five can be unlocked: the same patch, all five are always unlocked

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
- `CONVERT`, read when a game starts: a developer tool. The flag (0xD46AC1, set by `ScanParameters` at 0x641BAE) is read only in `GGame::StartGame` (0x54C1B2); when it is set, StartGame has already run `GGame::Init` (which loads the first land) and returns without ever calling `GGame::Loop`, so the game is never played
- The separate Creature Upload utility (`CreatureUpload.exe`) sent the profile's creature to Lionhead's creature site, where it could be viewed at a page of its own name: the site is gone; see [../pc_integration/](progress/pc_integration/)
- The program still knows the paths of several developer files: `savedmap.txt` (and `..\newtestbed\savedmap.txt`), `MemoryLeaks.txt`, a saved copy of `Data\PhysicsConstants.txt`, out-of-sync logs and a physics log: developer leftovers; `MemoryLeaks.txt` even ships in the game folder; which of them a player can trigger is unconfirmed

#### easter_eggs/secret_behaviours

From [Secret behaviours](progress/easter_eggs/secret_behaviours.md).

- Putting a music CD in the drive lets the game play it, and the advisors remark: "So you wanna hear your own music, huh?" (`HELP_TEXT_AUDIO_CD_INSERTED_01`), "What's the matter? Our in-game music not good…" (`HELP_TEXT_AUDIO_CD_INSERTED_02`), "Don't mind him. But watch out for your Creature." (`HELP_TEXT_AUDIO_CD_INSERTED_03`), "He'll always associate this music with what you…" (`HELP_TEXT_AUDIO_CD_INSERTED_04`), "Creatures are sensitive to music, you see." (`HELP_TEXT_AUDIO_CD_INSERTED_05`), "Good gracious. Pumpin' choonz." (`HELP_TEXT_AUDIO_CD_INSERTED_06`): nothing in the frozen exe says these lines (read 2026-11-07). They are texts 2414 to 2419 of the
  frozen text table (`HELP_TEXT_AUDIO_CD_INSERTED_01` to `_06`, `Scripts/InfoScript2.txt`): (1) no shipped script
  says them (`refs/chl`); (2) no message set holds them (none of the game's message sets, `help::message_sets`, spans
  2414..2419; test `HelpMessageSets.TheSetsAreTheGames`); (3) the exe holds 0x96E..0x973 only as other constants
  (persistent class numbers in `StaticCreate_*` 0x6C0B30.. and `GameOSFile::LoadInstance` 0x558DC0, and the text
  tables' own copies at 0x907E68 and the like), never as a text to say. The game plays only its own music banks: the
  CD player box is built but never shown (the line on music from the CD's audio tracks, under
  [audio/music](#audiomusic)), so nothing plays a CD's tracks either
- The creature kissing a friend's backside after asking it to hold still (row 208 KissFriendsArse, builder 0x4B4280): no desire or compassion-for-town table of `Scripts/info.dat` lists row 208, no mimic row notices or copies it, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is row 208, the computer gods' forced creature actions (0x64E000..0x670000) push none of it, no shipped script forces it (`refs/chl`), and its builder is referenced only from the action table's initialiser (read 2026-11-08). Friends still kiss by row 133 KissFriend ([friends_and_other_creatures.md](progress/creature/friends_and_other_creatures.md)).
- The creature's games of running round a race track, racing a friend and throwing stones at a can (rows 123 RunAroundRaceTrack, 139 RunRaceWithFriend and 140 PlayGameOfThrowingStonesAtCan, builders 0x4A6F50, 0x4A92E0 and 0x4A9D90): no desire or compassion-for-town table of `Scripts/info.dat` lists rows 123, 139 or 140, no mimic row notices or copies them, no constant action given to `ForceActivityAndForceAction` 0x4C4450 or `CreaturePlan::CreaturePlan` 0x4F1230 is one of them, the computer gods' forced creature actions (0x64E000..0x670000) push none of them, no shipped script forces them (`refs/chl`), and their builders are referenced only from the action table's initialiser (read 2026-11-08). Throwing its die (row 324 ThrowDie) is listed by the play desire and the toy mimic row, and is the original's ([secret_behaviours.md](progress/easter_eggs/secret_behaviours.md)).

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
- Four of the hidden phone box's recordings are never played (numbers 3, 12, 13 and 14): in the script sound bank
- Dance files named "Test" and "OnlyDanceThatWorks" in `Scripts/Dance`: (unconfirmed whether any dance loads them)
- A "cheat box" model (a plain box 180 × 400 × 180, sitting entirely below the ground) is in the model list; there is no bowling pin model, and the toy bowling ball and skittles are shipped toys used on the tutorial land: (unconfirmed whether the cheat box is used anywhere); the shipped toys and the cut bowling games: [../nature/toys.md](progress/nature/toys.md)
- The script sources reference text for cut challenges (25 lines for the marauders, 17 for whack-a-villager, the chimps' names): whether the shipped text still holds them is unconfirmed

### Engine

#### engine/asset_formats

From [Asset formats](progress/engine/asset_formats.md).

- Light glows (`.glw`) placed in the lands: the exe names seven `.glw` files, every one a temple room's own (`data\citadel\engine\challenge.glw`, `creature`, `credits`, `options`, `savegame`, `multi` and `main`, each from its room's constructor or `InitEngine`, e.g. `ChallengeRoom::ChallengeRoom` 0x781AF6, `WorldRoom::InitEngine` 0x79D2F3; `xref.py str glw`, any case, 2026-11-14), bw1-decomp names a `.glw` only in `WorldRoom.cpp`, and the shipped data has no `.glw` outside `Data/Citadel/engine`; so no land places glows, and the row keeps only the temple's
- Loose wave files (`.wav`): our wave decoder reads them (`src/Audio/Codec/WaveFile.cpp`), but the game has none to read. Our wiki differs: the original plays no loose wave files, every sound is in a bank ([audio](../formats/media-files.md#sas-and-animation-tables))
- The script library and language plug-ins (`Plug Ins/*.dll`): openblack builds the script VM in

- The creature's chat files (`Data/Language/blackhal.*`: `.aux`, `.ban`, `.grt`, `.swp`, `.trn`): read only by the language plug-in (`Plug Ins/LanguageR.dll`, a MegaHAL chat program, whose strings name them and `Data\Languagelackhal.txt` / `.log`). The exe loads that plug-in only from the map script's `LOAD_LANGUAGE` (case 0x13 of `GSetup::MapCommandProcess`, calling fn_005E67C0 0x714F67, which opens `Plug Ins\Language`), and the exe runs map-script commands (`GSetup::MapCommands`, handled by `GSetup::MapCommandProcess` 0x714D10) from only two files: `.\Scripts\Map.txt` at a new campaign (`GSetup::LoadMapScript` 0x715080, called by `GGame::Init` 0x54F7AB), whose one line in the frozen game is `LOAD_FEATURE_SCRIPT(".\scripts\land1.txt")`, and `.\Scripts\MPDebug.txt` on a network game's start (0x54F8E9), a developer's debug file that is in no folder of the game (`_gamedata/frozen`, checked 2026-11-07), so no shipped file asks for it. The row "Speech files" was removed from [Asset formats](progress/engine/asset_formats.md) (2026-11-07)
- The font files `Data/lrg.srf`, `med.srf`, `sml.SRF` and `Data/HelpFont.bmp`: the exe names no `.srf` file and no `HelpFont` (`xref.py str`, ASCII and UTF-16, and a byte search of `runblack.exe`, 2026-11-07); the fonts it does name are `DATA\LrgFont.fff` (0xC39170) and `Data\Font0-0.bmp` (`start_system`, 0x6430EC), which stay on the row "The other fonts" of [Asset formats](progress/engine/asset_formats.md)
- The colour lookup and contrast tables (`Data/Textures/old_effect.clut`, `contrast.cnv`): the exe names no `.clut` or `.cnv` file and no `contrast` (same searches, 2026-11-07). The creature portrait sets (`.cps`) stay on their row: `CopyBlockComputerSymbol` 0x5E146A reads `Lethis.cps` / `LethisL.cps`
- The weather and country tables (`Scripts/weatherinfo.lhw`, `Scripts/country.lhw`): read only by the internet weather, whose service is gone ([real weather](#pc_integrationreal_weather)). `weatherinfo.lhw` is opened by its set-up for a game and its fetch (`LHInetWeather::InitForGame` 0x5E98B0, fn_005E9350); `country.lhw` (`.\scripts\country.lhw`, 0x92B3B0) is read at start-up by the internet weather's constructor (sub_5E7CE0, from the static initialiser fn_0054A650) into its list of country names (fn_005E9740, `code=Name` lines, in `LHInetWeather.cpp`), which only names the online place (`xref.py str`, `xref.py brief`, 2026-11-07; an earlier reading said the exe held no `country.lhw` name). openblack reads neither and takes its climates from `info.dat`. The row "Weather and country tables" was removed from [Asset formats](progress/engine/asset_formats.md) (2026-11-07)

#### engine/file_system_and_packs

From [File system and packs](progress/engine/file_system_and_packs.md).

- Player-made data overrides (mods): the game has none. openblack's old mod support was removed (2026-10-06); what stays is the minimal mod loader library (`src/ModLoader`), and only with it and its mods folder read, a switched-on mod's files replace the game's files of the same path (data mods), a 🎯 chosen difference of 2026-10-22 ([data mods](mod-loader.md#data-mods-a-mods-files-replace-the-games))

#### engine/game_loop_and_clock

From [Game loop and clock](progress/engine/game_loop_and_clock.md).

- A land's map script setting the turn's length (`SET_GAME_TICK_TIME`, case 5, which writes the milliseconds per turn) or the start date, time and days gone (`SET_DATE`, `SET_TIME`, `SET_TURNS_PER_YEAR`, cases 2 to 4): the exe runs map-script commands (`GSetup::MapCommands`, handled by `GSetup::MapCommandProcess` 0x714D10) from only two files: `.\Scripts\Map.txt` at a new campaign (`GSetup::LoadMapScript` 0x715080, called by `GGame::Init` 0x54F7AB), whose one line in the frozen game is `LOAD_FEATURE_SCRIPT(".\scripts\land1.txt")`, and `.\Scripts\MPDebug.txt` on a network game's start (0x54F8E9), a developer's debug file that is in no folder of the game (`_gamedata/frozen`, checked 2026-11-07), so none of these runs. The rows "A land's map script can set the length of a game turn" and "A land's map script can set the start date, start time and turns per year" were removed (2026-11-07)

#### engine/info_tables

From [Info tables](progress/engine/info_tables.md).

- The creature pens' table put to use: the game reads it with the other tables (`load_variables`, `GCreaturePenInfo::Load` / `LoadBinary`, as openblack does), but its only reader is a creature pen object (`GCreaturePenInfo::GetBaseInfo`, a virtual of the pen), and the one maker of a pen (fn_004EEF60, 0x4EEF60) is called only by the land script's `CREATE_CREATURE_PEN` (sub_715180, 0x7160A9) and by a planned pen's `CreatePlanned` (0x4EF0D1), which no shipped land or playground script reaches (see building/civic_buildings and engine/script_vm above); the temple's pen is drawn by `CitadelHeart::DrawCreaturePen` without the table. The row "Creature pens" (read, not used) was removed (2026-11-07)
- The arrows' table (`DETAIL_SHOT_INFO`) put to use: the game reads it with the other tables (`load_variables` 0x42CE91, `GArrowInfo::Infos` 0xC56010, as openblack does), but no code reads it after (`xref.py refs 0xC56010 --window 0x600`: only the loader, the table's constructor and destructor); its only reader would be an arrow object (`GArrowInfo::GetBaseInfo`, a virtual of the arrow), and the arrow's vtable 0x8C3F5C is written only by its one maker sub_4259B0 (0x4259EB), which has no caller, direct, by pointer or in data (`xref.py callers 0x4259B0`, 2026-11-07), so no arrow exists in the game. The row "Arrows, script highlights and showing a town's needs" of [Info tables](progress/engine/info_tables.md) was set done (2026-11-07)
- The dance table's area required put to use: the game reads it with the rest of `DETAIL_DANCE_INFO` (`load_variables`, `GDanceInfo::Infos` 0xCC4B80, 25 rows of 0xB0 bytes), but no code reads a row's +0x9C or +0xA0 after: `xref.py readers 0xCC4B80-0xCC5CB0` lists only +0x10, +0x14, +0x98, +0xA4 and +0xA8 (and row 6's and 8's +0x10), the callers of `Dance::GetInfo` 0x50C460 read +0xA8 (fn_00426130) and +0xAC (`Living::PerformDance`), and `Dance::Dance` 0x50B6A0 passes only the file name (+0x58) and the row to `Dance::Initialise` (2026-11-07). openblack reads it nowhere either (`GDanceInfo::areaRequired`). The row "Dances" of [Info tables](progress/engine/info_tables.md) was set done (2026-11-07)
- The help spirits' table (`DETAIL_HELP_SPIRIT_INFO`, `HelpSpiritInfo::Infos` 0xD15AE8, 3 rows) put to use: the game reads it with the other tables (`load_variables`, as openblack does); after that its only users are the two spirits the help system makes (`HelpSystem::CallVirtualFunctionsForCreation` 0x5C5860 → `HelpSpirit::Create` 0x5C4B30 → `HelpSpirit::HelpSpirit` 0x5C4AA0, `Object::Object(coords, &Infos[type])` 0x636520, the only place that takes the table's address besides the loader and `GetBaseInfo`: `gx.py grep HelpSpiritInfo::Infos`). The object reads one field of its row, the object type (+0x10), in `Object::InitialiseIsFixedForMapList` 0x63A640 (from the constructor and from `HelpSpirit::CallVirtualFunctionsForCreation` 0x5C4B90), which sets the fixed-for-map-list flag; the spirits are never put on the map (`Create` links nothing; `GameThing::GameThing` adds the thing to no list), their mesh is the help sprite's own (`HelpDude::LoadMesh` 0x5C0F30, set at 0x5C4B76), and their methods read nothing else of the row: `gx.py grep "vt->|->info" --in HelpSpirit` finds only that call, `HelpSystem::ProcessSpirits` 0x5C9060 calls only `HelpSpirit::Process` 0x5C5270, and the pointing and looking functions' virtual calls (vt +0x2C, +0x42C in 0x5C4FA0, 0x5C4E4E, 0x5C5170) are on the thing pointed at (2026-11-07). Not settled by a run: a read breakpoint on the table in the running original would confirm it. The two spirits' creation numbers are already taken (`object_index`, the first land starts at 2). The row "The help system, help spirits and help sprite guidance" of [Info tables](progress/engine/info_tables.md) was set done (2026-11-07)
- The jobs table (`DETAIL_JOB_INFO`, `GJobInfo::InfoList` 0xD19CC8, 16 rows of 0x58 bytes) put to use beyond the shepherds' seasons: the game reads it with the other tables (`load_variables` 0x42CD32, as openblack does), and after that its only reads are `GJobInfo::GetJobActivity` 0x5E1740 (the season's column, +0x28 + season x 4) from `Villager::ShepherdGotoFlock` 0x768E30 and `Villager::ShepherdTakesControlOfFlock` 0x768EC0 on row 10 (NORMAL_SHEPHERD), `Villager::ShepherdMoveFlockToWater` 0x768CC0, which takes row 10 and drops it, `Town::IsHarvestTime` 0x73B2D0 (the farmers' row), whose one caller `Villager::CheckNeededForHarvest` 0x7582F0 has no caller, direct, by pointer or in data, and `Town::PopulateTown` 0x739D10 (each row's job, +0x10), which has none either (`xref.py refs 0xD19CC8-0xD1A250`, `xref.py callers`, and a byte search of `runblack.exe` for the two addresses, 2026-11-07). The row "Jobs" of [Info tables](progress/engine/info_tables.md) was set done (2026-11-07)

- The special villagers' table columns beyond the name, sex and age put to use: the game reads the whole row with the other tables (`load_variables`, `GSpecialVillagerInfo` from info.dat, as openblack does), but after that a row's marriage, tribe, pet and face are read nowhere in the exe (`xref.py field GSpecialVillagerInfo` for each: 0 accesses; the only readers of the table `InfoList` 0xD9B154 are the name pick, the villager's constructor and destructor, its name getter, the start-up build, the map's clear, the gathering box's name compare (online) and a caller-less "are all names used" helper). The job column is read only by the name pick's filter (`GSpecialVillagerInfo::GetRandomName` 0x71F4B0 at +0x48), which skips it whenever the job asked for is 0, and its one caller (`SpecialVillager::Create` 0x71F1A0, from the plain villager's creation) always asks 0, so no shipped path filters by job. The contacts the mail link would put in the table do get a marriage, tribe, pet and face of their own, but that is the online-only mail feature ([villager names from contacts](progress/pc_integration/villager_names_from_contacts.md)); openblack reads the columns with the table and uses the same three

#### engine/options_and_settings

From [Options and settings](progress/engine/options_and_settings.md).

- The game can be started straight into any land: openblack's own `--start-level` switch, a developer tool listed under [diagnostics](#debugdiagnostics); the original's switches ([launch switches](progress/easter_eggs/launch_switches_and_files.md): `NEWGAME`, `MULTIPLAYER`, `SKIRMISH`, `PREINTROVIDEO`, `VERSION`, `NOLOADMUSIC`, `SETTINGS:`, `CONVERT`) pick no land, and `NEWGAME` keeps its row there
- A log file and per-part log levels: openblack's `--log-file` and `--log-level`; the original has no such switch and writes no log of its own (its only `.log` text is the creature's `chat.log`, from `Creature::AddSpeechItem`)
- Screenshots of a chosen frame, and running a set number of frames then quitting: openblack's `--screenshot-frame` and `--num-frames-to-simulate`, for its checks; the original has no such switch (its own screen grab, `LHScreen::SaveBitmap` 0x7DE930, stays a row of [display and frame rate](progress/rendering/display_and_frame_rate.md))
- Frame statistics and benchmarks: openblack's `--frame-stats`, `--frame-stats-views` and its benchmark recorder, listed under [diagnostics](#debugdiagnostics); the exe has no benchmark text, and its only frame-rate texts are its on-screen counters (`LHScreen::Flip` 0x7DE130, `Config::Process`)
- Screenshots of a chosen frame, and running a set number of frames then quitting: openblack's `--screenshot-frame` and `--num-frames-to-simulate`, for its checks; the original has no such switch, and no way to take a screenshot in play ([display and frame rate](#renderingdisplay_and_frame_rate))
- An interface scale for the debug windows: openblack's `--ui-scale`, for its own debug windows, which the original does not have

#### engine/platforms_and_performance

From [Platforms and performance](progress/engine/platforms_and_performance.md).

- The disc must be in the drive and its copy protection pass: openblack reads an existing install
- The minimum machine of 2001 (a 350 MHz processor, 64 MB of memory, an 8 MB 3D card): modern machines only
- The keyboard is read on its own thread and its keys handed to the loop under a lock: openblack reads keys from SDL's events on the main thread
- The game manages its own memory heaps and writes a leak report to the install folder on exit: openblack uses the standard allocator and owns memory through RAII
- Average and slowest (95th percentile) frame times can be logged: openblack's profiling (`src/Common/FrameStats.cpp`, `--frame-stats`), listed under [diagnostics](#debugdiagnostics); the exe's only frame-rate texts are its on-screen counters (`LHScreen::Flip` 0x7DE130), and it writes no timings
- The cost of each part of a frame can be measured, with the GPU's time per render view: openblack's profiler (`src/Profiler.cpp`, `--frame-stats-views`), a developer tool; the exe has no such measure
- Crowds of creatures and villagers can be benchmarked and the results written out: openblack's benchmark recorder (`src/Debug/BenchmarkRecorder.cpp`); the exe has no benchmark text or writer
- A fixed time per frame for repeatable runs: openblack's check setting `OPENBLACK_FIXED_FRAME_MS` (`src/Debug/FixedClock.cpp`); the exe has no such setting
- Memory use (process and graphics memory) can be logged for profiling: openblack's `src/Debug/MemoryStats.cpp` with `OPENBLACK_PROFILE`; the exe's only memory report is the leak report above, a developer tool too
- A crash writes a report with the call stack into a crashes folder, named after the time, then exits: openblack's crash handler (`src/Common/CrashHandler.cpp`), listed under [diagnostics](#debugdiagnostics); the exe sets no crash handler of its own (`SetUnhandledExceptionFilter` is reached only from the C runtime's start-up table, `__CxxSetUnhandledExceptionFilter` 0x7D51A6, with no direct caller) and has no crash-report text
- Aborts, assertions, uncaught exceptions and signals are each reported with an exit code as the shell expects: the same crash handler of openblack's; same evidence
- The system's own crash dialogs can be asked for instead: openblack's `--crash-dialogs`, which only turns its own handler off; the original always leaves a crash to the system
- Running on Linux and building for Android, iOS, ARM Linux and ARM Windows: openblack's own ports (the Ubuntu CI, `.github/workflows/ci-cross-compile.yml`, `src/FileSystem/AndroidFileSystem.cpp`); the original is the Windows exe

#### engine/saving_and_loading

From [Saving and loading](progress/engine/saving_and_loading.md).

- Creatures shared with Creature Isle: expansion not supported
- A save forced at once whatever the interval (for example when leaving a land): the autosave's forced branch
  (`GameOSFile::AutoSave` 0x564050, its argument) is never taken, as its only caller, `GGame::EndTurn` at 0x54EA43,
  passes 0 (checked 2026-11-10: `xref.py callers 0x564050`, one call); leaving the game saves through
  `InstantSaveGame` directly, not this branch. openblack keeps the forced branch (`save_triggers::AutoSaveDue`, tested
  in `SaveTriggers.TheAutosaveSavesEverySixThousandTurnsUnlessHeldBack`) only for the debug bar's Save menu,
  "Autosave now" ([saved games](../engine/saved-games.md))
- A "Loading..." box over the last frame before a saved game loads (raffclar's 46109a7d5): the box's drawing draws
  nothing while the game initialises, and every load of play (the quick load, the save game room's, the start's
  autosave) runs inside the game's initialisation; only a network game's debug load could show it, and the developer
  key handler that calls it has no caller (checked 2026-11-12: `xref.py brief` of `GameLoadingBox` 0x557F90 and
  `GameOSFile::LoadAllGame` 0x5587B0, `gx.py` on `GGame::Init`) ([the loading box](../engine/saved-games.md#the-loading-box))
- The particle rule UR_BankedTurning and its atom data (save type 0xDF): the rule is never made. Its `ModifyAtomCore` 0x6A1380 only gives the atom its data, FirstUpdate 1, and does nothing else (bw1-decomp PSysModifiers.cpp:1207 matches it). Nothing can make the rule itself: the frozen exe has no `UR_BankedTurning` name for `RegisterPersistent::RegisterModifiers` (every made rule has one, e.g. `UR_ObjectArcer` 0xC07FD8 pushed at 0x6BB430), no `StaticCreate` or `DefineProperties` and no RTTI of the rule's class, only of its atom data (0xBED8D8). No shipped file names it (searched the frozen game data, 2026-11-13), and no frozen save holds a 0xDF record. So the save type is only a class `LoadThing` can read (GameOSFile.cpp:1306), never met in a save. openblack's save library reads the record's one byte as `LoadThing` would (`RuleDataRecords.cpp`, the 0xDF layout), and makes no rule; raffclar's world reader reads it the same way (checked 2026-11-13: `xref.py func BankedTurning`, `xref.py str BankedTurning`) ([saved games](../engine/saved-games.md))

#### engine/script_vm

From [Script virtual machine](progress/engine/script_vm.md).

- The map script's commands other than `LOAD_FEATURE_SCRIPT` (`SET_NO_PLAYERS`, `LOAD_TRIBE_DANCE`, `SET_DATE`, `SET_TIME`, `SET_TURNS_PER_YEAR`, `SET_GAME_TICK_TIME`, `PAUSE_GAME`, `CREATE_CREATURE`, the `OUTPUT_*` logs, `SAVE_FOR_NET_DEBUG`, `LOAD_FOR_NET_DEBUG`, `LOAD_LANDSCAPE`, `LOAD_GAME_SCRIPT`, `LOAD_RAW_GAME_SCRIPT`, `LOAD_LANGUAGE`): the exe runs map-script commands (`GSetup::MapCommands`, handled by `GSetup::MapCommandProcess` 0x714D10) from only two files: `.\Scripts\Map.txt` at a new campaign (`GSetup::LoadMapScript` 0x715080, called by `GGame::Init` 0x54F7AB), whose one line in the frozen game is `LOAD_FEATURE_SCRIPT(".\scripts\land1.txt")`, and `.\Scripts\MPDebug.txt` on a network game's start (0x54F8E9), a developer's debug file that is in no folder of the game (`_gamedata/frozen`, checked 2026-11-07). The row on map scripts of [Script virtual machine](progress/engine/script_vm.md) keeps only the new campaign's `Map.txt` loading the first land (2026-11-07)
- Creature Isle's version of the program format: expansion not supported
- The land-script commands that no shipped land script uses: CREATE_PATH, CREATE_CREATURE_PEN, CREATE_WALL_SECTION, CREATE_PLANNED_WALL_SECTION, CREATE_PITCH, CREATE_TOWN_TEMPORARY_POTS, CREATE_FURNITURE, TOWN_NEEDS_POS, FLY_BY_FILE, BRUSH_SIZE, MULTIPLAYER_DEBUG, SET_LAND_BALANCE and EDIT_LEVEL (and COUNTRY_CHANGE and HEIGHT_CHANGE, see scripts/land_script_commands): none appears in any `Scripts/*.txt` or `Scripts/Playgrounds/*.txt` of the frozen game, and openblack's handlers for them are empty

#### rendering/display_and_frame_rate

From [Display and frame rate](progress/rendering/display_and_frame_rate.md).

- 16 or 32 bit colour: modern displays are always 32 bit
- Anti-aliasing: the game has none, and openblack draws none in play or in the checks: four-sample multisampling of the back buffer is a debug switch only (the debug bar's View menu, off by default; a06adda2d), a 🎯 chosen difference of 2026-10-21 ([the frame](../rendering/frame.md#comparison-with-openblack)). `FrameBuffer` (`src/Graphics/FrameBuffer.cpp`) multisamples an off-screen target only when asked, and none asks
- Taking a screenshot in play (the row moved here on 2026-10-26): the original cannot. Its one screen grab,
  `LHScreen::SaveBitmap` 0x7DE930, would write `.\Screen Shots\B&W Screen %i.bmp` (0xC315C8; the number is the count of
  `.bmp` files already in the folder, 0xC315EC, plus 1; a 24-bit bitmap of the back buffer with the pointer drawn in),
  but nothing calls it: xref finds no call, jump or reference, the frozen `runblack.exe` holds its address nowhere (no
  vtable, table or immediate) and no rel32 call or jump in any section lands on it, and its three strings are used only
  inside it. No key binding or action names a screenshot. The game's other captures are its own pictures, not the
  player's: `ScriptedScreenShot::Process` 0x821750 (the challenge room's script pictures, the save game pictures and the
  citadel's, requested through fn_00828E90) and the creature's photo `creatureshot_%i.jpg` (`CreatureRoom::TakeScreenShot`
  0x789A70, for the online creature page, listed above). The TGA writers sub_8710C0 (`c:\tmp\%d.tga`), sub_821F40
  (`%s\PAK%x.tga`) and sub_82AF30 (`%s%x.tga`) have no callers either. openblack's `--screenshot-frame` /
  `--screenshot-path` and the debug bar's Screenshot button are check and debug tools.
- Gamma or brightness setting: no gamma or brightness setting in openblack. Our wiki differs: the original has no gamma, post-processing or fog setting in its frame ([parity](../rendering/frame.md))

#### rendering/level_of_detail

From [Level of detail](progress/rendering/level_of_detail.md).

- Objects beyond a distance stop being drawn: openblack draws everything. Our wiki differs: the original never fades or hides models by distance and has no far plane ([parity](../rendering/frame.md))
- Models swap to coarser meshes, fade out and are left out by their depth into the view (raffclar's mesh detail, his #167 and #782): in this exe the level-of-detail factor and its clamp are both 100000 (the two loaders' stores are taken out), so every model draws its first level, its high mesh, at any depth, with no fade and no leaving out ([visible-blocks-and-lod](../rendering/visible-blocks-and-lod.md#the-model-level-of-detail-in-v142)). His unwired `Graphics/MeshDetail` and `detail_level::ModelDetail` were removed: their bands and their 0.2 to 0.5 table are those of the exe without the patch
- The temple's outside has its own detail level: the exe's detail settings are UseSmallBump, Clouds, CloudShadows, WaterTiling, LandRef, MaxObjectDistance, the five temple ones (CitadelReflections, CitadelLightmaps, CitadelGlows, CitadelVolumeLight, CitadelPeople, all of the temple's rooms), RainSplash, LightBoost, Fog, Weather, Light, FixeLand, ShadowsOnObjects, UseHighTexture, and UseMultiLayerOnLandscape (the key names, side by side in the frozen exe, read 2026-11-07); none is for the temple's outside, which draws as any other model ([the detail table](../rendering/frame.md))

#### rendering/light_beams

From [Beams and lights of worship](progress/rendering/light_beams.md).

- Coloured lights in red, green, yellow, blue or white follow the dancers of a dance: the game keeps them but never draws them: the routine that would light the land under them (`DanceLight*.raw`, 6 by 6) is never called in any PC version

#### rendering/particles

From [Particles](progress/rendering/particles.md).

- The animated mesh creators' other-mesh blend and clip pack (read 2026-11-03): `ParticleAnimCreator`'s draw blends to
  `MeshFileName1/2` between `FrameToStartBlend` and `FrameToEndBlend`, and `AnimEnum` picks its clip from the
  animation pack. Every loaded file that names them (SF_Butterflies, SF_ButterfliesOnObject and SF_Forest's three
  creators) gives `MeshFileName1` and `MeshFileName2` as `NULL_STRING`, and no file names an `AnimEnum` (each names an
  `AnimFileName`). So the game never blends nor takes a pack clip; they need no port
  ([particle creators](../physics/particle-creators.md)).
- The hand's colour pulse after a recognised gesture (`HandPulseDuration` of SF_Gesture's `UR_GesturingRecognised`):
  the rule writes the hand's specular in the particle step, and every frame the hand's draw writes it again from the
  land light before the hand is drawn, so no frame shows it ([the hand's pulse](../magic/hand-casting.md#the-hands-pulse)).
  openblack does not write it and draws the same hand.
- Six particle rules that only spell files the game never loads name (read 2026-11-02): `UR_FireWorkSimple`
  (SF_FireWorksNew), `ER_BurstFromParentAtom` (SF_SparklesBurstFromObject), `CreateNewBaseAtom`, `SplineToTarget` and
  `AppearanceRuleFadeOut` (SF_TargettedFireBall) and `UR_ThermoNuclear` (SF_ThermoExplosion). No other file of
  Data\Spells\ZSpellFiles names them, and the exe names none of the four files: its particle type table fn_0068EA00
  lists every spell file it loads by path, and `xref.py str` finds no string with FireWorksNew,
  SparklesBurstFromObject, TargettedFireBall or ThermoExplosion. So the game never runs them; they need no port.
- The animated and creature branches of an object parent's point (`RenderParticleGameObjectRef::GetRandomSurfacePos`
  0x6C8230: a random bone of an animated model, or the walk over a creature's drawn body mesh), read 2026-11-03. Of the
  loaded files, only SF_SparklesFromObject emits from an object parent's atoms (`ER_EmitFromParentAtom`; the other
  `CreateRule_GameObjectRef` files, SF_ButterfliesOnObject, SF_FliesOnObject and SF_HighlightOnObject, have no
  emitter from it, and SF_SparklesBurstFromObject is not loaded). Its one owner is the food pile's spot visual 46 (the
  speed-up sparkle), a static object, so the game only ever takes the static branch, which is ported
  ([particle rules](../physics/particle-rules.md#pending)). openblack logs once and lets nothing out on those branches.
- Effects shown in the sea's reflection (read 2026-11-14): the land view draws its reflections inside the landscape's
  draw, before the sea (`GLandscape::Draw` 0x5E42E0): the ships (`PetitNavire::PreDraw`), the hand and what it holds,
  the physics objects (fn_00646FE0) and the local creature's body, each through its `DrawUnderWater` (vtable slot
  0x118; `xref.py vtcalls 0x118` lists every call through the slot, and none is a particle's), with the sharks and the
  fish farms' fish. The effects are drawn once a frame, after the landscape: `PSysGlobal::DrawLoop` 0x68F5E0 (its one
  caller `GGame::Process3dEngine` 0x54E019) queues them in the transparent queue (`PSysManager::AddDrawing` 0x6797D0,
  its one caller `GJPSysInterface::AddDrawing`), beside the fireflies, the spells (`Spell::DrawSpells` 0x7203F0) and
  the particle containers, so no effect shows in the sea ([the frame](../rendering/frame.md#draw-stages-in-order)).
  openblack draws the effects in the main view only, the same.

#### rendering/shadows

From [Shadows](progress/rendering/shadows.md).

- Homes, trees and fires casting shadows from the lights at night (read 2026-11-14): each object answers whether it
  casts a shadow at night (`IsCastShadowAtNight`, vtable slot 0x1A0: yes for `Abode` 0x401720 (the windmill too),
  `MobileStatic` 0x439700 (the bonfire among them) and `Tree` 0x55DA10; no for `GameThingWithPos` 0x4052E0,
  `StoragePit` 0x55CCF0, `TownCentre` 0x55DB60 and `Totem` 0x561530; bw1-decomp's headers agree), but nothing asks:
  `xref.py vtcalls 0x1A0` finds no call through the slot, none of the seven answers has a caller, and the Ghidra export
  holds no call at +0x1A0 of a vtable. The night draws the shadows the day does (the baked static shadows and the
  projected shadow list, [projected shadows](../rendering/shadows-and-reflections.md#projected-shadows-shadowinfo)), and
  so does openblack.

#### rendering/special_effects

From [Special effects](progress/rendering/special_effects.md).

- Wet prints a creature leaves after walking out of water (read 2026-11-14): the game's one list of creature prints is
  its "wet feet" list (`LH3DWetFeet`), and it is the prints every step leaves: its adder (`LH3DWetFeet::Add` 0x81F360,
  one caller, the creature's step sound `LH3DCreature::PlayASoundEffect` at its step moments 3 to 5) tests only the
  detail switch for creature prints (`g_b_use_creature_foot_print`), the date (the first of April's smiley) and the
  list's room (256), and reads the land's height at each corner; nothing in it or its caller looks at water. The step
  under water only sets the fish's emergency point (the foot below 1). So a print out of water is the same print as
  any other, the rows above it ([special effects](progress/rendering/special_effects.md#marks-on-the-land-and-water)).
- Motion blur (read 2026-11-14): the renderer's motion blur value (`LH3DRender::g_motion_blur_value`) is set to 0 at
  the renderer's start (`LH3DRender::Open` 0x82B531) and every frame to ftol(clamp(amount, 0, 1) × 190)
  (fn_006CA540 0x6CA57E, from `PSysLightMaps::AddDrawing`), the amount being the light maps list's blur amount
  (`PSysLightMaps::List` +4). That amount is written only by the list's start (0, fn_006CA4C0) and its step at each
  game loop's end (sub_6CA5E0, from `PSysGlobal::GameLoopEnd`): with the state 2 it falls by the frame's ms × 0.01 and
  is set to 0 with the state at 0 or below; with the state 1 it rises by the same up to 0.5. The state is written by the start (0),
  the step (0) and `LightMapList::StopBlur` 0x6CA5D0 (2, from `CameraModePath::Cleanup` as a camera path ends); no
  code sets it to 1 (`xref.py refs` and `writers` on the list's first eight bytes find only these). So the amount stays
  0 and so does the value; its two readers, `GLandscape::Draw` 0x5E4E73 (which leaves the sea out while it is set) and
  `LH3DWater::DrawSea` 0x879F8C (which then keeps the depth test), always read 0 ([the sea](../world/sea-coast-and-rivers.md#sea)). openblack has no motion blur.

#### rendering/sprites_and_billboards

From [Sprites and billboards](progress/rendering/sprites_and_billboards.md).

- Distant trees drawn as flat stand-ins: the game draws its trees as models (unconfirmed that it has no stand-ins)

#### audio/music

From [Music](progress/audio/music.md).

- The creature's mood shapes the music: Our wiki differs: the original has no mood-driven music; what looks like a music mood controller is network packets ([audio](../audio/music.md#intro-trailer-outro-videos-and-menu))
- Music from the CD's audio tracks, through a CD player box: the program builds one (`CDBox`, made and set up in `FrontEnd::Init` 0x53B4D0, with the texts "CD player controls" and "Poll the CD for track changes" (`HELP_TEXT_DIALOG_CDPLAYERPOLL`)); its controls show the track and the number of tracks (`CDBox::InitControls` 0x544050, MCI status), step the track up and down, stop (sub_633BD0, MCI stop) and play from the chosen track (sub_633B60, MCI play), all from its control callback sub_5440B0; but nothing shows the box: its pointer (0xCD0638) is read only by `FrontEnd::Init`'s own set-up call, and the callback's box pointer (0xCD0B7C) only by the callback, so no player can open it and the game plays its own banks; the advisors' CD lines are never said either ([easter_eggs/secret_behaviours](#easter_eggssecret_behaviours))

#### audio/sound_effects

From [Sound effects](progress/audio/sound_effects.md).

- Crowds impressed, or losing belief: not in the original: the in-game bank's crowd samples (H_ImpressedGroup, H_LoseBeliefGroup) are played by nothing in the exe or its scripts, and the impressed villagers' states play no sound ([world-sounds.md](../audio/world-sounds.md#villagers-buildings-and-trees-own-sounds)); what is heard is the spirits' belief remarks, see [voices_and_speech.md](progress/audio/voices_and_speech.md)

#### audio/voices_and_speech

From [Voices and speech](progress/audio/voices_and_speech.md).

- The spirits warn that the creature is under attack or that people are injured, and remark on other villages: not in the original: their three functions are in the v1.42 exe but nothing calls them, jumps to them or holds their address ([voices](../audio/voices.md#gguidance)); `guidance::WarnCreatureUnderAttack`, `WarnInjuredPeople` and `RemarkOtherVillages` stay uncalled as there

#### video/bink_playback

From [Bink video playback](progress/video/bink_playback.md).

- Videos are looked for on the CD first when the game runs from the CD: openblack reads the install folder
- Films switched off by the build option `OPENBLACK_BINK_DECODER` or the debug Game films switch, the debug Video window that plays any film, and builds without the decoder showing black (`NullVideoDecoder`): openblack's debug controls and build options

#### video/bink_videos

From [Bink videos](progress/video/bink_videos.md).

- No other videos ship with the game; Creature Isle is not supported: a search of the game's files found no expansion videos
- An option of the game's options for its films: there is none; the options' only video setting is the detail level, whose change the game tells with HELP_TEXT_DIALOG_VIDEOCHANGE ([the detail change message](../video/playback.md#the-detail-change-message), tracked with the options in [Options](progress/interface/options.md)); the films are asked for only by the command line's PREINTROVIDEO and the scripts

- While the falling spell's film plays, the keys 1 to 4 turn the falling copy of the creature to 0, π/2, π or 3π/2
  (`LHKeyboard::KeyState[2..5]`, 0xE85376..0xE85379, read by the falling spell's draw): a developer tool left in the
  released program; openblack leaves it out ([falling spell](../magic/falling-spell.md))

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
- The dance formations' random scatter (shape 12, 256 points at a random distance and angle from the centre): its maker `MakeShapeRandomScatter` 0x50FD90 has no caller but the shape table (0x8D4E68), and no shipped dance file asks for shape 12, so no dance lays dancers out that way; the 512 random draws it makes once at start-up never reach a land's random stream, since every land sets the seeds again ([dances](../beings/dances.md#shapes)); see [unused_content.md](progress/easter_eggs/unused_content.md)

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
- A replacement source of names (a contacts file or the system's contacts) and an openblack switch to turn the feature off: openblack's plans, not the original's behaviour

## openblack

None of this is ported as behaviour of the game, by that rule; where the original has no X, openblack has none either.
The rule's exceptions are openblack's debug controls, off by default (all of them listed in
[openblack's debug controls](#openblacks-debug-controls-kept-exempt-from-removal), below), and the 🎯 chosen differences
([the list](reconstruction.md#what-is-deliberately-different--chosen-differences)); the lines above that touch one
say so. Where openblack keeps code for such behaviour that never runs or never shows in play (such as the sea's
drift, three spirit remarks and the loose wave reader), its line says so too.
The rows that point to a missing engine subsystem (saved games, multiplayer) stay pending in their files.

### openblack's debug controls (kept, exempt from removal)

openblack's own debug and developer tooling: the debug bar and its windows, the editor, the creature spawner, the
script debugger, the testbed and its scenarios, the command-line switches and the small tools in `apps/`. It was once
counted as the progress domain `debug/`; by the maintainer's decision of 2026-10-26 it leaves the progress count,
since the original does not have it, and by CLAUDE.md (Part 2) it is kept as the exception to removal: each control is
off by default, so the game and the checks stay identical. Each line below is one former row, kept with its status
then and where it lives in the code, grouped by its former page and section.

Before the pages went, every row was read against the exe for a counterpart a player of the shipped game can reach,
which would have moved it to its system's page:

- The launch switches: the exe reads twelve words from its command line (`ScanParameters` 0x641AA0: `NEWGAME`,
  `MULTIPLAYER`, `LAND`, `SKIRMISH`, `PREINTROVIDEO`, `FORCEINETCONN`, `NOINETCONN`, `EDITOR`, `CONVERT`, `VERSION`,
  `NOLOADMUSIC`, `SETTINGS:`). `LAND` and `EDITOR` set bytes nothing reads ([0xD46ABB], [0xD46AC0]), and `SETTINGS:`
  names a configuration file whose only content is a quest path (`Settings::ParseConfigFile` 0x714AF0). None points
  the game at its files, picks a land or the testbed, sizes the window, logs, runs a set number of frames, takes a
  screenshot or catches crashes, so openblack's switches are all its own; the original's are on
  [launch switches and stray files](progress/easter_eggs/launch_switches_and_files.md).
- The screen grab: `LHScreen::SaveBitmap` 0x7DE930 has no caller, so the debug bar's Capture has no counterpart a
  player can press.
- The frame-rate texts: `LHScreen::Flip` 0x7DE090 draws its "fps" text only while the screen's +0x1AC is set, which
  the constructor clears and no instruction of the exe writes at the system's screen (`TheSystem` +0x1BC, no direct
  writer); `Config::Process` 0x46B1F0 writes its "GT:%d, FPS:%d" line only to a configuration's file. openblack's
  frame counter on the bar is the 🎯 chosen difference of 2026-10-23.
- The developers' keys (game speed, a spell cheat, free movement, footpaths) are unreachable in the release
  ([easter_eggs/hidden_keys_and_cheats](#easter_eggshidden_keys_and_cheats)); the game speed changes only through the
  challenge scripts ([game speed](progress/engine/game_loop_and_clock.md#game-speed)); the camera's lens only through
  the land scripts (`SET_CAMERA_LENS`, `MOVE_CAMERA_LENS`).
- What the shipped game does let a player choose already has its rows on the system pages, so the debug rows that
  touch it are not repeated there: the left or right hand ([options](progress/interface/options.md),
  [hand placement](progress/hand/placement.md)), the detail level ([options](progress/interface/options.md)), the
  resolution, the window and vertical sync ([display and frame rate](progress/rendering/display_and_frame_rate.md)),
  the pause (P, [hidden keys and cheats](progress/easter_eggs/hidden_keys_and_cheats.md)).
- One row was the shipped game's own behaviour: the system's mouse pointer stays hidden while the game's hand is
  drawn (`CHand::Show` 0x46C1B0 and `CHand::Hide` 0x46C2E0 turn the front end's cursor off and on). It moved to
  [hand placement](progress/hand/placement.md#showing-and-hiding) as a row of its own.

#### debug/debug_windows

Formerly the progress page Debug windows (`progress/debug/debug_windows.md`).

- Menu bar: The debug interface is drawn with Dear ImGui over the game, in its own render view (done): `src/Debug/Gui.cpp`
- Menu bar: Load a story land, a playground land or the creature testbed from the menu, with each land's description (done): "Load Island" with "Story Islands" and "Playground Islands", each land's description as its tooltip, and "Creature Testbed", which loads the flat testbed (`src/Debug/Gui.cpp`, `src/Debug/IslandLoad.h`); tests `IslandLoad.*` (`test/gui/test_island_load.cpp`)
- Menu bar: Set the time of day and the player's alignment, and read the sky's and camera's alignment (partial 80%): "World" menu: time of day, visual time, sky type, a sky alignment slider and the detail level (`src/Debug/Gui.cpp`); a player's alignment is set in the Temple window (`src/Debug/Temple.cpp`); the camera's alignment is not shown
- Menu bar: Open the editor and each debug window (done): "Debug" menu: "Editor" and "Windows"
- Menu bar: One switch for the ported fixes of the original's game-logic bugs, off by default (done): "Fix Vanilla Bugs" in the Debug menu (`EngineConfig::fixVanillaBugs`, `src/Debug/Gui.cpp`); off, the game behaves as the original
- Menu bar: Show villagers' names, details and states over them, with a debug view (done): "Villager Names" menu (`config.showVillagerOverlay`, `debugVillagerStates`, `debugVillagerNames`; its Names Display (N) is the game's own display, `config.showVillagerNames`)
- Menu bar: Turn the sky, water, land, objects and sprites on and off; wireframe, bounding boxes, footpaths and streams; an overlay of the game's detail settings (done): "View" menu, with the game detail overlay
- Menu bar: Switch the hand between right and left handed (done): the debug bar's Hand menu, "Right Handed" (`config.rightHandedHand`, `src/Debug/Gui.cpp`); the player's own choice is the options' left-handed box, a row of [options](progress/interface/options.md)
- Menu bar: Change and reset the field of view (done): "Field of View" menu; the original's lens is set only by the land scripts
- Menu bar: Slow down or speed up the game, by presets or a multiplier (done): "Game Speed" menu (presets and a multiplier); the original's player has no speed control (see above)
- Menu bar: Take a screenshot to a file (done): "Capture" menu; tests `ScreenshotCapture.APictureIsMadeFromTheFrameAndTakenOnce`, `ScreenshotCapture.PixelsAreWrittenWhole`; the original's screen grab has no caller (see above)
- Menu bar: Quit, and the frame rate shown on the bar (done): "Quit" and the frame rate on the bar (`src/Debug/Gui.cpp`); the frame counter is the 🎯 chosen difference of 2026-10-23, and the original's Quit is the menu's
- Windows: Profiler: the frame's CPU and GPU stages, with the draw toggles (done): `src/Debug/Profiler.cpp`
- Windows: Mesh pack viewer: every mesh with its bounding box, skeleton and animations, and spawning it (done): `src/Debug/MeshViewer.cpp`
- Windows: Texture viewer: every texture of the packs (done): `src/Debug/TextureViewer.cpp`
- Windows: Console: the log, and land script commands typed in with completion and history; what is under the mouse (done): `src/Debug/Console.cpp` (completion and history), `src/Debug/ConsoleLogSink.cpp`; re-measured 2026-10-21: commands with completion and history work; 2026-10-25: every logger also writes to the window, as `[logger] [level] text`, the newest last, 2000 lines kept (closes #191); the log: tests `ConsoleLogSink.TheLoggedLinesAreTakenOnceOldestFirstWithTheirLoggerAndLevel`, `ConsoleLogSink.ALogNotTakenKeepsOnlyItsNewestLines`
- Windows: Land: block and country counts, the small bump strength, and dumping the land's textures and height map (done): `src/Debug/LandIsland.cpp`
- Windows: The object draw list: its size against the cap, the pass, the rebuilds, what it leaves out of the frame and why, and a switch for drawing only what it draws (done): `src/Debug/LandIsland.cpp` ("Object draw list", closed by default; `EngineConfig::drawFollowsList`)
- Windows: Path finding: send a picked villager somewhere by teleport, straight walk or footpath (done): `src/Debug/PathFinding.cpp`; re-measured 2026-10-21: teleport and straight walk work; 2026-10-25: the footpath tab's Execute sends the villager along the selected footpath as the game does (`footpaths::DirectionToward`, the nearest node, its top state as the final one; closes #190); the footpath's way: test `FootpathsTest.DirectionGoesTowardTheEndNearerTheDestination`
- Windows: The villagers' overlay labels each villager with its top state, in its owner's colour (done): re-measured 2026-10-21: the top state is shown; 2026-10-25: each label takes its town's owner's colour as the land gives it (the statistics box's), white for no town or a neutral one, a female's lighter (`src/Debug/VillagerOverlayColour.cpp`; closes #192); test `VillagerOverlay.ALabelTakesItsOwnersColourAsTheLandGivesItAndAFemalesIsLighter`
- Windows: Audio: every sound and music pack to play, the emitters, the voice banks, and the ambient loops, one-shots, climates and storms (done): the Audio Player (`src/Debug/Audio.cpp`: Sound, Music and Channels tabs, the game sound and music switches), Audio banks (`src/Debug/AudioBanks.cpp`); the ambience's climates and storms in the Weather window; the switches: tests `AudioMenu.SwitchesAreOnByDefault`, `AudioMenu.AnOffSwitchSilencesAndAnOnOneKeepsTheGain`
- Windows: Temple: go to each room of the temple (done): `src/Debug/Temple.cpp`
- Windows: Camera: run the game's camera paths (done): `src/Debug/Camera.cpp`
- Windows: Weather: force rain, snow and storms over the island from presets or by hand (done): `src/Debug/Weather.cpp` (presets, "Made to measure", "Storms"); tests `WeatherMenu.PresetKeepsDirectionAndLength`, `WeatherMenu.OnlyClearForcesNoStorm`, `WeatherMenu.IslandStormCoversTheIsland`
- Windows: Magic: running miracles, any miracle cast or powered up, prayer power, dispensers and bubbles, creatures' spells, particles (done): `src/Debug/Magic.cpp` ("Running", "Miracle", "All miracles", "Particles"), `src/Debug/MagicMiracles.cpp`, `MiraclesCaster.cpp`, `src/Debug/Vortices.cpp`
- Windows: Gestures: the path drawn, its best template and score, drawing or sending gestures (partial 80%): `src/Debug/Gestures.cpp`: the path over the screen, what is looked for, the templates that match now and the last one recognised, and "Draw it" plays a template's stroke through the recogniser; no score is shown
- Windows: Key bindings: every action, its binding and whether it is built yet; rebinding and pressing actions (partial 50%): `src/Debug/KeyBindingsWindow.cpp` lists every action with its key and mouse binding, whether it is held, and bindings two actions share; it does not rebind or press actions, nor say whether one is built (the action map can be rebound in memory, `src/Input/GameActionMap.cpp`)
- Windows: A music window showing the land's music, its tracks and what plays next (done): the Music window (`src/Debug/Music.cpp`): the main volume, the six channels, a player for any music type and the game music's state
- Windows: A window of a land's game state hash, to check two runs stay the same (partial 50%): the per-turn state hash is written to a file with `OPENBLACK_STATE_HASH` (`src/Debug/StateHash.cpp`); no window shows it
- Windows: A fixed clock for running the game turn by turn at a set rate (done): `OPENBLACK_FIXED_FRAME_MS` (`src/Debug/FixedClock.cpp`); test `ReplayDeterminism.sameGameInTwoProcesses`
- Windows: A window for the help system, the advisors and their messages (partial 60%): the Consciences window (`src/Debug/Consciences.cpp`): the good and evil advisors' state, clips, voices and the help's message sets; no window for the rest of the help system
- Windows: A window for the land scripts' challenges, their state and their scrolls (partial 40%): the Scripts window (`src/Debug/Scripts.cpp`, the editor's script panel) shows the running tasks ([script_debugger.md](#debugscript_debugger)), not challenges as such

#### debug/diagnostics

Formerly the progress page Diagnostics (`progress/debug/diagnostics.md`).

- The game's log shown in a console window: openblack's own developer console and the editor's log panel (`src/Debug/Console.cpp`, `src/Editor/Panels/LogPanel.cpp`); the original has no log console and writes no log of its own: the exe's only "Console" texts are the C runtime's error R6019 ("unable to open console device", read only by `__NMSG_WRITE` and `__GET_RTERRMSG`) and the import `SetConsoleCtrlHandler`. A sink feeding the rest of the game's log to them (#191) is tooling
- Memory use by system (also read for [debug windows](#debugdebug_windows)): a developer readout; openblack logs the process's and the graphics memory with `OPENBLACK_PROFILE` (`src/Debug/MemoryStats.cpp`, listed under [engine/platforms_and_performance](#engineplatforms_and_performance)). The original's only per-pool memory text, `Heap::GetDebugText` 0x7E14B0 ("MemoryPool: %p, Size: %d FreeMemory: ..."), is called only from `UniqueKeyHeap::GetDebugText` 0x7E1920, which has no caller and no reader; its working-set profile, `MemProfile_BeginEntry` 0x7DB5A0 and `MemProfile_EndEntry` 0x7DB5D0 (the only callers of `QueryProcessWorkingSet` 0x7DB4D0), has none either
- Command line: Point the game at the original's files, pick the land to start on, or the testbed (done): `--game-path`, `--start-level` and `--testbed` (the flat testbed) (`src/main.cpp`)
- Command line: Window size, window mode, renderer backend, vertical sync, interface scale (done): `-W`, `-H`, `--window-mode`, `--backend-type`, `--no-vsync` (on by default, as the original), `--ui-scale` (`src/main.cpp`); the player's resolution, window and vertical sync are rows of [display and frame rate](progress/rendering/display_and_frame_rate.md)
- Command line: The original's detail level, 0 to 6 (done): `--detail-level`; the player's choice is the options' detail selector
- Command line: Run a set number of frames and quit, for automated runs (done): `--num-frames-to-simulate`
- Command line: A screenshot at a given frame, to a given path (done): `--screenshot-frame`, `--screenshot-path`
- Command line: Run a testbed scenario or benchmark (done): `--scenario`, `--benchmark-warmup`, `--benchmark-frames` and `--benchmark-out` (`src/main.cpp`); see [testbed.md](#debugtestbed); the benchmark's results: tests `BenchmarkRecorder.WritesJsonAndCsv`, `BenchmarkRecorder.ResultPathsAddBothExtensionsToTheBase`
- Logging: Logs to a file, the terminal or the Android log (done): `--log-file` (a file, `stdout` or `logcat`)
- Logging: A log level for each subsystem (done): `--log-level`
- Measuring: A profiler of the frame's stages on the CPU and of the renderer's views on the GPU (done): `src/Profiler.cpp`, `src/Debug/Profiler.cpp`
- Measuring: Frame statistics logged every so many frames: average, 95th percentile and slowest frame, CPU and GPU, each stage (done): `--frame-stats`, `src/Debug/FrameStatsLog.cpp`, `src/Common/FrameStats.cpp`; tests `FrameStats.*`
- Measuring: Benchmarks summed up as mean, 95th percentile and slowest, written as JSON and CSV to compare runs (done): `src/Debug/BenchmarkRecorder.cpp` (tests `BenchmarkRecorder.*`), run by the testbed's runner for a `benchmark.*` scenario started with `--scenario`, which writes its results as JSON and CSV with `--benchmark-out` (`Runner::SaveResults`, `src/Debug/TestbedScenarioRunner.cpp`); see [testbed.md](#debugtestbed)
- Measuring: Screenshots from the menu bar (done): "Capture" in `src/Debug/Gui.cpp`; test `ScreenshotCapture.PixelsAreWrittenWhole`
- Measuring: A hash of the game's state each turn to check two runs stay the same (done): `OPENBLACK_STATE_HASH` writes one hash line per turn (`src/Debug/StateHash.cpp`); test `ReplayDeterminism.sameGameInTwoProcesses`
- Crashes: Every kind of crash (failed assertions, runtime errors, abort, terminate, fatal signals, system exceptions, fatal renderer errors) is caught (done): `src/Common/CrashHandler.cpp`
- Crashes: A report with a stack trace goes to stderr, the log and a file in a crashes folder, with a minidump on Windows (done): `src/Common/CrashReport.cpp`, the minidump in `CrashHandler.cpp`; tests `CrashReport.*`
- Crashes: The game exits with a non-zero code instead of waiting on a dialog, breaking into a debugger first if one is attached (done): `src/Common/CrashHandler.cpp`; tests `CrashReport.AbortLikeCrashesExitWithThree`, `CrashReport.SystemExceptionsExitWithTheirCode`, `CrashReport.FindsTheCrashDialogsSwitch`
- Crashes: A switch keeps the system's own crash dialogs (done): `--crash-dialogs` (`CrashHandler.h`); test `CrashReport.FindsTheCrashDialogsSwitch`

#### debug/command_line_tools

Formerly the progress page Command-line tools (`progress/debug/command_line_tools.md`).

- Tools: packtool: list a pack's blocks, view a block's bytes, the info block, meshes, textures, animations and sounds (done): `apps/packtool/packtool.cpp`, `components/pack`
- Tools: packtool: extract a block, and write raw data and mesh packs (done): `apps/packtool/packtool.cpp` (extract, write-raw, write-mesh, write-animation)
- Tools: l3dtool: print a mesh's header, skins, extra points, bones, footprints, names and metrics (done): `apps/l3dtool/l3dtool.cpp`, `components/l3d`
- Tools: l3dtool: write a mesh from glTF and extract a mesh to glTF (done): `apps/l3dtool/l3dtool.cpp` (write and extract through glTF)
- Tools: anmtool: list an animation's keyframes and their contents (done): `apps/anmtool/anmtool.cpp`, `components/anm`
- Tools: anmtool: write an animation from glTF (done): `apps/anmtool/anmtool.cpp` (write)
- Tools: lndtool: print a land's low resolution textures, blocks, countries, materials and the rest (done): `apps/lndtool/lndtool.cpp`, `components/lnd`
- Tools: lndtool: write a land from a height map, noise and bump maps (done): `apps/lndtool/lndtool.cpp` (write)
- Tools: glwtool: list glows and their contents, and read and write them as JSON (done): `apps/glwtool/glwtool.cpp`, `components/glw`
- Tools: camtool: print a camera path's points, and write one from points (done): `apps/camtool/camtool.cpp` (read, write from points), `components/cam`
- Tools: morphtool: print the creatures' morph files, animation sets, hair groups and extra data (done): `apps/morphtool/morphtool.cpp`, `components/morph`
- Tools: morphtool: writing morph files back (pending): read only
- Tools: creaturemindtool: dump creature mind files, with their learning episodes, and check each writes back byte for byte (done): `apps/creaturemindtool/creaturemindtool.cpp` (`--episodes`, `--roundtrip`), `components/creaturemind` (tests `CreatureMindFile.*`)
- Tools: lhvmtool: read a compiled script program: its scripts, autostarts, global stack and data (done): `apps/lhvmtool/lhvmtool.cpp`; the program reader it uses: tests `LHVMSaveLayout.TheProgramIsItsGlobalsCodeAutostartScriptsAndDataWithNoStateAfterIt`, `LHVMSaveLayout.TheMainStackIsItsCountsThenEachPlaceAsTypeThenValue`
- Tools: lhvmtool: decompile it into source, naming constants from the script headers, on the recorded source lines, with how well it went (done): `apps/lhvmtool/lhvmtool.cpp` (decompile), `components/lhvmdecompiler`; test `test/lhvm/test_lhvm_decompiler.cpp`
- Tools: lhvmtool: compile source back into a program (done): `apps/lhvmtool/lhvmtool_compile.cpp`, `components/lhvmcompiler`; the compiler it runs: tests `ChlRoundTrip.ChallengeChlWithoutHeadersSynthetic`, `ChlCompiler.WrittenProgramsReadBack`
- Files without a tool: Gesture templates (pending): read through `components/gestures` (`GestureFile`) by `GestureTemplatesLoader` (`src/Resources/Loaders.cpp`, `src/Magic/Gestures/GestureTemplates.cpp`), no tool
- Files without a tool: Particle effect files (pending): read through `components/psys` (`ParticleFile`, enum headers, stacked bitmaps) by `src/Particles/ParticleSpellFile.cpp`, no tool
- Files without a tool: Raw images (the game's textures outside packs) (pending): read by `components/rawimage`, no tool
- Files without a tool: The game's info tables (pending): read by `src/Parsers/InfoFile.cpp`, no tool
- Files without a tool: The game's fonts (pending): read by `src/Graphics/GameFont.cpp`, no tool
- Files without a tool: Saved games and profiles (partial): savetool (`apps/savetool/savetool.cpp`, `components/gamesave`; tests `SaveWorldReader.*` and the other `Save*` suites in `test/gamesave/`) reads a saved game's world files (`<slot>.sav`, not the `Script_` files) as far as openblack knows their classes, checks each record's running sum and that the records write back to the file's own bytes; no tool for the profiles: the profile's creature is read and written as a mind file in `Scripts/CreatureMind` (`--creature-file`, `components/creaturemind`)
- Files without a tool: Sound banks outside packs, and the dance and footpath files (pending): (unconfirmed which of these need their own tool)

#### debug/creature_tools

Formerly the progress page Creature tools (`progress/debug/creature_tools.md`).

- Spawning and picking: Spawn a creature of any species with its body set up (size, alignment, fatness, strength, age), facing a way or at random (done): `src/Debug/CreatureSpawner.cpp` "Spawn" tab (facing, or "Random"); tests `CreatureSpawnerModel.NamesTheSpeciesFromTheCow`, `CreatureSpawnerModel.RandomPicksAreTheWindowsOwnAndRepeat`
- Spawning and picking: Put the body back to the species' defaults (done): "Species defaults" (`src/Debug/CreatureSpawner.cpp`)
- Spawning and picking: List the creatures on the land, pick one, remove one or all (done): "Creatures" tab ("Select", "Remove", "Remove all")
- Spawning and picking: The picked creature's tab, which the editor's inspector shows too (partial 50%): "Selected" tab; the editor's inspector does not host it: it has only one-line Creature, Creature movement and Creature mind views (`src/Editor/Panels/InspectorPanel.cpp`)
- Movement and actions: Order it to walk or run to a point, flee from it or face it (done): the orders "Walk here", "Run here", "Flee from", "Face" (`src/Debug/CreatureSpawner.cpp`)
- Movement and actions: Order it to pick up, throw at, knock down or point at a thing (done): the orders "Pick up", "Throw at", "Knock down", "Point at"
- Movement and actions: Stop it, turn it to the camera, and show its route (done): "Stop", "Face the camera", "Show route"
- Movement and actions: Pause its mind so the body is posed by hand (done): "Pause mind"
- Movement and actions: Play any animation, gesture, sit, stand (done): "Play", "Gesture", "Sit down", "Stand up"
- Movement and actions: Stroke or slap it as the hand would (done): "Stroke", "Slap"
- Movement and actions: Show its hair, and its footprints, with the first of April smileys (done): "Show hair" (`src/Debug/CreatureSpawner.cpp`), "Show footprints" and "1 April" (`src/Debug/CreatureSpawnerFootprints.cpp`)
- Body and needs: Fill up, starve, tire out, parch or bloat it (done): `src/Debug/CreatureSpawnerBody.cpp` ("Fill up", "Starve", "Tire out", "Parch", "Bloat")
- Body and needs: Have it sleep, wake, eat, drink, poo, puke or faint, or drop food in front of it (done): `src/Debug/CreatureSpawnerBody.cpp` ("Sleep", "Wake", "Eat", "Drink", "Poo", "Puke", "Faint", "Drop food in front")
- Body and needs: Turn fainting from exhaustion on and off (done): "Creatures faint" (`src/Debug/CreatureSpawnerBody.cpp`)
- Looks: Tattoos: pick a slot and colour, put it on or clear it (done): `src/Debug/CreatureSpawnerAppearance.cpp` ("Set slot", "Put on", "Clear slot")
- Looks: Wounds, burns and blood anywhere on its body, healed a step at a time, with the heal effect (done): `src/Debug/CreatureSpawnerAppearance.cpp` ("Add wound", "Add burn", "Add blood", "Heal a step", "Heal effect"); the random spot and wound: test `CreatureSpawnerModel.RandomPicksAreTheWindowsOwnAndRepeat`
- Looks: Drawing switches: blending its seams and its shadows (pending): no switches for the seams or the shadows in our spawner
- Hands, leash and fights: Hold things in either hand; put down, toss, lob, eat, drop or look over what it holds (done): `src/Debug/CreatureSpawnerHands.cpp`
- Hands, leash and fights: Make it leashable, put the leash on or off, tie it to a thing or another creature, keep it home or let it roam (done): `src/Debug/CreatureSpawnerLeash.cpp` ("Leashable", "Tie to nearest thing", "Tie to other creature", "Keep at home", "Let roam")
- Hands, leash and fights: Place leash posts at the hand and show the rope's points (partial 50%): "Show rope points" (`src/Debug/CreatureSpawnerLeash.cpp`); no placing of leash posts
- Hands, leash and fights: Start a fight between two creatures, or let them fight by themselves; knock out, kill or bring round (done): `src/Debug/CreatureSpawnerFight.cpp` ("Start fight", "Fights by itself", "Knock out", "Kill for good", "Bring round")
- Hands, leash and fights: Block and special moves on command, and the camera watching the fight (done): "Block", "Special", "Camera watches" (`src/Debug/CreatureSpawnerFight.cpp`)
- Mind: Open a mind file through a file dialog, start a fresh mind, save the mind back (partial 50%): the mind files of the game's folder are listed to load or spawn from, and "Fresh mind" (`src/Debug/CreatureSpawnerMindFiles.cpp`); no file dialog and no saving the mind back
- Mind: Load one of the game's own mind files (done): the game's own minds listed (`src/Debug/CreatureSpawnerMindFiles.cpp`); test `CreatureSpawnerModel.MindFilesLeaveOutTheSavedBodies`
- Mind: See and change its desires and likes, opinions of actions, decision trees and attitudes (done): `src/Debug/CreatureSpawnerMind.cpp`; the desires shown: tests `CreatureSpawnerModel.DesiresStrongestFirstEqualOnesInTheGamesOrder`, `CreatureSpawnerModel.LeavesOutDesiresSwitchedOffOrEmpty`
- Mind: Teach it by rewarding or punishing, clear what it has learnt (done): "Stroke", "Slap", "Clear learning" (`src/Debug/CreatureSpawnerMind.cpp`)
- Mind: Show it a skill or a miracle to copy, cast or do it near (done): "Show it", "Cast it near", "Do it near" (`src/Debug/CreatureSpawnerMind.cpp`)
- Sound: Mute creatures, or other players' creatures' voices (done): "Mute creatures", "Other players' creatures' voices" (`src/Debug/CreatureSpawnerAudio.cpp`)
- Sound: Play each animation's sounds (done): "Play" per sound (`src/Debug/CreatureSpawnerAudio.cpp`)

#### debug/editor

Formerly the progress page Editor (`progress/debug/editor.md`).

- Layout: the Debug menu opens it and its Close button closes it (F2 is the game's own key, 2026-10-31); closed, the game runs as without it (done): `src/Editor/EditorWindow.cpp`; tests `EditorSystemTest.ClosedItLeavesTheCameraAlone`, `EditorSystemTest.ClosingHandsTheCameraBack`
- Layout: Tool bar along the top, wrapping onto more rows on a narrow window (done): `EditorWindow::DrawToolbar`
- Layout: Outliner, inspector and the bottom tabs can each be shown or hidden (done): `src/Editor/EditorWindow.cpp`
- Layout: Its keys only act while it is open and the mouse is over the world (done): `EditorWindow::TakesEvent`
- Layout: Its colours shared by every panel (done): `src/Editor/EditorStyle.h`
- Tools and time: Select (W), move (E), rotate (R) and the game's own hand (H) (done): `src/Editor/EditorWindow.cpp`
- Tools and time: Dragging a picked thing over the land, and turning it about the up axis (done): `src/Editor/EditorMath.cpp`; tests `EditorMath.*`
- Tools and time: Snapping for moving and turning, set from the tool bar (done): `SnapPoint`, `SnapAngle` (`src/Editor/EditorMath.cpp`); test `EditorMath.SnapsToSteps`
- Tools and time: Duplicate (D) and Delete, removing what the thing alone owns (done): `src/Editor/EditorEntities.cpp`
- Tools and time: Put a thing back on the ground (done): "Ground" (`src/Editor/EditorWindow.cpp`)
- Tools and time: Pause and play the game, and step one turn (.) while paused (done): "Pause" or "Play", and the step key (`src/Editor/EditorWindow.cpp`); test `EditorSystemStep.*`
- Tools and time: Boxes round the picked thing and the one under the mouse (done): `src/Editor/EditorWindow.cpp`; tests `EditorMath.CarriesBoxesIntoTheWorld`, `EditorMath.RayMeetsBoxes`
- Tools and time: Undo and redo (pending)
- Tools and time: Picking several things at once (pending): one thing at a time (`src/Editor/EditorSelection.h`)
- Cameras: A free camera, an orbit round the picked thing (O) and a follow camera (Shift+O) (done): `src/Camera/EditorCameraModel.cpp`, `src/Editor/EditorMath.cpp`; tests `EditorMath.OrbitRoundTrips`, `EditorMath.OrbitTurnsAndZoomsWithinBounds`, `EditorMath.FollowSitsBehind`, `EditorSystemTest.OrbitTakesTheCameraAndFreeHandsItBack`
- Cameras: Bring the picked thing into view (G) (done): `src/Editor/EditorWindow.cpp`
- Cameras: Camera move speed stepped up and down and reset (+, -, 0) (done): `src/Camera/KeyboardMoveSpeed.h`; tests `KeyboardMoveSpeed.TwoStepsDoubleAndHalve`, `KeyboardMoveSpeed.SteppingBackReturnsToExactlyTheDefault`, `EditorSystemTest.TheMoveSpeedReachesTheCameraOnlyWhileOpen`
- Outliner and inspector: Everything on the land by kind, searched by words; a click picks, a double click brings into view (done): `src/Editor/Panels/OutlinerPanel.cpp`, `src/Editor/EditorOutline.cpp`; tests `EditorOutline.*`
- Outliner and inspector: The list is read twice a second and only the visible rows are drawn (done): `src/Editor/Panels/OutlinerPanel.cpp`
- Outliner and inspector: The picked thing's place, turn and size to change, and what it is made of (done): `src/Editor/Panels/InspectorPanel.cpp`
- Outliner and inspector: For a creature, every section of the creature tools (partial 20%): the inspector shows one line each for the creature's species and owner, its movement and its mind's stage (`InspectorPanel.cpp`); the sections of the [creature tools](#debugcreature_tools) are only in their own window
- Outliner and inspector: For a villager, its life and its states (done): the inspector's Villager and Living action views (`InspectorPanel.cpp`)
- Outliner and inspector: For a building, a town or a worship site, their own figures (partial 70%): the inspector shows a building's town, food, wood and inhabitants and a town's player and homeless (`InspectorPanel.cpp`); nothing for worship sites
- Palette: Things to place by kind: creatures, villagers and buildings by tribe, trees, features, mobile objects and statics, dispensers and bubbles (done): `src/Editor/Panels/PalettePanel.cpp`, `src/Editor/EditorPalette.cpp`, `src/Editor/EditorEntities.cpp`; tests `EditorPalette.*`, `EditorMiracles.*` and `EditorMiraclesTest.*` (dispensers and bubbles through a fake miracles service). A dispenser or a one-off bubble on the land is a palette item of its miracle (a bubble's is its seed's miracle at its power-up level, so a bubble the editor put down comes back as the same item); a bubble of no seed is not placeable
- Palette: Names made readable from the game's tables (done): `src/Editor/EditorPalette.cpp`; test `EditorPalette.MakesNamesReadable`
- Palette: A picked item follows the mouse until a click puts it down, turned by the wheel or [ and ] (done): `src/Editor/EditorWindow.cpp`
- Palette: A whole list laid out at once in rows (done): `src/Editor/EditorWindow.cpp`; test `EditorPalette.LaysOutInSquareRows`
- Palette: A building goes to the nearest town, or a new one of its tribe (done): `src/Editor/EditorEntities.cpp`
- Changing the land itself: Saving what was placed back to the land's script or a file of its own (pending): changes last until the land is reloaded
- Changing the land itself: Raising and lowering the land, and painting its countries (pending)
- Changing the land itself: Drawing footpaths and streams (pending): they can be shown, not edited
- Changing the land itself: Placing the land's script markers, scrolls and challenge places (pending)

#### debug/script_debugger

Formerly the progress page Script debugger (`progress/debug/script_debugger.md`).

- The scripts' original source files shown beside their code: a developer tool of openblack's script editor and debugger; the exe has no script debugger a player can reach (no "debugger" or breakpoint text; its script debug texts are the land script command `SHOW_DEBUG_INFO`, a word of the land script parser's command table, and the file name `.\Scripts\MPDebug.txt`), and it reads the challenge scripts only compiled (`Challenge.chl`, from `GScript::Reset` 0x6EB454). `lhvmtool --source-lines`, which puts statements on their recorded source lines, stays as tooling
- The land's own setup script (its features and map commands) shown and stepped: a debugger feature only; the original runs a land's script once as the land loads, through the land script parser, whose only line texts are its parse errors ("File %s: ... Line %d"), and has nothing for viewing or stepping it
- Browsing: Lists the loaded program's scripts by kind, searched, with how many tasks run each (done): `src/Editor/Panels/ScriptsPanel.cpp`, `src/Editor/Scripts/ScriptModel.cpp`; tests `EditorScripts.*`
- Browsing: Opens another compiled program, or goes back to the game's own (done): "Open program..." and "Game's program" (`src/Editor/Panels/ScriptsPanel.cpp`), both read through the blob cache; tests `CompiledProgramTest.*` (decompile, recompile, syntax error)
- Browsing: A script's details, and starting it as the game starts scripts (done): "Script" side tab, "Start a task"
- Reading code: The code coloured, every name filled in (done): `ScriptModel` (the "Disassembly" tab); test `EditorScripts.DisassemblesWithNamesResolved`
- Reading code: Clicking a jump or a call follows it, with Back and Next (done): `ScriptsPanel.cpp` (Back and "Next"); tests `EditorScripts.FindsCodeRangesAndOwners`, `EditorScripts.DisassemblesWithNamesResolved` (the targets followed)
- Reading code: A search in the code (done): `ScriptsPanel.cpp`; test `EditorScripts.SearchesTheCodeGoingRound`
- Reading code: The source beside the code, decompiled into the scripts' own language (done): the "Source" tab, `src/Editor/Scripts/Decompiler.cpp` with `components/lhvmdecompiler`; tests `CompiledProgramTest.DecompilesWithEveryLineLinkedToItsCode`, `LhvmDecompiler.EditorSourceView`
- Reading code: Editing the source and compiling it into the running program, other scripts keeping their places (done): "Compile" (`src/Editor/Scripts/Decompiler.cpp`, `components/lhvmcompiler`); tests `ChlCompiler.EditorRecompilesAScriptIntoTheRunningProgram`, `DebuggerTest.UpdatingTheProgramKeepsTasksAndValues`
- Debugging: Breakpoints set in the margin (done): the margin's breakpoints (`ScriptsPanel.cpp`); test `DebuggerTest.BreakpointsHoldTasksBeforeTheirInstruction`
- Debugging: The running tasks with their stacks, variables and what they wait for (done): the tasks list (`ScriptsPanel.cpp`); test `EditorScripts.SaysWhatTasksAreDoing`
- Debugging: Holding, stepping and continuing a task, or all of them; stopping a task or a script's tasks (done): "Hold", "Step", "Continue", "Hold all", "Continue all", "Stop", "Stop its tasks"; test `DebuggerTest.SteppingRunsOneInstructionAtATime`
- Debugging: The code view following the picked task's next instruction (done): "Follow task"
- Debugging: Global variables changed in place (done): "Globals"; tests `EditorScripts.ReadsAndWritesValuesByType`, `DebuggerTest.HoldsByHandAndChangesVariables`
- Debugging: The native functions the scripts call, and which openblack has written (done): "Natives" (with what openblack has written); test `EditorScripts.CountsNativesAndTheirCoverage`
- Debugging: The program's data (done): "Data"
- Debugging: Exception handlers of a task (done): "Handlers"
- Log: The scripts' output and errors from when the editor starts, kept in a ring (done): `src/Editor/Panels/LogPanel.cpp`, `src/Editor/Scripts/LogRing.h`; test `EditorScripts.KeepsTheLastLinesOfTheLog`
- Log: Levels to show, a search, and following the newest lines (done): `src/Editor/Panels/LogPanel.cpp`
- Log: The number of errors in the tab's title (done): `src/Editor/Panels/LogPanel.cpp`; test `EditorScripts.KeepsTheLastLinesOfTheLog` (the errors counted)

#### debug/testbed

Formerly the progress page Testbed (`progress/debug/testbed.md`).

- The testbed land: A flat land with a lake, loaded from the menu or `--testbed` instead of a story land (done): `src/3D/FlatLand.cpp`, `Game::LoadTestbed` (`src/Game.cpp`), "Creature Testbed" in the debug bar's menu (`src/Debug/Gui.cpp`); tests `FlatLand.*`, `LandData.*`
- The testbed land: No land script runs on it, so its clock and weather stay as set (done): `Game::LoadTestbed` stops every script task, and the game's start skips the story's scripts and the start-up question on the testbed (`src/Game.cpp`)
- The testbed land: The player has a ring of influence and plenty of prayer power on it (partial 60%): a ring of influence over the middle of the map (`Game::LoadTestbed`); no extra prayer power
- The testbed land: A grid of every miracle's dispenser laid out in front of the camera, kept or cleared per scenario (partial 40%): the layout and the rule for keeping it are pure data (`src/Debug/TestbedDispenserGrid.cpp`, `KeepsDispenserGrid`; tests `TestbedDispenserGrid.*`), but nothing lays the grid out yet
- The testbed land: Emptying the testbed of everything put on it (done): the window's "Empty testbed" loads it afresh (`src/Debug/TestbedScenarios.cpp`)
- Scenarios as data: Each scenario is plain data: a name, a description, the facet it shows and an id (done): `src/Debug/TestbedScenarioRegistry.*` and the facet files `src/Debug/TestbedScenario*.cpp`, about 200 scenarios; tests `TestbedScenarios.*`
- Scenarios as data: The weather, the hour and whether the clock runs, the body's game speed, fainting and fights by themselves (done): `Runner::SetUpEnvironment`, the island's weather by `Runner::SetIslandWeather` (`src/Debug/TestbedScenarioRunner.cpp`)
- Scenarios as data: The player's alignment and prayer power as it starts (partial 50%): the alignment is set; a scenario's prayer power logs "not in this tree yet"
- Scenarios as data: Where the camera starts: the testbed's view, an overview of everything, behind or in front of one creature, or a placed shot (done): `Runner::Frame` (`src/Debug/TestbedScenarioRunner.cpp`); tests `TestbedScenarios.OverviewSeesTheWholeBox`, `FollowAndHeadShotsScaleWithTheCreature`
- Scenarios as data: The creatures with their species, body, age, needs, desires, wounds, mind file and leash (done): `Runner::PlaceCreatures`; a "chosen:" mind file is always the game's file of that name, as no file picked in the spawner is kept; tests `TestbedScenarios.NeedOverridesApplyOnlyWhatIsGiven`, `TestbedScenarios.DesireOverridesAreFractionsOfTheirMaxima`
- Scenarios as data: The objects, trees, features, villagers, food and wood, buildings and fields of a town, and animals about them (partial 80%): `Runner::PlaceObjects`; the villagers' walks to worship through the stones log "not in this tree yet" (`miracles.teleport_worship`)
- Scenarios as data: Crowds of hundreds to thousands, laid out the same way every run (done): `src/Debug/TestbedCrowd.cpp`, spawned a batch a frame by the runner; tests `TestbedScenarios.BenchmarksMeasureCrowdsOfEachSize`, `CrowdsAreChecked`
- Scenarios as data: A timeline of commands at set seconds: moving, acting, needs, the hand, the leash, fights, learning, the camera keys, Creature Mode and the cave, seeds, gestures, miracles, key presses, the pointer (done): `testbed_scenarios::Advance` (pure; tests `TestbedScenarios.Timeline*`) and the runner, which handles every command kind (`Runner`, `src/Debug/TestbedScenarioRunner.cpp`); no command logs "not in this tree yet". What still does is the set-up of a few miracle scenarios (tribal power, prayer power, worship walks through the stones, seeds summoned from worship into the hand; `src/Debug/TestbedScenarioRunner.cpp`), covered by the rows above; re-read in the code 2026-10-22
- Scenarios as data: The pointer moved and pressed by the scenario in place of the mouse, through the game's own input (partial 90%): `src/Debug/TestbedPointer.cpp` (tests `TestbedPointer.*`), with no hook of its own: it moves the game's fixed cursor and writes the buttons the hand reads, so the buttons reach the hand only. The wheel and the camera's drags go as mouse events, which are dropped when the real input is ignored or the mouse is fixed
- Scenarios as data: Fixtures: a dispenser, a storm, the creature free, leashed or penned, a village, a fire, trees, a pile and a seed in the hand, each in a line or two (done): `src/Debug/TestbedFixtures.*`, checked by `Problems()` without the game and set out through the game's own archetypes and systems; tests `TestbedFixtures.*`. A dispenser needs a village fixture for its town
- Scenarios as data: A recorded hand demo replayed on the testbed, with fixtures at the points its presses meet the land (done): `src/Debug/TestbedDemoPoints.cpp` (pure; tests `TestbedDemoPoints.*`, `HandDemoRecords.*`), the plane lowered under a demo recorded with a low camera; the game's playback unchanged
- Scenarios as data: A scenario that writes into the game's folder runs only on a copy of the game's data (done): `writesGameData`, checked against the marker `openblack_game_data_copy.txt` by `Runner::PlaceFixtures`
- Running them: A list of scenarios by facet, searched, with Run, Restart and Stop (partial 80%): `src/Debug/TestbedScenarios.cpp`, `TestbedScenariosModel.cpp` (tests `TestbedScenariosModel.*`): picked by facet, with no search
- Running them: Overview and testbed camera views, following one of the creatures, and letting go of the camera (done): the window's Camera section (`src/Debug/TestbedScenarios.cpp`); tests `TestbedScenarios.OverviewSeesTheWholeBox`, `TestbedScenarios.FollowAndHeadShotsScaleWithTheCreature`
- Running them: A readout of each creature's needs and what it is doing as the scenario runs (done): the window's "Its creatures" table, and the run's log; tests `TestbedScenariosModel.StrongestDesireIsTheLargestActivated`, `TestbedScenariosModel.StatusLineSaysHowFarTheCommandsHaveGot`
- Running them: Setting one fixture out from the window (done): the window's Fixtures section (`TestbedScenarios::DrawFixtures`)
- Running them: The scenarios as a tab of the editor (done): `src/Editor/ScenariosTab.h`, `src/Editor/EditorWindow.cpp`; test `EditorScenariosTab.*`
- Running them: Saving a run's results to a file (done): "Save results" with `--benchmark-out` only (`Runner::SaveResults`); tests `TestbedScenariosModel.BenchmarkQuitsAndWritesOnlyWhereAsked`, `TestbedOptions.ResultsPathIsKeptAsGiven`
- Running them: `--scenario ID` starts the game on the testbed running a scenario (done): `src/main.cpp`, `src/Debug/TestbedOptions.cpp` (tests `TestbedOptions.*`); an unknown id logs the ids and quits
- Running them: Benchmarks: a crowd warmed up for some frames, then measured, its results written as JSON and CSV, and the game quits (done): `--benchmark-warmup`, `--benchmark-frames`, `--benchmark-out` (`src/main.cpp`), the runner over `src/Debug/BenchmarkRecorder.cpp` (tests `BenchmarkRecorder.*`)
- Running them: A scenario checks its own outcome and reports pass or fail (pending): checks live in the unit tests, and runs are compared with a reference run
- Running them: Scenarios on the story lands rather than the flat testbed (done): a batch's land entries start a story land with a hand demo, the start-up answer, the menu, the citadel's turns or a timeline (`demo=`, `skip=`, `menu=`, `citadel=`, `timeline=`, `src/Debug/TestbedOptions.cpp`); `timeline=` plays testbed scenarios' commands on the loaded land, at its map points (`src/Debug/TestbedLandTimeline.cpp`; tests `TestbedLandTimeline.*`), as the `story.*` scenarios do
- Running them: Scenarios recorded from play rather than written by hand (pending): written by hand; the game's own hand demos can be replayed on them
- Running them: The raw input of a run recorded frame by frame and played back in a later one (the mouse, its wheel and the keys) (partial 50%): `--record-input`, `--replay-input` and Debug > Input Recording (`src/Input/InputReplay.cpp`, the format in `src/Input/InputRecording.cpp`, tests `InputRecording.*`); the file is appended to every 300 frames, so a crash keeps it, and `--resume-input <old> --record-input <new>` plays a recording then gives the input back to the player, the new file starting with the old events, a developer tool to go on with a game a crash stopped (2026-11-08); used once to record a Land 1 run (2026-10-22); its replay is not verified yet; see [input recording](../engine/input-recording.md)

#### debug/testbed_scenarios

Formerly the progress page Testbed scenarios (`progress/debug/testbed_scenarios.md`).

- The creature: Idle: fidgeting, hanging around, wandering (done): `idle.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Faces, emotions, gestures and the emotes of its desires; eyes and blinking (done): `expressions.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: What it looks at: watching a walker (done): `senses.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Needs: thirst, hunger, sleep by night and day, poo, puking, fainting, cold, exhaustion (done): `needs.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Growing up: a time-lapse, the stages unlocking desires, the morph lineup (done): `growth.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Looks: skins and hair by alignment, tattoos and wounds (done): `appearance.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Footprints per species, in snow, grass and shallows, the first of April smileys (done): `footprints.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Moving: walking, running and turning, routes round obstacles, round the lake and through small trees (done): `movement.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Objects: picking up, looking over, putting down, throwing, eating, knocking down trees, pointing (done): `objects.*`; test `TestbedScenarios.CoversObjectsTheHandAndLeashes`
- The creature: The hand on it: stroking each part, slapping, the status panel, the hand's look by alignment (done): `hand.stroke`, `hand.slap`, `hand.status_panel`, `hand.look_*`; the hand held to a creature strokes, slaps and lets go through `HandStroke`, `HandSlap` and `HandLetGo` (`Runner`, `src/Debug/TestbedScenarioRunner.cpp`); read in the code 2026-10-22, not run; test `TestbedScenarios.CoversObjectsTheHandAndLeashes`
- The creature: The leashes: leading, tying, keeping home, the leash keys and picker, other players' creatures (done): `leash.*`, `examples.leashed_creature`; shaking a leash off is the game's scribble drawn with the empty hand (`LeashShake`, `src/Debug/TestbedScenarioRunner.cpp`); read in the code 2026-10-22, not run; tests `TestbedScenarios.CoversObjectsTheHandAndLeashes`, `TestbedScenarios.TiesTheLeashWithAPointerDoubleClick`
- The creature: Fights: by themselves, charged and quick blows, blocks, knock-outs, slapping other gods' creatures (done): `combat.*` play in full; slapping another god's creature goes through the hand held to it (`HandSlap`); read in the code 2026-10-22, not run; test `TestbedScenarios.CoversFights`
- The creature: Learning: rewards and punishments, by watching, from mind files, copying the player (done): `mind.*`; a "chosen:" mind file is the game's file of that name
- The creature: Creature Mode, following it with C, and the Creature Cave with its tattoos (done): `creature_mode.*`; the double click and the held camera keys go through the game's own input (`DoubleClick`, `CameraKeys`, #105); tests `TestbedScenarios.CreatureModeIsDoubleClickedAndTurnedWithTheCameraKeys`, `TestbedScenarios.TheTattooEditorIsOpenedAndASymbolDragged`
- The creature: Its voices by species and size, and the sounds of the land (done): `audio.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- The creature: Its shadow, its reflection, light on the land through the day (done): `light.*`; test `TestbedScenarios.CoversTheCreatureFeatures`
- Miracles and effects: Every miracle cast, held, powered up and its effect, each in its own scenarios (partial 70%): `miracles.*` are listed, set up and cast their miracles (`Runner::CastMiracle`, `src/Debug/TestbedScenarioRunner.cpp`); what a scenario asks that this tree cannot set up yet (tribal power, prayer power, the villagers' walks to worship, seeds summoned from worship into the hand) is logged as "not in this tree yet"; the debug Magic window casts any miracle (`src/Debug/Magic.cpp`, `MiraclesCaster.cpp`)
- Miracles and effects: Dispensers, globes and seeds from worship (partial 50%): the dispenser and hand-seed fixtures work (`src/Debug/TestbedFixtures.cpp`); the globe scenarios' dispensers (`Runner::PlaceDispensers`) and seeds held (`HoldSeed`) are set up; their tribal power is set as a built wonder raises it (`Runner::SetTribalPower`, `miracles.hand_tribal_power`); seeds summoned from worship into the hand still log "not in this tree yet" (`miracles.hand_extreme_lightning`)
- Miracles and effects: The creature casting its spells (partial 70%): `miracles.*` of the creature casting: the creature is taught and told to cast; a seed held in the hand is skipped; the hand is held still where a scenario asks
- Miracles and effects: Fire spreading, blasts, water putting fire out (done): the fire fixture lights objects or a bonfire; the blast and water scenarios cast their miracles through `Runner::CastMiracle`; read in the code 2026-10-22, not run; test `TestbedScenarios.TheBurningCowStandsInsideItsOwnFiresReach` (the spreading's set-up)
- Miracles and effects: Particles and sprites: sparkles, smoke, steam, mist, sorted sprites (done): `particles.*`; an effect takes its player's colour (`ParticleSetup::player`, given by `Runner::StartParticle` through `SetPlayer`, `src/Debug/TestbedScenarioRunner.cpp`)
- Miracles and effects: Gestures drawn through the recogniser (done): the gesture scenarios draw each gesture's stroke with the scenario's pointer for the game's recogniser (`DrawGesture`, `Runner::DrawGesture`, `src/Debug/TestbedScenarioRunner.cpp`); the debug Gestures window draws and sends gestures too (`src/Debug/Gestures.cpp`); read in the code 2026-10-22, not run; tests `TestbedGesture.TheStrokeIsTheTemplateAcrossTheMiddleOfTheWindow`, `TestbedGesture.TheDrawnCircleIsRecognisedAsTheCircleOnly`, `TestbedScenarios.DrawsTheShieldsFirstCircleWithThePointerOnTheLand`
- Miracles and effects: Hand navigation: dragging, edge turning, tilting, both buttons, middle button (partial 70%): `hand.click`, `hand.drag`, `hand.edge_*` and the rest, driven by the scenario's pointer (`src/Debug/TestbedPointer.cpp`), whose buttons reach the hand only; the wheel and the camera's drags are dropped when the real input is ignored
- Miracles and effects: Hand demos replayed with things set out where they press (done): `examples.miracle_near_village`, `examples.hand_throw_target` (`src/Debug/TestbedScenarioExamples.cpp`); tests `TestbedScenarios.ExamplesAreWellFormedFixtures`, `TestbedFixtures.APressIsWhereTheDemoPressedMovedByItsNudge`, `TestbedDemoPoints.APressMeetsThePlaneThroughItsMove`
- Other uses: Set ups for trying the editor on (done): `editor.*`
- Other uses: Crowd benchmarks of creatures and villagers (done): `benchmark.creatures_*`, `benchmark.villagers_*`; tests `TestbedScenarios.BenchmarksMeasureCrowdsOfEachSize`, `TestbedCrowd.CreaturesMixSpeciesAndOwnersAndKeepApart`, `TestbedCrowd.VillagersLiveInTownsOfHomes`
- Other uses: Example scenarios to copy, set out with fixtures (done): `examples.*` (`src/Debug/TestbedScenarioExamples.cpp`); test `TestbedScenarios.ExamplesAreWellFormedFixtures`
- Parts of the game without scenarios: Villagers' lives and jobs (partial): `villagers.*` (`villagers.footpaths`: a fisherman walking the footpaths to his farm and fishing, `villagers.knock_at_night`, `villagers.poisoned_pause`, `villagers.gait_limits`, `villagers.script_footpaths`, `src/Debug/TestbedScenarioFish.cpp` and the registry); the village fixture sets out huts, villagers and a storage pit; the other jobs and the day of a villager have no scenario
- Parts of the game without scenarios: Towns growing, their desires and building (pending): no scenario
- Parts of the game without scenarios: Buildings: construction, the workshop's scaffolds, damage and repair (partial): damage and repair: `physics.building_breaks`, `physics.building_repaired` (`src/Debug/TestbedScenarioPhysics.cpp`); buildings set out part built: `examples.abode_partly_built`, `examples.wonder_built_*`; construction by builders and the workshop's scaffolds have no scenario
- Parts of the game without scenarios: Wild animals and livestock on their own (done): `animals.*` (`src/Debug/TestbedScenarioAnimalWalks.cpp`, `TestbedScenarioFlocks.cpp`), `birds.*` (`TestbedScenarioBirds.cpp`), `sharks.*` (`TestbedScenarioSharks.cpp`); none asks for a set-up the runner cannot make; tests `TestbedScenarios.PutsFlocksOfBirdsAboutTheirHomes`, `TestbedScenarios.PutsASharkInTheLakeAndWalksItAlongATrack`, `TestbedScenarios.ACowGoesRoundAPillarWhileAnOverlappingPillarIsDeletedOrNot`
- Parts of the game without scenarios: Worship and prayer power from worshippers (partial): `worship.altar_needs` (`src/Debug/TestbedScenarioVillageTotem.cpp`): the town's share sent to worship and the need signs over the altar; the worship walks through the teleport stones and prayer power set by a scenario log "not in this tree yet" (`miracles.teleport_worship`)
- Parts of the game without scenarios: Weather on its own: climates, rain and snow, wind (partial 40%): the island's weather is a scenario setting, and the storm fixture makes a storm (`examples.storm_lightning`); the climates and the wind have no scenario. The debug Weather window forces climates, rain and storms (`src/Debug/Weather.cpp`)
- Parts of the game without scenarios: The temple and its rooms (done): `temple.*` (`src/Debug/TestbedScenarioTemple.cpp`: the challenge room and its choice, the library's history, blows on the temple), `birds.temple_bats`, `birds.temple_doves`, `examples.temple_lanterns`, `examples.temple_leash_posts`; none asks for a set-up the runner cannot make. The debug Temple window goes to each room (`src/Debug/Temple.cpp`); tests `TestbedScenarios.PutsDovesOrBatsAboutATemple`, `TestbedScenarios.TheTempleExampleShowsAllThreePosts`, `TestbedScenarios.TheCitadelScenarioOpensTheTempleWithTheLeashOnAndLeavesAtOnce`
- Parts of the game without scenarios: Land scripts and challenges (done): `script.*` give the land script's commands on the testbed (tests `TestbedScenarios.TheScriptCreateSettlesScenarioMakesEachMobileThingThatStartsItsPhysics`, `TheScriptDoActionScenarioForcesTheTwoLooksOnTheVillagerAndTheSitAtTheRock`); `story.*` play on the story lands themselves, as land timelines (`timeline=`, `src/Debug/TestbedLandTimeline.cpp`; tests `TestbedLandTimeline.*`)
- Parts of the game without scenarios: Physics of thrown objects other than the creature's and the miracles' (done): `physics.*` (`src/Debug/TestbedScenarioPhysics.cpp`): one and many rocks thrown, rocks splitting and burning, a thrown villager landing, things sinking, thrown in snow, a rock thrown at a creature; none asks for a set-up the runner cannot make; test `TestbedScenarios.PhysicsScenariosThrowTheirOwnThings`

## Pending

Nothing. openblack holds a sleeping creature as the original does (`creature_hand::MayHold`,
`src/Creature/CreatureHandRules.cpp`, has no sleep test; [creature/learning_from_feedback](#creaturelearning_from_feedback)).
