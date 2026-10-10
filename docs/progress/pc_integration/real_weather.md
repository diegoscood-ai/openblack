# Real weather

Outside multiplayer, the game could fetch the real weather where the player lives from Lionhead's weather service on
the internet, and turn it into the land's background weather: its temperature, cloud, rain, snow and wind, sometimes
with a storm. The player's town came from their account on the game's web site. The service is long gone, so in
openblack the fetching itself cannot work; what a replacement would need is listed at the end. The land's own weather
is owned by [../weather/](../weather/climates.md); the short summary row is in
[../weather/scripted_weather.md](../weather/scripted_weather.md) under "Real-world weather", and the service is listed with
the other online extras in [online_services.md](online_services.md).

**Progress: 1/31 done, 0 partial — 3%**

## Turning it on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The real weather is set up once as a game starts, never in a multiplayer game | todo | no real weather in our tree; the original log says it asks whether this is a multiplayer game and, if not, starts the weather system |
| It needs the weather preset file `Scripts/weatherinfo.lhw`; without it nothing happens | todo | our tree does not read `Scripts/weatherinfo.lhw` |
| Its settings live under the game's setup key in the registry: whether to use it, the server's name, the server's port and the address of the query page | n/a | value names: UseInternetWeatherSystem, WeatherSystemServerName, WeatherSystemServerPort, WeatherSystemServerURL |
| If the settings are missing, the game writes the defaults: use it = 1, server `weather.bwgame.com`, port 80, page `/query/` | n/a | the server is gone |
| The query is only made when the "use" value reads 0 | n/a | the game writes 1 as the default and then only runs when it reads 0, so it looks inverted; what the setup program wrote for "on" is not confirmed (unconfirmed) |
| The server name and page may each be up to 200 characters | n/a |  |
| It needs the current online player profile's login name and password; with no login name it does nothing | n/a | the password is kept encrypted in the profile and decrypted for the query; the accounts are gone |
| With no internet connection found at start-up, no query is made | todo | our tree makes no connection check (no online code) |
| There is no in-game option screen for it; it is set up outside the game | todo | no real weather in our tree and no option for it |

## Where the player lives

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's place is not typed into the game: the game asks the web site for the place stored with the player's account | n/a | the account (registered at the game's web site) held a country, an area and a city |
| The first request is a web page fetch from the weather server with the action "query user data", the user's name, password and today's date | n/a | the date is the computer's local date written day.month.year with two-digit day and month and a four-digit year |
| The reply starts with a status line that must be 1; anything else is a failure and the reply is thrown away | n/a |  |
| The next three lines are the country, the area and the city | n/a | the country is a two-letter code, the area and city up to 150 characters each |
| Country codes are named from the game's list of 242 countries, one "code=Name" per line (for example de=Germany, gb=Great Britain, uk=United Kingdom, us=United States) | todo | our tree does not read `Scripts/country.lhw` |

## Fetching the weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second request asks with the action "query weather", the country, the area, the city and today's date | n/a | plain web page fetch over port 80 with the game's own web client |
| The reply is lines of colon-separated fields, one line per day | n/a |  |
| Each line gives a date (three numbers), then six numbers: the temperature, the visibility, the weather type and three more | n/a | the meanings of the three others are not confirmed (unconfirmed); fields after the ninth are ignored |
| A line whose first field is empty is skipped | n/a |  |
| The days fetched are kept in a list and written to `weather.dat` in the player's profile folder | todo | our tree writes no `weather.dat` |
| The first day line in the reply becomes the player's weather | n/a | confirmed in the game's code during the bwgame.com service work (C:\projectswgame-service, docs/protocol.md); an earlier reading said the last line |
| The weather is fetched only once per game start, not refreshed while playing | todo | no real weather in our tree |

## Turning it into the land's weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a turn, while it is on and outside multiplayer, the game checks whether new real weather is waiting | todo | no real weather in our tree |
| The first time, the land's own background weather (temperature, rain, snow, overcast, the two winds and snow cover) is kept so it can be put back | todo | no real weather in our tree; the ambient weather it would keep is there (`weather::atmos::Ambient`, `src/ECS/Weather/Atmos.h`) |
| The background temperature becomes the real temperature, and the background overcast becomes the real visibility number, as whole numbers | todo | no real weather in our tree; `weather::atmos::SetAmbient` could take the values (`src/ECS/Weather/Atmos.h`) |
| The weather type then picks one of 13 presets (types 0 to 12) from `weatherinfo.lhw`, each record 112 bytes | todo | `weatherinfo.lhw` is not read |
| A preset can set, add to or take away from each of: overcast, snow cover, snowfall, rain and the two winds | todo | `weatherinfo.lhw` is not read; each is capped at 100 in the game |
| A preset can also make a storm, with its own strength, size and look | todo | no real weather in our tree; storms themselves exist (`src/ECS/Weather/Storms.cpp`) |
| Types 0 and 1 change nothing and make no storm | todo | `weatherinfo.lhw` is not read by our tree |
| Type 2: a storm with rain 40 | todo | `weatherinfo.lhw` is not read by our tree |
| Type 3: a storm with snowfall 50 and snow cover 50 | todo | `weatherinfo.lhw` is not read by our tree |
| Type 4: a storm with no rain | todo | `weatherinfo.lhw` is not read by our tree |
| Type 5: a storm with rain 100, the heaviest rain | todo | `weatherinfo.lhw` is not read by our tree |
| Type 6: a storm with rain 50, and 10 more overcast | todo | `weatherinfo.lhw` is not read by our tree |
| Type 7: a storm with rain 30, snowfall 30 and snow cover 30 (sleet) | todo | `weatherinfo.lhw` is not read by our tree |
| Type 8: a storm with no rain or snow of its own | todo | `weatherinfo.lhw` is not read by our tree |
| Type 9: a storm with rain 40 | todo | `weatherinfo.lhw` is not read by our tree |
| Type 10: a storm with snowfall 100, snow cover 100, wind 1 each way and 100 more overcast (a blizzard) | todo | `weatherinfo.lhw` is not read by our tree |
| Type 11: a storm with snowfall 100 and snow cover 100 | todo | `weatherinfo.lhw` is not read by our tree |
| Type 12: a storm with overcast set to 2 | todo | `weatherinfo.lhw` is not read by our tree |
| A message joins the game's message list naming the place, "City (Country)", or "City (Area,Country)" when there is an area | todo | no real weather in our tree; no message list in our tree |
| The message carries one of nine weather pictures chosen from the type | todo | no real weather in our tree; types 1 → picture 0; 8 and 12 → 2; 9 → 3; 2 and 6 → 4; 3 → 5; 11 → 6; 7 → 7; 5 and 10 → 8; 0, 4 and others → 1 in the game |
| Turning it off puts the kept background weather back and removes the storm it made | todo | no real weather in our tree |

## Developer keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A developer key shows the weather got: country, area, city, type, temperature and visibility, or that the system is not loaded | n/a | developer keys of the original, not a player feature |
| A developer key reloads the presets, and another switches the real weather off and on, putting the land's weather back | n/a | as above |
| A developer key steps through the nine weather pictures in a test message | n/a | as above |

## openblack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack's weather has calm, temperature 0 background air that storms add to | done | the ambient weather is all 0 in a game (`weather::atmos::Ambient`, `src/ECS/Weather/Atmos.h`), as the original without the internet weather; climates and storms add to it (`src/ECS/Weather/Climate.cpp`) |
| A replacement weather source (a public weather service, or the player's own settings) that gives temperature, visibility and a type | todo | the original service is gone; a replacement would need a place chosen in the game's options, a type mapped onto the 13 presets, and an opt-in since it reaches the internet |
| Reading the 13 presets from `weatherinfo.lhw` | todo | nothing in our tree reads the file |
