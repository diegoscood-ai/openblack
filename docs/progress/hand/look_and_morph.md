# Look and morph

How the god hand looks: it changes shape and colour with the player's alignment (gold for good, red and spiked for evil),
lights the land at night and casts a shadow.

**Progress: 11/11 done, 0 partial — 100%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Alignment morph

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The base hand is pulled towards the evil hand below alignment 0 and towards the good hand from 0, by how far the alignment is from 0 | done | `HandSystem::MorphVertices` (`HandMorph.cpp`) with `hand_morph::LookOf` and `Weight` (`src/3D/HandMorph.cpp`): base + w (variant - base) with w the alignment's size, the evil mesh below 0 and the good one from 0; test `HandMorph.PullsTowardsEvilBelowZeroAndGoodFromZero` |
| The alignment shown is held between -1 and 1 | done | `hand_morph::Target` (`src/3D/HandMorph.cpp`), from `HandSystem::UpdateMorphing`: the local player's alignment clamped to -1..1 (`OPENBLACK_TEST_HAND_ALIGNMENT` to check it); test `HandMorph.TargetIsTheAlignmentHeldToItsRange` |
| The skin is blended a 4-bit channel at a time, alpha too, in whole steps | done | `HandSystem::MorphTexture` with `hand_morph::BlendWeight` and `BlendTexel` (`src/3D/HandMorph.cpp`): the 4444 blend, each nibble in whole steps, t = trunc(256 x the alignment's size) at most 255; tests `HandMorph.SkinBlendsEveryChannelInWholeSteps`, `HandMorph.SkinWeightIsTruncated` |
| The hand doesn't ease: it jumps straight to the alignment once it has moved far enough from the one drawn | done | `hand_morph::Refresh` and `Advance` (`src/3D/HandMorph.cpp`), from `HandSystem::UpdateMorphing`: re-morphs at once when the alignment moved 0.03 from the one drawn; tests `HandMorph.JumpsStraightToTheAlignmentOnceFarEnough`, `HandMorph.StaysUntilTheAlignmentMovesFarEnough` |
| It starts neutral | done | `hand_morph::State` starts drawn at 0 (the hand system's `_morph`) |
| Going into or out of the player's influence blends the skin again | done | `hand_morph::Advance`: a change of the texture set (in or out of the influence, `HandSystem::InInfluence`) blends the skin again at the drawn alignment; test `HandMorph.CrossingTheInfluenceBlendsTheSkinAgainAtTheDrawnAlignment` |
| The influence is tested at the point picked under the cursor, a frame late, kept within reach of the map | done | `HandSystem::InInfluence` at the action point, which `UpdateMorphing` reads at the start of the frame, before the hand is placed again (the last frame's) |

## Light and shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand casts a soft shadow on the land under it | done | The hand casts a projected dynamic shadow (`graphics::shadow_list`, `src/Graphics/RendererShadows.cpp`); see [rendering.md](../../bw1-notes/rendering.md) |
| The shadow fades as the camera pulls away from the ground under the hand | done | `src/Graphics/ShadowMath.cpp`; test `ShadowMath.FadeByDistance` (`test/test_shadow_math.cpp`) |
| At night the hand carries a light that brightens the land around it | done | `src/Graphics/HandLight.cpp`; tests `HandLight.*` (`test/graphics/test_hand_light.cpp`) |
| At night its light glows warm on water near it, never in the temple | done | `src/Graphics/HandWaterGlow.cpp`; tests `HandWaterGlow.*` (`test/graphics/test_hand_water_glow.cpp`) |

The glows and effects the hand carries, and its effect on the world, are in [hand_effects_and_glows.md](hand_effects_and_glows.md).
