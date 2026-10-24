# Real weather

Outside multiplayer, the game could fetch the real weather where the player lives from Lionhead's weather service on
the internet, and turn it into the land's background weather. The service is gone, and nothing of it runs without
the service: the presets are only applied after a fetch that succeeded, and the cache the game wrote is never read
back. So it is not progress; what the original did is kept in
[not-in-the-original.md](../../not-in-the-original.md#pc_integrationreal_weather), with the evidence. The land's own
weather is owned by [../weather/](../weather/climates.md).

**Progress: 1/1 done, 0 chosen, 0 partial, 0 inferred — 100%**

## openblack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack's weather has calm, temperature 0 background air that storms add to | done | the ambient weather is all 0 in a game (`weather::atmos::Ambient`, `src/ECS/Weather/Atmos.h`), as the original without the internet weather; climates and storms add to it (`src/ECS/Weather/Climate.cpp`) |
