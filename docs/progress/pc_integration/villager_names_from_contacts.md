# Villagers named after the player's contacts

The game can read the names in the player's e-mail address book (Microsoft Outlook, or its own address book filled
from a POP3 mailbox) and give them to villagers, so that the player's friends live in the world as named villagers.
While the game runs it can also watch for new e-mail and have one of those named villagers announce it. Without an
address book, the same named villagers are taken from 48 built-in names, mostly of the Lionhead team and their
families. The named villagers themselves are owned by [../villager/special_villagers.md](../villager/special_villagers.md)
(see also [../villager/looks_and_voices.md](../villager/looks_and_voices.md)); this file covers where the names come from
and the e-mail side.

**Progress: 0/53 done, 2 partial — 2%**

## Turning it on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The feature is off until the player turns it on in the separate setup program (Setup.exe), under its e-mail settings, for one chosen player profile | todo | our tree has no setup program or e-mail settings; the readme describes the setup steps |
| The setup has a first box, "check for Internet e-mail", which turns the whole feature on | todo | our tree reads no address book or mail |
| A second box chooses Microsoft Outlook; left clear, the game uses a POP3 mailbox instead | todo | our tree reads no address book or mail |
| For POP3 the player fills in the mail server's name, their user name and their password in the setup | todo | our tree reads no address book or mail; the game keeps them as per-profile settings (POP3 server name, user name, password) (unconfirmed exactly where in the registry) |
| A setting gives how often the game checks for new mail (an "e-mail check time") | todo | our tree reads no address book or mail (the original's value and its units are unconfirmed) |
| Outlook Express is not supported, only full Outlook or POP3 | n/a | as the readme states; openblack has no mail reading at all |
| Outlook must be in "Internet Only" mode; in groupware (Exchange) mode it may refuse to give the names or mail | n/a | a limit of Outlook itself, from the readme |
| The feature only works in the single-player game | todo | no named villagers in our tree; see ../villager/special_villagers.md; in the game named villagers are never made in a multiplayer game, and mail is only checked outside multiplayer |
| The player must have registered on the game's web site and logged in to the online game at least once | n/a | the readme lists it as required; the game only starts its mail system when the online check succeeds and at least one online player profile exists; the online service is gone |
| There is no in-game prompt or warning about the address book being read; agreeing is done by ticking the setup box | todo | our tree reads no address book or mail |

## When the address book is read

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At start-up, once per run of the game, the game first checks whether the computer is connected to the internet | todo | our tree makes no connection check (no online code) |
| Only when connected, and only when at least one online player profile exists, the mail system is loaded | todo | our tree reads no address book or mail |
| The mail system is loaded with the player's own address book file, `addressbook.lhe`, in the current profile's folder | todo | our tree does not read `addressbook.lhe` |
| If the mail system fails to start, it is unloaded and the game carries on with built-in names only | todo | our tree reads no address book or mail |
| The name table for named villagers is then built once, at start-up, not per land and not per villager | todo | our tree builds no name table |
| With Outlook, the names come from Outlook's contacts through a small helper library shipped with the game (`outlookdll.dll`), which drives Outlook itself | n/a | uses Outlook's own automation; Windows and Outlook only |
| With POP3, the names come from the game's own address book, which grows with the senders of mail received while playing | todo | our tree reads no address book or mail |
| The number of contact names found is written to `inetlog.txt` | todo | our tree writes no `inetlog.txt` |

## Which names, and how they are cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game asks for at most 148 names: the table has room for 148 | todo | our tree reads no address book or mail; our tree reads no address book or mail |
| Each contact gives one name, the whole name string as the address book gives it, not split into first and last names | todo | our tree reads no address book or mail; our tree reads no address book or mail; which field Outlook gives (full name, display name) is not confirmed (unconfirmed) |
| A contact with no name is skipped | todo | our tree reads no address book or mail; our tree reads no address book or mail |
| Names are cut to 47 characters | todo | our tree reads no address book or mail; our tree reads no address book or mail; the name field holds 48 bytes with its end |
| Names are taken in the order the address book lists them | todo | our tree reads no address book or mail; our tree reads no address book or mail |
| No other filtering is done: duplicates, e-mail-address-only names and odd characters are kept | todo | our tree reads no address book or mail; our tree reads no address book or mail; none was found in the game (unconfirmed for the helper library) |

## What each contact villager is like

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each contact gets an age of 11 to 50 years, at random | todo | our tree reads no address book or mail; our tree reads no address book or mail; a local (non-game) random number below 40, plus 11 |
| Sexes alternate in the address book's order: the first contact is male, the second female, and so on | todo | our tree reads no address book or mail; our tree reads no address book or mail; the game cannot tell a contact's sex |
| A contact has no job of its own: it can be given to a villager of any job | todo | our tree reads no address book or mail; our tree reads no address book or mail |
| A contact is married or not, at random (half and half) | todo | our tree reads no address book or mail; our tree reads no address book or mail; whether the game uses this afterwards is not confirmed (unconfirmed) |
| A contact belongs to no particular tribe | todo | our tree reads no address book or mail; our tree reads no address book or mail |
| A contact gets a random pet number from 0 to 30, and face 0 | todo | our tree reads no address book or mail; our tree reads no address book or mail; whether the game uses these afterwards is not confirmed (unconfirmed) |

## The built-in names

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's info table holds 48 built-in named villagers, each with name, age, sex, job, married, tribe, pet and face | partial | our tree reads the 48 into `InfoConstants::specialVillager` (`src/InfoConstants.h`) but nothing uses them |
| They are mostly Lionhead staff, their families and friends: for example Peter Molyneux (40), Mark Webley (36) and family, Mark Healey, Jean-Claude Cottier, Claire Hedley, Steve Jackson, Jonty Barnes and children such as Sam Webley (5) and Rebecca Webley (3) | todo | no named villagers in our tree; see ../villager/special_villagers.md; the names are read from `Scripts/info.dat` but unused |
| The built-in entries carry their own ages, sexes, jobs and tribes (for example a few are children, some are fishermen or foresters) | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| With 48 or more contacts, only contacts are used and no built-in name appears | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| With fewer than 48 contacts, the rest of the 48 places are filled with built-in names, starting from the first built-in entry | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| With no mail system at all, the pool is the 48 built-in names | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| In a multiplayer game the pool is the 48 built-in names, but no named villager is ever made there | todo | no named villagers in our tree; see ../villager/special_villagers.md |

## Giving the names to villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every time a villager is made, there is a 2 in 10 chance that the game tries to make it a named villager instead | partial | the roll is drawn (`ecs::villager::RollSpecialVillager`, from `VillagerArchetype`), keeping the game's random stream, but a normal villager always comes out ([villagers](../../bw1-notes/villagers.md)) |
| The name is chosen by starting at a random place in the pool and taking the first name that fits | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| A name fits if it is of the villager's sex, has not been used yet, and is not empty; the job is not checked | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| Each name is used once per land; the use counts are cleared when the land is cleared | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| When no name is left, the villager is made as an ordinary one | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| A named villager takes the name's age, unless it is being born (a newborn keeps age 1) | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| The saved game stores only the name's place in the pool, so a load picks whatever name is at that place in the pool built at start-up | todo | no named villagers in our tree; see ../villager/special_villagers.md |
| The name floats over the named villager's head, in the colour of the player who owns its town (white for none), when the camera is very close or the villager is picked out (unconfirmed which view setting) | todo | no named villagers in our tree; see ../villager/special_villagers.md; our tree only draws "Villager #n" debug labels (`src/Debug/Gui.cpp`) |

## New mail while playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Outside multiplayer, the game checks for new mail whenever the set check time has passed, or at once when the mail system asks to be checked | todo | our tree reads no address book or mail |
| On start-up the game notes the time; only mail received after it is ever shown, so old mail left on the server is not repeated each session | n/a | relies on the mail headers' times; the readme notes mail with unreadable times or odd time zones is dropped |
| Mail older than the game's start-up time, or whose time cannot be read, is not shown, and a line saying so is logged | n/a |  |
| With Outlook, all folders holding new mail are scanned, which can take minutes for big folders | n/a | from the readme |
| Each new mail gives an entry in the game's message list, with its sender and subject | todo | our tree reads no address book or mail |
| The game looks through the player's towns, house by house and then the homeless, for a living named villager whose name is the sender's | todo | our tree reads no address book or mail |
| If one is found, that villager says "&lt;name&gt;: Hey! I've sent you an e-mail about &lt;subject&gt;" in a speech bubble over its head | todo | our tree reads no address book or mail |
| Otherwise any living named villager whose name can be shown says "Hey! &lt;sender&gt; has sent you an e-mail about &lt;subject&gt;" | todo | our tree reads no address book or mail |
| With no named villager at all, nobody speaks; the message list still has the mail | todo | our tree reads no address book or mail |
| The speech bubble is 250 by 175 with a text size of 19, holds up to 1023 characters, floats 1.5 units over the head and lasts 6 seconds | todo | our tree reads no address book or mail |
| The bubble goes when it runs out, or when the villager dies or is removed | todo | our tree reads no address book or mail |
| The game may also point the player to the speaking villager (unconfirmed) | todo | our tree reads no address book or mail |
| A sender who is not in the address book is added to it and saved, and becomes a named villager on the next run | todo | our tree reads no address book or mail |

## openblack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A replacement source of names (a contacts file, or the system's contacts) | todo | the original mail helpers are Windows- and Outlook-only; our tree would need its own source, and a clear opt-in, to give the same feature |
| A switch to turn the feature off | todo | the original is off unless ticked in setup; our tree should stay off by default |
