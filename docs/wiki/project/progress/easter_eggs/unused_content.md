# Unused content

What was made for the game but never reaches the player: challenges whose scripts were never compiled into the shipped
challenge file, scenes cut from shipped challenges, creature actions that do nothing, and data files nothing loads.
"Unused" here was checked by comparing the script sources of the Mac release (the original sources, kept with the
challenge compiler work in openblack's research folder) against the 514 scripts actually inside
`Scripts/Quests/challenge.chl`. Unused puzzle kinds and the best-known cut games (cow bowling, whack-a-villager, the
cup final and so on) are listed in [../story/minigames.md](../story/minigames.md); they are not repeated here.

**Progress: 0/1 done, 0 chosen, 1 partial, 0 inferred — 90%**

## Inside the program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dance formations can lay dancers out as shapes: circle, heart, spiral, octagon, square, triangle, wavy circle, lines, random scatter, a figure eight, lightning, a star, a man, a fish, a cockerel and the letters of the alphabet (`Data/Letter*.DAN` and friends) | partial 90% | the computed shapes, the triangle and the heart among them, are in our tree (`ecs::dance_shapes::Build`, `src/ECS/DanceShapes.cpp`; test `DanceShapes.TheShapesAreWorkedOutAsTheGameDoes`), and the shapes of the `Data/*.DAN` files (the letters, the man, the fish and the rest) are read through the resource caches (`DanceSystem`, `dance::DanceShapeFile`; test `DanceShapeFile.AShapeFileHoldsItsPointsInOrder`); left: the random scatter's points, at the centre, which no dance file uses; see [../scripts/dance_scripts.md](../scripts/dance_scripts.md) |

