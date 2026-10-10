/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicLoop.h"

#include <cstdio>
#include <cstdlib>

#include "Audio/Services/SpellSounds.h"
#include "Core/Players.h"
#include "Core/Spell.h"
#include "Core/SpellGrid.h"
#include "Core/SpellSeed.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireDebugHooks.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Town/TownProcess.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Vortex.h"
#include "ECS/Weather/WeatherLoop.h"
#include "GameClock.h"
#include "Hand/HandCasting.h"
#include "Locator.h"
#include "Magic/Objects/MagicTree.h"
#include "Objects/FallingSpell.h"
#include "Objects/MagicFireBall.h"
#include "Objects/ShieldDebugHooks.h"
#include "Particles/Creators/Chain.h"
#include "Particles/Creators/LightMap.h"
#include "Particles/Creators/Mesh.h"
#include "Particles/Creators/Mist.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Particles/Rules/LightningStrike.h"
#include "Particles/Utility.h"
#include "Spells/SpellStormAndTornado.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// The shields' objects (Locator::magicShieldSystem)
ecs::systems::MagicShieldSystemInterface& TheMagicShieldSystem()
{
	if (!Locator::magicShieldSystem::has_value())
	{
		std::fputs("magic: no shield service in the locator (Locator::magicShieldSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicShieldSystem::value();
}

/// The teleport stones (Locator::teleportSystem)
ecs::systems::TeleportSystemInterface& TheTeleportSystem()
{
	if (!Locator::teleportSystem::has_value())
	{
		std::fputs("magic: no teleport service in the locator (Locator::teleportSystem)\n", stderr);
		std::abort();
	}
	return Locator::teleportSystem::value();
}

/// What the tornadoes carry (Locator::tornadoSystem)
ecs::systems::TornadoSystemInterface& TheTornadoSystem()
{
	if (!Locator::tornadoSystem::has_value())
	{
		std::fputs("magic: no tornado service in the locator (Locator::tornadoSystem)\n", stderr);
		std::abort();
	}
	return Locator::tornadoSystem::value();
}

/// The game's fires; stops with a message when there are none (before the game or after it has gone)
ecs::systems::FireSystemInterface& Fires()
{
	if (!Locator::fireSystem::has_value())
	{
		std::fputs("magic: no fires in the locator (Locator::fireSystem)\n", stderr);
		std::abort();
	}
	return Locator::fireSystem::value();
}
} // namespace

void magic::OnLoadMap()
{
	ClearSpells();
	players::Reset();
	// every map cell is cleaned, both object lists emptied (ECS/MapCells). openblack's Registry::Reset follows (Game.cpp)
	ecs::map_cells::Clear();
	// the players' stone lists (Objects/MagicTeleport); also registers the REACT_TO_TELEPORT spread
	TheTeleportSystem().Reset();
	ecs::effects::reactions::Clear();
	Fires().Reset(); // the fires, then their graphics
	ecs::fire::ResetDebugHooks();
	fireball::Clear();
	TheMagicShieldSystem().Reset();  // Magic/Objects/MapShield, then Magic/Spells/SpellShield
	spell_storm::Clear();            // Magic/Spells/SpellStormAndTornado
	ecs::villager_fire::Clear();     // also registers the REACT_TO_FIRE spread
	ecs::villager_shield::Clear();   // ECS/Systems/Implementations/VillagerShield: the shield reaction states
	ecs::villager_mourning::Clear(); // ECS/Villager/VillagerMourning: the REACT_TO_DEATH followers and takers
	spell_grid::Clear();
	magic_tree::Clear(); // Magic/Objects/MagicTree (the forests that lost a magic tree)
	ecs::systems::hand_grain::Reset();
	// the climates, storms, rain and weather things (ECS/Weather)
	Locator::weatherSystem::value().Reset();
	hand_casting::OnLoadMap();       // Hand/HandCasting.cpp: the gestures, the hand FX, the utility effects
	psys::explode_object::Clear();   // the exploded meshes' queues
	psys::lightning_strike::Clear(); // the storms' queued fork strikes
	worship::OnLoadMap();            // Worship/Worship.cpp (after hand_casting: it registers the icon provider)
	ResetDebugHooks();
}

void magic::ProcessGameInputs()
{
	// the game inputs (the power-up system with the last frame's time) run before the game code
	hand_casting::ProcessTurn(); // Hand/HandCasting.cpp
}

void magic::ProcessTurnStart(uint32_t turn)
{
	// The game turn, the order of the calls.
	//  1 the atmosphere's game update
	weather::ProcessTurnStart(turn); // ECS/Weather/WeatherLoop.cpp
	//  2 the influence rings (+ the towns' influence)
	Locator::influenceSystem::value().ProcessTurn(turn);
	influence::RunDebugHooks(); // OPENBLACK_TEST_INFLUENCE (ECS/Influence/InfluenceDebugHooks.cpp)
	//  3 the players: first each town's process (ECS/Town), then the teleport stones' travellers, and the alignment.
	//    (approximate) The original does the three player by player; here each runs for all the players
	ecs::town_process::ProcessPlayers();       // ECS/Town/TownProcess.cpp
	TheTeleportSystem().ProcessTurn();         // Objects/MagicTeleport.cpp
	ecs::effects::alignment::ProcessPlayers(); // ECS/Effects/Alignment.cpp
	//    then the players' influence power (the heart beat reads it)
	influence::CalculateInfluencePowers(); // ECS/Influence/InfluenceSources.cpp
	TheTeleportSystem().RunDebugHooks();   // OPENBLACK_TEST_TELEPORT (Objects/TeleportDebugHooks.cpp)
	// the local player's interface alignment, after the loop over the active players
	ecs::effects::alignment::UpdateInterfaceAlignment();
	//  4 the dances
	//    (and the first turn's post-load clean-up, the worship test hooks, the spell dispensers)
	worship::ProcessTurn(turn); // Worship/Worship.cpp
	                            // --- the global game lists: Game.cpp's ProcessPuzzleGamesTurn, between 4 and 5
}

void magic::ProcessForests(uint32_t turn)
{
	//  5 the forests and their trees: ECS/Trees.cpp (the spell forests are made by Magic/Spells/SpellForest)
	ecs::ProcessTreesTurn(turn);
	// --- the living things: Game.cpp's livingActionSystem, between 5 and 6
}

void magic::ProcessTurn(uint32_t turn)
{
	//  6 the fire effects
	ecs::fire::RunDebugHooks(turn); // OPENBLACK_TEST_FIRE (ECS/Fire/FireDebugHooks.cpp)
	ecs::fire::graphic::SetTurn(turn);
	Fires().ProcessTurn();
	//  7 the reactions (the data only: the turn stamp)
	// (the reactions' clock is set at the start of the turn: Game.cpp, reactions::BeginTurn)
	//  8 the spells
	ProcessSpells(turn);
	fireball::ProcessTurn(turn); // Magic/Objects/MagicFireBall: the balls whose atoms are gone
	                             //  9 the particle containers (the particle system's turn, at the end of Game.cpp's scripts
	                             //    block; the spells' own PSys were stepped in 8)
	                             // 10 the physics' GameTurnUpdate (already in openblack)
	// 11 ProcessSpellParticlesEndOfLoop, 13 ProcessHandTurn: called by Game::GameLogicLoop at their steps
}

void magic::ProcessSpellParticlesEndOfLoop()
{
	// 11 the PSys game loop end, first each vortex's turn (its contents, its light map, the fades' end, the land under
	//    it; ECS/Vortex). (pending) the object mover's step
	ecs::vortex::ProcessAll(game_clock::Turn());
	// 11 then the PSys sounds
	//    (first the EXPLODE_OBJECT effect empties the exploded meshes' queue)
	psys::explode_object::GameLoopEnd(); // Particles/Rules/ExplodeObject.cpp
	//    then the other utility effects: SF_OnFire, SF_ManaPathNew, SF_BeliefSprite, SF_Gesture, SF_LightningStrike
	psys::utility::ProcessTurn(); // Particles/Utility.cpp
	//    (the turn's length in seconds)
	audio::spell_sounds::ProcessTurn(static_cast<float>(game_clock::MsPerTurn()) * 0.001f); // Audio/Services/SpellSounds.cpp
}

void magic::ProcessHandTurn()
{
	// (Game.cpp: the scripts, the weather things, the bookmarks, the climate and the belief come before)
	// 13 the hand's turn: the grain's raise (ECS/.../HandGrain.cpp), a held object no longer available is thrown, the
	//    hold type is updated, the clean-up; (pending) two more hand fields. The held object's ProcessInHand is NOT
	//    here: it runs at the turn's start (HandSystem::ProcessTurn, HandTurn.cpp)
	//    (the turn's length in seconds)
	ecs::systems::hand_grain::GameTurnUpdate(static_cast<float>(game_clock::MsPerTurn()) * 0.001f);
	//    then the hold (HandSystem::GameTurnUpdate, HandTurn.cpp)
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().GameTurnUpdate();
	}
	// (the rewards come after the dead list: not here)
}

void magic::Update(float seconds)
{
	// the one-off spell seeds, with the game time step
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().UpdateGlobes(seconds);
	}
	// the atmosphere's 3D update: the rain streaks (the rain system)
	weather::UpdateFrame(seconds);
	// the effects' mesh atoms move between turns: the instances are rebuilt every frame while there are any
	if (psys::mesh_atoms::Any())
	{
		Locator::entitiesRegistry::value().SetDirty();
	}
	// the objects' ghosts melt with the game time step; drawn every frame while there are any
	ecs::object_ghosts::Update(seconds * 1000.0f);
	if (ecs::object_ghosts::Count() != 0)
	{
		Locator::entitiesRegistry::value().SetDirty();
	}
	// the PSys mists (the water cloud) go to mists::Submit (Particles/Creators/Mist.cpp)
	psys::mist_atoms::SubmitFrame(seconds * 1000.0f);
	// the light maps' land stamps (land_light)
	psys::light_map_atoms::SubmitFrame();
	// the chains' v-scroll with the game time step (Particles/Creators/Chain.cpp)
	psys::chain_atoms::AdvanceScroll(seconds * 1000.0f);
	// what the tornados carry follows its atom (Locator::tornadoSystem, Particles/Rules/Storm.cpp)
	TheTornadoSystem().Update();
	// the landscape vortexes' decal state and ground effects with the frame time, before every other effect's frame step
	// below, as they are drawn before the land (ECS/Vortex.cpp)
	ecs::vortex::UpdateGroundEffects(seconds);
	// the flames, steam and smoke with the game time step
	Fires().Update(seconds);
	// the gesture sampling and the power-up system (the frame's inputs), the in-hand PSys, the hand FX and the
	// utility effects (Hand/HandCasting.cpp)
	hand_casting::Update(seconds);
	// the icons' charge rings, the sites' strain pulse, the totems' rise (Worship/Worship.cpp)
	worship::Update(seconds);
	// the physical shields' drawing (the matrix lerped over the turn, the alpha)
	TheMagicShieldSystem().Update(seconds);
	// the seeds that follow their spell, drawn over it (Core/SpellSeed.cpp)
	seed::DrawSpells();
	shield_debug::OnFrame(); // OPENBLACK_TEST_SHIELD_FRAMES (test hook, ShieldDebugHooks.cpp)
	// the teleport stones' vortex, stepped with the frame time (Locator::teleportSystem, Objects/MagicTeleport.cpp)
	TheTeleportSystem().Update(seconds);
	// the landscape vortexes' effects over the land and their light maps' alpha, with the frame time (ECS/Vortex.cpp)
	ecs::vortex::UpdateOverLandEffects(seconds);
	// the falling spell and its finish frame callback (the sparks, the light bursts, the fall.cm2 camera) with the
	// frame time, after video::GetFallingSpell().ProcessFrame (Game.cpp)
	falling_spell::FrameUpdate();
}
