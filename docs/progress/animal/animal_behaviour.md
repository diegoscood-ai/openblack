# Animal behaviour

What all animals share: their needs, how they move and react, how they are drawn, and how they die.

**Progress: 23/28 done, 2 partial — 86%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../bw1-notes/animals.md), [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md).

## Needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals get hungry, thirsty and tired, and need to breed, each at its kind's rate | done | the needs counters (hunger, sleep, breeding) up to their info.dat maximum, in the states that have the needs flag (`src/ECS/AnimalAI.cpp`). Our wiki differs: the needs are hunger, sleep and breeding; there is no thirst ([animals](../../bw1-notes/animals.md#herbivores-sheep-tortoise-cow-horse-pig)) |
| Each need is met by looking for food, water, a place to sleep or a mate in the cells around | done | the grazing spot found in a spiral in front of it, the flock's sleep cell, a birth in the flock (`src/ECS/AnimalAI.cpp`). Our wiki differs: no animal looks for water or a mate; it sleeps at its flock's sleep cell and gives birth when the flock is short ([animals](../../bw1-notes/animals.md#herbivores-sheep-tortoise-cow-horse-pig)) |
| Animals eat what their kind eats: meat, plants, grass or any | done | the grazers graze, the predators hunt and eat animals (`src/ECS/AnimalPredators.cpp`), and a food pile draws only the hungry ones whose food kinds take it (`src/ECS/AnimalFlee.cpp`) |
| A starving animal dies | todo | no animal dies of hunger in our tree |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal turns by at most its kind's turn angle, harder close to its goal, and steps its speed | done | the turn by at most the kind's turn angle, harder inside its turning circle (`src/ECS/AnimalAI.cpp`) |
| It goes only where it can reach without circling | done | the turning circle rule, so it does not keep circling its goal (`src/ECS/AnimalAI.cpp`) |
| Its clip plays by the ground it covers while moving, else by the clock | done | the clip per state and species, the walk by the ground covered (`src/ECS/AnimalAnimations.cpp`) |
| It is drawn between its last two turns | done | the smooth drawing between turns (`src/ECS/MobileDrawing.cpp`) |
| It looks round as it goes, within its view angle | done | the grazing search in front, within half its view angle each side (`src/ECS/AnimalAI.cpp`) |
| It walks round obstacles and buildings | done | the detour round objects (`src/ECS/AnimalWallHug.cpp`); `test/mobile_wall_hug/test_mobile_wall_hug.cpp`; see `../physics/` |
| Ground animals keep their feet on the land and cast a ground blob shadow | done | on the land, with their blob shadows (`src/Graphics/GroundBlobs.cpp`) |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Animals flee from fire, falling trees, flying objects and fights | partial | the predators flee what the hand throws (`animal_ai::SpreadFlyingObjectReaction`); the fire reaction is not ported. Our wiki differs: only predators react to flying objects, no code makes the falling-tree reaction and no animal reacts to fights ([animals](../../bw1-notes/animals.md#reactions)) |
| Animals look at miracles and flee the frightening ones | todo | the spell and look reactions are not ported for animals (`src/ECS/AnimalFlee.cpp`) |
| Animals look at and go to food and wood the hand or a miracle put down | done | the hungry grazers go to a food pot the hand puts down and eat 50 of it (`animal_ai::SetupPotReaction`, `src/ECS/AnimalFlee.cpp`). Our wiki differs: the magic food piles of the hand and the miracles draw no animal, and no animal reacts to wood ([animals](../../bw1-notes/animals.md#reactions)) |
| Animals react to being picked up and dropped by the hand | done | `IN_HAND` with its clip, then the flight and the landing (`src/ECS/AnimalAI.cpp`, `src/ECS/LivingPhysics.cpp`) |
| Animals react to the creature's gifts and to the creature itself | todo | the creature reaction is not ported for animals |
| Animals react to a shield, a teleport and a magic tree | n/a | Our wiki differs: no animal reacts to a shield, a teleport or a magic tree ([animals](../../bw1-notes/animals.md#reactions)) |
| Animals look at a death and at someone fainting | n/a | Our wiki differs: no animal reacts to a death or a fainting ([animals](../../bw1-notes/animals.md#reactions)) |
| Animals chase a ball (unconfirmed) | n/a | Our wiki differs: no animal reacts to a ball ([animals](../../bw1-notes/animals.md#reactions)) |

## Hand, physics and scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks up the animals the table allows; held, they play their in-hand clip | done | by the species' `playerCanPickUp`, leaving its flock, with the in-hand clip (`src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp`); see ../hand/picking_up.md |
| Thrown animals fly, land and play their landed clip, hurt by a hard landing | done | `FLYING`, the landing pose, heading and clip, hurt by a hard landing (`src/ECS/LivingPhysics.cpp`, `src/ECS/AnimalAI.cpp`); see ../physics/thrown_living.md |
| Animals drown in deep water | done | an animal in the sea sinks and is deleted (`src/ECS/LivingPhysics.cpp`) |
| Hurting or helping an animal moves the player's alignment (kind or nasty) | done | a miracle's effect on an animal moves its caster's alignment (`alignment::Update`, `src/ECS/Effects/Alignment.cpp`) |
| Scripts take control of an animal, play its clips and let it go | done | the script states, MOVE_GAME_THING and the release (`src/ECS/AnimalScript.cpp`, `src/ECS/ScriptHeld.cpp`) |

## Death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An animal with no life left falls dying, playing its kind's dying clip once, then lies in its dead clip | done | `animal_ai::DestroyedByEffect` and the dying and dead clips (`src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp`) |
| A dead animal lies 600 turns and one more, then goes | done | the shared corpse counter (`living::DeadTick`), 600 turns |
| Killed again, its body only lies its full time afresh | done | `animal_ai::DestroyedByEffect` restarts the corpse's time |
| It goes in a puff of grey smoke | done | the carcass's smoke (`src/ECS/DisappearSmoke.cpp`, from `src/ECS/AnimalAI.cpp`) |
| Miracles hurt and heal animals as other living things | done | `effects::ApplyEffect` hurts and heals them through `ecs::life` (`src/ECS/Effects/EffectValues.cpp`) |
| Fire burns animals to death | done | the fire's damage and `fire::traits::DestroyedByEffect` to dying (`src/ECS/Fire/FireObjectTraits.cpp`); see ../physics/fire.md |
| A dead animal is food for hunters and the creature (unconfirmed) | partial | predators pounce on and eat the animals and villagers they bring down (`src/ECS/AnimalPredators.cpp`); the creature's eating of carcasses: see ../creature/ |
