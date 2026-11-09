# The ending

How the game ends: Nemesis's last stand, the fight between the creatures, the creature's sequence at the volcano and the
people waving goodbye, then the end of the game.

**Progress: 0/11 done, 0 chosen, 0 partial, 0 inferred — 0%**

## Nemesis's defeat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Winning all of Nemesis's towns leaves him without power | pending | not yet reached (the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real), and the town checks it needs (a town's player, `GetTownWithId`) are real; see land_5.md; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#nemesiss-last-stand-before-the-fight) |
| Nemesis says he is not finished and calls his creature | pending | never reached (Land 5); his lines would play (`RunText`), and a script can make his creature now (`CREATE` of a creature and `CreatureCreateRelativeToCreature` are real); his creature is a mirror image of the player's: [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-mirror-creature-introduction) |
| The big fight: the player's creature against Nemesis's, with its own music | pending | never reached (Land 5); the creature fight code exists (`CreatureFightSystem`), and `SetCreatureAutoFighting` and the fight queue natives are real; `IsAutoFighting` and `SetFightExit` are stubs; see ../creature/fighting.md; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-fight) |
| The fight's camera circles the fighters with glittering effects | pending | never reached (Land 5); the camera natives are real, the fight is not |
| Nemesis's temple is destroyed | pending | never reached (Land 5); deleting a temple's heart from a script starts its destruction (`ObjectDelete`, `TempleDestructionSystem::DeleteByScript`); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#nemesiss-temple-falls) |

## The last sequence

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature walks to the volcano | pending | never reached (Land 5); moving a creature from a script is real (`MoveGameThing`), and the walk's end waits on `PlayedPercentage`, now real (the creature's body share); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The villagers wave the creature off, shown in windows that open and close over the picture | pending | `SetClippingWindow` and `ClearClippingWindow` are stubs; never reached; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits); a Celtic footballer is among the waving villagers and a Norse one in the special shepherd's place: [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) |
| The creature's end sequence music | pending | `StartMusic` plays any music of the game's table (`Audio/Services/GameMusic`); the script that starts it is never reached; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The outro music and the end | pending | `StartMusic` plays it (`Audio/Services/GameMusic`); the script that starts it is never reached; the first credits shot shows three crusaders at the Magic Dragon's cave exit: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |
| Game over: the advisors give up and "Game Over" shows when the player's temple is destroyed (losing every villager does not end the game) | pending | `TempleDestructionSystem` starts the `GameOver` script; its drawn text (`GameDrawText`, `SetDrawTextColour`, `FadeAllDrawText`) is a stub, the temple's position (`GetTemplePosition`) is real; not checked in a story game; the whole sequence and what starts it: [losing_and_game_over.md](losing_and_game_over.md) |
| After the ending the game goes on in the last land | pending | never reached; the land's control script sleeps after the credits, leaving the player in the fifth land: [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |

