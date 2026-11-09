# Creature physiology

A creature's body lives through time: it gets hungry, tired, thirsty, hot or cold, needs to poo, grows stronger from
work and fatter from overeating, is healed by sleep and faints when exhausted, starved or out of life. Each species has
its own rates, from the game's creature tables.

**Progress: 36/53 done, 0 chosen, 9 partial, 4 inferred — 78%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Life and damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature has a life from 0 to 1, shown on the status panel as damage | done | `creature_physiology::Needs::life` (`src/Creature/CreaturePhysiology.h`); the panel's damage is 1 − life (`creature_panel::FromNeeds`, `src/Creature/CreatureStatusPanel.cpp`), set by `Game.cpp` and drawn by `GameInterface`; tests in `test/test_creature_status_panel.cpp`, `test/creature/test_creature_panel_choice.cpp` |
| Miracles, fire and thrown things take life away | partial 80% | a miracle's crush and hit, divided by the creature's drawn size, and burning take its body's life (`CreatureApplyEffect` in `src/ECS/Effects/EffectValues.cpp`, `fire::traits::ReduceLifeDueToBurning`), none from its own miracle; a thrown thing's crush hurts it (`test/test_creature_struck.cpp`); tests `CreatureSpellHooksTest.ACreatureDefendsByItsSizeAgainstBurnCrushHitAndFlyingAway`, `BurningHurtsACreatureAsItHurtsAnythingElse`, `ACreatureTakesNoHarmFromItsOwnMiracle`; missing: with an opponent the harm also comes off its fight health; see [../physics](../physics/) |
| A creature with no life left is knocked out, not killed | done | out of life it faints (`creature_physiology::ShouldFaint`), and `CreatureMindSystem` knocks it out through `CreatureFightSystem::KnockOut` |
| The heal miracle gives life back, and scripts can make a creature unable to die (a killing miracle restores it instead) | partial 60% | a heal gives life back by its strength and the creature's heal multiplier, and a miracle that takes the last of its life faints it, or gives all its life back to a creature that cannot die (`CreatureApplyEffect` and `DestroyedByEffect` in `src/ECS/Effects/EffectValues.cpp`, `Creature::canDie`; tests `CreatureSpellHooksTest.AHealHealsACreaturesLifeAndMendsItsMarks`, `AMiracleThatTakesTheLastOfItsLifeFaintsIt`, `ACreatureThatCannotDieGetsAllItsLifeBack`); missing: no script sets the switch, `DEV_FUNCTION` 8 and 9 are not ported (`ECS/PlayerCreature.cpp`, test `PlayerCreatureNativesTest.OtherDevFunctionsAreNotPorted`); see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| Sleeping and resting heal it by the species' rate | done | `creature_physiology::SleepTurn`, run by `CreaturePhysiologySystem`; test `CreaturePhysiology.SleepHealsAndRestsThenWakes` |
| While the player's hand holds it, its body's time stands still: no turn counted, no ageing, growth, strength from carrying, or use of energy, fat, freshness, water and warmth | done | `CreaturePhysiologySystem` skips `TickTurn` while the creature hand holds the creature; the sleep's rest and healing still run; the creature debug window's body section says so |
| Fights take a quarter of the fight health lost off the real life | done | `creature_fight::k_LifeLostShare` (`src/Creature/CreatureFight.h`), applied by `CreatureFightSystem`; see [fighting.md](fighting.md) |
| Low life feeds the wish to rest and get better, and fear and anger from being damaged | inferred | the body drives the restoring-health source as 1 − life (`creature_physiology::SourceValue`), a value not read in the exe (the body sources' getters are unnamed); fear and anger from damage are not driven at all; see [desires.md](desires.md) |
| A creature only dies for good when a script says so | partial 40% | the permanent death works (`CreatureFightSystem::KillPermanently`, test `test/creature/test_creature_fight_turns.cpp`), but only the debug spawner calls it: no script native reaches it |

## Energy, hunger and fat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Energy runs down over time at the species' rate; hunger is energy missing | done | `creature_physiology` `TickTurn`, `Hunger`; test `CreaturePhysiology.EnergyRunsDownSlowerForBigOrSleepingCreatures` |
| Bigger creatures use energy more slowly, down to half as fast at size 2, and much more slowly sleeping at home or resting to get better (by the action it carries out) | done | `k_EnergySizeFactor`, `k_RestingEnergyDivisor`, `creature_physiology::SlowsEnergy`, the action taken from the mind at the body's turn (`CreatureMindSystemInterface::CurrentActionOf`); tests `CreaturePhysiology.EnergyRunsDownSlowerForBigOrSleepingCreatures`, `CreaturePhysiology.WhichActionsCountAsAsleep`, `CreaturePhysiologySystemTest.ItGoesByTheActionItsMindCarriesOutAtItsOwnTurn` |
| Below half energy it burns fat, getting thinner | done | `k_FatBurnBelowEnergy` 0.5 and the species' burn, as the exe's body turn (energy < 0.5 then fat −= the species' rate, 0x4EF717..0x4EF754); test `CreaturePhysiology.HungryCreaturesBurnFat` |
| Eating fills it by the food's value, up to its size; a big meal pushes energy past full | done | `creature_physiology::Eat`, called through `CreaturePhysiologySystem::Eat` when `CreatureObjectActionSystem` eats; test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Overeating makes it fatter by the species' factor | done | test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Fatness shows on the body slowly, a step each turn | done | see [appearance.md](appearance.md) |
| Each meal is counted | done | `Needs::meals` |
| What it eats changes it: villagers eaten make it more evil (unconfirmed how), poisoned food makes it sick and holds back its hunger for a while | pending | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Starving, it faints | done | `creature_physiology::ShouldFaint`; test `CreaturePhysiology.FaintingOnlyForGrownUpOwnedCreatures` |

## Tiredness and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving tires it at the species' rate; young creatures up to four times faster, less as they age | done | (4 − 0.15 × age ticks, at least 1) × the species' rate while moving, as the exe (0x4EF757..0x4EF7F1); test `CreaturePhysiology.MovingTiresYoungAndHungryCreaturesFaster` |
| Walking with too little energy tires it faster still | done | 0.3 × the rate more below the species' low-energy mark (0x4EF7AB..0x4EF7C7); test `CreaturePhysiology.MovingTiresYoungAndHungryCreaturesFaster` |
| Actions cost energy and tiredness by the table, less for bigger creatures | done | `creature_physiology::ApplyActionCost`, through `CreaturePhysiologySystem`; test `CreaturePhysiology.ActionsCostEnergyAndBuildStrength` |
| Casting miracles costs energy and tires it | done | `creature_spell_casting` paid through `MagicCreatureCasting.cpp`, by a share for the kind of miracle; see [creature_casting.md](creature_casting.md) |
| Exhausted, it moves only slowly | done | test `CreatureLocomotion.ExhaustedCreaturesGoSlowly` |
| Asleep it rests at the species' rate; it never wakes in the first few turns, wakes fully rested in the day or nearly rested after sleeping long enough for its size and the day's length | done | `creature_physiology::SleepTurn`, `LongEnoughAsleep` (the day from `DayNightClock::GetDuration`); tests `CreaturePhysiology.SleepHealsAndRestsThenWakes`, `CreaturePhysiologySystemTest.HowLongItSleepsFollowsTheDaysLengthOnTheClock` |
| Sitting rests it a fifth as much as sleep | done | `creature_physiology::SitTurn`, through `CreaturePhysiologySystem::Sit`, which the mind calls once a turn while the body plays the sit loop (`CreatureNeeds::sitting`); tests `CreaturePhysiology.SittingRestsAFifthAsMuchAsSleep`, `CreaturePhysiologySystemTest.SittingRestsItAFifthAsMuchAsSleep` |
| Night makes it sleepy | inferred | the body drives the night tiredness source as 1 at night and 0 by day, from the sky's clock (`creature_physiology::SourceValue`); the original's getter for that source is not read; see [desires.md](desires.md) |
| Fully exhausted, it faints | done | `creature_physiology::ShouldFaint` |

## Thirst, poo and sickness

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thirst builds up over the species' time to dehydrate, once grown enough, the turns cut to whole numbers | done | tests `CreaturePhysiology.ThirstBuildsUpOnceGrownEnough`, `TheTurnsToDehydrateAreCutToWholeNumbers` |
| Drinking quenches it fully and cools it to 0, putting out a fire on it | done | `creature_physiology::Drink` and the fire system's `SetTemperature`, from the mind's `DrinkFromTheSea` (`CreatureMindSystem`); test `CreaturePhysiologySystemTest.DrinkingQuenchesItAndCoolsItToZero` |
| Eating builds up poo by the energy gained | done | `creature_physiology::Eat`; test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Having a poo empties it and leaves a lump on the ground behind it, sized by the creature | partial 60% | `CreaturePhysiologySystem::Poo` empties it and makes a lump of poo behind it, sized by the creature (test `CreaturePhysiologySystemTest.APooIsALumpOfPooTurnedOnTheSyncedStream`); the game throws the lump back along the ground, here it is set down there |
| Being sick throws drops from its mouth that lie on the land a while and fade | partial 50% | `CreaturePhysiologySystem::Puke` (test `CreaturePhysiologySystemTest.SickIsDropsOfItsOwnThatGoAfterFourSeconds`); the original's puke routine is not found in the exe (only the action's name), so how the drops look, fall and how long they last is not read ([overview](../../../creature/overview.md#pending)) |
| Illness, from the ill spell or bad food, makes it want to be sick | partial 50% | the ill spell drives the illness desire (`creature_spells`, `src/Creature/CreatureSpells.cpp`; see [../miracles](../miracles/)); illness from food is pending |
| Itchiness makes it want to scratch; only the itchy spell makes it itch | done | `Needs::itchiness` and the itchy spell in `src/Creature/CreatureSpells.cpp` |
| The creature can get high, which makes its eyes stoned and its desires odd; eating magic mushrooms and toadstools causes it | pending | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |

## Warmth and weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Warmth follows how far the temperature where it stands is from what its species likes, through the game's sigmoid | done | test `CreaturePhysiology.WarmthFollowsTheSigmoidOfTheTemperature`; the temperature from `weather::GetTemperatureAt` (`CreaturePhysiologySystem.cpp`) |
| Too cold it wants to get warmer and shivers; too hot it wants to get colder and shows it | partial 40% | the get-warmer and get-colder sources are driven from the warmth (`SourceValue`, values not read in the exe); `Shiver` and `ShowHotness` are plan actions (`src/Creature/CreaturePlanActions.cpp`), see [object_actions.md](object_actions.md) |
| Fire on or near it warms it, and water cools it (unconfirmed) | inferred | unconfirmed: the body's warmth changes only by the weather in the original's body turn (0x4EF864..0x4EF928); whether fire or water reach the warmth elsewhere is not read (drinking sets the fire system's temperature, not the warmth) |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Carrying heavy things while moving makes it stronger | partial 80% | the step is the exe's, by half over the species' carry minutes times 600 times the held thing's weight over its own, 0..1 (0x4EF541..0x4EF5DA; `src/Creature/CreaturePhysiology.cpp`; tests `CreaturePhysiology.CarryingMakesItStronger`, `CarryingsStepDoesNotFollowTheTurnsLength`); the held thing's weight is its info weight alone (`WeightOf` in `CreatureObjectActionSystem.cpp`), where the original's is scale cubed times the info weight, and a rock weighs nothing |
| Actions add strength by the table | done | test `CreaturePhysiology.ActionsCostEnergyAndBuildStrength` |
| Strength fades over time at the species' rate | partial 80% | each body turn strength is multiplied by the species' rate and kept within 0..1 (`src/Creature/CreaturePhysiology.cpp`), as the exe (0x4EF60F..0x4EF679); no test pins the fade (the test species' rate is 1) |
| Strength shows on the body as weak or strong | done | see [appearance.md](appearance.md) |
| Strength sets how heavy a thing it can pick up | pending | see [object_actions.md](object_actions.md) |

## Ageing, fainting and the rest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It ages a tick per species' length of game time, counted in whole turns a second | done | tests `CreaturePhysiology.ItAgesOncePerTickOfGameTime`, `CreaturePhysiology.ItsAgeTickCountsWholeTurnsASecond` |
| A new creature starts with its species' energy and warmth | done | `creature_physiology::Start`; test `CreaturePhysiology.ANewBodyStartsFromItsSpecies` |
| Before the first stages of growing up the body only counts its turns, which count towards its age | done | tests `CreaturePhysiology.OnlyItsTurnsAreCountedBeforeTheBodyStage`, `CreaturePhysiology.TurnsBeforeTheBodyStageCountTowardsItsAge` |
| Only creatures owned by a player and grown far enough faint, never one a script controls or the computer's | done | `creature_physiology::ShouldFaint` with `FaintGates`; tests `CreaturePhysiology.FaintingOnlyForGrownUpOwnedCreatures`, `CreaturePhysiologySystemTest.OnlyACreatureNoScriptControlsAndNoComputerPlaysFaints`; nothing sets the computer's player type yet |
| Fainted, it lies still, is carried to its pen and comes round no longer quite exhausted, starved or parched | done | `CreaturePhysiologySystem::WakeFromFaint`; `CreatureFightSystem` carries a knocked-out creature home; see [home_and_pen.md](home_and_pen.md) |
| How far a creature sees depends on its size | done | `src/Creature/CreatureLook` range, used by `CreatureMindSystem`; see [idle_behaviour.md](idle_behaviour.md) |
| The body drives the desire sources: hunger, tiredness, thirst, poo, warmth, itch, health | inferred | `SourceValue` gives hunger as 1 − energy, tiredness, thirst, poo, warmth, itch and 1 − life as the values the sources read (tests `CreaturePhysiology.TheBodyDrivesItsDesireSources`, `LeftAloneItsBodyDrivesItsHungerThirstAndPoo`); the original's getters for these sources are not read, so the mapping is unverified |
| Each species has its own rates from the game's tables | done | `CreaturePhysiologySystem` reads the species' row of the creature tables (`src/InfoConstants.h`); the body turn's reads of the row (+0x210 … +0x234) are the exe's (0x4EF510..0x4EF928, [body](../../../creature/body.md#growth-size-and-age-as-time-passes)); test `CreaturePhysiologySystemTest.ABodysNeedsAreTheSpeciesStartUntilSetAndASetIsKept` |
| Scripts can read and set the body's values (warmth, fatness, energy, itchiness, poo, exhaustion, thirst, fight health) | done | `GET_PROPERTY` and `SET_PROPERTY` read and set all eight on a creature (`player_creature::GetCreatureProperty` / `SetCreatureProperty`, `src/CHLApi.cpp`) |
| The body is saved with the game | pending | no saved games; see [../engine](../engine/) |
