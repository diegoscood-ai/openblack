# Hand sounds

Every sound the god hand makes or sets off: gripping the land and the sea, picking things up and putting them down,
tapping, holding and shaking off miracles, crossing influence borders, and the voices of the villages answering what the
hand does. The sound system itself (banks, 3D placement, music) is in [../audio/](../audio/).

**Progress: 22/38 done, 4 partial — 63%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Gripping the land and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gripping the land plays one of six land-grab sounds, picked at random | done | `HandSystem::GripLandSound` (`HandFish.cpp`): a random one of the six G_HandGrabLand samples, from `HandPlacement.cpp` at the start of a land grip |
| The land-grab sound is heard flat, not placed in the world | done | `HandSystem::GripLandSound`: a sound tag, not 3D |
| Gripping the sea plays the ten water sounds one after another, in turn | done | `HandSystem::SplashHand`: InGame 99 plus `audio::Counter::HandInWater` (0..9 in turn) |
| The water sound is placed on the water's surface where the hand went in | done | `HandSystem::SplashHand`: 3D at (x, 0.2, z) where the hand went in |
| Gripping is silent while a script holds the cinema bars | partial | `HandPlacement.cpp`: no land-grip dust or sound while a script's widescreen is on; the water splash is only held back while paused |

## Picking up and putting down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking an object up plays the pick-up sound | done | `HandSystem::GenericPickupSounds` (`HandHolding.cpp`): a G_PickUpObject (10) tag at the object, not for a rooted tree or forest; a villager also screams |
| Taking food from a store or field plays the food pick-up sound | done | `HandEffects.cpp`: G_PickUpFood (44) looping while scooping food (piles, fields, fish farms), its pitch rising with the scoop (60 + 180 t^2) |
| Taking wood from a store or forest plays the wood pick-up sound | done | `HandEffects.cpp`: G_PickUpWood (98) for a wood pile, the same way |
| Food or wood let fall from the hand lands with a thud sized by the amount: a small or a big pile sound | done | A hand pot put down becomes a pile through `pot_resource::AddResourceToPos`, which plays `pot_resource::PlayPileSound` (small or big by the amount, `src/ECS/PotResource.cpp`) |
| Putting a tree back in the ground plays a planting sound, one of three | done | `HandPhysics.cpp`: a tree landed and replanted plays G_PlantTree_01 + tick count % 3 at the tree |
| Pulling a tree out of the ground plays a tree breaking sound | done | `HandHolding.cpp`: uprooting plays a random one of three G_TreeBreak samples at the tree; see [tug.md](tug.md) |
| A scaffold taken into the hand plays its appearing sound, and its vanishing sound when it leaves | todo | Not found in our tree (scaffolds have tap, combine and plant sounds only) |
| Something thrown at another thing plays an impact sound where it hits | done | `src/ECS/Physics/CollisionSounds.cpp`: the collision sound by the surfaces hit (ground, water, fragment, objects) |
| A thrown rock or fireball whooshing past the camera plays one of five rock or fireball whooshes | done | `PhysicsObjects.cpp`: a body entering 10 m of the camera faster than 20 m/s plays one of G_RockPast_01..05; the fireball's own whoosh is in `src/Particles/Rules/Fireball.cpp` |
| Dropping an animal from a height squashes it with one of four squash sounds | todo | (unconfirmed it is the hand's drop that plays it) |

## Tapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Knocking on houses, tapping rocks and scaffolds, opening reward chests | partial | Houses (`abodes::InterfaceTap`), rocks (`Rocks::Tap`) and scaffolds (`scaffolds::Tap`) play their tap sounds; no reward chests. See [clicking_and_activating.md](clicking_and_activating.md) |
| Tapping a one-shot miracle bubble pops it | done | `magic::one_off::InterfaceTap` (`src/Magic/Core/OneOffSpellSeed.cpp`): G_SpellBubblePop_04 at the hand |
| Tapping a leash post clicks | todo | No leash posts in our tree |
| Opening a reward chest plays the chest and reward stings | todo | No reward chests |
| Tapping a script's highlighted marker plays its chime | done | `ecs::script_highlight::InterfaceTap`: its InGame chime for a "Did you know?" tapped by the local player |

## Miracles in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bands flying onto the hand play the power-up band sound | done | `hand_fx::AddSpellToHandVisuals`: G_SpellPowerUpBand |
| A held miracle hums in a loop that follows the hand | partial | Our tree loops G_HandGesture_02 at the hand while a hand-gesture seed is armed with the button down (`HandSystem::BeginApplyOnRelease`); no other hold loop found |
| Shaking a miracle off plays the shake sound | done | `hand_fx::RemoveHandSpellVisuals`: G_ShakeHand_01 |
| A miracle that can't be cast where it is let go plays the failure sound | done | `HandSpellSeed.cpp`: G_SpellCastFailure on a failed apply |
| The announcer names the power-up the hand has reached | done | `seed::SetPowerUp`: SpellDialogue 10, 11 or 12 |
| A recognised gesture plays its sound | done | The recognised gesture's sound (2D for the local interface), see [audio.md](../../bw1-notes/audio.md#the-miracles-on-channels-in-openblack); see [../gesture/gesture_effects.md](../gesture/gesture_effects.md) |

## Influence and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand crossing an influence border plays a sound once, at the hand, while the game runs | done | `influence::ProcessHandCrossing` (`src/ECS/Influence/InfluenceCircles.cpp`): G_HandThroughInfluence_01 at the hand, not while paused |
| Moving a village totem plays its grinding sound | todo | No totem in our tree; see [totem.md](totem.md) |
| The totem's raising and lowering are followed by the guidance sounds while it moves | todo | `audio::guidance::StartTotemRaiseSound` exists (`src/Audio/Services/Guidance.cpp`) but there is no totem to call it |
| The local player hears a heartbeat as the guidance's warning | done | `audio::guidance::UpdateHeartBeat` (`src/Audio/Services/Guidance.cpp`), at the citadel; see [../audio/](../audio/) |

## The villages answering the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A deed of the hand that impresses a village sets off a group cheer, louder the more the player stands out from the other gods | todo | Not found in our tree; see [../worship/](../worship/) |
| A deed that loses belief sets off a group groan, by the same measure | todo | Not found in our tree |
| Making a disciple has the village call out the job given, at most so often | partial | `audio::guidance::PlayDiscipleRemark` exists but nothing calls it (dormant); see [../villager/](../villager/) |
| Dropping food or wood into a town makes the town answer, by how much it had: plenty, some or little | todo | `audio::guidance::PlayBeliefRemark` exists, but its call from the town stores is pending (`src/ECS/Town/TownStores.cpp`); see [../town/](../town/) |
| A villager dying in the hand or by its throw sets off the death cries | todo | Not found in our tree; see [../villager/](../villager/) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroking and slapping the creature has it make the sounds of its reaction | done | `CreatureAudioSystem` plays the sounds of the reaction animations; see [../creature/](../creature/) |
| A glow on the creature's hands carries a looping glow sound | todo | The creed glow sound actions are named in `src/Creature/CreatureAudio.cpp` but never played; see [hand_effects_and_glows.md](hand_effects_and_glows.md) |
| Giving the creature something plays the sound of it taking it | todo | Giving the creature something is not ported; see [creature_contact.md](creature_contact.md) |
