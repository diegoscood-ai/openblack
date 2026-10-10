# Online services

The game's links to the internet outside network play: real weather where the player lives, villagers named after the
player's e-mail contacts, and uploading the creature.

**Progress: 0/2 done, 0 partial — 0%**

## Online extras

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Real weather: the land's weather follows the weather at the player's town, fetched online | n/a | the original's weather service is gone; our tree has no online code; see [../pc_integration/real_weather.md](../pc_integration/real_weather.md) |
| Choosing the player's country and town for the real weather | n/a | as above |
| Villagers are named after the player's e-mail contacts | todo | no address-book names in our tree; see [../pc_integration/villager_names_from_contacts.md](../pc_integration/villager_names_from_contacts.md) |
| New e-mail is announced by a villager in the game | n/a | relies on Outlook or a POP3 account |
| The creature is packed into a file for uploading and sharing | todo | our tree reads creature mind files (`resources::CreatureMindLoader`, `Locator::resources`), but not the original's upload format; see [../pc_integration/online_services.md](../pc_integration/online_services.md) |
| The registration site for online play | n/a | the original's site is gone |
