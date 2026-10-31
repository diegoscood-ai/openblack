# Creature appearance

A creature's body shows what it has become. Each species has a base mesh and a mesh at either end of three axes: evil
to good, thin to fat and weak to strong. The body is drawn as the base pulled towards one mesh on each axis, and its
skin is blended towards the evil or good skin, so an evil creature grows dark and twisted and a good one bright and
noble.

**Progress: 29/34 done, 0 chosen, 2 partial, 2 inferred — 89%**

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
| The miracles it casts move its alignment by what they do to the world | pending | in the exe an effect whose applier is a creature moves the creature's own alignment ([whose alignment](../../../magic/influence-and-alignment.md#the-original)); the miracles a creature casts do not change its alignment in our tree |
| What it does moves its alignment: the plan it carries out by its desire's per-turn change, an eaten villager by its kind's weight, damped by its own alignment | done | `creature_deed_alignment` (`src/Creature/CreatureDeedAlignment.cpp`), as the exe's add ([creature alignment](../../../magic/influence-and-alignment.md#creature-alignment)); tests `CreatureDeedAlignment.TheDesireOfThePlanGivesTheChange`, `AnEatenVillagerReplacesTheChangeOfTheDesire`, `TheAlignmentOfTheCreatureChangesTheChange`; the miracles it casts are the row above |
| The nice and nasty spells pull its alignment for a while and put it back | done | `creature_spells` nice and nasty (`src/Creature/CreatureSpells.cpp`, run by `src/Magic/Spells/SpellCreature.cpp`); tests `CreatureSpells.ASpellEasesInHoldsForItsTimeAndEasesOut`, `ItIsSavedAsItWasBeforeItsSpells`; see [../miracles](../miracles/) |
| The aggression and compassion leashes don't change how it looks, only how it feels | done | see [leash.md](leash.md); test `LeashSystemTest.TheCompassionAndAggressionLeashesLeaveHowItLooksAlone` |
| Scripts can read and set alignments | done | `GET_ALIGNMENT` and `SET_ALIGNMENT` work on players' alignments (`src/CHLApi.cpp`); a creature's ALIGNMENT property is read and set (`ecs::player_creature::GetCreatureProperty`, `SetCreatureProperty`; test `PlayerCreature.TheCreaturePropertiesAreStrengthAlignmentAndTheBodys`), and a set is followed by the body at once, as the exe's: the shown fatness takes a turn's step and the shape is drawn anew (test `CreaturePropertyTest.ANewAlignmentStepsTheShownFatnessAndRedrawsTheBodyAtOnce`; debug: the Creature Spawner's drawn shape and shown fatness readout); every ALIGNMENT property in the shipped scripts is a creature's, so the property of anything else is not in the game ([not in the original](../../not-in-the-original.md)) |
| The alignment is kept in creature files | done | read by `src/Creature/CreatureMindFileBody.cpp` and written by `src/Creature/CreatureMindModel.cpp`; test `test/test_creature_mind_file_body.cpp` (`CreatureMindFileBody.TakesTheBodyAFileKeeps`) |

## Drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is drawn with the land's lighting | done | `src/Graphics/RendererCreature.cpp`; see [../rendering](../rendering/) |
| A creature casts a soft shadow of its silhouette onto the land and what stands on it, from the scene's light, kept at least 45 degrees up | done | a complex caster in `src/Graphics/ShadowList.cpp` with `shadow_math::LightCreature`; tests `CreatureShadow.TheLightIsKeptAtLeast45DegreesUp`, `ShadowMath.Lights` |
| The shadow fades out as the camera pulls away from the creature | done | the projected shadows' fade in `src/Graphics/ShadowList.cpp`; tests `ShadowMath.FadeByDistance`, `ShadowMath.Alphas` |
| The hair casts no shadow | done | the shadow is cast from the body mesh only (`src/Graphics/ShadowList.cpp`) |
| A frozen creature looks icy, a dark blue sheen growing over it as it freezes | partial 20% | the freeze is kept; the body and its eyes are drawn in the exe's frozen tint (`creature_spells::FrozenTint`, the land's light hazed times it, as the tint after the haze; `creature_draw::FrozenLidColour`; tests `CreatureSpells.TheFrozenLookTintsTowardsDarkBlue`, `CreatureDrawEyes.AFrozenEyelidIsItsColourTimesTheFrozenTint`), and the hair is hidden from a fifth frozen (`creature_fizz_look::HairShown`), as the exe; missing: the ice sheen over the body and the eyes, and the vertex blends off while frozen ([drawing](../../../creature/drawing.md#the-frozen-look)) |
| An invisible creature dissolves into static | done | a fizzing body, its eyes and its hair are drawn as the exe draws them: two passes through the static (`Renderer::DrawCreatureBody`, `DrawFizzed`, fs_object_creature.sc; `creature_draw::FizzPasses`, tests in `test/creature/test_creature_draw.cpp`), and its reflection is left out from a fizz of 0.2 (`sea_pass::ReflectsCreature`) ([the fizz](../../../creature/drawing.md#the-fizz-in-openblack)) |
| A fizzed-out creature casts no shadow | inferred | not read in the exe: the shadow list's creature entry (`CreateDynamicShadow`) is not known to test the fizz; ours still casts it (`src/Graphics/ShadowList.cpp`) |
| Species-specific extras: eyes, eyelids and hair tufts | done | see [face_eyes_hair.md](face_eyes_hair.md); tests `MorphCreatureBlock.Version21HasActionPointsEyesTattooSitesAndBank`, `CreatureEyes.ThePointIsOnTheTriangleAndItsNormalPointsIn`, `CreatureDrawEyes.EachEyeballComesBeforeItsEyelidInTheSkinsColour`, `CreatureHair.StrandsGrowStraightOutOfTheSurface` |
| Each player's creature can be told apart by its tattoos and colours | inferred | tattoos are drawn (see [creature_tattoos.md](creature_tattoos.md)); a colour by player on the creature is not found in the exe's creature draw ([drawing](../../../creature/drawing.md)) |
| A creature carrying a glow from its player's creed (the hand's glow) when a script sets it | done | `SET_CREATURE_CREED_PROPERTIES` gives the glows (`components::CreatureHandGlows`) and the hum; each is drawn as the glowing shell over the creed model where the body is drawn (`creature_hand_glow::PlaceOn`, `Renderer::DrawCreedGlow`; tests `CreatureCreedGlowDraw.*`) ([the Creed glows](../../../creature/drawing.md#the-creed-glows-in-openblack)) |
