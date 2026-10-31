# Beams and lights of worship

The light the game puts over the places of belief: the faint column of light standing over every village centre a
player owns, the temple's own column that beats with its owner's heart, the curtain of light round each worship site's
altar, the trails of prayer power that run to a miracle, the glow of disciples, the beams over challenge scrolls and the
vortex, and the arcs of light the temple throws when it is struck. The influence border is in
[../worship/influence.md](../worship/influence.md), the village lanterns in [special_effects.md](special_effects.md). The
game's bursts of light are drawn only in the falling-spell film ([../video/bink_playback.md](../video/bink_playback.md)),
and a newly won town gets fireworks and a fountain of its owner's symbols rather than a beam
([../town/belief_and_conversion.md](../town/belief_and_conversion.md)).

**Progress: 19/37 done, 0 chosen, 8 partial, 0 inferred — 63%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## The column over each village centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every village centre owned by a player has a tall column of light standing over it | done | `src/Graphics/RendererLightColumns.cpp` (ported from raffclar's tree; its draw checked against v1.42 on 2026-10-22, [the column of light](../../../magic/temple-and-citadel.md#the-hearts-draw)); test `LightColumn.ShowsOnlyForAPlayerWhoseBorderShows` |
| It is made with the village centre and drawn every frame while the village centre is in play | done | collected every frame of the world view outside the temple (`Renderer::CollectLightColumns`, `src/Graphics/RendererLightColumns.cpp`, from `src/Graphics/Renderer.cpp`); no unit test of the collection itself, which is the renderer's draw loop: what it shows and where are the tests of the rows below |
| It shows only once its owner's influence border shows (their temple stands); towns of the neutral player have none | done | `light_column::Shown` over `InfluenceSystemInterface::IsBorderShown` (`src/Graphics/LightColumn.cpp`; test `LightColumn.ShowsOnlyForAPlayerWhoseBorderShows`) |
| It stands at the middle of the village centre's model, turned every frame about the upright to face the camera | done | `light_column::Model` (test `LightColumn.ItStandsAtItsPointFacingTheEyeAsWideAsItsModelAndStretchedUpright`) |
| It takes its owner's colour and is very faint: drawn additively at about 8% strength (20 of 255), steady, with no pulse | partial 80% | `light_column::k_TownAlpha`; the colour is the player colour table's, without the original's remap of the players by land (as the town symbols) |
| Its mesh is the column of the mesh pack: three faces of a half tube 5.8 across and 800 high, with the light band of mesh-pack texture 94 (0x5E) | done | the columns draw mesh 530 of `AllMeshes.g3d` (`MeshId::SpellColumn` in `src/3D/AllMeshes.h`, `ColumnMesh` in `src/Graphics/RendererLightColumns.cpp`) with its own texture; the game's column object takes mesh 530 too (checked against v1.42, 2026-10-22, [the column of light](../../../magic/temple-and-citadel.md#the-hearts-draw)); test `SubMeshDraw.TheLightColumnIsMesh530` (`test/test_submesh_draw.cpp`) |
| The column keeps its model's width and is stretched three times upright, drawn additively and without writing depth | done | `light_column::Model` (test `LightColumn.ItStandsAtItsPointFacingTheEyeAsWideAsItsModelAndStretchedUpright`): checked against v1.42 (2026-10-22, #203); the size of three the game notes on the object only widens its screen test, so the earlier three times wider and nine times taller column is gone ([the column of light](../../../magic/temple-and-citadel.md#the-hearts-draw)). Mode 13 (SRCALPHA / ONE, no depth write), one Z object each in the transparency queue; (inferred) keyed as a model's |
| It is drawn every frame in the world view, day and night and at any distance; it makes no sound | done | the game's drawing of it has no time, distance or sound check, nor has ours: collected every frame of the world view (`Renderer::CollectLightColumns`, `src/Graphics/RendererLightColumns.cpp`); (inferred) a column off screen is left out |

## The temple's column

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player's temple has the same column of light in its owner's colour, made with the temple | done | at the citadel heart (`src/Graphics/RendererLightColumns.cpp`); tests `LightColumn.TintIsTheOwnersColourAtItsStrength`, `LightColumn.TheTemplesStrengthBeatsFrom55To110RoundedDown` |
| It stands at the temple's position, turned every frame to face the camera, drawn like the village centre's | done | `src/Graphics/RendererLightColumns.cpp`; test `LightColumn.ItStandsAtItsPointFacingTheEyeAsWideAsItsModelAndStretchedUpright` |
| It shows only once its owner's influence border shows | done | `src/Graphics/RendererLightColumns.cpp`; test `LightColumn.ShowsOnlyForAPlayerWhoseBorderShows` |
| Its strength beats with its owner's heartbeat, from 55 to 110 of 255, rising and falling smoothly with each beat | partial 70% | `light_column::TempleAlpha` over the guidance's pulse (`Locator::heartbeatSystem`, `src/ECS/Systems/Implementations/HeartbeatSystem.cpp`). Only the local player's heart beats: a computer player's temple stays at 55. Pending: whether the original steps a computer player's heartbeat (its draw reads the owner's interface's guidance) |
| The heartbeat quickens with danger, from 0.75 beats a second with none to 2.5 at the most, eased in by a fifth each turn | partial 60% | worked out every ten turns for the heartbeat sound (`src/Audio/Services/Guidance.cpp`, `HeartBeat` in `src/ECS/AudioQueries.cpp`; test `GuidanceTest.HeartBeatPitchAndTheCitadelHeart`); enemy creatures near the towns add nothing yet, and nothing visual follows it |
| During the temple's destruction the column stays for the first 21.8 seconds and then goes | done | `light_column::TempleShown` (test `LightColumn.TheTempleKeepsItsColumnForTheFirstPartOfItsDestruction`) |

## The light round a worship site's altar

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ring of light stands round each built worship site's altar, in the player's colour | pending | see [../worship/worship_sites.md](../worship/worship_sites.md) |
| The ring is ten points on a circle of radius 3 about the site's altar point, closed back on itself | pending |  |
| It rises as a curtain: brightest just above the ground and fading to nothing at the top, its top flared out to 1.5 times the ring | pending |  |
| Its height ripples round the ring, up to about 10, the ripple running round as time passes | pending |  |
| Its brightness follows how full the site's prayer battery is (stored prayer over the battery's size), the strength travelling round the ring as it changes | pending | the site's battery is in `src/Worship/WorshipSite.cpp`; no ring is drawn |
| It is drawn additively with `S_LightSheetStars.raw`, the texture sliding along the curtain as fast as the site is strained | pending |  |
| When the miracles ask more than the dancers give, the ring's colour throbs with the site's strain pulse | pending | the strain pulse is worked out (`site::UpdateStrainVisual` in `src/Worship/WorshipSite.cpp`), nothing draws the ring |

## Trails of prayer power

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a miracle spends prayer power, a mote of light runs from its caster to the miracle for every 100 prayer power spent | done | only a miracle cast from a worship site's icon, as the original: once its upkeep, or apart its events, are past 100 (`magic::AccumulateSpellPoint`), `worship::site::CreateManaPathSprite` gives the site's SF_ManaPathNew a mote from the icon to the cast position or the event's (`src/Magic/Core/Spell.cpp`, `src/Worship/WorshipSite.cpp`); checked against v1.42 (2026-10-26, [the mana path motes](../../../magic/spell-core.md#the-mana-path-motes)); tests `SpellChantRules.aMoteOfLightOncePastAHundred`, `MagicSystemTest.AWorshipSitesMiracleSendsAMoteForEveryHundredItSpends`, `MagicSystemTest.OnlyASiteIconsMiracleSendsMotes`; see [../miracles/prayer_cost.md](../miracles/prayer_cost.md) |
| The motes take the caster's player colour, hug the land 0.4 above it, weave from side to side and travel at a steady 40 a second | done | the rule `UR_ManaPathNew` (`src/Particles/Rules/ManaPath.cpp` over `src/Particles/ManaPathMaths.cpp`), drawn as horizontal sprites by the generic particle draw; the site's alpha follows its strain pulse (`site::ManaPathAlpha`); a spark's step checked against v1.42 in the oracle (2026-10-26, bit-exact, [the mana path](../../../physics/particle-rules.md#the-mana-path)); tests `ManaPath.ASparksStepIsTheOriginals`, `ManaPathRule.*`, `MagicSystemTest.TheManaPathThrobsWithTheSitesStrain` |
| Reaching beyond the influence border with prayer to spare, the same motes run from the border to the hand (unconfirmed what it costs) | pending | see [../worship/influence.md](../worship/influence.md) |

## Disciples

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A disciple glows: a soft orange-gold light (alpha 128), sized by the villager and raised above it by its size, following the villager where it is drawn each frame | done | ported from raffclar PR #261: `ecs::disciple_marks::Update` (the smoke sheet's puffs, added); the place, size and colour checked against v1.42 (2026-10-20): over the villager's drawn position, not its last turn's (`disciple::GlowPoint`); tests `DiscipleRules.GlowAndSymbolStandOverTheDrawnVillager`, `DiscipleRules.TheSymbolIsYellow` |
| The disciple held in the hand, or the one under the hand, shows its disciple sign over its glow: a sprite two and a half times its size above the glow, kept the same size on screen | done | ported from raffclar PR #261: `ecs::disciple_marks::Update` (the leash sheet's cells); the place, size and colour checked against v1.42 (2026-10-20): over the glow of the villager as drawn (`disciple::SymbolPoint`), in opaque yellow (`disciple::k_SymbolColour`); tests `DiscipleRules.GlowAndSymbolStandOverTheDrawnVillager`, `DiscipleRules.TheSymbolIsYellow`, `DiscipleRules.Symbols` |

## Scrolls and the vortex

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A started silver scroll has a faint pale blue column of light over it, facing the camera | partial 40% | the scroll's Draw works out the camera-facing glow and its colour (`script_highlight::GlowOf`, `ActiveGlowArgb`, `src/ECS/ScriptHighlight.cpp`; tests `ScriptHighlight.ActivePulseAndGlow`, `ScriptHighlight.AStartedScrollGlowsAsAColumnAtItsCentre`) and the renderer draws it as a column of light (`Renderer::CollectLightColumns`, `light_column::ScrollGlow`; test `LightColumn.AScrollsGlowIsAColumnOfItsOwnMeshAndColourUnlessItsAlphaIsNone`); not yet compared with the original on a shot; see [../interface/scrolls_and_signs.md](../interface/scrolls_and_signs.md) |
| A started gold scroll has a yellow column that only shows for an instant at the top of each pulse | partial 40% | the colour, truncated as the original's, is worked out (`ActiveGlowArgb`) and drawn with the silver one's column, left out while its alpha is 0; not yet compared on a shot |
| The scroll columns use the blast-centre mesh (a half tube 400 high), stretched three times upright, at the middle of the scroll | partial 40% | the glow matrix, stretched three times upright at the scroll's centre (`GlowOf`, `light_column::Model`), drawn with the blast-centre mesh, whose materials the scroll's creation sets as the original's; not yet compared on a shot |
| All scrolls share one pulse, (1 − cos phase) / 2, its phase going round about once every 1.26 seconds | done | `StepPulse` and `ActivePulse` in `src/ECS/ScriptHighlight.cpp` (5 radians a second); test `ScriptHighlight.PulseOfProcessHighlights` |
| The "see this" beam: a yellow column (alpha 180, 1.5 times size) facing the camera, beating with the scroll pulse, fading out over 2 seconds when ended | partial 50% | `SF_SeeThisBeam` is made by the particle engine for scripts (SPECIAL_EFFECT_POSITION and SPECIAL_EFFECT_OBJECT in `src/CHLApi.cpp`, `src/Particles/ParticleManager.cpp`); the only test is the effect's file name (`MagicTables.particleTypeFiles`); its colour, size, beat and fade against the original are unchecked |
| On land 1, once the way out is opened, a "see this" beam marks where the vortex will open, and goes when the vortex appears | partial 50% | the land 1 script's special effect is started through `ParticleSystemInterface::StartSpotVisual` (`src/CHLApi.cpp`); no test and no run cites it: not checked in game |

## The temple struck

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle that hits the temple passes its harm to one of the temple's parts, and an arc of crackling light runs from the temple's heart to it | pending |  |
| The arcs are two wiggling beams of `S_Beam.raw`, drawn additively, with one of five crackle sounds in turn | pending | `SF_SimpleBeamCitadel` is only named in the particle tables (`src/Particles/ParticleTypes.cpp`, `src/Particles/ParticleManager.cpp`, with no rule); nothing makes it |
| A temple below full life throws arcs of light over itself from time to time | done | only while no building or homeless villager of its player takes its blows; every (0.4 + 2.6 x life) s in whole turns, between two random points of the heart drawn with the local random, with one of five spark sounds at 0.6 of its life or less: `TempleDestructionSystem::TransferredDamageEffect`, `physics::temple_heart::DamageInterval` ([temple-interior.md](../../../interface/temple-interior.md)); the waits and sounds per life pinned to the original's numbers run in the game; tests `TempleHeartDamage.OnlyAHurtHeartBeamsAndOneWithNoLifeAtItsFiercest`, `TempleHeartDamage.TheLessLifeTheOftenerItBeams`, `TempleHeartWounds.TheOriginalsWaitAndSoundForEachLife` |
| The temple's destruction plays a timed show of arcs, magic and explosions over 21.8 seconds | done | the arcs by the destruction clock, the magic over the citadel at 10 s, the explosion at 21.5 s and the smoke at 22 s: `TempleDestructionSystem::Beam` / `Step` ([temple-and-citadel.md](../../../magic/temple-and-citadel.md)); past 21.8 s the heart is no longer drawn, as the original (`temple_destruction::HeartDrawn`, `k_HiddenAfter` in `src/Temple/TempleDestruction.h`); tests `TempleDestruction.EachMomentHappensOnceOnTheTurnTheClockPassesIt`, `TempleDestruction.TheHeartIsNotDrawnFromALittleAfterTheExplosion` |

## Other lights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two lanterns light the land at the temple's door at night | done | `worship::citadel_lanterns` (`src/Worship/CitadelLanterns.cpp`): two town lamps with a town light each, made on the temple's first frame drawn more than 7 % built, drawn with it, not drawn from a little after the explosion of the temple being destroyed (`temple_destruction::HeartDrawn`); tests `CitadelLanterns.TheFirstLitFrameMakesBothLampsWithATownLightEach`, `CitadelLanterns.TheLampsComeOnlyAboveSevenPercentBuilt`, `CitadelLanterns.TheLampsAreNotDrawnFromALittleAfterTheExplosionOfTheTempleBeingDestroyed` |
