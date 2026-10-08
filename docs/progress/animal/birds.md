# Birds

The flying animals: crows, doves, swallows, pigeons, seagulls, bats and vultures, flying in flocks over the land, and
the doves or bats that circle each temple. The flock miracle's doves and bats are in `../miracles/`.

**Progress: 11/15 done, 2 partial — 80%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../bw1-notes/animals.md), [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make flocks of birds and the birds in them | done | CREATE_FLOCK, CREATE_ANIMAL and CREATE_NEW_ANIMAL (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/AnimalArchetype.cpp`) |
| Each kind has its own models, clips (fly, stand, eat, sleep, thrown, dead) and table values | done | the table values and each kind's clips, flapping or gliding at random (`BirdClip`, `src/ECS/AnimalAnimations.cpp`) |
| A bird is born at a random age and sized for it | done | the random age and the scale for it (`src/ECS/AnimalAI.cpp`); `test/test_animal_age.cpp` |
| Doves circle a good player's temple, bats an evil one's, changing as the alignment changes (unconfirmed exact rule) | todo | the temple's dove and bat kinds are in the tables only |
| Seagulls keep to the coast (unconfirmed) | n/a | Our wiki differs: every bird kind flies the same legs from where it is; nothing keeps seagulls to the coast ([animals](../../bw1-notes/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)) |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flock's leader flies legs about where the flock lives, between its kind's inner radius and the flock's reach | done | the leader's legs of up to the domain radius from where it is (`src/ECS/AnimalBirds.cpp`) |
| Followers fly to points near the leader, then keep a formation behind it by their place in the flock | done | the followers' point near the leader and their formation slot (`src/ECS/AnimalBirds.cpp`) |
| Birds keep a height above the land within their kind's band, climbing slowly | done | the height band, the slow climb and never under 2 m (`src/ECS/AnimalBirds.cpp`) |
| Birds turn by at most their kind's turn angle and bank into turns | done | the turn angle and the banking through the drawn matrix (`src/ECS/AnimalBirds.cpp`, `src/ECS/MobileDrawing.cpp`) |
| Birds land to eat and to sleep, on the ground or on objects, and take off again | n/a | Our wiki differs: birds never land in this version of the original, as every bird's sleep value is 0 ([animals](../../bw1-notes/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)) |
| Flocks of the same kind can merge, and a small flock joins a larger one | partial | the flock merge is ported for the animals' interact decision (`LookForFlocksInSpiral`, `src/ECS/AnimalAI.cpp`); whether a bird flock reaches it was not checked |
| Birds sleep at night | n/a | Our wiki differs: birds have no day and night; the bats fly by day ([animals](../../bw1-notes/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)) |

## Birds and the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bats and vultures frighten creatures | todo | no creature fear of bats or vultures was found |
| Birds look at and flee from miracles and the hand (unconfirmed which kinds) | n/a | Our wiki differs: birds take no reactions ([animals](../../bw1-notes/animals.md#reactions)) |
| A bird killed falls out of the sky | done | a dying bird goes into the physics with its flight speed along its heading (`BirdDying`, `src/ECS/AnimalBirds.cpp`) |
| A dead bird lies its time, then goes in a puff of grey smoke | done | the corpse's time and the carcass's smoke, as every animal (`src/ECS/AnimalAI.cpp`, `src/ECS/DisappearSmoke.cpp`) |
| Tornadoes carry birds off | done | the tornado carries any animal (`src/Particles/Rules/Storm.cpp`) |
| The hand can pick up birds the table allows, and throw them | done | the hand takes only what the table allows (`playerCanPickUp`). Our wiki differs: no bird's table entry allows it, so the hand never holds a bird ([animals](../../bw1-notes/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)) |
| The creature watches birds, and may catch and eat them (unconfirmed) | partial | the creature's eye is caught by animals; eating birds: see `../creature/` |
