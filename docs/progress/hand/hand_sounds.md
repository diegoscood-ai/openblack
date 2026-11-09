# Hand sounds

Every sound the god hand makes or sets off: gripping the land and the sea, picking things up and putting them down,
tapping, holding and shaking off miracles, crossing influence borders, and the voices of the villages answering what the
hand does. The sound system itself (banks, 3D placement, music) is in [../audio/](../audio/).

**Progress: 30/37 done, 0 chosen, 2 partial, 4 inferred — 85%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Gripping the land and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gripping the land plays one of six land-grab sounds, picked at random | done | `HandSystem::GripLandSound` (`HandFish.cpp`): a random one of the six G_HandGrabLand samples, from `HandPlacement.cpp` at the start of a land grip |
| The land-grab sound is heard flat, not placed in the world | done | `HandSystem::GripLandSound`: a sound tag, not 3D |
| Gripping the sea plays the ten water sounds one after another, in turn | done | `HandSystem::SplashHand`: InGame 99 plus `audio::Counter::HandInWater` (0..9 in turn) |
| The water sound is placed on the water's surface where the hand went in | done | `HandSystem::SplashHand`: 3D at (x, 0.2, z) where the hand went in |
| Gripping the land is silent, and throws no dust, while a script holds the cinema bars; the sea's splash still plays | inferred | `HandPlacement.cpp`: no land-grip dust or sound while a script's widescreen is on; the water splash is held back only while paused. The widescreen test is not in the wiki's reading of the original's grip start ([page](../../../hand/picking.md)): unverified |

## Picking up and putting down

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking an object up plays the pick-up sound | done | `HandSystem::GenericPickupSounds` (`HandHolding.cpp`): a G_PickUpObject (10) tag at the object, not for a rooted tree or forest; a villager also screams |
| Taking food from a store or field plays the food pick-up sound | done | `HandEffects.cpp`: G_PickUpFood (44) looping while scooping food (piles, fields, fish farms), its pitch rising with the scoop (60 + 180 t^2) |
| Taking wood from a store or forest plays the wood pick-up sound | done | `HandEffects.cpp`: G_PickUpWood (98) for a wood pile, the same way |
| Food or wood let fall from the hand lands with a thud sized by the amount: a small or a big pile sound | done | A hand pot put down becomes a pile through `pot_resource::AddResourceToPos`, which plays `pot_resource::PlayPileSound` (small or big by the amount, `src/ECS/PotResource.cpp`) |
| Putting a tree back in the ground plays a planting sound, one of three | done | `HandPhysics.cpp`: a tree landed and replanted plays G_PlantTree_01 + tick count % 3 at the tree |
| Pulling a tree out of the ground plays a tree breaking sound | done | `HandHolding.cpp`: uprooting plays a random one of three G_TreeBreak samples at the tree; see [tug.md](tug.md) |
| A scaffold taken into the hand plays its appearing sound, and its vanishing sound when it leaves | done | While the local hand holds a scaffold whose plan offers a building, each drawn frame plays its phantom building's appear sound once as it fades in (pitched by the building) and its disappear sound once as it fades out (`scaffolds::PhantomSoundOf`, from `Game.cpp`); test `test_scaffold_phantom_sounds.cpp` |
| Something thrown at another thing plays an impact sound where it hits | done | `src/ECS/Systems/Implementations/DynamicsSystem.cpp`: the collision sound by the surfaces hit (ground, water, fragment, objects) |
| A thrown rock or fireball whooshing past the camera plays one of five rock or fireball whooshes | done | `PhysicsObjects.cpp`: a body entering 10 m of the camera faster than 20 m/s plays one of G_RockPast_01..05; the fireball's own whoosh is in `src/Particles/Rules/Fireball.cpp` |

## Tapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Knocking on houses, tapping rocks and scaffolds, opening reward chests | done | Houses (`AbodeKnockSystem::Tap`), rocks (`object_physics::TapRock`) and scaffolds (`scaffolds::Tap`) play their tap sounds; a reward chest plays its tap, sample 173 of the in-game bank (`RewardSystem`). See [clicking_and_activating.md](clicking_and_activating.md) |
| Tapping a one-shot miracle bubble pops it | done | `magic::one_off::InterfaceTap` (`src/Magic/Core/OneOffSpellSeed.cpp`): G_SpellBubblePop_04 at the hand |
| Tapping a leash post clicks | done | `worship::TempleLeash` tap (`src/Worship/TempleLeash.cpp`): the in-game bank's click (`leash_posts::k_TapSample`), 2D, when `leash_posts::Tap` says so ([leash posts](../../../creature/temple-leash-posts.md)); tests in `test/temple/test_leash_posts.cpp` |
| Opening a reward chest plays the chest and reward stings | done | The opening plays samples 40 and 41 of the in-game bank, 2D, heard wherever the camera is (`RewardSystem`) |
| Tapping a script's highlighted marker plays its chime | done | `ecs::script_highlight::InterfaceTap`: its InGame chime for a "Did you know?" tapped by the local player |

## Miracles in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Bands flying onto the hand play the power-up band sound | done | `hand_fx::AddSpellToHandVisuals`: G_SpellPowerUpBand; test `MiracleFxSystemTest.APowerUpPlaysTheBandsSoundThenNamesTheLevel` |
| A held miracle sounds while it is in the hand: its in-hand effect's own sounds, and the hand-gesture loop while an armed seed is pressed | partial 50% | G_HandGesture_02 loops in 3D at the hand while a seed is armed with the button down and ends on release (`HandSystem::BeginApplyOnRelease`), as the original ([in game](../../../audio/magic-and-weather-sounds.md#in-game)); missing: the in-hand effect's own sounds (the lightning's, the fire's loop) are not checked to be heard from the effect drawn in the hand |
| Shaking a miracle off plays the shake sound | done | `hand_fx::RemoveHandSpellVisuals`: G_ShakeHand_01; test `MiracleFxSystemTest.AMiracleShakenOffPlaysTheShakeSound` |
| A miracle that can't be cast where it is let go plays the failure sound | done | `HandSpellSeed.cpp`: G_SpellCastFailure on a failed apply |
| The announcer names the power-up the hand has reached | done | `seed::SetPowerUp`: SpellDialogue 10, 11 or 12 |
| A recognised gesture plays its sound | done | The recognised gesture's sound (2D for the local interface), see [audio.md](../../../audio/magic-and-weather-sounds.md#the-miracles-on-channels-in-openblack); see [../gesture/gesture_effects.md](../gesture/gesture_effects.md) |

## Influence and worship

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand crossing an influence border plays a sound once, at the hand, while the game runs | done | `influence::ProcessHandCrossing` (`src/ECS/Influence/InfluenceCircles.cpp`): G_HandThroughInfluence_01 at the hand, not while paused; tests `UiSfxTest.InfluenceCrossing*` (`test/audio/test_ui_sfx.cpp`) |
| A village totem let go eases to its new share with its rising sound, and a stop sound as it arrives; sliding it in the hand plays none | done | the let go's share reaches `worship::percentage::SetWorshipPercentage`, whose `SetTotemPercentage` tags the rising loop and `ProcessTotem` stops it with the stop sound at the arrival (`src/Worship/WorshipPercentage.cpp`, tests `VillageTotemEase.*` and `test/audio/test_sound_tag_channels.cpp`; the debug bar's Audio window shows each totem's tag); the hand's slide only sets the share it holds ([worship](../../../magic/worship.md), [the village totem](../../../magic/village-totem.md), [worship totem](../../../audio/world-sounds.md)); see [totem.md](totem.md) |
| The local player hears a heartbeat as the guidance's warning | done | `audio::guidance::UpdateHeartBeat` (`src/Audio/Services/Guidance.cpp`), at the citadel; see [../audio/](../audio/) |

## The villages answering the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A deed of the hand that impresses a village sets off a group cheer, louder the more the player stands out from the other gods | inferred | Not found in our tree, nor in the wiki's reading of the original's guidance sounds ([voices](../../../audio/voices.md)): unverified that the original has it; see [../worship/](../worship/) |
| A deed that loses belief sets off a group groan, by the same measure | inferred | Not found in our tree, nor in the wiki: unverified that the original has it |
| Making a disciple has the village call out the job given, at most so often | partial 80% | `audio::guidance::PlayDiscipleRemark` plays when a villager whose disciple changed in the hand is put down by the local player's let-go (`villager::DisciplePutDownByHand`, `physics::from_hand::IsMyLetGo`); the original calls it from the hand's let-go too, 2D at volume 85 from its table ([voices](../../../audio/voices.md)); missing: a test of the remark and its table; see [throwing.md](throwing.md) and [../villager/](../villager/) |
| Dropping food or wood near a town makes the town answer, pleased or displeased by how much it already has | done | `audio::guidance::PlayResourceDropRemark` from a pot's resource put down (`src/ECS/PotResource.cpp`) and a delivery (`src/ECS/ObjectDelivery.cpp`), the nearest town within 100; the original's two callers are the same ([voices](../../../audio/voices.md)); tests `GuidanceTest.ResourceDropSamples` and the remark's tests in `test/audio/test_guidance.cpp`; see [../town/](../town/) |
| Adding to a town's belief, a god who is not the town's most believed hears the people's awe, picked by its share of the strongest belief and heard only near enough | done | `audio::guidance::PlayBeliefRemark` from the town stores (`town_stores` belief sound, `src/ECS/Town/TownStores.cpp`), at the thing, by the distance from the hand's interface point ([voices](../../../audio/voices.md)); tests in `test/audio/test_guidance.cpp` |
| A villager dying in the hand or by its throw sets off the death cries | inferred | Not found in our tree, nor in the wiki's reading of the hand: unverified that the original ties death cries to the hand; see [../villager/](../villager/) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Stroking and slapping the creature has it make the sounds of its reaction | done | `CreatureAudioSystem` plays the sounds of the reaction animations; see [../creature/](../creature/) |
| A glow on the creature's hands carries a looping glow sound | done | the creed powers a script gives start and stop the hum (`CreatureAnimationSystem::SetCreedPower`, sample 166); the creed glow actions of the animations' moments set glows and make no sound, as the original (`CreatureAnimationSystem::PlayCreedEvent`); see [hand_effects_and_glows.md](hand_effects_and_glows.md) |
| Giving the creature something plays the sound of it taking it | pending | Giving the creature something is not ported; see [creature_contact.md](creature_contact.md) |
