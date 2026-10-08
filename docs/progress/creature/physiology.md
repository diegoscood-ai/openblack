# Creature physiology

A creature's body lives through time: it gets hungry, tired, thirsty, hot or cold, needs to poo, grows stronger from
work and fatter from overeating, is healed by sleep and faints when exhausted, starved or out of life. Each species has
its own rates, from the game's creature tables.

**Progress: 35/52 done, 8 partial — 75%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Life and damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature has a life from 0 to 1, shown on the status panel as damage | partial | `creature_physiology::Needs::life` (`src/Creature/CreaturePhysiology.h`); the status panel's damage bar (`src/Creature/CreatureStatusPanel`) is only used by tests, nothing draws it |
| Miracles, fire and thrown things take life away | todo | a burning creature takes no harm (`src/ECS/Fire/FireObjectTraits.cpp`) and a miracle's damage leaves a creature's life alone (`src/ECS/Effects/EffectValues.cpp`); see [../physics](../physics/) |
| A creature with no life left is knocked out, not killed | done | out of life it faints (`creature_physiology::ShouldFaint`), and `CreatureMindSystem` knocks it out through `CreatureFightSystem::KnockOut` |
| The heal miracle gives life back, and scripts can make a creature unable to die (a killing miracle restores it instead) | todo | no heal for creatures and no "can die" switch: `DEV_FUNCTION` 8 and 9 are not ported (`ECS/PlayerCreature.cpp`, test `PlayerCreatureNativesTest.OtherDevFunctionsAreNotPorted`); see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| Sleeping and resting heal it by the species' rate | done | `creature_physiology::SleepTurn`, run by `CreaturePhysiologySystem`; test `CreaturePhysiology.SleepHealsAndRestsThenWakes` |
| Fights take a quarter of the fight health lost off the real life | done | `creature_fight::k_LifeLostShare` (`src/Creature/CreatureFight.h`), applied by `CreatureFightSystem`; see [fighting.md](fighting.md) |
| Low life feeds the wish to rest and get better, and fear and anger from being damaged | partial | the body drives the restoring-health source (`creature_physiology` `SourceValue`); fear and anger from damage are not driven; see [desires.md](desires.md) |
| A creature only dies for good when a script says so | partial | `CreatureFightSystem::KillPermanently` exists, but only the debug spawner calls it; no script native reaches it |

## Energy, hunger and fat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Energy runs down over time at the species' rate; hunger is energy missing | done | `creature_physiology` `TickTurn`, `Hunger`; test `CreaturePhysiology.EnergyRunsDownSlowerForBigOrSleepingCreatures` |
| Bigger creatures use energy more slowly, down to half as fast at size 2, and much more slowly asleep or resting | done | `k_EnergySizeFactor`, `k_RestingEnergyDivisor`; test `CreaturePhysiology.EnergyRunsDownSlowerForBigOrSleepingCreatures` |
| Below half energy it burns fat, getting thinner | done | test `CreaturePhysiology.HungryCreaturesBurnFat` |
| Eating fills it by the food's value, up to its size; a big meal pushes energy past full | done | `creature_physiology::Eat`, called through `CreaturePhysiologySystem::Eat` when `CreatureObjectActionSystem` eats; test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Overeating makes it fatter by the species' factor | done | test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Fatness shows on the body slowly, a step each turn | done | see [appearance.md](appearance.md) |
| Each meal is counted | done | `Needs::meals` |
| What it eats changes it: villagers eaten make it more evil (unconfirmed how), poisoned food makes it sick and holds back its hunger for a while | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |
| Starving, it faints | done | `creature_physiology::ShouldFaint`; test `CreaturePhysiology.FaintingOnlyForGrownUpOwnedCreatures` |

## Tiredness and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving tires it at the species' rate; young creatures up to four times faster, less as they age | done | test `CreaturePhysiology.MovingTiresYoungAndHungryCreaturesFaster` |
| Walking with too little energy tires it faster still | done | test `CreaturePhysiology.MovingTiresYoungAndHungryCreaturesFaster` |
| Actions cost energy and tiredness by the table, less for bigger creatures | done | `creature_physiology::ApplyActionCost`, through `CreaturePhysiologySystem`; test `CreaturePhysiology.ActionsCostEnergyAndBuildStrength` |
| Casting miracles costs energy and tires it | todo | the creature casts no miracle; see [creature_casting.md](creature_casting.md) |
| Exhausted, it moves only slowly | done | test `CreatureLocomotion.ExhaustedCreaturesGoSlowly` |
| Asleep it rests at the species' rate; it never wakes in the first few turns, wakes fully rested in the day or nearly rested after sleeping long enough for its size | done | `creature_physiology::SleepTurn`; test `CreaturePhysiology.SleepHealsAndRestsThenWakes` |
| Night makes it sleepy | partial | the body drives the night tiredness source from the sky's clock (`SourceValue`); see [desires.md](desires.md) |
| Fully exhausted, it faints | done | `creature_physiology::ShouldFaint` |

## Thirst, poo and sickness

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thirst builds up over the species' time to dehydrate, once grown enough | done | test `CreaturePhysiology.ThirstBuildsUpOnceGrownEnough` |
| Drinking quenches it fully | done | `creature_physiology::Drink`, from the mind's `DrinkFromTheSea` (`CreatureMindSystem`) |
| Eating builds up poo by the energy gained | done | `creature_physiology::Eat`; test `CreaturePhysiology.EatingFillsItUpFattensAndBuildsPoo` |
| Having a poo empties it and leaves a lump on the ground behind it, sized by the creature | partial | `CreaturePhysiologySystem::Poo` makes a lump of poo behind it, sized by the creature; the game throws the lump back along the ground, here it is set down there |
| Being sick throws drops from its mouth that lie on the land a while and fade | done | `CreaturePhysiologySystem::Puke` |
| Illness, from the ill spell or bad food, makes it want to be sick | partial | the ill spell drives the illness desire (`creature_spells`, `src/Creature/CreatureSpells.cpp`; see [../miracles](../miracles/)); illness from food is todo |
| Itchiness makes it want to scratch; only the itchy spell makes it itch | done | `Needs::itchiness` and the itchy spell in `src/Creature/CreatureSpells.cpp` |
| The creature can get high, which makes its eyes stoned and its desires odd; eating magic mushrooms and toadstools causes it | todo | see [feeding_and_thrown_things.md](feeding_and_thrown_things.md) |

## Warmth and weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Warmth follows how far the temperature where it stands is from what its species likes, through the game's sigmoid | done | test `CreaturePhysiology.WarmthFollowsTheSigmoidOfTheTemperature`; the temperature from `weather::GetTemperatureAt` (`CreaturePhysiologySystem.cpp`) |
| Too cold it wants to get warmer and shivers; too hot it wants to get colder and shows it | partial | the sources are driven; `Shiver` and `ShowHotness` are plan actions (`src/Creature/CreaturePlanActions.cpp`), see [object_actions.md](object_actions.md) |
| Fire on or near it warms it, and water cools it (unconfirmed) | todo |  |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Carrying heavy things while moving makes it stronger | done | test `CreaturePhysiology.CarryingMakesItStronger` |
| Actions add strength by the table | done | test `CreaturePhysiology.ActionsCostEnergyAndBuildStrength` |
| Strength fades over time at the species' rate (unconfirmed) | partial | `Species::strengthDecay` is read from the species' row and applied (`src/Creature/CreaturePhysiology.cpp`); whether as the game does is unconfirmed |
| Strength shows on the body as weak or strong | done | see [appearance.md](appearance.md) |
| Strength sets how heavy a thing it can pick up | todo | see [object_actions.md](object_actions.md) |

## Ageing, fainting and the rest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It ages a tick per species' length of game time | done | test `CreaturePhysiology.ItAgesOncePerTickOfGameTime` |
| A new creature starts with its species' energy and warmth | done | `creature_physiology::Start`; test `CreaturePhysiology.ANewBodyStartsFromItsSpecies` |
| The body changes nothing before the first stages of growing up | done | test `CreaturePhysiology.NothingChangesBeforeTheBodyStage` |
| Only creatures owned by a player and grown far enough faint | done | `creature_physiology::ShouldFaint`; test `CreaturePhysiology.FaintingOnlyForGrownUpOwnedCreatures` |
| Fainted, it lies still, is carried to its pen and comes round no longer quite exhausted, starved or parched | done | `CreaturePhysiologySystem::WakeFromFaint`; `CreatureFightSystem` carries a knocked-out creature home; see [home_and_pen.md](home_and_pen.md) |
| How far a creature sees depends on its size | done | `src/Creature/CreatureLook` range, used by `CreatureMindSystem`; see [idle_behaviour.md](idle_behaviour.md) |
| The body drives the desire sources: hunger, tiredness, thirst, poo, warmth, itch, health | done | `SourceValue`; tests `CreaturePhysiology.TheBodyDrivesItsDesireSources`, `LeftAloneItsBodyDrivesItsHungerThirstAndPoo` |
| Each species has its own rates from the game's tables | done | `CreaturePhysiologySystem` reads the species' row of the creature tables (`src/InfoConstants.h`) |
| Scripts can read and set the body's values (warmth, fatness, energy, itchiness, poo, exhaustion, thirst, fight health) | todo | `GET_PROPERTY` and `SET_PROPERTY` have no creature properties (`src/CHLApi.cpp`) |
| The body is saved with the game | todo | no saved games; see [../engine](../engine/) |
| The debug spawner shows and sets every value | done | `src/Debug/CreatureSpawnerBody.cpp` (openblack only) |
