/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicLoop.h"

#include "Audio/Services/SpellSounds.h"
#include "Core/OneOffSpellSeed.h"
#include "Core/Players.h"
#include "Core/Spell.h"
#include "Core/SpellGrid.h"
#include "Core/SpellSeed.h"
#include "Objects/FallingSpell.h"
#include "Objects/MagicFireBall.h"
#include "Objects/MapShield.h"
#include "Objects/ShieldDebugHooks.h"
#include "Spells/SpellShield.h"
#include "Spells/SpellStormAndTornado.h"
#include "Objects/MagicTeleport.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Trees.h"
#include "Magic/Objects/MagicTree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireDebugHooks.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireGraphic.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Town/TownProcess.h"
#include "ECS/Weather/WeatherLoop.h"
#include "GameClock.h"
#include "Hand/HandCasting.h"
#include "Locator.h"
#include "PSys/Creators/Chain.h"
#include "PSys/Creators/LightMap.h"
#include "PSys/Creators/Mesh.h"
#include "PSys/Creators/Mist.h"
#include "PSys/Rules/ExplodeObject.h"
#include "PSys/Utility.h"
#include "PSys/Rules/Storm.h"
#include "Worship/Worship.h"

using namespace openblack;
using namespace openblack::magic;

void magic::OnLoadMap()
{
	ClearSpells();
	players::Reset();
	// GlobalGameLists::ClearMap 0x591A92 -> fn_006013D0 (g_game +0x59B8, the GMap): MapCell::Clean 0x601380 on every
	// cell (0x6013FF), both object lists emptied (ECS/MapCells). openblack's Registry::Reset follows (Game.cpp)
	ecs::map_cells::Clear();
	teleport::Clear(); // Objects/MagicTeleport: the players' stone lists (also registers the REACT_TO_TELEPORT spread)
	ecs::effects::reactions::Clear();
	ecs::fire::Clear();                   // ECS/Fire
	ecs::fire::graphic::Clear();
	ecs::fire::ResetDebugHooks();
	fireball::Clear();
	map_shield::Clear();   // Magic/Objects/MapShield
	spell_shield::Clear(); // Magic/Spells/SpellShield
	spell_storm::Clear();  // Magic/Spells/SpellStormAndTornado
	ecs::villager_fire::Clear();          // also registers the REACT_TO_FIRE spread
	ecs::villager_shield::Clear();        // ECS/Systems/Implementations/VillagerShield: the shield reaction states
	ecs::villager_mourning::Clear();      // ECS/Villager/VillagerMourning: the REACT_TO_DEATH followers and takers
	spell_grid::Clear();
	magic_tree::Clear(); // Magic/Objects/MagicTree (the forests that lost a magic tree)
	ecs::systems::hand_grain::Reset();
	weather::OnLoadMap(); // ECS/Weather/WeatherLoop.cpp
	hand_casting::OnLoadMap(); // Hand/HandCasting.cpp: the gestures, the hand FX, the utility effects
	psys::explode_object::Clear(); // PSysGlobal::OnClearMap 0x68F820: the exploded meshes' queues
	worship::OnLoadMap();      // Worship/Worship.cpp (after hand_casting: it registers the icon provider)
	ResetDebugHooks();
}

void magic::ProcessGameInputs()
{
	// GGame::ProcessOneGameTurn 0x54D620 runs ProcessGameInputs (GInterface::Process: ProcessPowerUpSystem with the last
	// frame's time) before ProcessGameCode                                     [M2 gestures]
	hand_casting::ProcessTurn(); // Hand/HandCasting.cpp
}

void magic::ProcessTurnStart(uint32_t turn)
{
	// GGame::ProcessTurn 0x54E5C0, the order of the calls. Each lane adds its line in its slot.
	//  1 LH3DAtmos::UpdateGame 0x8356E0                                     [M6a weather]
	weather::ProcessTurnStart(turn); // ECS/Weather/WeatherLoop.cpp
	//  2 InfluenceRing::ProcessRings 0x5CDB90 (+ the towns' influence)       [M1i influence]
	influence::ProcessTurn();
	influence::RunDebugHooks(); // OPENBLACK_TEST_INFLUENCE (ECS/Influence/InfluenceDebugHooks.cpp)
	//  3 GPlayer::ProcessPlayers 0x649A20 -> GPlayer::Process 0x6494E0: first each town's Town::Process (0x649551, ECS/Town
	//    of the session asistente), then fn_005FCC70 (0x6496BC), the teleport stones' travellers, and the alignment
	//    (0x6496C5 -> GAlignment::ProcessForPlayer 0x4141A0). (aproximado) The original does the three player by player;
	//    here each runs for all the players
	ecs::town_process::ProcessPlayers(); // ECS/Town/TownProcess.cpp
	teleport::ProcessPlayers(); // Objects/MagicTeleport.cpp
	ecs::effects::alignment::ProcessPlayers(); // ECS/Effects/Alignment.cpp
	//    then the influence power +0x8C (GPlayer::CalculateInfluencePower 0x64AD00, at 0x64971D; the heart beat and
	//    fn_0064B700 read it)
	influence::CalculateInfluencePowers(); // ECS/Influence/InfluenceSources.cpp
	teleport::RunDebugHooks();  // OPENBLACK_TEST_TELEPORT (Objects/TeleportDebugHooks.cpp)
	// fn_0064AC30 on the local player, after the GetNextActivePlayer loop of GPlayer::Process (0x64A666..0x64A697)
	ecs::effects::alignment::UpdateInterfaceAlignment();
	//  4 Dance::ProcessDances 0x50BB60                                      [M7]
	//    (and the first turn's GPlayer::PostLoadCleanup, the worship test hooks, the spell dispensers)
	worship::ProcessTurn(turn); // Worship/Worship.cpp
	// --- GlobalGameLists::Process 0x591370 (0x54E651): Game.cpp's ProcessPuzzleGamesTurn, between 4 and 5
}

void magic::ProcessForests(uint32_t turn)
{
	//  5 Forest::ProcessForests 0x539D70 -> Forest::Process 0x539DA0 (Tree::Process 0x74A290 of their trees): ECS/Trees.cpp
	//    of the "arboles" session (the spell forests are made by Magic/Spells/SpellForest)
	ecs::ProcessTreesTurn(turn);
	// --- Living::ProcessLiving 0x5EC810 (0x54E65B): Game.cpp's livingActionSystem, between 5 and 6
}

void magic::ProcessTurn(uint32_t turn)
{
	//  6 FireEffect::ProcessList 0x730760                                   [M5 fire]
	ecs::fire::RunDebugHooks(turn); // OPENBLACK_TEST_FIRE (ECS/Fire/FireDebugHooks.cpp)
	ecs::fire::graphic::SetTurn(turn);
	ecs::fire::ProcessList(); // ECS/Fire/FireEffect.cpp
	//  7 Reaction::ProcessReactions 0x6E3B50 (the data only: the turn stamp)
	// (the reactions' clock is set at the start of the turn: Game.cpp, reactions::BeginTurn)
	//  8 Spell::ProcessSpells 0x720300
	ProcessSpells(turn);
	fireball::ProcessTurn(turn); // Magic/Objects/MagicFireBall: the balls whose atoms are gone
	//  9 GParticleContainer::ProcessParticleContainers (psys::manager::ProcessTurn, at the end of Game.cpp's scripts
	//    block; the spells' own PSys were stepped in 8)
	// 10 the physics' GameTurnUpdate (already in openblack)
	// 11 ProcessPSysGameLoopEnd, 13 ProcessHandTurn: called by Game::GameLogicLoop at their steps
}

void magic::ProcessPSysGameLoopEnd()
{
	// 11 PSysGlobal::GameLoopEnd 0x68F5B0 (0x54E688) -> fn_006D11A0, the PSys sounds    [S sounds]
	//    (first fn_006721B0 -> fn_006717F0: the EXPLODE_OBJECT effect empties the exploded meshes' queue)
	psys::explode_object::GameLoopEnd(); // PSys/Rules/ExplodeObject.cpp
	//    then fn_006721B0's other slots: SF_OnFire, SF_ManaPathNew, SF_BeliefSprite, SF_Gesture, SF_LightningStrike
	psys::utility::ProcessTurn(); // PSys/Utility.cpp
	//    (fn_006D11A0 0x6D11AB..0x6D11C5: [0xD01A38] x 0.001)
	audio::spell_sounds::ProcessTurn(static_cast<float>(game_clock::MsPerTurn()) * 0.001f); // Audio/Services/SpellSounds.cpp
}

void magic::ProcessHandTurn()
{
	// (Game.cpp: GScript::Process, the weather things, the bookmarks, the climate and the belief come before, 0x54E693..)
	// 13 CHand::GameTurnUpdate 0x46E4E0: first HandStateGrain's raise (fn_005B2D70, ECS/.../HandGrain.cpp), then the held
	//    object's ProcessInHand (a spell seed: SpellSeed::ProcessInHand)
	//    (0x46E4E3..0x46E4FB: [0xD01A38] x 0.001 [0x8AA3B0])
	ecs::systems::hand_grain::GameTurnUpdate(static_cast<float>(game_clock::MsPerTurn()) * 0.001f);
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		auto& registry = Locator::entitiesRegistry::value();
		if (held.has_value() && registry.Valid(*held) && registry.AllOf<ecs::components::SpellSeed>(*held))
		{
			seed::ProcessInHand(*held);
		}
		else if (held.has_value() && registry.Valid(*held) &&
		         influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, ecs::fire::traits::FireCentre(*held)) > 0.0f)
		{
			// Object::ProcessInHand 0x639AD0: inside the holder's influence it catches the fires it is held over
			// (inferido: openblack has only the local player's hand, taken as PLAYER_ONE)
			ecs::fire::CheckToSeeIfObjectIsNearOnFireObject(*held);
		}
	}
	// (Reward::ProcessList 0x6E6890 comes after GameThing::ProcessDeadList, 0x54E70C: not here)        [M7b]
}

void magic::Update(float seconds)
{
	// OneOffSpellSeed::UpdateFrame 0x72A570 with g_game_time_inc (ms)
	one_off::UpdateFrames(seconds * 1000.0f);
	// LH3DAtmos::Update3D 0x8357A0 (GGame::Process3dEngine): the rain streaks (ECS/Weather/Rain.cpp)
	weather::UpdateFrame(seconds);
	// the effects' mesh atoms move between turns: the instances are rebuilt every frame while there are any
	if (psys::mesh_atoms::Any())
	{
		Locator::entitiesRegistry::value().SetDirty();
	}
	// RenderParticleMist::DrawAt 0x67A670: the PSys mists (the water cloud) go to mists::Submit (PSys/Creators/Mist.cpp)
	psys::mist_atoms::SubmitFrame(seconds * 1000.0f);
	// ParticleLightMap::DrawAt 0x67B220 -> PSysLightMaps::AddDrawing 0x6CA6E0: the light maps' land stamps (land_light)
	psys::light_map_atoms::SubmitFrame();
	// fn_0067B3F0 0x67BE88: the chains' v-scroll with g_game_time_inc (PSys/Creators/Chain.cpp)
	psys::chain_atoms::AdvanceScroll(seconds * 1000.0f);
	// RenderParticleGameObject::DrawAt 0x67B170: what the tornados carry follows its atom (PSys/Rules/Storm.cpp)
	psys::storm::UpdateCarriedObjects();
	// FireEffect::Draw 0x730330 -> fn_00731560 with g_game_time_inc: the flames, steam and smoke (ECS/Fire/FireGraphic)
	ecs::fire::graphic::Update(seconds);
	// the gesture sampling and ProcessPowerUpSystem (GGame::ProcessFrameInputs), the in-hand PSys, the hand FX and the
	// utility effects (Hand/HandCasting.cpp)
	hand_casting::Update(seconds);
	// the icons' charge rings, the sites' strain pulse, the totems' rise (Worship/Worship.cpp)
	worship::Update(seconds);
	// Spell::DrawSpells -> fn_0072BF50: the physical shields' DrawShield (the matrix lerped over the turn, the alpha)
	map_shield::DrawShields();
	// Spell::DrawSpells 0x7203F0 -> Spell::Draw 0x720430 -> DrawSpellSeed 0x721360 -> 0x729020: the seeds that follow
	// their spell, over it (Core/SpellSeed.cpp)
	seed::DrawSpells();
	shield_debug::OnFrame(); // OPENBLACK_TEST_SHIELD_FRAMES (test hook, ShieldDebugHooks.cpp)
	// MagicTeleport::Draw 0x5FCCC0: the stones' vortex, stepped with the frame time (Objects/MagicTeleport.cpp)
	teleport::UpdateFrame(seconds);
	// Process3dEngine case 2 0x54DDE0 FallingSpell::Draw and its finish frame callback 0x526480 (the sparks, the
	// light bursts, the fall.cm2 camera) with g_delta_time, after video::GetFallingSpell().ProcessFrame (Game.cpp)
	falling_spell::FrameUpdate();
}
