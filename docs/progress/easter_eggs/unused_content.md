# Unused content

What was made for the game but never reaches the player: challenges whose scripts were never compiled into the shipped
challenge file, scenes cut from shipped challenges, creature actions that do nothing, and data files nothing loads.
"Unused" here was checked by comparing the script sources of the Mac release (the original sources, kept with the
challenge compiler work in openblack's research folder) against the 514 scripts actually inside
`Scripts/Quests/challenge.chl`. Unused puzzle kinds and the best-known cut games (cow bowling, whack-a-villager, the
cup final and so on) are listed in [../story/minigames.md](../story/minigames.md); they are not repeated here.

**Progress: 0/2 done, 1 partial — 25%**

## Inside the program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dance formations can lay dancers out as shapes: circle, heart, spiral, octagon, square, triangle, wavy circle, lines, random scatter, a figure eight, lightning, a star, a man, a fish, a cockerel and the letters of the alphabet (`Data/Letter*.DAN` and friends) | partial 50% | the computed shapes (circle, spiral, wavy circle, lines and the others) are in our tree (`src/ECS/DanceShapes.cpp`), but not the triangle and the heart, nor the random scatter's points; the shapes read from `Data/*.DAN` (the letters, the man, the fish and the rest) are not read, so those dancers stand at the centre; no dance file uses the random scatter; see [../scripts/dance_scripts.md](../scripts/dance_scripts.md); re-measured 2026-10-21: the computed shapes are in but the triangle, the heart and the scatter; the shapes read from the .DAN files are not |

