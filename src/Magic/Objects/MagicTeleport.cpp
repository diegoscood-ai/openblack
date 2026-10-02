/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTeleport.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Audio/GAudio/BankTables.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"

using namespace openblack;
using namespace openblack::magic;
using ecs::components::MagicTeleport;
using ecs::components::Transform;

namespace
{
constexpr float k_UnitsPerMetre = ecs::MapInterface::k_PositionToGridFactor; // MapCoords: 6553.6 a metre

auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

MagicTeleport* StoneOf(entt::entity stone)
{
	auto& registry = Reg();
	return registry.Valid(stone) ? registry.TryGet<MagicTeleport>(stone) : nullptr;
}

std::vector<entt::entity>& ListOf(PlayerNames player)
{
	return players::MagicOf(player).teleportStones;
}

float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

int32_t ToUnits(float metres)
{
	return static_cast<int32_t>(metres * k_UnitsPerMetre); // MapCoords(LHPoint) 0x603160: ftol
}

/// Living::MoveByTeleport 0x5EC342..0x5EC372: SoundTag::Create(MapCoords&, sample, track 0, mode 2, loops 0, +0x10 0,
/// is3D 1, AUDIO_SFX_BANK_TYPE 1 = IN_GAME, delay 0) 0x71EB60, a point tag at (x, the land + the MapCoords' height, z)
/// that plays once. `mapPosition` is a map position (x, height above the land, z), as magic::ToMap gives.
void PlayInGameSample(int sample, const glm::vec3& mapPosition)
{
	audio::tags::CreateAtMapCoords(mapPosition.x, mapPosition.z, mapPosition.y, sample, false, 2, 0, false, true,
	                               audio::SfxBank::InGame, 0);
}

/// Object::AsMultiMapFixed (vt 0x678) != NULL: the MultiMapFixed classes openblack has as components
bool IsMultiMapFixed(entt::entity entity)
{
	using namespace ecs::components;
	return Reg().AnyOf<Abode, Field, Feature, TotemStatue, FishFarm, SpellIcon, WorshipSite, Temple, AnimatedStatic,
	                        MobileStatic, MagicTeleport>(entity);
}

/// fn_005FCBA0 for one stone: the travellers that are gone, not available (vt 0x2C) or not reacting to the stone's
/// reaction (Living::GetReaction 0x5ECA60) lose every entry
void ProcessTravellers(entt::entity stone)
{
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	auto& registry = Reg();
	auto& list = component->travellers;
	for (size_t i = 0; i < list.size();)
	{
		const auto living = list[i].living;
		const bool keep = living != entt::null && registry.Valid(living) &&
		                  ecs::villager_teleport::CurrentReaction(living) == component->reaction;
		if (keep)
		{
			++i;
			continue;
		}
		std::erase_if(list, [living](const MagicTeleport::Traveller& entry) { return entry.living == living; });
		i = 0; // the original restarts from the node after the one it looked at; with the entries gone it is the same
	}
}
} // namespace

// ---- pure rules ----

int32_t teleport::FastDistance(const glm::vec3& a, const glm::vec3& b)
{
	const int32_t dx = std::abs(ToUnits(a.x) - ToUnits(b.x));
	const int32_t dz = std::abs(ToUnits(a.z) - ToUnits(b.z));
	return dx < dz ? (dx >> 1) + dz : (dz >> 1) + dx;
}

bool teleport::IsWorthTheDetour(const glm::vec3& living, const glm::vec3& destination, const glm::vec3& stone,
                                const glm::vec3& other)
{
	// fild of the int64 {d, 0} then the float compare: 1.2 x (d(l, this) + d(T, dest)) < d(l, dest)
	const auto direct = static_cast<float>(FastDistance(living, destination));
	const auto detour = static_cast<float>(FastDistance(living, stone) + FastDistance(other, destination));
	return detour * k_DetourFactor < direct;
}

int teleport::ChooseTarget(const glm::vec3& living, const glm::vec3& destination, const std::vector<glm::vec3>& others,
                           bool force, float* saving)
{
	float best = force ? -1000000.0f : 0.0f; // 0xC9742400
	int target = -1;
	for (size_t i = 0; i < others.size(); ++i)
	{
		const float s = Distance2D(destination, living) - Distance2D(destination, others[i]);
		if (s > best) // test ah, 0x41: strictly greater
		{
			best = s;
			target = static_cast<int>(i);
		}
	}
	if (saving != nullptr)
	{
		*saving = best;
	}
	return target;
}

float teleport::JumpCost(float saving, float costPerKilometer)
{
	return -saving * costPerKilometer * 0.001f;
}

int teleport::FindRouteStone(const std::vector<glm::vec3>& stones, const glm::vec3& from, const glm::vec3& to,
                             float maxDistance)
{
	float nearFrom = maxDistance;
	float nearTo = maxDistance;
	int best = -1;
	for (size_t i = 0; i < stones.size(); ++i)
	{
		const float d1 = Distance2D(stones[i], from); // fn_00605CD0: GetDistanceInMetres
		if (d1 < nearFrom)
		{
			nearFrom = d1;
			best = static_cast<int>(i);
		}
		const float d2 = Distance2D(stones[i], to);
		if (d2 < nearTo)
		{
			nearTo = d2;
		}
	}
	return nearFrom + nearTo < maxDistance ? best : -1;
}

// ---- the stones ----

bool teleport::TraceEnabled()
{
	static const bool trace = std::getenv("OPENBLACK_TELEPORT_TRACE") != nullptr || std::getenv("OPENBLACK_SPELL_TRACE") != nullptr;
	return trace;
}

glm::vec3 teleport::MapPositionOf(entt::entity object)
{
	const auto* transform = Reg().TryGet<const Transform>(object);
	return transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
}

std::optional<PlayerNames> teleport::PlayerOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr || !component->hasPlayer)
	{
		return std::nullopt;
	}
	return component->player;
}

const std::vector<entt::entity>& teleport::StonesOf(PlayerNames player)
{
	return ListOf(player);
}

entt::entity teleport::Create(const glm::vec3& mapPosition, entt::entity spell)
{
	auto& registry = Reg();
	// operator new (0xA4) + MagicTeleport(pos, spell) 0x5FC130: MobileStatic(pos, GMobileStaticInfo 0xD3B614
	// (fn_005FC420), NULL, 0, 1.0)
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, ToWorld(mapPosition), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& stone = registry.Assign<MagicTeleport>(entity);
	stone.spell = spell;
	// +0xA0 = spell->GetPlayer() (Spell vt 0x1C: +0xA4)
	if (registry.Valid(spell))
	{
		if (const auto* component = registry.TryGet<const ecs::components::Spell>(spell); component != nullptr && component->hasPlayer)
		{
			stone.hasPlayer = true;
			stone.player = component->player;
		}
	}
	if (stone.hasPlayer)
	{
		// the head of the player's list (+0xA58), one more (+0xA5C)
		auto& list = ListOf(stone.player);
		list.insert(list.begin(), entity);
		// Reaction::CreateReaction(this, REACT_TO_TELEPORT, GetPlayer(), 0): spread at once over the cells in its radius
		stone.reaction = ecs::effects::reactions::CreateReaction(entity, openblack::Reaction::ReactToTeleport, stone.player, false);
	}
	// CallVirtualFunctionsForCreation 0x5FC260: MobileStatic's (0x609700), then, unless the object is already being
	// deleted (+0xA bit 0), PSysInterface::Create(no spell, PT 73, the world position (altitude + MapCoords y), direction
	// 0, magnitude 1.0, 0) and psys->SetPlayer(GetPlayer()) (vt 0x20)
	const auto file = psys::ParticleTypeFile(static_cast<ParticleType>(k_VortexParticleType));
	if (!file.empty())
	{
		auto& component = registry.Get<MagicTeleport>(entity);
		component.psys = psys::manager::StartForSpell(std::string(file), ToWorld(mapPosition), glm::vec3(0.0f), 1.0f, nullptr);
		if (component.psys != 0)
		{
			psys::manager::SetPerFrame(component.psys); // MagicTeleport::Draw steps it with the frame time
			if (auto* effect = psys::manager::Find(component.psys); effect != nullptr)
			{
				effect->SetPlayer(component.hasPlayer ? static_cast<int>(component.player) : -1);
			}
		}
	}
	// SetScale(GetScale() x 0.01) (vt 0x124 / 0x120): the stone has no mesh, so nothing shows it
	registry.Get<MagicTeleport>(entity).scale *= 0.01f;
	registry.SetDirty();
	if (TraceEnabled())
	{
		const auto& component = registry.Get<const MagicTeleport>(entity);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: stone {} at ({:.1f}, {:.1f}) for spell {} player {} (stones {}) reaction {} psys {}",
		                   static_cast<uint32_t>(entity), mapPosition.x, mapPosition.z, static_cast<uint32_t>(spell),
		                   component.hasPlayer ? static_cast<int>(component.player) : -1,
		                   component.hasPlayer ? ListOf(component.player).size() : 0, component.reaction, component.psys);
	}
	return entity;
}

void teleport::ToBeDeleted(entt::entity stone)
{
	auto& registry = Reg();
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	// MagicTeleport::ToBeDeleted 0x5FC310: its reactions, the vortex (vt 4 delete), out of the player's list, the
	// travellers' list; then MobileStatic::ToBeDeleted
	ecs::effects::reactions::RemoveAllReactionsInitiatedByObject(stone);
	if (component->psys != 0)
	{
		psys::manager::Delete(component->psys);
		component->psys = 0;
	}
	if (component->hasPlayer)
	{
		std::erase(ListOf(component->player), stone);
	}
	component->travellers.clear();
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: stone {} deleted", static_cast<uint32_t>(stone));
	}
	registry.Destroy(stone);
	registry.SetDirty();
}

bool teleport::ShouldLivingThingReact(entt::entity stone, entt::entity living)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr || !component->hasPlayer || !ecs::villager_teleport::IsMoving(living))
	{
		return false;
	}
	const auto destination = ecs::villager_teleport::FinalDestination(living); // vt 0x884
	const auto at = MapPositionOf(living);
	const auto here = MapPositionOf(stone);
	for (const auto other : ListOf(component->player))
	{
		if (other == stone)
		{
			continue;
		}
		if (IsWorthTheDetour(at, destination, here, MapPositionOf(other)))
		{
			return true;
		}
	}
	return false;
}

void teleport::RegisterDestination(entt::entity stone, entt::entity living, const glm::vec3& destination)
{
	auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return;
	}
	auto& list = component->travellers;
	std::erase_if(list, [living](const MagicTeleport::Traveller& entry) { return entry.living == living; });
	if (living != entt::null)
	{
		list.insert(list.begin(), {living, destination});
	}
}

int teleport::DoTeleport(entt::entity stone, entt::entity living, bool force)
{
	auto* component = StoneOf(stone);
	if (component == nullptr || !component->hasPlayer)
	{
		return 0;
	}
	const auto entry = std::find_if(component->travellers.begin(), component->travellers.end(),
	                                [living](const MagicTeleport::Traveller& t) { return t.living == living; });
	if (entry == component->travellers.end())
	{
		return 0;
	}
	const auto destination = entry->destination;
	const auto at = MapPositionOf(living);
	std::vector<entt::entity> others;
	std::vector<glm::vec3> positions;
	for (const auto other : ListOf(component->player))
	{
		if (other != stone)
		{
			others.push_back(other);
			positions.push_back(MapPositionOf(other));
		}
	}
	float saving = 0.0f;
	const int index = ChooseTarget(at, destination, positions, force, &saving);
	if (index < 0)
	{
		return 0;
	}
	const auto target = others[static_cast<size_t>(index)];
	const auto spell = component->spell;
	auto& registry = Reg();
	if (spell != entt::null && registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell))
	{
		// spell->SpellEvent({2, (x, MapCoords y, z) of this stone, velocity 0, strength 1, no shields, no target})
		psys::SpellEventInfo event;
		event.type = psys::SpellEventInfo::Point;
		const auto here = MapPositionOf(stone);
		event.position = glm::vec3(here.x, here.y, here.z);
		OpsOf(registry.Get<const ecs::components::Spell>(spell).spellClass).spellEvent(spell, event);
		// fn_005FBF20 (the GMagicTeleportInfo) +0x58 costPerKilometer; fn_005FBF10 = PayFor(x, true)
		if (registry.Valid(spell) && registry.AllOf<ecs::components::Spell>(spell))
		{
			const auto* info = GetMagicInfoAs<GMagicTeleportInfo>(Locator::infoConstants::value(),
			                                                        registry.Get<const ecs::components::Spell>(spell).magicType);
			const float perKilometre = info != nullptr ? info->costPerKilometer : 0.0f;
			auto& spellComponent = registry.Get<ecs::components::Spell>(spell);
			const float before = spellComponent.chants;
			chants::PayFor(spellComponent, ChantContextOf(spell), JumpCost(saving, perKilometre), true);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: spell {} PayFor({:.2f}, forced): chants {:.2f} -> {:.2f}",
				                   static_cast<uint32_t>(spell), JumpCost(saving, perKilometre), before, spellComponent.chants);
			}
		}
	}
	// GParticleContainer::CreateSpotVisual 0x63E540 (pos, SPOT_VISUAL 14, 1.0, NULL) where it is and where it goes. The
	// plain CreateSpotVisual passes its float on to 0x63E4B0 with the duration of the SV entry itself (entry +0x44), so
	// the 1.0 is not a duration: here 0 = the entry's own life
	const auto targetPosition = MapPositionOf(target);
	psys::manager::CreateSpotVisual(k_SpotVisualVillagerTeleport, ToWorld(at), 0.0f, entt::null);
	psys::manager::CreateSpotVisual(k_SpotVisualVillagerTeleport, ToWorld(targetPosition), 0.0f, entt::null);
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: {} {} from stone {} ({:.1f}, {:.1f}) to stone {} ({:.1f}, {:.1f}) for ({:.1f}, {:.1f}): saving {:.2f} m{}",
		                   registry.AllOf<ecs::components::Villager>(living) ? "villager" : "living", static_cast<uint32_t>(living),
		                   static_cast<uint32_t>(stone), at.x, at.z, static_cast<uint32_t>(target), targetPosition.x,
		                   targetPosition.z, destination.x, destination.z, saving, force ? " (forced)" : "");
	}
	MoveByTeleport(living, targetPosition); // vt 0xAF0
	return 1;
}

bool teleport::ValidToApplyVillagerDirectly(entt::entity stone, entt::entity villager)
{
	const auto* component = StoneOf(stone);
	if (component == nullptr)
	{
		return false;
	}
	const auto villagerPlayer = ecs::villager_teleport::PlayerOf(villager);
	if (villagerPlayer.has_value() != component->hasPlayer || (component->hasPlayer && *villagerPlayer != component->player))
	{
		return false;
	}
	// GetPlayer() +0xA5C (the count) != 1 (a stone without a player compares NULL with NULL and then reads it: never here)
	return component->hasPlayer && ListOf(component->player).size() != 1;
}

int teleport::ApplyVillagerDirectly(entt::entity stone, entt::entity villager)
{
	if (StoneOf(stone) == nullptr)
	{
		return 0x17;
	}
	// SetTopState(FLYING), the interface puts it down at the stone (fn_005DA0C0), SetTopState(LANDED), DecideWhatToDo
	ecs::villager_teleport::LandAt(villager, MapPositionOf(stone));
	// GetFinalDestPos (vt 0x884) at 0x5FC549, after DecideWhatToDo. (aproximado) the original's DecideWhatToDo may have
	// chosen a new walk by then; openblack's only sets the state, so the goal read here is still the one it had
	RegisterDestination(stone, villager, ecs::villager_teleport::FinalDestination(villager));
	if (DoTeleport(stone, villager, true) == 1)
	{
		ecs::villager_teleport::DecideWhatToDo(villager);
		return 1;
	}
	return 0x17;
}

void teleport::MoveByTeleport(entt::entity living, const glm::vec3& mapPosition)
{
	auto& registry = Reg();
	auto* transform = registry.TryGet<Transform>(living);
	if (transform == nullptr)
	{
		return;
	}
	// SoundTag::Create(the living's MapCoords +0x14, 0x27 G_SpellTeleportEnergiseGo, ...) 0x5EC358 and (the argument's
	// MapCoords, 0x26 G_SpellTeleportEnergiseArrive, ...) 0x5EC372, bank IN_GAME
	PlayInGameSample(39, ToMap(transform->position));
	PlayInGameSample(38, mapPosition);
	const auto world = ToWorld(glm::vec3(mapPosition.x, 0.0f, mapPosition.z));
	// MoveMapObject (vt 0x55C): the new position, at the land
	transform->position = world;
	ecs::villager_teleport::OnMoved(living);
	registry.SetDirty();
}

bool teleport::AnyMultiMapFixedNear(const glm::vec3& mapPosition, float radius)
{
	if (!Locator::entitiesMap::has_value())
	{
		return false;
	}
	auto& registry = Reg();
	const auto& map = Locator::entitiesMap::value();
	// fn_00604C30: max(ceil(2R / 10), 3)^2 cells in a spiral around the point; the objects of each cell (FindType -1)
	// that pass the predicate, other than the excluded one, nearer than R. Here only whether there is one.
	const auto centre = ecs::MapInterface::GetGridCell(glm::vec2(mapPosition.x, mapPosition.z));
	const int side = std::max(static_cast<int>(std::ceil(2.0f * radius / 10.0f)), 3);
	const int half = side / 2;
	const auto test = [&](entt::entity entity) {
		if (!registry.Valid(entity) || !IsMultiMapFixed(entity))
		{
			return false;
		}
		const auto* transform = registry.TryGet<const Transform>(entity);
		return transform != nullptr && Distance2D(transform->position, mapPosition) < radius;
	};
	for (int dz = -half; dz <= side - 1 - half; ++dz)
	{
		for (int dx = -half; dx <= side - 1 - half; ++dx)
		{
			const int x = static_cast<int>(centre.x) + dx;
			const int z = static_cast<int>(centre.y) + dz;
			if (x < 0 || z < 0 || x >= ecs::MapInterface::k_GridSize.x || z >= ecs::MapInterface::k_GridSize.y)
			{
				continue;
			}
			const ecs::MapInterface::CellId cell(static_cast<uint16_t>(x), static_cast<uint16_t>(z));
			for (const auto entity : map.GetFixedInGridCell(cell))
			{
				if (test(entity))
				{
					return true;
				}
			}
			for (const auto entity : map.GetMobileInGridCell(cell))
			{
				if (test(entity))
				{
					return true;
				}
			}
		}
	}
	// the stones are MultiMapFixed in the cells too (MultiMapFixed::InsertMapObject); openblack keeps them out of the
	// grid (no mesh, no footprint), so they are looked up in the players' lists
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		for (const auto stone : ListOf(static_cast<PlayerNames>(p)))
		{
			if (Distance2D(MapPositionOf(stone), mapPosition) < radius)
			{
				return true;
			}
		}
	}
	return false;
}

void teleport::ProcessPlayers()
{
	// GGame::ProcessTurn -> GPlayer::ProcessPlayers 0x649A20 -> GPlayer::Process 0x6494E0 +0x1DC: fn_005FCC70 on each
	// player, every stone that IsAvailable (vt 0x2C: GameThing 1)
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		for (const auto stone : std::vector<entt::entity>(ListOf(static_cast<PlayerNames>(p))))
		{
			ProcessTravellers(stone);
		}
	}
}

void teleport::UpdateFrame(float seconds)
{
	auto& registry = Reg();
	std::vector<entt::entity> stones;
	registry.Each<MagicTeleport>([&stones](entt::entity entity, const MagicTeleport&) { stones.push_back(entity); });
	for (const auto entity : stones)
	{
		auto& component = registry.Get<MagicTeleport>(entity);
		if (component.psys == 0)
		{
			continue;
		}
		// MagicTeleport::Draw 0x5FCCC0: psys->SetPos(world pos) (vt 0xFC), psys->Process_({0..., power 1, enabled 1},
		// g_game_time_inc) and Draw_(1.0, 1) (vt 0x104)
		const auto& transform = registry.Get<const Transform>(entity);
		psys::manager::SetOrigin(component.psys, transform.position);
		psys::ProcessInfo info;
		info.power = 1.0f;
		info.enabled = true;
		if (!psys::manager::ProcessForSpell(component.psys, info, seconds))
		{
			component.psys = 0;
		}
	}
}

std::vector<entt::entity> teleport::HandCollisionStones()
{
	std::vector<entt::entity> result;
	auto& registry = Reg();
	registry.Each<MagicTeleport>([&](entt::entity entity, const MagicTeleport&) {
		if (SeedOf(entity) != entt::null)
		{
			result.push_back(entity);
		}
	});
	return result;
}

uint32_t teleport::ReactionOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	return component != nullptr ? component->reaction : 0;
}

entt::entity teleport::SeedOf(entt::entity stone)
{
	const auto* component = StoneOf(stone);
	auto& registry = Reg();
	if (component == nullptr || component->spell == entt::null || !registry.Valid(component->spell))
	{
		return entt::null;
	}
	const auto* spell = registry.TryGet<const ecs::components::Spell>(component->spell);
	if (spell == nullptr || spell->seed == entt::null || !registry.Valid(spell->seed) ||
	    !registry.AllOf<ecs::components::SpellSeed>(spell->seed))
	{
		return entt::null;
	}
	return spell->seed;
}

void teleport::Clear()
{
	for (size_t p = 0; p < static_cast<size_t>(PlayerNames::_COUNT); ++p)
	{
		ListOf(static_cast<PlayerNames>(p)).clear();
	}
	ecs::villager_teleport::Clear();
}
