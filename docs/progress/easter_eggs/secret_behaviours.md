# Secret behaviours

Odd things the creature and the world do that the manual never mentions. Each row is something the program really
builds; how often the creature picks it depends on its desires, which are owned by
[../creature/desires.md](../creature/desires.md) and [../creature/decision_making.md](../creature/decision_making.md).
The playful villagers (football, the Mexican wave, gossip) are in
[../villager/play_and_gossip.md](../villager/play_and_gossip.md); knocking on houses is in
[../hand/clicking_and_activating.md](../hand/clicking_and_activating.md); the rival gods casting on their own creature
"for a laugh" is in [../rival_gods/magic.md](../rival_gods/magic.md).

**Progress: 2/17 done, 4 partial — 24%**

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To get the player's attention it picks something up and throws it at the camera, at the point the player is looking from | pending | no such plan action in our tree (`src/Creature/CreaturePlanActions.cpp`) |
| Out of curiosity it looks at its reflection: it walks to the water's edge, stares down at its feet, makes a gesture, moves to a second spot and does it again, then goes back to the first | pending | no such plan action in our tree |
| At play it pulls silly faces at someone: it walks up, faces them, two times in three makes a gesture, then pulls three random faces | partial 30% | `PullSillyFaces` plays a playful face on the spot (`src/Creature/CreaturePlanActions.cpp`); it does not walk up to anyone or pull three faces; re-measured 2026-10-21: a face on the spot; it does not walk up or pull three faces |
| It has a "desire to get high", which it satisfies by picking something up and eating it; magic mushrooms are the things that count, and its web page keeps a count of mushrooms eaten | pending | no such desire acted on in our tree; what raises it is unconfirmed |
| To be with the player it walks into the middle of the screen, following the camera, and turns to face the player | pending | no such plan action in our tree |
| It can look straight at the camera during cinema scenes, and point at the camera or at the hand | partial 70% | `PointAtCamera` turns it to the camera and points at it, `LookAtHand` turns it to the camera and plays an animation (`src/Creature/CreaturePlanActions.cpp`); in a script's cinema scenes `LookAtCameraInWideScreen` has it watch the camera, its head on it, now and then pointing, waving or talking; its head does not track the hand; re-measured 2026-10-21: it points at the camera and watches it in cinema scenes; its head does not track the hand |
| It can mimic the player, copying what the hand does | partial 50% | it copies the player's deeds in stages (`creature_watching::StepMimicry`, `src/Creature/CreatureWatching.cpp`), but only with the actions our tree carries out; see [../creature/learning_by_observation.md](../creature/learning_by_observation.md); re-measured 2026-10-21: copied in stages, with only the actions our tree carries out |
| With a "mental illness" desire it behaves strangely | pending | not in our tree (unconfirmed what it does) |
| It looks about: at the moon, at the sun, out to sea, at the mountains, at its temple | partial 80% | it looks at the mountains, out to sea, at the sun and at the moon (`LookAtMountains`, `LookOutToSea`, `LookAtSun`, `LookAtMoon`: a generic look about); at its temple is pending; re-measured 2026-10-21: four of the five looks. Looking down a cliff is not in the original (its builder is empty and fails; see [not-in-the-original.md](../../not-in-the-original.md#creatureidle_behaviour)), and forced in openblack it fails too (test `CreatureFart.LookingDownACliffIsNeverBuilt`) |
| It plays games: throwing stones at a can, racing a friend, running round a race track, throwing a die it owns | pending | no games in our tree; the die is one of its toys, see [../story/rewards.md](../story/rewards.md) |
| Angry at another creature, it tells it to clear off | pending | no such plan action in our tree |
| To make friends with another creature it asks it to hold still for four seconds and kisses its backside | pending | no creature friendships in our tree (unconfirmed how often this happens in Black & White) |
| It can swap minds with another creature | pending | not in our tree (unconfirmed whether anything in Black & White leads to it) |
| In a football match it can celebrate or mourn a goal, and play as goalkeeper, defender or attacker | pending | it joins by acting on a villager who is playing, and its side comes from its player number: [../town/football.md](../town/football.md#the-player-and-the-creature) |

## The world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Putting a music CD in the drive lets the game play it, and the advisors remark: "So you wanna hear your own music, huh?", "What's the matter? Our in-game music not good enough?", "Don't mind him. But watch out for your Creature.", "He'll always associate this music with what you do to him when it's playing.", "Creatures are sensitive to music, you see.", "Good gracious. Pumpin' choonz." | pending | the CD player is n/a in [../audio/music.md](../audio/music.md); whether the creature really links CD music to how it is treated is unconfirmed in the code |
| When a villager is made, there is a 2 in 10 chance it becomes a named villager, often a Lionhead developer | done | `ecs::villager::RollSpecialVillager` and `MakeSpecialVillager` (`src/ECS/Villager/VillagerCore.cpp`); the name over its head is not drawn yet; see [../villager/special_villagers.md](../villager/special_villagers.md) |
| Fireflies that come out at night hide in trees and rocks by day, and lifting the right tree or rock gives a random miracle seed (the "seeds under trees" players remember) | done | fireflies come out in the evening and go back to the trees and rocks in the morning (`ecs::ProcessFireFliesTurn`, `src/ECS/FireFlies.cpp`); lifting the one a firefly sleeps in gives a one-shot seed (`fire_fly::Reward`, `src/Worship/FireFlyReward.cpp`); see [../miracles/dispensers_and_seeds.md](../miracles/dispensers_and_seeds.md) |
