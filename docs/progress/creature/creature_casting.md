# Creature casting

A creature can cast the miracles it has learnt. It chooses a miracle the way it chooses any action, to satisfy a desire:
lightning or a fireball when angry, a heal or food when kind, a spell on another creature when playful. It walks to a
good distance, turns, may draw the miracle's gesture, then takes up its casting pose while the miracle flows from its
hands. It pays with its own energy, not the player's prayer power, and a miracle it has only half learnt may fizzle.
What each miracle does belongs to [../miracles/](../miracles/); how miracles are learnt is in
[learning_by_observation.md](learning_by_observation.md); casting in a fight is in [fighting.md](fighting.md).

**Progress: 0/62 done, 3 partial — 2%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Which miracles it may try

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It may try a miracle once it has seen it half the times it needs to learn it | todo | our tree counts the sightings of a miracle (`creature_watching::SeeMiracle`, see [learning_by_observation.md](learning_by_observation.md)) but a creature never tries one: no creature casting in our tree |
| A try short of all but one of the sightings it needs fizzles, and counts as one more sighting, so trying teaches it | todo | no creature casting in our tree |
| A fizzled try shows a light bulb over its head | todo | no creature casting and no light bulb effect |
| After a fizzle it is embarrassed, and one time in two sad as well | todo | no creature casting; the embarrassed animation exists (`CreatureLayers.cpp`) but nothing plays it for a fizzle |
| It tries a power-up of a miracle only once grown past a stage | todo | no creature casting in our tree |
| How skilled it is at a miracle can make it miss its aim | todo | not modelled |
| Scripts can give it the most skill at a miracle | todo | not modelled |
| It can't cast a miracle at something it can't be cast at; trying leaves it frustrated, wanting food above all | todo | the magic cast rules answer no for every creature (`src/Magic/CastRules.h`: creatures are not ported) |

## Paying for it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature pays with its body: chants for its size and strength over the energy it has above a floor | todo | no creature casting; `SpellCreator` gives a creature 0 chants (`src/Magic/Core/SpellCreator.h`) |
| Paying costs energy (by a share for the kind of miracle) and tires it, by no more than seven tenths at a time | todo | no creature casting in our tree |
| It can't cast a miracle that would leave it too exhausted to pay for making it | todo | no creature casting in our tree |
| A miracle that lasts keeps drawing on the creature while it runs | todo | a creature as a spell's creator is not ported (`src/Magic/Core/SpellCreator.h`) |
| The player's prayer power is not spent on the creature's miracles | todo | no creature casting in our tree |
| In a fight each miracle costs stamina, and it can't cast one it hasn't the stamina for | todo | a fight cast plays only its animation (`CreatureFightSystem`: the creature's miracles come later); stamina is never used up (`CreatureFight.h`) |

## How it casts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One time in five it first shows how it feels: angry before lightning, kind before a helpful miracle, playful before a spell on a creature | todo | no creature casting in our tree |
| Lightning is cast from fifty away, backing off to ten more than its height | todo | no creature casting in our tree |
| A helpful miracle is cast from twice its height; a spell on a creature from five times, backing off to twice | todo | no creature casting in our tree |
| Getting away from the thing, it goes to the nearest clear area as wide as it is tall | todo | no creature casting in our tree |
| It turns to face the thing, within an eighth of a turn | todo | no creature casting in our tree |
| Going near gets stuck: it gives up and holds back that desire for a while | todo | no creature casting in our tree |
| It draws the miracle's gesture in the air before casting | todo | no creature casting in our tree |
| It takes up its casting pose: start, loop, end; the miracle is cast as the loop begins, held three seconds, and let go as the pose ends | partial | only in fights: the cast start, loop and end animations (`CreatureFight.h`, `k_StartCast` to `k_EndCast`) play in `CreatureFightSystem`, with no miracle made |
| Water is cast with a scattering pose instead | todo | not modelled |
| A move the body can't make gives up the rest of the casting | todo | no creature casting in our tree |
| The miracle is as big as what it is cast at (a little bigger than its radius, a creature by its height, a fire miracle by the caster's height) | todo | no creature casting in our tree |
| The miracle's effect flows from between the creature's hands towards what it is cast at, following its hands as it moves | todo | no creature casting in our tree |
| Food, wood and water are cast from a point above the target, with a beam from each of its hands to that point | partial | the table's flag is read (`magic::IsCreatureCastFromAbove`, test `MagicTables.effectGetters`), shown in the debug Magic window only; no creature casts |
| A miracle that lasts only while held, such as lightning, stops when it lets go | todo | no creature casting in our tree |
| The miracle runs for the creatures' own time from the tables | partial | `magic::GetTimerWhenCreatureCasting` reads the table; nothing calls it, as no creature casts |
| Its town fears or respects it for what it casts | todo | no creature casting; see [town_actions.md](town_actions.md) |

## Choosing a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Angry, it casts lightning at something | todo | the planner weighs no miracle actions (`src/Creature/CreaturePlanActions.h`: miracles wait for the systems they need) |
| Kind, it heals a villager hurt to seven tenths of its life or less | todo | not in the planner's actions (`CreaturePlanActions.h`) |
| Playful or spiteful, it casts a spell on another creature: freeze, small, big, weak, strong, fat, thin, invisible, nice, angry, hungry, frightened, tired, ill, thirsty, itchy | todo | the spells' effects on a creature exist (`src/Creature/CreatureSpells.*`, `Magic/Spells/SpellCreature`, cast by the player), but a creature never casts one; see [../miracles/](../miracles/) |
| It casts a spell meant to amuse another creature | todo | no creature casting in our tree |
| Angry, it casts a fireball or an explosion | todo | no creature casting in our tree |
| It casts a lightning storm or a tornado | todo | no creature casting in our tree |
| It casts the power-ups of lightning, fireball, explosion, food, heal and shield | todo | no creature casting in our tree |
| It casts a miracle to impress villagers | todo | not modelled |
| It casts food by a worship site, or into the storehouse | todo | the deeds exist for mimicry (`src/Creature/CreatureDeeds.h`) but no creature casting; see [town_actions.md](town_actions.md) |
| It casts wood by a building site or the workshop | todo | no creature casting; see [town_actions.md](town_actions.md) |
| It sprinkles water on crops or waters trees for a town | todo | no creature casting; see [town_actions.md](town_actions.md) |
| It puts out a fire with water | todo | no creature casting; see [town_actions.md](town_actions.md) |
| It casts a forest | todo | not modelled |
| It shields a town, or casts a physical shield | todo | not modelled |
| It looks for a town of its own that needs a miracle, and decides whether the nearest town should be helped or attacked | todo | not modelled |
| It heals itself | todo | not modelled |
| On fire, it puts the fire out on itself with water | todo | not modelled |
| It casts teleport to travel to something, or explores and casts teleport | todo | not modelled; only villagers use the teleport stones in our tree (`VillagerTeleport.cpp`) |
| It copies a miracle it saw its god cast | todo | not modelled; see [learning_by_observation.md](learning_by_observation.md) |
| It shows a friend how to cast fireball, lightning, the storm, food or wood | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It casts warming or cooling spells, or cures illness, on a friend | todo | see [friends_and_other_creatures.md](friends_and_other_creatures.md) |
| It swaps minds with another creature | todo | not modelled |

## One-off miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a one-off miracle, it casts it to attack | todo | not modelled |
| Holding a one-off miracle, it casts it to help | todo | not modelled |
| Holding a one-off miracle, it casts it in play | todo | not modelled |
| Holding a one-off miracle, it casts it to restore its health | todo | not modelled |
| It picks up a one-off miracle lying about and casts it, for any of those four | todo | not modelled |
| The player can hand it a one-off miracle | todo | giving things to the creature isn't modelled; see [object_actions.md](object_actions.md) |
| It steals miracles and miracle seeds | todo | not modelled |
| Catching a fireball, it throws it back | todo | not modelled |

## Telling it to cast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The debug tools and testbed can tell a creature to cast any miracle at something | todo | the debug spawner can only cast a miracle near a creature for it to watch (`CreatureSpawnerMind.cpp`, "Cast it near"), not have the creature cast; no testbed in our tree |
| Scripts can make a creature cast a miracle | todo | the script functions that force creature actions are stubs in `src/CHLApi.cpp` (`CREATURE_DO_ACTION`, `SET_CREATURE_QUEUE_FIGHT_SPELL`) |
