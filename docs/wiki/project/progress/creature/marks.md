# Wounds, burns and blood

Fights and fire leave marks on a creature's skin: cuts, bruises, grazes and burns that start fresh and red and fade
over time to old scars, and drops of blood that darken as they dry. The marks heal as the creature does and go once
they are old enough.

**Progress: 12/22 done, 0 chosen, 6 partial, 0 inferred — 66%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).
Read in the original on 2026-10-21: the marks' healing step (`LH3DCreature::TimeWarpHeal` 0x481930), the wounds'
lifetimes (`BruiseHealTimes` 0x8CF0B4), the blood's strength (`BloodColourStrength` 0x8CF0B0, 0.75), the skin's
repaint (`LH3DCreature::MorphTexture` 0x48D540) and who cuts the creature (`LH3DCreature::BeCut` 0x486390).

## The marks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wounds are small pictures from an atlas of damage, 8 kinds by 8 pictures of 32 texels | done | `creature_marks` (`src/Creature/CreatureMarks.cpp`); damage atlases (`Data/damage_new256.raw`, `damage_old256.raw` and their alpha) loaded by `CreatureSkinArtLoader` (`src/Resources/Loaders.cpp`); test `CreatureSkinArtLoaderTest.ARequiredFileOfTheWrongSizeThrows` |
| Each wound has a place on one of the base mesh's skins, a kind and a picture | done | `creature_marks::Mark`; tests `CreatureMarks.WoundsFadeFromFreshToOld`, `WoundsAreCentredAndCutAtTheEdges`; the original keeps the age in the low 6 bits and the kind in bits 8..10 of each wound (0x4819C7..0x4819EF) |
| A wound starts with its fresh look and fades towards its old look as it ages | done | test `CreatureMarks.WoundsFadeFromFreshToOld` |
| Each kind of wound lasts its own number of healing steps | done | `k_WoundLifetimes` {32, 64 × 6, 1}, the original's `BruiseHealTimes` (0x8CF0B4, read from the exe); a wound ages only while younger than 63 and goes when its age equals its kind's (0x4819D2..0x4819F2), so the six kinds of 64 stay at their oldest look and never go (`k_OldestAge`); tests `CreatureMarks.MarksGoOnceTheirKindHasLasted`, `WoundsOfTheKindsThatLast64StepsStayAtTheirOldest` |
| Wounds are centred on their place and cut off at the skin's edges | done | test `CreatureMarks.WoundsAreCentredAndCutAtTheEdges` |
| A wound's colour is blended over the skin at 8 bits and cut back to 4 | done | test `CreatureMarks.ColoursBlendAt8BitsAndCutTo4` |
| Blood is painted a texel at a time, red when fresh and browner as it dries, covering three quarters of the skin under it | done | `creature_marks::PaintBlood` and `BloodTexel`: each channel the skin's 4 bits widened to 8 times 0.25 plus the blood's times 0.75, truncated and cut back to 4, as the original's `TextureRef::ColourIntersectionPoint` 0x866D00 with `BloodColourStrength` 0.75 (0x8CF0B0, read only, by `UpdateBlood` 0x481820 and `MorphTexture` 0x48D71E); in floats the sum is exact, so it is (skin + 3 × blood) / 4 rounded down (ported from raffclar's #594; before it, a blend at 191 of 255 that rounds some texels a step lower); tests `CreatureMarks.BloodDarkensAsItDries`, `BloodTakesThreeQuartersOfItsColourRoundedDown` |
| Blood goes after a set number of steps | done | a drop already 63 (`k_OldestAge`) goes at the next step, the others age, as the original's test of the cut's age at 0x481983 before it ages it: a drop lasts 64 steps; test `CreatureMarks.MarksGoOnceTheirKindHasLasted` |
| A creature keeps at most 1024 of each; a new one past that takes the oldest's place | done | test `CreatureMarks.AFullListLosesItsOldestMark` |
| The skin is painted tattoos first, then wounds, then blood | done | painted by `creature_skin` (`src/Creature/CreatureSkin.cpp`) in `CreatureSkinSystem`, in the original's order (`GTattoo::Draw`, then `DrawCutAroundIntersectionPoint`, then `ColourIntersectionPoint`, 0x48D682..0x48D731); test `CreatureMarks.SkinsArePaintedTattoosThenWoundsThenBlood` |

## Where marks come from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Blows in fights leave wounds where they land | partial 50% | `CreatureFightSystem` adds a wound for each unblocked blow (`CreatureSkinSystem::AddWound`), as the original's fight action cuts the creature (`DoFightActionAction` 0x48AE8F); but where on the skin is not found from the body: ours draws the place at random on the first skin (two `GameRand` draws, then the kind's and the picture's), not where the blow lands; no test checks the wound a blow leaves; see [fighting.md](fighting.md) |
| Blows cut, bruise or graze by where they land; some kinds also bleed | partial 20% | a wound is left, but its kind is guessed (`fight::WoundKind`, `src/Creature/CreatureFight.h`); the kind the original's blow passes to `BeCut` is not read |
| Bleeding wounds leave trails of blood | partial 30% | ours adds drops at the blow (`fight::Bleeds`, guessed; `CreatureSkinSystem::AddBlood`); the original runs up to eight trails, each moved down the body and painted every turn for up to 12 s (`LH3DCreature::UpdateBlood` 0x481760, from `UpdateTime`) |
| Fire and fireballs leave burns | pending | the burn kind exists but fire never adds one; in the original the fire's turn cuts a creature it burns (`FireEffect::ProcessList` → 0x72F1D4 → `BeCut`) |
| Lightning and other harmful miracles cut and scar the creature | pending | no miracle marks a creature's skin in our tree; in the original every effect applied to a creature can cut it (`Creature::BeCutAndScarredByEffect` 0x478890, from `ApplyEffect` 0x478F10), with its own game random draws (0x478A3D..0x478B32) |
| Being hit by thrown objects marks it | pending | in the original a thrown thing's strike applies its crush as an effect (0x479B04, [hits-and-catch.md](../../../creature/hits-and-catch.md#struck-by-a-thrown-thing)), and an effect out of a fight cuts the creature (0x478890, [spell-core.md](../../../magic/spell-core.md#a-creature-taking-an-effect)); our strike (`src/ECS/CreatureStruck.cpp`) takes the life but marks nothing |
| Every third hard slap in a row bruises the skin, by the slap's speed | partial 50% | every third hard slap in a row at one height on the body counts as a bruise (`creature_feedback::CountSlap`, test `CreatureTouch.EveryThirdHardSlapInARowAtOneHeightOnTheBodyBruises`); the hard-slap packet takes a hundredth of the life (`creature_hand_packets::ApplyHardSlap`) and the bruise's column is worked out (`creature_feedback::BruiseColumn`, test `CreatureTouch.ABruiseIsMarkedByTheSlapsSpeedAndTakesAHundredthOfLife`), but no mark is painted, where the original paints the bruise at the hand's touch point and keeps it among its marks (0x4866F0, from the packet's handler); see [learning_from_feedback.md](learning_from_feedback.md) |
| The game keeps a record of the damage on the body: its cuts and bruises, each aging until it heals | partial 30% | openblack keeps the skin's wounds (`CreatureSkinSystem::AddWound`); the exe's body keeps two lists, cuts and bruises, each a packed texture mark with a type and an age (bw1-decomp `Creature3D.cpp:1014..1037`, saved and loaded with the body), a bruise aging every step and removed once its age reaches its type's heal time (`Creature3D.cpp:1301..1313`), and the share of bruises out of 1024 (`LH3DCreature::GetDamageFraction` 0x47F8A0) is read by an effect's cut (`Creature::BeCutAndScarredByEffect` 0x478890) (read 2026-11-05); missing: the bruises' list, their aging and healing, and that share |

## Healing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Marks age a step every 600 counts of healing; a game turn heals by one count | done | `creature_marks::Heal` once a turn in `CreatureSkinSystem::ProcessTurn`, as `TimeWarpHeal` (600 at 0x48194F) from `UpdateTime` 0x481FD1; tests `CreatureMarks.MarksAgeEvery600Counts`, `CreatureSkinSystemTest.TheTurnsHealTheMarks` |
| The heal miracle heals marks by its strength | partial 80% | a heal applied to the creature mends its marks by its strength × `k_HealEffectCounts` (9600) counts, truncated (`CreatureApplyEffect`, `src/ECS/Effects/EffectValues.cpp`; tests `CreatureSpellHooksTest.AHealHealsACreaturesLifeAndMendsItsMarks`, `TheMarksMendByWholeCountsAndNotWithoutAHeal`); missing: the full heal of the body by 9600 (0x50530E) has no caller in openblack; in the original a heal applied to the creature heals the marks by its strength × 9600 counts (`ApplyEffect` 0x478D7D / 0x478F42), and a full heal of the body by 9600 (0x50530E) |
| Old marks go once their kind has lasted, and the skin is repainted | done | `creature_marks::Heal` from `CreatureSkinSystem::ProcessTurn`; as the original (`TimeWarpHeal` 0x481930, 0x481988..0x481991, 0x4819FB..0x481A0B), the list's last mark moves into the place of the one that goes and takes its own step there, so the painting order changes as the original's; test `CreatureMarks.TheLastMarkTakesThePlaceOfOneThatGoesAndAgesThere` |
| Marks are kept in the creature's files | pending | not saved; the original writes both lists in the creature's physique file every 600 turns ([players-creature.md](../../../creature/players-creature.md#the-creatures-files)); saved games are not read |
