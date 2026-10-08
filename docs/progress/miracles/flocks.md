# Flocks

Two flock miracles are swept out of the hand. The flying flock makes doves for a good god or bats for an evil one; they
fly off together, leave a trail and frighten creatures (bats). The ground flock makes a pack of wolves that run down a
corridor, hunting and eating villagers in their way. Wild animals and livestock are in [../animal/](../animal/).

**Progress: 46/50 done, 1 partial — 93%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Flying flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Doves for a caster whose alignment is good enough, bats otherwise | done | `src/Magic/Spells/SpellFlock.cpp` (the caster's alignment under the switch, -0.2: bats); test `SpellFlock.goodOrEvil` |
| Twelve birds times tribal power, rounded half to even | done | `src/Magic/Spells/SpellFlock.cpp` (numberToCreate x tribal power, rounded to nearest even); test `SpellFlock.numberToCreate` |
| Birds are made twelve a second along the hand's sweep, their height eased along it, each nudged a little at random | done | `src/Magic/Spells/SpellFlock.cpp` (the exit loop: twelve a second along the last point to the hand, height interpolated, a 0.1 m jitter); test `SpellFlock.spawnPoint` |
| Birds that would start off the map, or outside the human caster's influence, are skipped but still counted | done | `src/Magic/Spells/SpellFlock.cpp` (off the map, or a human caster with no influence there: skipped, still counted) |
| Birds keep being made until all are, even after the miracle is over | done | `src/Magic/Spells/SpellFlock.cpp` (the exit loop runs while fewer are made than wanted) |
| They fan out across the camera's heading (or the creature's throw), side chosen by which way the hand swept | done | `src/Magic/Spells/SpellFlock.cpp` (the destination: the camera's heading for a human, side from the sweep, angle 2 x made x side / N); test `SpellFlock.fan` |
| Each flies for a point up to 800 m away in whole map cells, halving the distance down to 12.5 m until it is on the map | done | `src/Magic/Spells/SpellFlock.cpp` (whole cells, halved down to 12.5 m until on the map); test `SpellFlock.destination` |
| Birds fly at their kind's normal height; followers take the leader's goal | done | `src/Magic/Spells/SpellFlock.cpp` (the leader flies at its kind's normal height; the others take the leader's goal) |
| Every bird is the same size, 2.8 to 3.0 times the model, whatever its birth size | done | `src/Magic/Spells/SpellFlock.cpp` (2.8 plus up to 0.2 times the model's scale) |
| A bird faces its goal from where it actually appeared | done | `src/Magic/Spells/SpellFlock.cpp` (the angle from the jittered birth point to the goal) |
| The local caster's hand leaves a trail of sparkles (good) or smoke (evil) until all birds are made | done | `src/Magic/Spells/SpellFlock.cpp` (the cast effect follows the local hand and is stepped every frame, closed when all are made) |

## Flying flock: the birds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader wanders from wherever it is now, so the flock roams away from the cast point | done | `src/Magic/Spells/SpellFlock.cpp` (the flock is placed where its leader is each turn), `src/ECS/AnimalBirds.cpp` |
| Each leg's height varies within the kind's limits | done | `src/ECS/AnimalBirds.cpp`, `src/ECS/AnimalAI.cpp` |
| Random wander points avoid places the map's walls block | done | `src/ECS/AnimalAI.cpp` (the random point is refused where it collides, `living::Collides` in `src/ECS/LivingPos.cpp`) |
| Birds bank into their turns and flap | done | `src/ECS/AnimalBirds.cpp`, `src/ECS/AnimalAnimations.cpp` |
| Birds are lit by the brightest land light | done | `src/ECS/Systems/Implementations/RenderingSystem.cpp` (the land light on objects) |
| A bird crossing a shield into it strikes the shield, sparks, and fades if the shield holds | done | `src/Magic/Spells/SpellFlock.cpp` (`psys::shields::FindShieldCrossedInto` with the bird's 2D radius, a shield-collide event, then the fade if the shield holds) |
| When the miracle ends every bird fades over 20 turns, but stays opaque until it vanishes | done | `src/Magic/Spells/SpellFlock.cpp` (the fade zoomer over 20 turns, the alpha only changes in the turn); test `SpellFlock.fade` |
| The miracle lasts 25 s for a player and costs 40 a turn while its birds or effect last | done | The tables' timer and cost per turn (`src/Magic/Spells/SpellFlock.cpp`); test `SpellFlock.realInfoDat`. Our wiki differs: from a hand seed the first 5000 prayer runs out at 40 a turn after 12.5 s, before the 25 s timer ([page](../../bw1-notes/miracles.md#pending--not-faithful)) |
| Bats frighten creatures | todo | Nothing in our tree asks whether an animal frightens a creature |

## Flying flock: effects and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each bird trails pale sparkles (doves) or black smoke (bats), sized to the bird | done | `src/Particles/Rules/Flock.cpp` (follow targets: a trail on each bird, scaled by it) |
| A trail stays where its bird was when the bird goes, and fades with the effect | done | `src/Particles/Rules/Flock.cpp` (a target that goes is forgotten, its atom stays) |
| One coo (doves) or screech (bats) per cast, not one per bird | done | `src/Particles/Rules/Flock.cpp` (one sound only, unless it is the collection's only atom) |

## Ground flock: casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Always wolves, whatever the caster's alignment | done | `src/Magic/Spells/SpellFlock.cpp` |
| Fourteen wolves times tribal power, rounded half to even | done | `src/Magic/Spells/SpellFlock.cpp`; test `SpellFlock.numberToCreate` |
| Made twelve a second along the sweep on the ground, skipped off the map or outside influence | done | `src/Magic/Spells/SpellFlock.cpp` (the ground exit loop) |
| A puff of magic appears where each wolf appears | done | `src/Magic/Spells/SpellFlock.cpp` (a magic-object-created spot visual at each wolf) |
| Every wolf is 1.5 to 2.0 times the model, whatever its birth size | done | `src/Magic/Spells/SpellFlock.cpp` (1.5 plus up to 0.5 times the model's scale) |
| Wolves are born grown up and hungry, owned by the caster | done | `src/Magic/Spells/SpellFlock.cpp` (made an adult, grown-up age plus one, with the info's hunger, owned by the caster) |

## Ground flock: running and hunting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack runs down a 45 m-wide corridor to its goal at the wolves' run speed | done | `src/Magic/Spells/SpellFlock.cpp` (the corridor's normal and 45 m half width, the run speed); test `SpellFlock.wolfCorridor` |
| Wolves reaching their goal fade away | done | `src/ECS/AnimalPredators.cpp` (within 30 m of the final goal, the fade) |
| A hungry wolf hunts villagers inside the corridor and not behind it | done | `src/ECS/AnimalPredators.cpp` (IsHuntingTargetValid: inside the corridor, not behind the wolf's cell); test `SpellFlock.wolfCorridor` |
| Once a wolf remembers prey, a new prey must be much closer to take its place | done | `src/ECS/AnimalPredators.cpp` (NearerThanStored) |
| It chases, pounces when close and facing, and gives up when the prey gets too far | partial | `src/ECS/AnimalPredators.cpp` (chase, stalk, pounce, abandon). Our wiki notes the spell wolves circle 30 to 50 m from the prey and seldom reach the leap; not checked against the game ([page](../../bw1-notes/miracles.md#pending--not-faithful)) |
| Within a metre of the prey the pounce brings it down, again each turn it stays that close | done | `src/ECS/AnimalPredators.cpp` (TargetPounce: within 1 m the villager is downed at 0.05 life) |
| The downed villager plays its attacked fall | done | `src/ECS/AnimalPredators.cpp` (ProcessDownedVillagers: the downed clip, then being eaten) |
| The villager is eaten over 300 turns and then dies the normal villager death, killed by an animal | done | `src/ECS/AnimalPredators.cpp` (300 turns, then the villager's death by an animal, `ecs::villager::VillagerDead`) |
| The wolf takes 15 to 24 mouthfuls, then runs on | done | `src/ECS/AnimalPredators.cpp` |
| Things that can't be eaten are never hunted | done | `src/ECS/AnimalPredators.cpp` (a thing that cannot be eaten, `script_held::CannotBeEaten`, is no prey) |
| A remembered prey that can no longer be reached is dropped | done | `src/ECS/AnimalPredators.cpp` (`ecs::villager::IsReachable` and the other validity checks) |

## Ground flock: look, sound and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wolves leave a land-tinted dust trail | done | `src/Particles/Rules/Flock.cpp` (the ground dust trail) |
| One howl per cast | done | `src/Particles/Rules/Flock.cpp` (one sound per cast) |
| Wolves bank into their turns and play the right clip in each state | done | `src/ECS/AnimalAI.cpp`, `src/ECS/AnimalAnimations.cpp` |
| When the miracle ends the wolves turn translucent white and fade over 2 s while running on | done | `src/Magic/Spells/SpellFlock.cpp` (the fade, the animal keeps moving), drawn white with its alpha by `src/ECS/Systems/Implementations/RenderingSystem.cpp` |
| A burning wolf is drawn charred with flames | done | `src/ECS/Systems/Implementations/RenderingSystem.cpp` (the spell wolf with a fire: the charring grey and the glow); flames from the fire graphics (`src/ECS/Fire/FireGraphic.cpp`) |
| The player can't pick up the miracle's wolves | done | `ecs::animal_ai::ValidForPlaceInHand` (the species' pick-up rule), used by `src/ECS/Systems/Implementations/HandPlacement.cpp` |
| The miracle lasts 60 s for a player and costs 35 a turn | done | The tables' timer and cost per turn; test `SpellFlock.realInfoDat`. Our wiki differs: from a hand seed the first 5000 prayer runs out at 35 a turn after 14.5 s, before the 60 s timer ([page](../../bw1-notes/miracles.md#pending--not-faithful)) |

## Both flocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The leader is a miracle event each turn with no effect on anything | done | `src/Magic/Spells/SpellFlock.cpp` (the leader's point event, reaction to an impressive miracle) |
| A running flock and its animals are kept in a saved game | todo | No game save system |
| A creature can cast either flock, throwing it the way it faces | todo | Creatures cast no miracles in our tree; the creature's direction rule is in `src/Magic/Spells/SpellFlock.cpp` but nothing calls it for a creature |
