# Creature appearance

A creature's body shows what it has become. Each species has a base mesh and a mesh at either end of three axes: evil
to good, thin to fat and weak to strong. The body is drawn as the base pulled towards one mesh on each axis, and its
skin is blended towards the evil or good skin, so an evil creature grows dark and twisted and a good one bright and
noble.

**Progress: 31/32 done, 0 chosen, 1 partial, 0 inferred — 99%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/body.md).

## The body's shape

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species has a base mesh and evil, good, thin, fat, weak and strong meshes | done | `creature_body` mesh names (`src/3D/CreatureBody.cpp`); test `NamesTheMeshOfEachAppearance` |
| The ogre's variants keep its base name | done | test `TheOgresVariantsKeepItsBaseName` |
| A species missing a variant mesh uses its base for that side | done | `creature_morph` (`src/Creature/CreatureMorph.cpp`); test `MissingMeshesFallBackOnTheBase` |
| The creature's alignment is the evil to good axis | done | test `AlignmentIsTheEvilGoodAxis` |
| Its fatness runs from thin to fat | done | test `FatnessRunsFromThinToFat` |
| Its strength, with its species' own strength counted in, runs from weak to strong | done | test `StrengthCountsFourTimesTheSpecies` |
| Each axis pulls every vertex and normal from the base towards the mesh on its side | done | drawn by `src/Graphics/CreatureDraw.cpp`; tests `EachAxisPullsTowardsTheMeshOnItsSide`, `BlendMovesTheBaseTowardsEachMesh`, `CreatureDrawBodies.TheShapeIsBlendedTowardsTheLoadedMeshesByHowFarAlongEachAxis` |
| The fatness the body shows follows the creature's fatness by at most a hundredth a turn, last in the creature turn (after the actions), kept within 0 and 1 | done | `CreaturePhysiologySystem::ProcessShownFatness` (`shownFatness`); tests `ShownFatnessFollowsByAHundredthATurn`, `ShownFatnessStaysWithinNoneAndFull`, `TheFatnessShownStepsAfterTheTurnTowardsTheFatness` |
| The body is only redrawn once an axis has moved far enough | done | test `SmallChangesDoNotRedrawTheBody` |
| A new alignment redraws every axis; a new fatness the fatness and strength; a new strength only the vertices | done | tests `ANewAlignmentRedrawsEveryAxis`, `ANewFatnessRedrawsFatnessAndStrength`, `ANewStrengthOnlyReshapesTheVertices` |
| The rest pose and the animations follow the evil to good and thin to fat axes | done | see [animation.md](animation.md); tests `CreatureAnimation.TranslationsBlendLinearlyOnBothAxes`, `CreatureAnimation.TheRestPoseBlendsAsTheBody` |

## The skin

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each skin of the base mesh is blended towards the matching skin of the evil or good mesh | done | `creature_skin` in `CreatureSkinSystem`; tests `SkinsArePairedByTheirPlaceInTheList`, `WeightIsTheAxisIn256StepsTruncated` |
| A neutral creature shows its base skins | done | test `NeutralShowsNoVariant` |
| The blend is done a 4-bit channel at a time in whole steps, as the game does | done | tests `ChannelIsAWeightedMeanRoundedDown`, `TexelBlendsEachChannelOnItsOwn`, `WeightIsCappedAt255`, `HandMorph.SkinBlendIsTheCreatureSkinsBlend` |
| The skin is repainted only when the alignment drawn, the tattoos or the marks change | done | `CreatureSkinSystem` revisions; tests `CreatureSkinSystemTest.EachChangeBumpsItsRevision`, `CreatureDrawSkins.ASkinIsUploadedOnceUntilItIsPaintedAgain` |

## How the alignment moves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature has its own alignment, separate from its player's | done | `components::Creature::alignment` (`src/ECS/Components/Creature.h`); see [../worship](../worship/) for the player's; test `Alignment.ACreaturesAlignmentAndItsPlayersMoveApart` |
| Its alignment moves by at most its species' change a turn | done | `effects::alignment::ProcessCreatures` and `StepPending` (`src/ECS/Effects/Alignment.cpp`), at the end of the creature loop; test `ThePlanOfACreatureMovesItsAlignmentEachTurn`; the exe's add and step at the end of the creature's turn, read by disassembly ([creature alignment](../../../magic/influence-and-alignment.md#creature-alignment)) |
| The miracles it casts move its alignment by what they do to the world | done | a miracle a creature casts is paid with the creature as its applier (`spell_event::GetPaidEffectValues`, `src/Magic/Core/SpellEvent.cpp`: `appliedByCreature` from the spell's creator), and the last step of `effects::ApplyEffect` (`src/ECS/Effects/EffectValues.cpp`) moves that creature's own pending alignment by the life the thing lost or won (`alignment::Update`), not its player's, as the exe's `Object::ApplyEffect` 0x637B82..0x637C05 ([whose alignment](../../../magic/influence-and-alignment.md#the-original)); the creatures' turn steps it into the alignment (`alignment::ProcessCreatures`, from `CreatureLoop.cpp`); test `CreatureSpellHooksTest.ACreatureThatHarmsAThingMovesItsOwnAlignment` (read 2026-11-14) |
| What it does moves its alignment: the plan it carries out by its desire's per-turn change, an eaten villager by its kind's weight, damped by its own alignment | done | `creature_deed_alignment` (`src/Creature/CreatureDeedAlignment.cpp`), as the exe's add ([creature alignment](../../../magic/influence-and-alignment.md#creature-alignment)); tests `CreatureDeedAlignment.TheDesireOfThePlanGivesTheChange`, `AnEatenVillagerReplacesTheChangeOfTheDesire`, `TheAlignmentOfTheCreatureChangesTheChange`; the miracles it casts are the row above |
| The nice and nasty spells pull its alignment for a while and put it back | done | `creature_spells` nice and nasty (`src/Creature/CreatureSpells.cpp`, run by `src/Magic/Spells/SpellCreature.cpp`); tests `CreatureSpells.ASpellEasesInHoldsForItsTimeAndEasesOut`, `ItIsSavedAsItWasBeforeItsSpells`; see [../miracles](../miracles/) |
| The aggression and compassion leashes don't change how it looks, only how it feels | done | see [leash.md](leash.md); test `LeashSystemTest.TheCompassionAndAggressionLeashesLeaveHowItLooksAlone` |
| Scripts can read and set alignments | done | `GET_ALIGNMENT` and `SET_ALIGNMENT` work on players' alignments (`src/CHLApi.cpp`); a creature's ALIGNMENT property is read and set (`ecs::player_creature::GetCreatureProperty`, `SetCreatureProperty`; test `PlayerCreature.TheCreaturePropertiesAreStrengthAlignmentAndTheBodys`), and a set is followed by the body at once, as the exe's: the shown fatness takes a turn's step and the shape is drawn anew (test `CreaturePropertyTest.ANewAlignmentStepsTheShownFatnessAndRedrawsTheBodyAtOnce`; debug: the Creature Spawner's drawn shape and shown fatness readout); every ALIGNMENT property in the shipped scripts is a creature's, so the property of anything else is not in the game ([not in the original](../../not-in-the-original.md)) |
| The alignment is kept in creature files | done | read by `src/Creature/CreatureMindFileBody.cpp` and written by `src/Creature/CreatureMindModel.cpp`; test `test/test_creature_mind_file_body.cpp` (`CreatureMindFileBody.TakesTheBodyAFileKeeps`) |

## Drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is drawn with the land's lighting | done | `src/Graphics/RendererCreature.cpp`; its body lit by the posed models' own rule, as the original's animated models, not its eyes nor its reflection (`graphics::submesh_draw::CreatureBodyPosedLight`, test `SubMeshDraw.ACreaturesBodyTakesThePosedModelsLightButNotInItsReflection`; from raffclar's #695); see [../rendering](../rendering/) and [model lighting](../../../rendering/model-lighting.md) |
| A creature casts a soft shadow of its silhouette onto the land and what stands on it, from the scene's light, kept at least 45 degrees up | done | a complex caster in `src/Graphics/ShadowList.cpp` with `shadow_math::LightCreature`; tests `CreatureShadow.TheLightIsKeptAtLeast45DegreesUp`, `ShadowMath.Lights` |
| The shadow fades out as the camera pulls away from the creature | done | the projected shadows' fade in `src/Graphics/ShadowList.cpp`; tests `ShadowMath.FadeByDistance`, `ShadowMath.Alphas` |
| The hair casts no shadow | done | the shadow is cast from the body mesh only (`src/Graphics/ShadowList.cpp`) |
| A frozen creature looks icy, a dark blue sheen growing over it as it freezes | done | the freeze is kept; the body and its eyes are drawn in the exe's frozen tint (`creature_spells::FrozenTint`, the land's light hazed times it, as the tint after the haze; `creature_draw::FrozenLidColour`; tests `CreatureSpells.TheFrozenLookTintsTowardsDarkBlue`, `CreatureDrawEyes.AFrozenEyelidIsItsColourTimesTheFrozenTint`), and the hair is hidden from a fifth frozen (`creature_fizz_look::HairShown`), as the exe; the ice sheen over the body and the eyes is drawn (`Renderer::DrawFrozenSheen`, `creature_frozen_sheen`, tests `CreatureFrozenSheen.*`, `CreatureDrawFrozen.*`); its seams are not blended while frozen (`creature_fizz_look::SeamsBlended`, test `CreatureFizzLook.AFrozenBodysSeamsAreNotBlended`); the ice's depth test lets the equal depth through, as the original's LESSEQUAL, so it shows over the body (`creature_draw::FrozenSheenPass`; it had shown nowhere, the user's joint602 play) ([drawing](../../../creature/drawing.md#the-frozen-look)) |
| An invisible creature dissolves into static | done | a fizzing body, its eyes and its hair are drawn as the exe draws them: two passes through the static (`Renderer::DrawCreatureBody`, `DrawFizzed`, fs_object_creature.sc; `creature_draw::FizzPasses`, tests in `test/creature/test_creature_draw.cpp`), and its reflection is left out from a fizz of 0.2 (`sea_pass::ReflectsCreature`); it is drawn at its place in the land's draw list, so solid things after it there do not show through its specks (`object_order::SegmentRows`, test `ObjectOrder.AFizzingCreatureIsDrawnOnItsOwnAtItsPlaceInTheList`; from raffclar's #572) ([the fizz](../../../creature/drawing.md#the-fizz-in-openblack)) |
| Species-specific extras: eyes, eyelids and hair tufts | done | see [face_eyes_hair.md](face_eyes_hair.md); tests `MorphCreatureBlock.Version21HasActionPointsEyesTattooSitesAndBank`, `CreatureEyes.ThePointIsOnTheTriangleAndItsNormalPointsIn`, `CreatureDrawEyes.EachEyeballComesBeforeItsEyelidInTheSkinsColour`, `CreatureHair.StrandsGrowStraightOutOfTheSurface` |
| Each player's creature can be told apart by its tattoos, each in the colour chosen for it | partial 60% | tattoos are painted per creature, the local player's creature's on the local player's creatures and on all inside the temple (`creature_skin::ShowsLocalCreaturesTattoos`; test `CreatureSkin.TheLocalPlayersCreaturesAndAllInsideTheTempleShowTheLocalCreaturesTattoos`), as `LH3DCreature::MorphTexture` 0x48D540 (bw1-decomp `Creature3D.cpp:6179`); there is no colour by player on the body: `GTattoo::Draw` 0x5DF310 takes the tattoo's own colour bytes; missing: the mask taken from the creature's player's cell ([0xD19B28] by player times 8 plus slot in `GTattoo::Draw`) and `MorphTexture`'s leader-interface gate are not compared with ours; see [creature_tattoos.md](creature_tattoos.md) (read 2026-11-05) |
| A creature carrying a glow from its player's creed (the hand's glow) when a script sets it | done | `SET_CREATURE_CREED_PROPERTIES` gives the glows (`components::CreatureHandGlows`) and the hum; each is drawn as the glowing shell over the creed model where the body is drawn (`creature_hand_glow::PlaceOn`, `Renderer::DrawCreedGlow`; tests `CreatureCreedGlowDraw.*`) ([the Creed glows](../../../creature/drawing.md#the-creed-glows-in-openblack)) |
