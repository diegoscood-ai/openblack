# The ending

How the game ends: Nemesis's last stand, the fight between the creatures, the creature's sequence at the volcano and the
people waving goodbye, then the end of the game.

**Progress: 0/11 done, 0 partial — 0%**

## Nemesis's defeat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Winning all of Nemesis's towns leaves him without power | todo | never reached (Land 5 needs `LOAD_MAP`), and the town and belief checks it needs (`BeliefForPlayer`, `GetTownWithId`) are stubs; see land_5.md; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#nemesiss-last-stand-before-the-fight) |
| Nemesis says he is not finished and calls his creature | todo | never reached (Land 5); his lines would play (`RunText`), but no script can make his creature (`CREATE` of a creature is not implemented); his creature is a mirror image of the player's: [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-mirror-creature-introduction) |
| The big fight: the player's creature against Nemesis's, with its own music | todo | never reached (Land 5); the creature fight code exists (`CreatureFightSystem`), the script fight commands (`SetCreatureAutoFighting`, the fight queue) are stubs; see ../creature/fighting.md; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-fight) |
| The fight's camera circles the fighters with glittering effects | todo | never reached (Land 5); the camera natives are real, the fight is not |
| Nemesis's temple is destroyed | todo | never reached (Land 5), and deleting a temple's heart from a script logs "not implemented" (`ObjectDelete`); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#nemesiss-temple-falls) |

## The last sequence

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature walks to the volcano | todo | never reached (Land 5), and moving a creature from a script is not implemented (`MoveGameThing` on a creature); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The villagers wave the creature off, shown in windows that open and close over the picture | todo | `SetClippingWindow` and `ClearClippingWindow` are stubs; never reached; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits); a Celtic footballer is among the waving villagers and a Norse one in the special shepherd's place: [../town/football.md](../town/football.md#the-cup-final-and-the-footballers) |
| The creature's end sequence music | todo | `StartMusic` plays any music of the game's table (`Audio/Services/GameMusic`); the script that starts it is never reached; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The outro music and the end | todo | `StartMusic` plays it (`Audio/Services/GameMusic`); the script that starts it is never reached; the first credits shot shows three crusaders at the Magic Dragon's cave exit: [silver_scrolls/the_magic_dragon.md](silver_scrolls/the_magic_dragon.md); [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |
| Game over: the advisors give up and "Game Over" shows when the player's temple is destroyed (losing every villager does not end the game) | todo | nothing in our tree starts the `GameOver` script, which also needs stub natives (the drawn text, the temple's position); the whole sequence and what starts it: [losing_and_game_over.md](losing_and_game_over.md) |
| After the ending the game goes on in the last land | todo | never reached; the land's control script sleeps after the credits, leaving the player in the fifth land: [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |

## Unused

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nemesis's mirror fight against the player's creature's opposite self | n/a | in the scripts but never started; [gold_scrolls/so_this_is_a_fight_to_the_death.md](gold_scrolls/so_this_is_a_fight_to_the_death.md#unused-and-cut-parts) |
| A script steering Nemesis's battle tactics | n/a | in the scripts but never started |
