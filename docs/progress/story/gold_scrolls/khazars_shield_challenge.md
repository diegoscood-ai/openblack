# Khazar's Shield Challenge

A land 2 gold scroll and Khazar's lesson in defence: at a lone hut on a small island off the east shore Khazar casts a
Physical Shield and lets the player throw rocks at it, then hands over three shield seeds; the player shields the hut,
Khazar drops three boulders on it, and whatever happens the Physical Shield miracle is given to the player's village.
It is started, together with [Khazar's Fireball Challenge](khazars_fireball_challenge.md), by the "Miracle Challenge"
scroll Khazar leaves after the [Worship Site](worship_site.md) lesson (that scroll and Khazar's tour are described in
the fireball file). The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), the miracle itself in
[../../miracles/physical_shield.md](../../miracles/physical_shield.md).

**Land:** 2 · **Giver:** Khazar (the friendly god), through a scroll on the island hut · **Script:** PhysicalShieldChallenge · **Reward:** the Physical Shield miracle at the player's home village's worship site, given whatever the result · **Repeatable:** no

Sources: the quest's script source (`Land2ShieldChallenge.txt`), the scroll that starts it
(`Land2FireballChallenge.txt`), the land's scroll-notify helper (`SetupLand2.txt`), the hand demo (`HandDemos.txt`) and
the land's control script, checked against the compiled `challenge.chl`; the game's English text table; and the land's
map script (`Scripts/Land2.txt`) for the hut. The attack on the hut is Khazar's own boulders, not Lethys's (the land
file's summary row says Lethys). openblack's state is judged on this tree: of the 55 script functions this challenge
needs, 9 still only log "not implemented" in `src/CHLApi.cpp` (among them Khazar's hand, the held object, hits on an
object, aiming a rock and the challenge log), and the land's control script never runs in openblack (the land-loading
function does nothing and the story always begins with Land 1's script), so the quest never appears; every row below is
todo unless the notes say otherwise.

**Progress: 0/61 done, 3 partial — 2%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Started only by clicking the "Miracle Challenge" scroll by the worship site, at the same moment as the fireball challenge; the two can be done in either order | todo | plain script; see [khazars_fireball_challenge.md](khazars_fireball_challenge.md#the-miracle-challenge-scroll); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| If Khazar has died before the Miracle Challenge scroll is clicked, this challenge is never started | todo | Khazar's death needs computer-player natives (stubs) |
| Nothing in the land waits for it: no flag is set on completion | todo | the same script; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| What it unlocks: only the Physical Shield miracle at the home village's worship site; the same miracle also comes with Town2 (Khazar's Greek town at the start) and Lethys's home town when they are won | todo | `Land2.txt` gives towns 2 and 5 a physical shield miracle and our map loading reads it; the challenge's own gift is never reached |
| While it runs Khazar counts as busy, which holds back his death and so the end of the land (see Script quirks) | todo | Khazar's death needs computer-player natives (stubs) |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut is the single house of a tiny empty Norse village on an island, a neutral village the map makes uninhabitable for the challenge | partial | the house is made by our map loading (`AbodeArchetype`); the uninhabitable flag is not |
| A gold scroll appears on the hut | todo | `CreateHighlight` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| While the camera is within 100 of the scroll and the scroll is on screen, the good advisor steps out, points at it and says "Here's one of Khazar's lessons. It's the Shield. We've got to try it, Leader.", at most once every 30 seconds and only outside films | todo | the notify loop calls real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The quest waits for the scroll or the hut to be clicked; the scroll is then switched to its active look | todo | `GameThingClicked` is real and `SetActive` handles highlights; never reached: Land 2 needs `LOAD_MAP`, an empty native |
| If Khazar dies before the click, the scroll is made active and nothing more happens | todo | Khazar's death needs computer-player natives (stubs) |

## The introduction and Khazar's demonstration

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click Khazar counts as busy and his hand flies fast to a spot on the shore south-west of the hut | todo | `MoveComputerPlayerPosition` is a stub, and the wait for his hand never ends (`ComputerPlayerReady`) |
| A ring of influence of radius 40 is made round the hut, and the hut is mended to full health | todo | `InfluencePosition` is real, but a building's health (property 1) is not handled by `SetProperty` |
| A film starts with Khazar's music; the camera moves in on the hut over 3 seconds | todo | `StartMusic` and the camera moves are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The log entry "Khazar's Shield Challenge" is recorded at 0 with the reminder "We need to throw stones at Khazar's Shield" | todo | `Snapshot` is a stub |
| Khazar: "Soon you will need to protect your Villagers and their buildings."; the camera slides round over 6 seconds | todo | `RunText` and the camera are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Three rocks (0.7 scale) appear on the mainland about 80 m west of the hut, 2 m apart | todo | `CreateWithAngleAndScale` makes rocks; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Khazar: "The Physical Shield protects against objects which are not of a spiritual nature." and "When you have the Miracle, this is how you cast a Shield." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Whatever the player is holding is deleted, so the hand is empty for the demo | todo | the held object comes from `GetObjectHeld` (a stub); `ObjectDelete` is real |
| Khazar's shield hand demo: the camera frames the hut over 2 seconds and a shield miracle is put in the hand with seven times a normal shield's strength, "to deflect the 3 rocks"; Khazar: "You move the Hand to where you wish to start the Shield.", "Then you hold down the Action Button and trace the perimeter of the Shield. Like this.", "And the Shield appears." as the demo hand draws the circle and the shield goes up over the hut | todo | `PlayHandDemo` is real, but putting a shield miracle in the hand with its strength needs `SetProperty` cases not handled |
| The camera moves over 2 seconds to a spot by the rocks, aimed at the hut, and the player's control is cut down to the hand | todo | `SetInterfaceInteraction` and the camera are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| As the film ends Khazar says "Now throw these rocks at the hut and watch the Shield." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Throwing the rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest finds the shield within 5 of the hut and waits until the shield is gone or hit, or 20 seconds pass | todo | `GameThingHit` is a stub |
| The shield stopping thrown rocks (and wearing down) is the miracle's normal behaviour | partial | the physical shield works in our tree ([../../miracles/physical_shield.md](../../miracles/physical_shield.md)); the challenge never runs |
| Full control comes back after the wait | todo | `SetInterfaceInteraction` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If the shield was hit or broke: a film, the camera rising over 6 seconds to look down at the hut; Khazar: "See? The hut is protected. But a strong enough attack will destroy the Shield." then "Now you can try. Shield the hut and I will attack it." | todo | real camera and text natives; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If 20 seconds pass with no hit: Khazar says "The Shield deflects the rocks." (whether or not any rock was thrown) as the camera rises, then "Now you can try. Shield the hut and I will attack it." | todo | real camera and text natives; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three Physical Shield one-shot seeds appear on the shore about 40 m south-west of the hut, 2 m apart, each with three times a normal shield's strength | todo | `CREATE` of one-shot seeds is real (`magic::script::CreateOneShotSpell`), but their strength through `SetProperty` is not handled |
| Khazar's demonstration shield is removed and the hut mended to full health again | todo | `ObjectDelete` is real, but mending the hut needs health (property 1), not handled |
| The player's control is cut down to the hand | todo | `SetInterfaceInteraction` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The log entry moves to 0.3 with the new reminder "We need to cast a Shield over the hut on this island." | todo | `UpdateSnapshot` is a stub |
| The quest waits until the player holds a shield seed or a shield stands within 5 of the hut; every 30 seconds (the first at once) the good advisor says "Tap the Shield Miracle and wait." | todo | `GetObjectHeld` and `IsOfType` are stubs |
| Lost seeds come back: about every sixth pass, a seed not held that is off screen or more than 12 m from the first seed's spot fades out and is made again at its own spot | todo | real field-of-view, distance, delete and create natives; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Once a seed is held, the evil advisor says "Hold the Action Button and draw a circle on the ground to place the Shield." | todo | follows from the in-hand check, a stub |
| The player then gets one go: the quest waits 1 second, looks for a shield within 5 of the hut, and if there isn't one waits until the hand is empty and 3 seconds more, then moves on whether or not a shield was cast | todo | the empty-hand wait reads a stub |
| A shield drawn so that its centre is within 5 of the hut counts; a shield beside the hut does not | todo | `CallNear` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If a shield is found, the good advisor steps out: "Hey! You've cast the Miracle! Marvellous!" | todo | `SpiritEject` and `RunText` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Full control comes back | todo | `SetInterfaceInteraction` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Khazar's attack (the test)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the hut is mended to full health again and Khazar's hand rises to 20 m above the hut; the camera moves to the shore over 4 seconds, looking up at it | todo | mending the hut (health) is not handled and Khazar's hand is a stub |
| Khazar: "The Shield is ready." (said whether or not the player cast one) | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Three boulders, each a random 0.6 to 0.8 scale, appear 50 m above the hut at random spots up to 25 m away and are each aimed to land on the hut within 2 seconds, half a second apart | todo | `CreateWithAngleAndScale` makes the rocks, but aiming them (`SetTarget`) is a stub |
| Three seconds later Khazar's hand goes back to the shore | todo | Khazar's hand is a stub |

## Success and failure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera closes on the hut over 3 seconds and the log entry is set to 1 either way | todo | the log is a stub; the camera is real |
| If the hut is still at full health (any damage at all counts as a failure): Khazar says "The hut survived my onslaught. Well done.", "Different sized objects do different levels of damage to a Shield." and "It'll disappear after taking excessive damage as well." | todo | a building's health (property 1) is not handled by `GetProperty` |
| Otherwise: "You failed to Shield the hut. It is destroyed." and "It is no disaster this time. However you will need practice further." | todo | the result reads health (not handled) |
| There is no retry and no penalty: both endings go on to the reward | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera pulls back over 4 seconds and the Physical Shield miracle is given to the player's home village, so its worship site can pray for it | todo | `SetMagicInObject` is real, but the home town comes from `GetTownWithId` (a stub); see [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Khazar: "I have given the knowledge of the Physical Shield Miracle to your Village." and "From now on your people can worship for this Miracle at your Worship Site." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The music stops, the film ends, Khazar is released and no longer busy, and any shield seeds left fade away | todo | `StopMusic` and `ObjectDelete` are real; `ReleaseComputerPlayer` is a stub |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hut stays as damaged as the boulders left it; the boulders and the three practice rocks stay on the land | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The influence ring round the hut is never removed by the script (undetermined: whether it goes when the script ends) | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor owns the notification, the log reminders, the tap tip and the praise; the evil advisor gives the drawing tip | todo | `SpiritEject` and `RunText` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Clicking the log entry speaks its current reminder through the good advisor, who steps out to say it | partial | the shared reminder calls real natives; the log entry is a stub |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's script music plays from the click until the reward | todo | `StartMusic` and `StopMusic` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; nothing in the scripts reacts to it (undetermined what happens if it picks up the seeds or rocks) | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The loop meant to give the player time and fresh seeds counts the remaining seeds into a total it never adds to, so it always believes the seeds are used up: the player gets exactly one attempt, ending 3 seconds after the hand is next empty | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The third seed's respawn check is guarded by whether the first seed exists instead of the third (both waits), so with the first seed gone the third is never replaced, and with the first present a used third seed may be remade (undetermined which happens in the engine) | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The seeds' spot is about 40.6 m from the hut, just outside the radius-40 influence ring, with one seed inside it and one outside (undetermined whether the player can reach all three without the home village's influence) | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Holding any object (a villager, a rock) when the demonstration starts deletes it | todo | the held object comes from `GetObjectHeld` (a stub) |
| Khazar counts as busy from the click to the reward; a challenge clicked and left half done (seeds never taken) keeps him busy, which holds back Khazar's death and so the rest of the land | todo | Khazar's death needs computer-player natives (stubs) |
| The busy flag is shared with the fireball challenge and Khazar's other lessons, so whichever finishes first clears it while this one may still be running | todo | Khazar's death needs computer-player natives (stubs) |
| Khazar's "The Shield is ready." is spoken even when no shield was cast | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand demo's line "Make sure you join up with the start position." is cut; the script's comment says the speech was too slow for the demo | n/a | in the text table, commented out in the demo |
| Unused retry lines in the text table: "I give you more Seeds to practice with.", "You were unsuccessful. Try again.", "Try recasting the Shield Miracle." and "Draw a circle with the Action Button pressed to mark the position and size of the Shield.", which fit the multi-attempt loop the shipped script never gives; two more entries are marked "NOT USED" | n/a | never spoken by the shipped script |
| A commented-out timer that would have dropped the boulders after 30 seconds whatever the player did | n/a | left in the source as comments |
| A commented-out final log update at the end | n/a | commented out |
