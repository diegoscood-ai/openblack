# Special villagers

Most villagers are nameless members of their tribe. A few are not: now and then a new villager is born or made with a
real person's name floating over its head (from the game's own list of 48, or from the player's address book), the
story's characters are special kinds of villager with their own models (the piper, the hermit, the sculptor, the
monk, the sailors and the rest), the opening family are drawn in high detail, and missionary disciples move into other
towns to preach. Ordinary looks and voices are in [looks_and_voices.md](looks_and_voices.md), disciples in general in
[disciples.md](disciples.md), and each story character's challenge in its land's file
([land 1](../story/land_1.md), [land 2](../story/land_2.md), [land 3](../story/land_3.md),
[land 4](../story/land_4.md), [land 5](../story/land_5.md)).

**Progress: 29/70 done, 0 chosen, 7 partial, 0 inferred — 45%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-states.md).

## The table of names

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's info table holds 48 named people (developers and friends of the studio), each with a name of up to 47 characters, an age, a sex, a job, whether married, a tribe, a pet animal and a face number | done | read into `InfoConstants::specialVillager` (48 entries, `src/InfoConstants.h`); the name pick reads the name and sex, a named villager the age (`ecs::villager::RollSpecialVillager`, `MakeSpecialVillager`); tests `VillagerCoreTest.SpecialVillagerRollPicksAFreeNameOfTheVillagersSex`, `VillagerCoreTest.SpecialVillagerTakesItsNamesAge` |
| The table is built once at start-up | done | the info's 48 rows, read once with the info (`InfoConstants::specialVillager`); openblack has no mail link, so no contacts come first |
| When the player's address book gives contact names, they fill the table first and the game's own names only fill what is left up to 48; with 48 contacts or more only contacts are used | pending | see ../pc_integration/villager_names_from_contacts.md for how contacts are read |
| A contact's entry gets an age of 11 to 50, sexes alternating male then female in the order read, no job, married at random, any tribe and a random pet number | pending | see ../pc_integration/villager_names_from_contacts.md |
| The table has room for 148 names | pending |  |
| In a multiplayer game only the 48 built-in slots count | pending |  |
| Ordinary villagers have no name at all; only named villagers answer with one | done | only a named villager carries the `NamedVillager` component with its row (`src/ECS/Components/VillagerLabel.h`); test `VillagerCoreTest.SpecialVillagerTakesItsNamesAge` |

## Becoming a named villager

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever the game makes a villager (born, made by a script or a building, or thrown from a vortex), it rolls a number from 0 to 9; on 0 or 1 (2 in 10) it tries to make a named villager instead | done | `ecs::villager::RollSpecialVillager`, called by `VillagerArchetype::Create` (`src/ECS/Archetypes/VillagerArchetype.cpp`) before anything is made; test `VillagerCoreTest.SpecialVillagerRollPicksAFreeNameOfTheVillagersSex` |
| Named villagers are never made in a multiplayer game | pending |  |
| The name is picked at random among the names of the villager's sex that are not yet in use in this land, by walking the table from a random place | done | `ecs::villager::RollSpecialVillager` draws the place among the table's 48, then `ecs::special_villager::PickName` (`src/ECS/SpecialVillagers.cpp`) walks the rows in turn; pinned to the original's own pick (`SpecialVillagers.PickMatchesTheOriginal`, `test/test_special_villagers.cpp`) |
| When every name of that sex is in use, an ordinary villager is made instead | done | `special_villager::PickName` gives none after the one draw, and `VillagerArchetype::Create` makes a normal villager; test `SpecialVillagers.NoNameWhenEveryOneOfTheSexIsGiven` |
| A named villager takes its name's age, unless it is being born (age 1) | done | `ecs::villager::MakeSpecialVillager`: SetAge again with the start and grown scale for that age, as the original draws them; test `VillagerCoreTest.SpecialVillagerTakesItsNamesAge` |
| Each name is used by at most one villager per land; the counts are cleared when the land is cleared | done | `RegistryContext::namedVillagersUsed` (`src/ECS/RegistryContext.h`), made new with each land's registry; test `VillagerCoreTest.SpecialVillagerRollPicksAFreeNameOfTheVillagersSex` |
| The name's job, marriage, tribe, pet and face are not used when it is chosen (unconfirmed whether anything else uses them) | done | the pick reads only the sex, the name and whether it was given (`special_villager::PickName`); tests `SpecialVillagers.PickWalksFromTheDrawToTheFirstFreeNameOfTheSex`, `SpecialVillagers.PickMatchesTheOriginal` |
| A named villager otherwise lives, works, breeds and dies like any villager of its kind | done | it is made by the same `VillagerArchetype::Create` from the same row of the villagers' table; only its component and age differ |
| A named villager is saved and loaded with its name | pending |  |

## Showing the name

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The name floats over the villager's head: 1.75 above where its model is drawn (above its position when it has none), with a pointer down to that point | done | `ecs::villager_label_source::Gather` (`src/ECS/VillagerLabelSource.cpp`): the drawn position (`ecs::DrawnPosition`) + 1.75, projected as the game projects a point; the box over it and the pointer (`gui::villager_labels::Board::Add`, `PointerShape`); tests `VillagerLabels.LabelStandsOverItsPoint`, `VillagerLabels.PointerReachesThePointAndKeepsItsSize`; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |
| The name is white, or the colour of the player who owns the villager's town, put half way to white | done | `gui::villager_labels::NameColour` with the town owner's colour on this land (`ecs::player_colours::Colour`); test `VillagerLabels.ColourGoesHalfWayToWhite`; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |
| Names show while the "show villager names" display is up (it fades in and out); villagers marked to always show their name show it anyway | partial | N toggles the display (`input::ShortcutKeys`), which grows in and out at 0.003 a millisecond of game time on the frames that draw the land, the names taking its size from before the frame's step (`gui::villager_labels::StepNameFadeFrame`, `Game::PlaceVillagerNames`; tests `VillagerLabels.NameDisplayTakesAThirdOfASecondOfGameTimeToGrowIn`, `VillagerLabels.FrameNamesUseTheNameDisplayFromBeforeTheStep`, `VillagerLabels.NameDisplayStandsStillWhenTheLandIsNotDrawnOrTheGameIsPaused`); left: the always-show mark (the status bit 0x100), whose writer is not found; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |
| No name is shown for a villager a script is controlling | done | no name while a script holds it (`script_held::IsInScript`, `gui::villager_labels::NameSize`); test `VillagerLabels.OnlyNamedVillagersNoScriptHoldsOrMadeShowAName`; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |
| No name is shown while a script has the wide screen on, while the villagers' details are shown, or for a villager a script made | partial | none while the help system's wide screen is on, and a script's wide screen drops those placed (`Gather`, `Game::PlaceVillagerNames`); a thing a script makes is hidden for good (`components::NameHidden` from `script_held::AddScriptThing`; test `ReleaseTest.AThingAScriptMadeHidesItsNameForGood`); left: the details (the S key) are not ported, and the hidden mark is not saved with the world; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |
| Names are drawn in the depth-sorted pass with the other see-through things | done | one Z object a name at its point's squared distance, queued from left to right (`ZObject::villagerName`, `Renderer::DrawVillagerName`), drawn at the bubbles' draw depth and tested against the world's depth; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names) |

## High-detail villagers (the opening family)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can turn a villager into a high-detail one and back | done | SET_HIGH_GRAPHICS_DETAIL (`src/CHLApi.cpp`) to `super_villager::SetHighGraphicsDetail` (`src/ECS/SuperVillager.cpp`); what it shows goes into both its meshes, so the top detail level draws it too, and the release gives both back (`super_villager::WhileSuperVillager`; tests `SuperVillagerDrawnMeshes.*`, `SuperVillagerHighMesh.*`) |
| The father, mother and son of the opening use their own high-detail models; every Celtic and Norse boy uses the son's | done | the high mesh's HD model: the man, the woman and the boy of `Data\MISC\Intro` (`src/ECS/SuperVillager.cpp`); see ../story/tutorial.md; test `HighDetail.OnlyTheOpeningsFamilyAndTheTrainerHaveDetailedModels` |
| The creature trainer who offers the first creatures uses his own high-detail model | done | the trainer's HD model in the same table (`src/ECS/SuperVillager.cpp`); test `HighDetail.OnlyTheOpeningsFamilyAndTheTrainerHaveDetailedModels` |
| High-detail models have the same 22 bones as villagers, so the villager animations drive them | done | the villager's own skeleton and clips drive the HD mesh (`src/ECS/SuperVillager.cpp`, `src/ECS/Animations.cpp`) |
| They cross-fade between animations over 300 ms and smooth their turning | done | the 300 ms cross-fade and the second yaw smoothing (`components::SkeletalAnimation::crossFade*`, `DrawPosition::follow*`); `test/test_super_villager_smooth.cpp` |
| Their eyes are separate eyeballs with four lids; they blink every 1 to 5 seconds at random and glance together | done | the mesh eyes with their lids, blinks and glances (`src/ECS/SuperVillagerEyes.cpp`); `test/test_super_villager_eyes.cpp` |
| They are lit by the default sun with no haze | done | drawn lit by the default sun (`SuperVillagerFrame::litByDefaultSun`, `src/ECS/SuperVillager.cpp`) |
| They stay high detail only while a script holds the wide-screen cinema; otherwise they are released at the end of the frame | done | released by the landscape's draw without a script wide screen (`src/ECS/SuperVillager.cpp`); tests `HighDetail.KeptOnlyWhileAScriptHoldsTheBars`, `SuperVillagerRebuilds.TheEndOfTheWideScreenAsksOnceTheListIsNotEmpty` |
| A swimming high-detail villager is drawn cut by the water's surface and leaves a ring every second | done | the swimmer's cut draw and a ring every second (`src/ECS/SuperVillager.cpp`, `ecs::AddWaterRing`); see ../ocean/ |
| They cast projected shadows | partial 90% | a projected shadow lit by the fixed sun, on the land only (`components::DynamicShadow`, `src/ECS/SuperVillager.cpp`); see ../rendering/; made at the head of the projected shadows' list in the order the villagers became SuperVillagers, as the original (`shadow_list::TemporaryShadowsInMakingOrder`; tests `ShadowListOrder.*`); left, see ../rendering/shadows.md: shadows of different kinds made in the same frame are listed by kind; cast with the villager's own matrix and clip, as the original (tests `SuperVillagerSmooth.ItsShadowTakesTheObjectsMatrixNotTheTurnedCopy`, `OtherCastersKeepTheirDrawnMatrix`) |
| A script can give a high-detail villager special orders in a cinematic (follow the opening hand's grip, snap to it, mirror its turn, turn a quarter left or right) | done | THING_JC_SPECIAL sets the follow, snap and turn flags (`src/CHLApi.cpp`, `src/ECS/SuperVillager.cpp`); tests `HighDetail.TheOpeningsOrdersTurnTheBoyAndEaseHisTurning`, `SuperVillagerList.HeldByTheIntroHandItIsDrawnAtTheGripWhereItsEyesAreMade` |
| Every high-detail villager is released when the land is cleared | done | every one released when the map is cleared (`src/ECS/SuperVillager.cpp`); test `SuperVillagerList.ReleaseAllGoneAndAlive` |

## Special kinds of villager

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game has 21 special kinds besides the tribes' seven each: piper, monk, idol builder, hermit, hippy, priest, priestess, marauder, two footballers, engineer, shepherd, nomad, Aztec leader, creature trainer, Norse sailor, breeder, healer, sculptor, crusader and sailor with an accordion | done | in the villager table with their meshes (`src/Enums.h`, `src/InfoConstants.h`); the editor can place one (`src/Editor`); test `VillagerTableData.TwentyOneSpecialKindsFollowTheTribes` |
| The special kinds are only made by challenge scripts, never by towns | done | a script's CREATE of a villager takes any table row (`src/CHLApi.cpp`); towns and births only make the tribes' jobs (`src/ECS/Villager/VillagerBirth.cpp`); test `VillagerBirthTest.ChildBornNumber` |
| Each special villager lives by its own kind's row of the table, though all of them share one tribe and one job: its food kind for the predators, its sex (the priestess and the trainer are women), whether it takes the looks of another tribe's town (the footballers do), its meshes when it grows up or dies, its help texts, its sacrifice value, and its kind as a script or the vortex reads it | done | the row it is made with is kept on the villager (`Villager::infoRow`, set by `VillagerArchetype::Create` and `villager::ChangeInfo`) and read by `ecs::VillagerInfoOf` and `DynamicsSystem::ObjectInfo`; tests `VillagerInfoRow.*`, `VillagerHomeTest.ChangeInfoKeepsTheNewRow` (unverified against v1.42 in play) |
| The pied piper (Land 1): plays in his cave and leads the town's children away; he cannot be hurt; if the children die he retires | partial 20% | the script makes the villager with its kind's row and model (`Villager::infoRow`, tests `VillagerInfoRow.*`), the share counted here; his challenge has not been played through against the original, so none of it is counted yet: see ../story/land_1.md |
| The hermit (Land 1): his hut must not be wrecked; he offers a gesture to be left alone, and the advisors argue if the player kills him | partial 20% | the script makes the villager with its kind's row and model (`Villager::infoRow`, tests `VillagerInfoRow.*`), the share counted here; his challenge has not been played through against the original, so none of it is counted yet: see ../story/land_1.md |
| The sculptor (Land 1): wants a particular rock from the old quarry brought to him to carve the cow gate stone, the last of the creature-gate stones | pending | his quest is played in openblack, the rock carved into the cow stone, by a quest run of Choose Your Creature played to the choice (quest=SetupLand1/ProtectGateKeysFlag=1/ProtectGateKeys/ChooseYourCreature, 2026-10-27), not yet against the original; see ../story/land_1.md and [../story/gold_scrolls/the_sculptor.md](../story/gold_scrolls/the_sculptor.md) |
| The hippy (Land 1): wants the strongest shaking magic mushroom for his experiment; also at the singing stones | pending | see ../story/land_1.md and ../story/minigames.md |
| The creature trainer (Land 1): offers the choice of creature and turns up in the creature's lessons | partial 20% | the script makes the villager with its kind's row and model (`Villager::infoRow`, tests `VillagerInfoRow.*`), the share counted here; his challenge is played in openblack to the choice by a quest run of Choose Your Creature played to the choice (quest=SetupLand1/ProtectGateKeysFlag=1/ProtectGateKeys/ChooseYourCreature, 2026-10-27), not yet against the original, so none of it is counted yet: see ../creature/species_choice.md |
| The Norse sailors and the accordion sailor (Land 1): the missionaries who build their ark from the wood the player gives, sing three verses and sail away; they return in Land 5 with an offering | partial 20% | the script makes the villager with its kind's row and model (`Villager::infoRow`, tests `VillagerInfoRow.*`), the share counted here; his challenge has not been played through against the original, so none of it is counted yet: see ../story/land_1.md and ../story/land_5.md |
| The priest and priestess (Land 2): offer a sacrifice as a gift; a priest is also the spiritual healer | pending | see ../story/land_2.md |
| The engineer (Land 2): comes out of the workshop to explain it | pending | see ../story/land_2.md and ../building/workshop_and_scaffolds.md |
| The idol builder: a special kind the game uses only on Land 5, among the villagers who wave the creature off at the end | pending | the end credits are never reached; see ../story/ending.md; Land 2's idol maker is an ordinary Indian trader, see [the_idol.md](../story/silver_scrolls/the_idol.md) |
| The monk (Land 3): walks back to his temple if followed; part of taking the Japanese town | pending | see ../story/land_3.md |
| The nomad (Land 4): lives in his shack and holds one of the meteorite quests | pending | see ../story/land_4.md |
| The crusaders (Land 5): march in the crusaders' and traitor's challenges | pending | see ../story/land_5.md |
| The footballers (a Norse and a Celtic team): both walk on in the shipped Land 5 end credits, a Celtic footballer among the ten waving villagers and a Norse one in the special shepherd's place; otherwise only in a cup-final challenge that is not compiled into the game | pending | the end credits are never reached; see [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) and ../story/ending.md; the cup final: ../story/minigames.md |
| The marauder, Aztec-leader actor, breeder and healer only appear in the end credits; the special shepherd never appears: the credits put a Norse footballer in his place | pending | the end credits are never reached; see ../story/ending.md and [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) |
| In the end credits the special characters walk on one by one beside the names | pending | the end credits are never reached; see ../story/ending.md |

## Story characters played by ordinary villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lost brother and his sister (Land 1): she gives the gate key when he is found in the trees through the pass | pending | their quest is played in openblack, reunited away from the house, by a quest run of Choose Your Creature played to the choice (quest=SetupLand1/ProtectGateKeysFlag=1/ProtectGateKeys/ChooseYourCreature, 2026-10-27), not yet against the original; see ../story/land_1.md |
| The shepherd of the lost flock (Land 1): has 5 sheep and pays when they are brought back, worse off for each one that dies | partial 20% | the script makes the villager with its kind's row and model (`Villager::infoRow`, tests `VillagerInfoRow.*`), the share counted here; his challenge has not been played through against the original, so none of it is counted yet: see ../story/land_1.md |
| The shepherd sucked into the first vortex who comes back out (Land 1) | pending | see ../story/portals.md |
| The family on the beach (Land 2) whose children must be saved from the sea | pending | see ../story/land_2.md |
| The greedy farmer (Land 2): wants the cow thieves stopped, not killed | pending | see ../story/land_2.md |
| The woman with the old scroll of clues (Land 2): gives the treasure clues once her house is fixed, and repeats them when her door is knocked | pending | see ../story/land_2.md |
| The slavers (Land 2) who march down to attack a town | pending | see ../story/land_2.md; [the_slavers.md](../story/silver_scrolls/the_slavers.md) |
| The man who wants to be thrown (Land 3) | pending | see ../story/land_3.md and ../story/minigames.md |
| The blind woman (Land 4): carries potions to cure her brother, using one each time she is hurt; the player watches over her journey | pending | see [the_treacherous_path.md](../story/silver_scrolls/the_treacherous_path.md) |
| The ogre (Land 4): a champion who holds a guardian stone and must be fought | pending | see ../story/land_4.md |
| The Japanese traitor (Land 5) | pending | see ../story/land_5.md |
| The creature breeder (Lands 1, 2, 4, 5): offers other creatures to swap for | pending | see ../creature/species_choice.md |

## Missionaries

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A missionary disciple goes to the nearest abode of any town and joins that town as a missionary of its own player | done | `villager::MissionaryDisciple` (`src/ECS/Villager/VillagerMissionary.cpp`): the closest abode, `AddMissionary` for its own player, as the exe's 0x76A220 calls (FindClosestAbode, Town::AddMissionary, ForceMoveVillagerToAbode); test `VillagerDiscipleTest.MissionariesPreachThroughControlsInTheTownsList`; see disciples.md |
| It is moved into that abode and changes there | done | `ForceMoveVillagerToAbode`, then `GO_HOME_AND_CHANGE` (`villager::MissionaryDisciple`), as the exe's; test `VillagerLifeWorld.AMissionaryMovesIntoTheNearestTownsHutAndChangesThere` (`test/test_villager_life_world.cpp`, on synthetic and the game's data) |
| With no abode anywhere it goes back to deciding what to do | done | `villager::MissionaryDisciple` sets `DECIDE_WHAT_TO_DO` (a missionary of no player too, a guard); test `VillagerLifeWorld.AMissionaryWithNoAbodeAnywhereGoesBackToDeciding` (`test/test_villager_life_world.cpp`, on synthetic and the game's data) |
| The town keeps its missionaries in a list with their count, saved with the game | partial 50% | the list, newest first, run at the town turn's missionary step (`Town::missionaries`, `villager::ProcessMissionaries` from `src/ECS/Town/TownProcess.cpp`); test `VillagerDiscipleTest.MissionariesPreachThroughControlsInTheTownsList`; not saved: openblack saves no towns |
| Each missionary's control follows it from its first turn on, and goes, freeing it, when the missionary dies or is no longer one, or its reaction ends | done | `villager::ProcessMissionary` / `DeleteMissionary` (`src/ECS/Villager/VillagerMissionary.cpp`), as the original's `MissionaryControl::Process` and `ToBeDeleted`; test `VillagerDiscipleTest.MissionariesPreachThroughControlsInTheTownsList` |
| A town that changes owner deletes its missionaries, which stop being missionaries (see disciples.md) | done | `DeleteAllMissionaries` from `villager::ResetAllDiscipleStates` (`src/ECS/Villager/VillagerDisciple.cpp`), each control's delete freeing its missionary, as the original's `Town::ResetAllDiscipleStates`; test `VillagerDiscipleTest.ATownChangingHandsFreesTheDisciplesHeldAtTheirJobsInItsAbodes` |
