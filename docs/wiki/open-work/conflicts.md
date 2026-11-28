# Conflicts with raffclar's tree

## Summary

Where raffclar's tree and ours do the same job differently, or a piece of his is taken in part, the choice made and
why ("Taking pieces from raffclar": the version that matches the original wins, ours when that cannot be told). His
deliberate deviations that are taken are listed on [Deliberate deviations](../project/deviations.md).

## Choices

| Area | His | Ours | Why |
|---|---|---|---|
| Particles: the effects a thing's draw steps (2026-11-20) | They step at most 60 times a second, each step taking all the game time since the last (`Common/FramePace`, through `particles::drawn_steps::StepTime`, `StepSeconds` and `TurnStepSeconds`), as the original's frames were held to the screen's refresh | His `DrawOrder` and `LeftOut` are taken (`src/Particles/DrawnEffectSteps.h`); the effects step in every frame that draws their thing, by that frame's game time | The original steps them in every drawn frame (its draws call the step with the frame's game time); his pace is a deliberate deviation, not taken |
| Temple: the miracles being cast on the pool's map (2026-11-21) | `TempleMapSpells` (`src/3D/TempleMapSpells.h`, his ac5bb2962 and 25ecfa578): every frame the miracle-activity button is on, each spell with a seed is shown, a new one unturned; with the button off the seeds and their holder effects are cleared, so they start again unturned when it comes back on; the seeds turn by `magic::visuals::TurnSeed`, and the holder effects play in the temple through his particle rules | `TempleInterior::CreateMapSpells` / `UpdateMapSpells`: the graphics are made once on entry, as `WorldRoom::InitEngine` does with `CreateWorldMapSpells`, the button only gates the draw (`DrawAdditional`), so a seed keeps its turn while hidden; each draw is the gesture icons' draw-now step (`magic::gestures::StepIconSeed`, pinned to the oracle) at a tenth of the size, through the map position's land-height round trip; no holder effects yet (our temple pass draws no particle effects) | Ours matches the original's lifecycle and rounding (bw1-decomp `WorldRoom.cpp`, `CreateWorldMapSpells`, `DrawWorldMapSpells`); his holder-effect part (`TempleMapSpells::Effects`) is the piece to take once the temple pass draws particle effects ([pending](../interface/temple-interior.md#pending)) |
| Temple: the challenge pictures' files (2026-11-24) | d0751948c (PR #901): his `ChallengePictureStore` in `SaveGameSystem`, written on the save writer's thread, his room (`TempleChallengeRoom`) drawn from the store's cells, the save's copy as a step of his own, and his `DedicatedServer` wiring | his `ChallengePictureFiles` component and tests taken whole; the store kept in our `SaveGameSystem`, written at once through its file system; our `TempleChallengePictures` kept, the record's pictures set from the files' decoded cells as the picture is taken and after a load (`challenge_pictures::ShowFromFiles`, `Refresh`); our save's existing copy of the Current files kept; the server part left out | ours is what the other pieces of our tree use (the room, the save steps); what the files hold and when is his, the original's (read 2026-11-24: `fn_00784690`, `SaveTextureCitadel`, `CopyChallengeFiles`); online is parked |
| Creature mind: a god's order, commanded (2026-11-27) | His god's order, refitted into ours (08615bdd97): the plan it forces is commanded and marked as forced by its player's action, so carried out to its end it still counts for its desire | Commanded only, not marked: the plan counts for no desire | The original's completion counts a plan unless it is commanded and not forced by its player's action (`Creature::UpdateOnCompletion` 0x475D30), but nothing in the frozen exe ever sets that flag: every access, by name and at its offset (mind +0x1C18), clears it (`Creature::Initialise`, `FinishActionUnsuccessfully`, `StopWhatIAmDoing`, `CreatureAgenda::Clear`, `ConsiderDecision`). Ours matches the original ([plans-and-needs.md](../creature/plans-and-needs.md), "Which completed plans count", with the commanded-completion change) |

## The whole thing a repeated or copied plan is about (2026-11-24)

raffclar's `CreatureMindSystem::WholeOfForcedPlan` (his CreatureMindFromHand.cpp, from his 664cbcc08: the whole thing a
shown thing is part of through `WholeOfShownThing`, else the nearest town when `DesireRules::aboutSomething`) and
openblack's rule for the stroke repeat and the copy of a deed (`CreatureMindSystem::BelongsTo`, else the nearest town
when `DesireRules::social`) do the same job; openblack's is kept, as it matches the original where they differ:

- the flag: his `aboutSomething` is the desire record's +0x20 (his `CreatureInitialDesireInfo` puts `sources` there
  first), which openblack reads as `searchesForGoal`; the original's repeat (`Creature::RepeatLastAction` 0x50A1F0, the
  read at 0x50A2E7 of `g_CreatureInitialDesireInfos` +0x30, 0x1C0 a desire) and its copy (bw1-decomp
  `CreatureMimic.cpp:231`, `GetInfo()[desire].Social`) test the +0x30 Social flag;
- a creature: his `WholeOfShownThing` gives a creature no whole thing (then the nearest town), where the original's
  `Creature::AddBeliefAboutAssociatedObject` 0x4D8BB0 returns the creature's own belief (bw1-decomp
  `CreatureMentalBelief.cpp`, `CREATURE_BELIEF_TYPE_CREATURE: return belief`).

His other callers of `WholeOfForcedPlan` (the leash's and the hand's forced plans) are not compared here.

## The test each desire asks of a thing it may act on (2026-11-24)

raffclar's `creature_activity::ThingSuits` (his `src/Creature/CreatureActivity.cpp`) is taken, with his `ThingFacts`
and his tests; the kind of a thing and its facts are read by openblack's own helpers (`mind_detail::DesireMayActOn`).
Where it differs from the original, the original wins:

- a totem: the original's `Totem` class (a town's totem building, `Abode::Create`'s `ABODE_TYPE_TOTEM` case, and
  `PuzzleTotem`) answers yes to `CanBeImpressedByCreature` 0x561500 and `CanBePlayedWithByCreature` 0x561510; his kinds
  have no totem, so it is a building there (impress no, play by its row). openblack adds `Thing::Totem`;
- a rock: the original's `Rock::CanBeAttackedByCreature` and `CanBePlayedWithByCreature` (through `MobileStatic`) hold
  for its subclasses `Bonfire`, `Fragment`, `DeadTree` and `FelledTree`; his kinds put a fragment with the statics.
  openblack counts a fragment a rock (`object_physics::IsRock`).

The tests are the desire table's +0x14 slot (0xC87864, read by `CreatureMental::IsObjectSuitableForDesire` 0x4E3730,
which lets anything for a desire with no test): desire 0 `CanBeImpressedByCreature`, 1 `CanBeHelpedByCreature`, 2
`CanBeAttackedByCreature`, 3 `CanBePlayedWithByCreature`, 5 `CanBeFrighteningToCreature`, 6 `CanBeInspectedByCreature`,
7 `CanBePoodUponByCreature`, 8 and 23 `CanBeSleptNextToByCreature`, 13 `CanBePickedUpByCreature`, 16, 31, 32 and 37
`CanBeBefriendedByCreature`, 34 `IsMushroom`.
