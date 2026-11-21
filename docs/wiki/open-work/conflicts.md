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
