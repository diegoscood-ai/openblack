/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysManager.h"

#include "TownBelief.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string_view>
#include <deque>
#include <iterator>
#include <list>
#include <tuple>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PSys/Creators/Mist.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// GSpotVisualInfo (info.dat DETAIL_SPOT_VISUAL) -> PARTICLE_TYPE -> spell file (name table 0xD4EBB0), and the default
/// life in turns. Empty: no spell file (the effect is drawn by other code or not at all).
struct SpotVisual
{
	std::string_view file;
	int life;
};
constexpr std::array<SpotVisual, 50> k_SpotVisuals = {{
    {"", 0},                            // 0 NONE
    {"", 100},                          // 1 APPLY_SPELL_EFFECT
    {"SF_GripLandscape", 100},          // 2 GRIP_LANDSCAPE
    {"", 100},                          // 3 SUCEED_CAST
    {"SF_FailedApply", 100},            // 4 FAIL_CAST
    {"", 200},                          // 5 FIREWORK_SINGLE
    {"SF_FireWorks", 200},              // 6 FIREWORKS
    {"SF_FireWorks", 200},              // 7 FIREWORKS_PU1
    {"SF_FireWorks", 200},              // 8 FIREWORKS_PU2
    {"SF_MagicObjectCreated", 100},     // 9 MAGIC_OBJECT_CREATED
    {"SF_CreatureTarget", 100},         // 10 COMMAND_SUCCEED
    {"SF_MagicObjectCreated", 100},     // 11 COMMAND_FAIL
    {"SF_CreatureTarget", -1},          // 12 CREATURE_TARGET
    {"SF_SimpleBeamCreatureCast", -1},  // 13 CREATURE_CAST_VISUAL
    {"SF_TeleportVillager", 30},        // 14 VILLAGER_TELEPORT
    {"", 30},                           // 15 FIRE_FX
    {"", 30},                           // 16 FIRE_FX_ON_OBJECT
    {"SF_VolFX", 30},                   // 17 MAGIC_FX
    {"SF_VolFXArtifact", -1},           // 18 MAGIC_FX_ON_OBJECT
    {"SF_VolFXCitadel", -1},            // 19 MAGIC_FX_ON_CITADEL
    {"SF_SimpleBeam", 30},              // 20 MAGIC_BEAM
    {"SF_SimpleBeamCitadel", -1},       // 21 MAGIC_BEAM_ON_CITADEL
    {"SF_Steam", 30},                   // 22 STEAM
    {"SF_Smoke", 30},                   // 23 SMOKE
    {"", 30},                           // 24 DUST
    {"SF_Bonfire", 30},                 // 25 BONFIRE
    {"SF_EvilSmoke", 30},               // 26 EVIL_SMOKE
    {"SF_MagicObjectCreated2", 30},     // 27 OBJECT_APPEAR
    {"", 30},                           // 28 OBJECT_DISAPPEAR
    {"SF_SmokeExplode", 30},            // 29 BANG
    {"", 30},                           // 30 SING_STONES_GLOW
    {"SF_PlayerIconFountain", 60},      // 31 PLAYER_ICON_FOUNTAIN
    {"SF_BeamExplosionCitadel", -1},    // 32 EXPLOSION_CITADEL
    {"SF_HealChakra", 60},              // 33 HEAL_FX
    {"SF_HighlightOnObject", -1},       // 34 HIGHLIGHT_ON_OBJECT
    {"SF_LightningSingleStrike", 40},   // 35 LIGHTNING_STRIKE
    {"SF_BeamExplosionFX", 60},         // 36 BEAM_EXPLOSION_FX
    {"SF_Butterflies", 200},            // 37 BUTTERFLIES
    {"SF_ButterfliesOnObject", 200},    // 38 BUTTERFLIES_ON_OBJECT
    {"SF_Flies", 200},                  // 39 FLIES
    {"SF_FliesOnObject", 200},          // 40 FLIES_ON_OBJECT
    {"SF_SimpleBeamCreatureSwap", 200}, // 41 MAGIC_BEAM_CREATURE_SWAP
    {"SF_Flash", 50},                   // 42 FLASH
    {"SF_TickerTape", 100},             // 43 TICKER_TAPE
    {"SF_ForestCreated", 50},           // 44 FOREST_CREATED
    {"SF_SingingStonesHeal", 100},      // 45 SINGING_STONES_HEAL
    {"SF_SparklesFromObject", 100},     // 46 PILEFOOD_SPEEDUP
    {"SF_SeeThisBeam", 100},            // 47 SEE_THIS_BEAM
    {"", 100},                          // 48 SEE_THIS_BEAM2
    {"", 100},                          // 49 TEST
}};

struct Running
{
	std::unique_ptr<Effect> effect;
	bool ownedBySpell {false}; ///< stepped by its spell (StartForSpell), not by ProcessTurn
	bool perFrame {false};     ///< stepped every frame (the hand's and the interface's effects): drawn as last stepped
	bool inContainer {false};  ///< a GParticleContainer's (CreateSpotVisual): stepped by its container in ProcessTurn
	DrawPath path {DrawPath::Sorted}; ///< SetDrawPath; Spell::Draw's Draw_(1) (0x720441) until changed
};

struct Container
{
	uint32_t effect {0};
	entt::entity object {entt::null}; ///< the script's handle
	entt::entity owner {entt::null};
	int turns {-1}; ///< -1: forever
	bool hadOwner {false};
};

/// The running effects in the order the original's owner lists walk them: every one of them (GParticleContainer
/// fn_0063E0F0 0x63E151, Spell 0x71FC4C, MapShield 0x72C0AC, the seed graphics 0x726E70, the vortices 0x5FEA49) puts
/// a new one at the head and walks from the head, so the newest comes first; looked up by id. (openblack had an
/// unordered_map: stepped and drawn in hash order)
class EffectList
{
public:
	using List = std::list<std::pair<const uint32_t, Running>>;
	using iterator = List::iterator;

	/// the effect of `id`, made at the head when there is none
	Running& operator[](uint32_t id)
	{
		if (const auto it = find(id); it != end())
		{
			return it->second;
		}
		_list.emplace_front(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple());
		_index[id] = _list.begin();
		return _list.front().second;
	}
	iterator find(uint32_t id)
	{
		const auto it = _index.find(id);
		return it == _index.end() ? _list.end() : it->second;
	}
	iterator erase(iterator it)
	{
		_index.erase(it->first);
		return _list.erase(it);
	}
	void erase(uint32_t id)
	{
		if (const auto it = find(id); it != end())
		{
			erase(it);
		}
	}
	void clear()
	{
		_list.clear();
		_index.clear();
	}
	iterator begin() { return _list.begin(); }
	iterator end() { return _list.end(); }

private:
	List _list;
	std::unordered_map<uint32_t, iterator> _index;
};

EffectList g_Effects;
/// GParticleContainer's list (g_game +0x205BCC, next +0x3C): a new one at the head (fn_0063E0F0 0x63E151..0x63E168)
std::deque<Container> g_Containers;
uint32_t g_NextId = 1;
bool g_DebugDone = false;
} // namespace

uint32_t manager::Start(const std::string& file, glm::vec3 origin, float magnitude, game_random::psys::NetGameType type)
{
	return Start(File::Load(file), origin, magnitude, type);
}

uint32_t manager::Start(std::shared_ptr<const File> data, glm::vec3 origin, float magnitude,
                        game_random::psys::NetGameType type)
{
	if (!data)
	{
		return 0;
	}
	const uint32_t id = g_NextId++;
	g_Effects[id].effect = std::make_unique<Effect>(std::move(data), origin, magnitude, type);
	return id;
}

uint32_t manager::StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude,
                                SpellSink* sink, game_random::psys::NetGameType type)
{
	const uint32_t id = Start(file, origin, magnitude, type);
	if (id == 0)
	{
		return 0;
	}
	auto& running = g_Effects[id];
	running.ownedBySpell = true;
	running.effect->SetDirection(direction);
	running.effect->SetSink(sink);
	return id;
}

bool manager::ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt)
{
	const auto it = g_Effects.find(id);
	if (it == g_Effects.end())
	{
		return false;
	}
	auto& effect = *it->second.effect;
	effect.SetProcessInfo(info);
	effect.Step(dt);
	if (effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown()))
	{
		g_Effects.erase(it);
		return false;
	}
	return true;
}

void manager::Delete(uint32_t id)
{
	g_Effects.erase(id);
}

void manager::SetPerFrame(uint32_t id)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.perFrame = true;
	}
}

void manager::SetDrawPath(uint32_t id, DrawPath path)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.path = path;
	}
}

manager::DrawPath manager::GetDrawPath(uint32_t id)
{
	const auto it = g_Effects.find(id);
	return it == g_Effects.end() ? DrawPath::Sorted : it->second.path;
}

manager::DrawPath manager::SpotVisualDrawPath(uint32_t singleZSort)
{
	// 0x63E190 cmp [info +0x4C], 1; sete -> container +0x38; fn_0063E240 0x63E24A: AddDrawing when set
	return singleZSort == 1 ? DrawPath::Queued : DrawPath::Sorted;
}

Effect* manager::Find(uint32_t id)
{
	const auto it = g_Effects.find(id);
	return it == g_Effects.end() ? nullptr : it->second.effect.get();
}

uint32_t manager::IdOf(const Effect* effect)
{
	for (const auto& [id, running] : g_Effects)
	{
		if (running.effect.get() == effect)
		{
			return id;
		}
	}
	return 0;
}

void manager::CloseDown(uint32_t id)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.effect->CloseDown();
	}
}

void manager::SetOrigin(uint32_t id, glm::vec3 origin)
{
	if (const auto it = g_Effects.find(id); it != g_Effects.end())
	{
		it->second.effect->SetOrigin(origin);
	}
}

namespace
{
/// The container of CreateSpotVisual / CreateSpotVisualTurns, for `turns` (nullopt: the entry's own life)
entt::entity CreateSpotVisualFor(int spotVisual, glm::vec3 position, std::optional<int> turns, entt::entity owner,
                                 float magnitude)
{
	if (spotVisual < 0 || spotVisual >= static_cast<int>(k_SpotVisuals.size()))
	{
		return entt::null;
	}
	const auto& info = k_SpotVisuals[static_cast<size_t>(spotVisual)];
	if (info.file.empty())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "PSys: spot visual {} has no spell file", spotVisual);
		return entt::null;
	}
	// GParticleContainer::Create 0x63E508 -> fn_0063E410: Create(..., NET_GAME_TYPE 1) (push 1 at 0x63E436), synced
	const uint32_t id = manager::Start(std::string(info.file), position, magnitude, game_random::psys::NetGameType::Synced);
	if (id == 0)
	{
		return entt::null;
	}
	// the container draws its effect by the entry's SingleZSort (fn_0063E0F0 0x63E190, fn_0063E240 0x63E26A / 0x63E277).
	// (inferido) without the info block, 1: every entry of info.dat that has a spell file has SingleZSort 1
	// (tmp_dis\psys\psys_report.md, the SPOT_VISUAL table)
	const uint32_t singleZSort = Locator::infoConstants::has_value()
	                                 ? Locator::infoConstants::value().spotVisual.at(static_cast<size_t>(spotVisual)).singleZSort
	                                 : 1u;
	manager::SetDrawPath(id, manager::SpotVisualDrawPath(singleZSort));
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const int life = turns.value_or(info.life);
	g_Effects[id].inContainer = true;
	g_Containers.push_front({id, object, owner, life, owner != entt::null});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys: spot visual {} ({}) at ({:.1f}, {:.1f}, {:.1f}) for {} turns", spotVisual,
	                   info.file, position.x, position.y, position.z, life);
	return object;
}
} // namespace

entt::entity manager::CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner,
                                       float magnitude)
{
	// turns = ftol(seconds x 1000 / turn ms); 0 takes the entry's life
	std::optional<int> turns;
	if (seconds < 0.0f)
	{
		turns = -1;
	}
	else if (seconds > 0.0f)
	{
		turns = static_cast<int>(seconds * 1000.0f / static_cast<float>(game_clock::MsPerTurn()));
	}
	return CreateSpotVisualFor(spotVisual, position, turns, owner, magnitude);
}

entt::entity manager::CreateSpotVisualTurns(int spotVisual, glm::vec3 position, int turns, entt::entity owner,
                                            float magnitude)
{
	// 0x63E580 passes its int straight to Create 0x63E4B0 -> fn_0063E410's +0x30 (0x63E489), unlike CreateSpotVisual
	// 0x63E540, which passes the entry's life (+0x44). Process 0x63E2A0..0x63E2B1: < 0 forever, else dec and closed when
	// <= 0, so 0 closes it at its first Process (not the entry's life)
	return CreateSpotVisualFor(spotVisual, position, std::optional(turns < 0 ? -1 : turns), owner, magnitude);
}

void manager::CloseSpotVisual(entt::entity object)
{
	// the container's effect closes down on the next turn, as when a script deletes it (ProcessTurn)
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && registry.Valid(object))
	{
		registry.Destroy(object);
	}
}

void manager::ProcessTurn(float turnSeconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	static const bool trace = std::getenv("OPENBLACK_PSYS_TRACE") != nullptr;
	static uint32_t turn = 0;
	++turn;
	// Process_ returns 5 (delete) when finished, or at once on close-down with DeleteOnCloseDown
	const auto step = [turnSeconds](uint32_t id, Effect& effect) {
		effect.Step(turnSeconds);
		if (trace && turn % 20 == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys trace: effect {} {} age {:.1f} atoms {} closing {}", id,
			                   effect.GetFile().name, effect.GetAge(), effect.AtomCount(), effect.Closing());
		}
		return effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown());
	};
	// GParticleContainer::ProcessParticleContainers 0x63E090: from the head (the newest, fn_0063E0F0 0x63E151..0x63E168),
	// the next taken first (0x63E0A3); each container's Process 0x63E280 closes its effect when its owner has gone or its
	// turns are over, sets its origin and steps it (Process_, 0x6736B0); 5 (finished) deletes the container
	// (ToBeDeleted 0x63E1D0) with its effect
	for (auto it = g_Containers.begin(); it != g_Containers.end();)
	{
		const auto effect = g_Effects.find(it->effect);
		if (effect == g_Effects.end())
		{
			if (registry.Valid(it->object))
			{
				registry.Destroy(it->object);
			}
			it = g_Containers.erase(it);
			continue;
		}
		const bool ownerGone = it->hadOwner && !registry.Valid(it->owner);
		const bool deleted = !registry.Valid(it->object);
		if (ownerGone || deleted)
		{
			effect->second.effect->CloseDown();
		}
		else if (it->turns >= 0 && --it->turns <= 0)
		{
			effect->second.effect->CloseDown();
		}
		// (pending) 0x63E2F9 sets the container's own stored position (+0x14); openblack follows the owner's Transform
		// while it has one: who moves the container in the original (0x63E3E0's callers) is not read yet
		if (it->hadOwner && !ownerGone)
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(it->owner); transform != nullptr)
			{
				effect->second.effect->SetOrigin(transform->position);
			}
		}
		else if (!deleted)
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(it->object); transform != nullptr)
			{
				effect->second.effect->SetOrigin(transform->position);
			}
		}
		if (step(effect->first, *effect->second.effect))
		{
			g_Effects.erase(effect);
			if (registry.Valid(it->object))
			{
				registry.Destroy(it->object);
			}
			it = g_Containers.erase(it);
			continue;
		}
		++it;
	}
	// The effects of no container and no spell (the spell dispensers', the flying flock's cast, a test effect): stepped
	// here once a turn, newest first. (pending) the original steps those two in their owner's Draw with the frame's ms
	// (SpellDispenser::Draw 0x722940 0x7229FE, FlockFlying::Draw 0x724100 0x7241D1; documentacion/motor/psys_order.md)
	for (auto it = g_Effects.begin(); it != g_Effects.end();)
	{
		if (it->second.ownedBySpell || it->second.inContainer)
		{
			++it;
			continue;
		}
		if (step(it->first, *it->second.effect))
		{
			it = g_Effects.erase(it);
			continue;
		}
		++it;
	}
}

void manager::RunDebugHooks()
{
	// OPENBLACK_TEST_PSYS="SF_Name,x,z[,height[,magnitude[,seconds]]]": that spell file at (x, ground + height, z);
	// seconds > 0 closes it down after that long
	if (g_DebugDone || !Locator::terrainSystem::has_value())
	{
		return;
	}
	g_DebugDone = true;
	const char* test = std::getenv("OPENBLACK_TEST_PSYS");
	if (test == nullptr)
	{
		return;
	}
	char name[64] = {};
	float x = 0.0f, z = 0.0f, height = 0.0f, magnitude = 1.0f, seconds = -1.0f;
	if (std::sscanf(test, "%63[^,],%f,%f,%f,%f,%f", name, &x, &z, &height, &magnitude, &seconds) >= 3)
	{
		const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
		const auto id = Start(name, glm::vec3(x, ground + height, z), magnitude);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys test: {} at ({}, {}, {}) -> effect {}", name, x, ground + height, z, id);
		if (id != 0 && seconds > 0.0f)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto object = registry.Create();
			registry.Assign<ecs::components::Transform>(object, glm::vec3(x, ground + height, z), glm::mat3(1.0f), glm::vec3(1.0f));
			g_Effects[id].inContainer = true;
			g_Containers.push_front({id, object, entt::null, game_clock::TicksForSeconds(seconds), false});
		}
	}
}

void manager::Clear()
{
	g_Effects.clear();
	g_Containers.clear();
	g_DebugDone = false;
	town_belief::Clear();
	audio::spell_sounds::Clear();
}

namespace
{
std::vector<manager::DrawableSource>& DrawableSources()
{
	static std::vector<manager::DrawableSource> sources;
	return sources;
}
} // namespace

std::vector<manager::Drawable> manager::Collect(Creator::Kind kind)
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	std::vector<Drawable> result;
	for (const auto& [id, running] : g_Effects)
	{
		Drawable drawable {running.effect->GetOrigin(), {}, running.perFrame ? 1.0f : t, running.path, id};
		running.effect->Collect(drawable.t, drawable.atoms, kind);
		if (!drawable.atoms.empty())
		{
			result.push_back(std::move(drawable));
		}
	}
	if (kind == Creator::Kind::Sprite && Locator::camera::has_value() && Locator::entitiesRegistry::has_value())
	{
		town_belief::Collect(Locator::camera::value().GetOrigin(), result);
	}
	for (const auto source : DrawableSources())
	{
		source(result);
	}
	return result;
}

std::vector<Effect::DrawChain> manager::CollectChains()
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	std::vector<Effect::DrawChain> result;
	for (const auto& [id, running] : g_Effects)
	{
		const size_t first = result.size();
		running.effect->CollectChains(running.perFrame ? 1.0f : t, result);
		for (size_t i = first; i < result.size(); ++i)
		{
			result[i].path = running.path;
			result[i].effect = id;
		}
	}
	return result;
}

void manager::AddDrawableSource(DrawableSource source)
{
	DrawableSources().push_back(source);
}

namespace
{
/// The town belief and the DrawableSources, as Collect appends them to the sprites
std::vector<manager::Drawable> CollectSources()
{
	std::vector<manager::Drawable> result;
	if (Locator::camera::has_value() && Locator::entitiesRegistry::has_value())
	{
		town_belief::Collect(Locator::camera::value().GetOrigin(), result);
	}
	for (const auto source : DrawableSources())
	{
		source(result);
	}
	return result;
}

/// The ordered walk (Effect::CollectOrdered) of every effect of one path
std::vector<manager::OrderedEffect> CollectOrderedOf(manager::DrawPath path)
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	std::vector<manager::OrderedEffect> result;
	for (const auto& [id, running] : g_Effects)
	{
		if (running.path != path)
		{
			continue;
		}
		manager::OrderedEffect effect {id, path, running.effect->GetOrigin(), running.perFrame ? 1.0f : t, {}, {}};
		running.effect->CollectOrdered(effect.t, effect.items, effect.chains);
		for (auto& chain : effect.chains)
		{
			chain.path = path;
			chain.effect = id;
		}
		if (!effect.items.empty())
		{
			result.push_back(std::move(effect));
		}
	}
	return result;
}
} // namespace

manager::SortedFrame manager::CollectSorted()
{
	// g_game +0x205D64, the fraction of the turn GJPSysInterface::Draw_ 0x67370D passes on
	const float t = game_clock::TurnFraction();
	SortedFrame frame;
	const auto add = [&frame](const Effect::DrawAtom& atom, uint32_t effect, float drawT) {
		const auto* creator = atom.creator;
		if (creator->kind == Creator::Kind::Sprite)
		{
			// Particle3DSprite::DrawAt 0x67AF8A..0x67AFD6: the sprite's position is the PSR's +0x24, raised by
			// sprite +0x10 (the height) x +0xC (the size) x 0.5 ([0x8AA3B4]) with the flag +0x25 & 1 (CentreAtBase); the
			// size is the scale, at least 0.0001 (0x67AEA4..0x67AEBE), the height the stretch
			glm::vec3 key = atom.position;
			if (creator->centreAtBase)
			{
				key.y += atom.stretch * std::max(atom.scale, 1e-4f) * 0.5f;
			}
			frame.sprites.push_back({key, atom, effect, drawT});
		}
		else if (creator->kind == Creator::Kind::Mesh)
		{
			frame.meshes.push_back({atom.position, atom, effect, drawT});
		}
		else if (dynamic_cast<const MistCreator*>(creator) != nullptr)
		{
			frame.mists.push_back({atom.position, atom, effect, drawT});
		}
		else if (creator->className == "ZR_SurfRevol")
		{
			frame.surfaces.push_back({atom.position, atom, effect, drawT});
		}
		else
		{
			frame.others.push_back({atom.position, atom, effect, drawT});
		}
	};
	for (const auto& [id, running] : g_Effects)
	{
		if (running.path != DrawPath::Sorted)
		{
			continue;
		}
		const float drawT = running.perFrame ? 1.0f : t;
		std::vector<Effect::OrderedItem> items;
		std::vector<Effect::DrawChain> chains;
		running.effect->CollectOrdered(drawT, items, chains);
		for (const auto& item : items)
		{
			if (item.chain >= 0)
			{
				// fn_0067B380: the joint n / 2 (the item's atom, Effect::CollectOrdered)
				auto& chain = chains[static_cast<size_t>(item.chain)];
				chain.path = DrawPath::Sorted;
				chain.effect = id;
				frame.chains.push_back({item.atom.position, std::move(chain), id, drawT});
				continue;
			}
			add(item.atom, id, drawT);
		}
	}
	for (const auto& drawable : CollectSources())
	{
		if (drawable.path != DrawPath::Sorted)
		{
			continue;
		}
		for (const auto& atom : drawable.atoms)
		{
			add(atom, drawable.effect, drawable.t);
		}
	}
	return frame;
}

std::vector<manager::OrderedEffect> manager::CollectQueued()
{
	auto result = CollectOrderedOf(DrawPath::Queued);
	for (auto& drawable : CollectSources())
	{
		if (drawable.path != DrawPath::Queued || drawable.atoms.empty())
		{
			continue;
		}
		OrderedEffect effect {drawable.effect, DrawPath::Queued, drawable.origin, drawable.t, {}, {}};
		for (auto& atom : drawable.atoms)
		{
			effect.items.push_back({atom, -1});
		}
		result.push_back(std::move(effect));
	}
	return result;
}

std::vector<manager::OrderedEffect> manager::HandEffects()
{
	return CollectOrderedOf(DrawPath::Immediate);
}
