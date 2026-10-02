/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireEffect.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/MapCoords.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/Implementations/VillagerFire.h"
#include "ECS/Weather/Weather.h"
#include "FireGraphic.h"
#include "FireObjectTraits.h"
#include "FireSound.h"
#include "Locator.h"
#include "Magic/Spells/SpellStormAndTornado.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::fire;

namespace
{
/// Every FireEffect, owned; the list order (g_game +0x205C14, newest first) is g_List
std::vector<std::unique_ptr<FireEffect>> g_Pool;
std::vector<FireEffect*> g_List;
std::unordered_map<entt::entity, FireEffect*> g_ByObject;
std::unordered_map<uint32_t, FireEffect*> g_ById;
uint32_t g_NextId = 1;
/// 0xDA09E4 (the tag ProcessList processes; 0 every turn) and 0xDA09E5 (the tag of the next fire; 0 after each)
uint8_t g_ProcessTag = 0;
uint8_t g_CreateTag = 0;

bool Trace()
{
	return TraceEnabled();
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// GUtils::GetDistanceInMetres 0x74CD70 (and its twin fn_0074CD50): x, z only, through the table hypotenuse 0x74F680
float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// MapCoords::InBounds 0x6042C0: the 10 m cell inside the map ((port) 512 cells when no land is loaded: tests only)
bool InBounds(const glm::vec3& position)
{
	const uint16_t side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	return map_coords::InBounds(position, side); // MapCoords(LHPoint) truncates (__ftol), then the unsigned high words
}

/// MapCoords::IsWater 0x6035B0: the cell's hasWater bit; 1 outside the map or without a block (ecs::sea_cells)
bool IsWater(const glm::vec3& position)
{
	return sea_cells::IsWater(position);
}

/// MapCoords::IsCoastal 0x6036A0: no water in the cell but the coast line bit (ecs::sea_cells)
bool IsCoastal(const glm::vec3& position)
{
	return sea_cells::IsCoastal(position);
}

/// GClimate::GetMaxRainingOrSnowing 0x771600 at MapCoords::GetLHPoint of the fire centre (the land plus its height):
/// max(rain, snow) of the weather there (0..127, ECS/Weather)
float MaxRainingOrSnowing(const glm::vec3& centre)
{
	return weather::GetMaxRainingOrSnowingAt(glm::vec3(centre.x, LandAt(centre.x, centre.z) + centre.y, centre.z));
}

/// GameThing::GetPlayer (vt 0x1C) of an object: a villager's town's owner; the others none (inf: per class)
bool PlayerOfObject(entt::entity object, PlayerNames& player)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const components::Villager>(object);
	    villager != nullptr && registry.Valid(villager->town))
	{
		if (const auto* town = registry.TryGet<const components::Town>(villager->town); town != nullptr)
		{
			player = town->owner;
			return true;
		}
	}
	return false;
}

/// Reaction::RemoveAllReactionsOfTypeInitiatedByObject 0x6E4780
void RemoveReactions(entt::entity object, Reaction type)
{
	effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(object, type);
}

/// The objects of the 10 m map cell (MapCoords::FindType(-1) walks the cell's list): fixed and mobile, the held
/// object excluded (it is out of the map while in the hand)
void CellObjects(const glm::ivec2& cell, std::vector<entt::entity>& out)
{
	out.clear();
	// MapCoords::ToMap 0x603430 gives NULL off the map (InBounds 0x6042C0: the cell unsigned against 512)
	if (!Locator::entitiesMap::has_value() || !map_coords::InBounds(cell, MapInterface::k_GridSize.x))
	{
		return;
	}
	const auto& map = Locator::entitiesMap::value();
	const MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
	out.insert(out.end(), map.GetFixedInGridCell(id).begin(), map.GetFixedInGridCell(id).end());
	out.insert(out.end(), map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
	// (inf) the cell lists are unordered sets here: sort for a stable order
	std::sort(out.begin(), out.end());
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(out, [&registry](entt::entity e) { return !registry.Valid(e) || traits::InHand(e); });
}

// ---- fire groups (the +0x40 / +0x44 chain and the root's firemen list) ----

/// FireEffect::IsInSameFireGroupAs 0x72FEA0
bool IsInSameFireGroupAs(const FireEffect& self, const FireEffect* other)
{
	if (other == nullptr)
	{
		return false;
	}
	if (other->root == self.root)
	{
		return true;
	}
	for (const auto* member = self.root; member != nullptr; member = member->next)
	{
		if (member == other)
		{
			return true;
		}
	}
	return false;
}

/// The firemen list of `from`'s root appended to `to`'s (0x72FC30.. / 0x72FD36..)
void MoveFiremen(FireEffect& from, FireEffect& to)
{
	to.firemen.insert(to.firemen.end(), from.firemen.begin(), from.firemen.end());
	from.firemen.clear();
}

/// FireEffect::AddToMyFireGroup 0x72FBE0: the other fire (with the rest of its group) goes right after this one
void AddToMyFireGroup(FireEffect& self, FireEffect& other)
{
	if (IsInSameFireGroupAs(self, &other))
	{
		return;
	}
	FireEffect* otherRoot = other.root;
	FireEffect* myRoot = self.root;
	if (otherRoot != nullptr && !(otherRoot == &other && other.next == nullptr))
	{
		MoveFiremen(*otherRoot, *myRoot);
		FireEffect* member = otherRoot;
		while (member->next != nullptr)
		{
			member->root = myRoot;
			member = member->next;
		}
		member->root = myRoot;
		member->next = self.next;
		self.next = otherRoot;
		return;
	}
	MoveFiremen(other, *myRoot);
	other.next = self.next;
	self.next = &other;
	other.root = myRoot;
}

/// fn_0072FD20: this root hands the group over to `newRoot` (a member), which goes first
void HandRootTo(FireEffect& self, FireEffect& newRoot)
{
	if (self.root != &self)
	{
		return;
	}
	MoveFiremen(self, newRoot);
	FireEffect* oldRoot = self.root;
	for (FireEffect* member = oldRoot; member != nullptr; member = member->next)
	{
		if (member->next == &newRoot)
		{
			member->next = newRoot.next;
			newRoot.root = &newRoot;
			newRoot.next = oldRoot;
		}
		member->root = &newRoot;
	}
}

/// fn_0072F910 / fn_0072F930: the group's firemen stop (all of them, or those fighting this fire)
void StopFiremen(FireEffect& self, bool onlyThisFire)
{
	auto* root = self.root;
	if (root == nullptr)
	{
		return;
	}
	const auto firemen = root->firemen; // StopFireFighting takes them off the list
	for (const auto villager : firemen)
	{
		if (!onlyThisFire || villager_fire::FireOf(villager) == self.id)
		{
			villager_fire::StopFireFighting(villager);
		}
	}
}

/// FireEffect::RemoveFromFireGroup 0x72FDD0
void RemoveFromFireGroup(FireEffect& self)
{
	FireEffect* root = self.root;
	if (root == &self)
	{
		if (self.next == nullptr)
		{
			StopFiremen(self, false);
			return;
		}
		StopFiremen(self, true);
		HandRootTo(self, *self.next);
		self.root->next = self.next;
		self.root = &self;
		self.next = nullptr;
		return;
	}
	if (root == nullptr)
	{
		return;
	}
	StopFiremen(self, true);
	for (FireEffect* member = root; member != nullptr && member->next != nullptr; member = member->next)
	{
		if (member->next == &self)
		{
			member->next = self.next;
			self.root = &self;
			self.next = nullptr;
			if (member->next == nullptr)
			{
				break;
			}
		}
	}
}

// ---- heat ----

/// fn_0072F960: 0.1 x 100 x dT
float HeatFromDifference(float difference)
{
	return 0.1f * 100.0f * difference;
}

/// fn_0072F980: this fire heats the object `target`
void HeatTransfer(FireEffect& source, entt::entity target)
{
	auto& registry = Locator::entitiesRegistry::value();
	// villagers fighting a fire (on the ground) are immune to it
	if (traits::IsObjectInMap(source.object) && villager_fire::IsFireMan(target))
	{
		return;
	}
	if (source.source != entt::null && source.source == target)
	{
		return;
	}
	const float radius = source.FireRadius();
	if (radius == 0.0f)
	{
		return;
	}
	// a hot object that doesn't burn only heats what catches fire below its temperature
	if (!source.IsOnFire() && !(traits::CombustionTemperature(target) <= source.temperature))
	{
		return;
	}
	const auto targetCentre = traits::FireCentre(target);
	const auto sourceCentre = traits::FireCentre(source.object);
	const float distance = Distance2D(targetCentre, sourceCentre); // GetDistanceInMetres 0x74CD70 at 0x72FA44
	if (!(traits::DefaultFireRadius(target) + radius > distance))
	{
		return;
	}
	// heights above the land: the source far above the target (a fireball, a held tree) misses it
	if (!(sourceCentre.y < 3.0f && targetCentre.y < 3.0f))
	{
		const float above = sourceCentre.y - targetCentre.y;
		if (!(traits::Height(target) + source.FlameHeight() > above))
		{
			return;
		}
	}
	// GetActualObjectToEffect (vt 0x5D8): the object itself (CitadelPart / Heart redirect, not ported)
	const entt::entity actual = target;
	if (!registry.Valid(actual))
	{
		return;
	}
	const float difference = source.temperature - GetTemperature(actual);
	if (!(difference > 0.0f))
	{
		return;
	}
	FireEffect* fire = Find(actual);
	if (fire == nullptr)
	{
		fire = Create(actual, source.hasPlayer, source.player, entt::null);
		if (fire == nullptr)
		{
			return;
		}
	}
	const float heat = std::min(HeatFromDifference(difference), source.HeatContent() * 0.5f);
	fire->AddHeat(heat, difference);
	if (!source.IsOnFire())
	{
		source.AddHeat(-heat, -difference);
	}
	if (!traits::InHand(actual) && fire->root != source.root)
	{
		AddToMyFireGroup(source, *fire);
	}
	if (traits::IsVillager(actual) && !villager_fire::IsInOnFireState(actual))
	{
		villager_fire::SetupOnFire(actual, source.id);
	}
}

/// fn_0072EFB0: FireEffect::Process, once per turn
void Process(FireEffect& fire)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (the port) the object was deleted by something else: Object::ToBeDeleted takes its fire with it
	if (!registry.Valid(fire.object))
	{
		ToBeDeleted(fire);
		return;
	}
	// fully cooled with no charring left
	if (fire.temperature - fire.Ambient() < 0.1f && fire.charring == 0.0f)
	{
		ToBeDeleted(fire);
		return;
	}
	if (fire.source != entt::null && !registry.Valid(fire.source))
	{
		fire.source = entt::null;
	}
	fire.flags &= 0xF0;
	const float tc = fire.Tc();
	if (fire.temperature > 3.0f * tc)
	{
		fire.flags |= FireEffect::VeryHot;
	}
	if (fire.previous < tc && fire.temperature >= tc)
	{
		fire.flags |= FireEffect::JustIgnited;
		// TODO(M8): a creature gets 3 flames (Creature3D::AddFlame fn_00486390, random points in its box)
	}
	const float defenceBurn = traits::DefenceMultiplierBurn(fire.object);
	const bool cools = fire.temperature < tc || defenceBurn == 0.0f; // bl != 0: not burning (or not hurt by it)
	float multiplier = 1.0f;
	const auto centre = traits::FireCentre(fire.object);
	bool heat = false;
	if ((IsCoastal(centre) || IsWater(centre)) && centre.y < 2.0f)
	{
		// in the water: cools 50 times faster
		fire.flags |= FireEffect::Cooling;
		multiplier = 50.0f;
	}
	else if (const float rain = MaxRainingOrSnowing(centre); rain > 0.0f && traits::RainCoolingMultiplier(fire.object) > 0.0f)
	{
		multiplier = traits::RainCoolingMultiplier(fire.object) * rain + 1.0f;
		fire.flags |= FireEffect::Cooling;
		// fn_0072DCC0 (0x72F2D9, with the object's +0x14): the storm spell whose radius covers it gets
		// REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (Magic/Spells/SpellStormAndTornado)
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const components::Transform>(fire.object))
		{
			magic::spell_storm::ReactToRainOnFire(transform->position);
		}
	}
	else
	{
		if (fire.previous > fire.temperature)
		{
			fire.flags |= FireEffect::Cooling;
		}
		heat = !cools;
	}
	if (heat)
	{
		// burning: T += 0.1 T / 2 Tc, up to 2 Tc
		fire.temperature += 0.1f * fire.temperature / fire.Tmax();
		if (fire.temperature > fire.Tmax())
		{
			fire.temperature = fire.Tmax();
		}
	}
	else
	{
		const float ambient = fire.Ambient();
		fire.temperature = ambient < fire.temperature ? fire.temperature : ambient;
		if (!(fire.temperature > fire.previous))
		{
			// nothing heated it this turn: T -= (T + 10 - Tamb) x 4 H r x 0.1 x m / cap
			const float area = 4.0f * traits::Height(fire.object) * traits::Radius(fire.object);
			fire.temperature -= (fire.temperature + 10.0f - fire.Ambient()) * area * 0.1f * multiplier / fire.Capacity();
			if (fire.previous >= tc && fire.temperature < tc)
			{
				fire.flags |= FireEffect::JustExtinguished;
			}
		}
	}
	if (fire.temperature >= tc)
	{
		// burning: damage and charring
		// 0.1 [0x999630] (0x72F418); 0.6 [0x999658] (0x72F428); +0.04 [0x999654] (0x72F435); 1 / 0.6 [0xDA09C0] (made
		// at 0x72EA16, used at 0x72F467)
		const float damage = (fire.temperature - tc) / (fire.Tmax() - tc) * defenceBurn * 0.1f;
		const float life = life::LifeOf(fire.object);
		if (life < 0.6f)
		{
			float charring = std::min(fire.charring + 0.04f, 1.0f);
			const float limit = std::max((0.6f - life) * (1.0f / 0.6f), 0.0f);
			fire.charring = limit < charring ? limit : charring;
		}
		if (life > 0.0f)
		{
			traits::ReduceLifeDueToBurning(fire.object, damage, fire.hasPlayer, fire.player);
			if (!registry.Valid(fire.object))
			{
				return;
			}
			if (!traits::IsAvailable(fire.object) || life::LifeOf(fire.object) <= 0.0f)
			{
				if (!traits::IsCreature(fire.object))
				{
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: object {} burnt down (T {:.0f})",
						                   static_cast<int>(fire.object), fire.temperature);
					}
					// vt 0x5F8 (0x72F510) gets the fire's GetPlayer (0x72F509). (aproximado: the player is not passed:
					// VillagerDead and the abode/animal handlers lose who burnt it)
					traits::DestroyedByEffect(fire.object);
					if (!registry.Valid(fire.object))
					{
						ToBeDeleted(fire); // Object::ToBeDeleted took the fire with it
					}
				}
				if ((fire.flags & FireEffect::Deleted) != 0)
				{
					return;
				}
			}
		}
	}
	else
	{
		const float life = life::LifeOf(fire.object);
		if (fire.charring != 0.0f && life != 0.0f)
		{
			const float charring = std::max(fire.charring - 0.02f, 0.0f); // 0.02 [0x999650] (0x72F558); 0.6 0x72F57F
			const float limit = std::max((0.6f - life) * (1.0f / 0.6f), 0.0f);
			fire.charring = charring < limit ? charring : limit;
		}
	}
	// spread: every object in the spiral of cells within R + 10 m of the fire centre. (The wind fn_00771B10 is
	// computed there but its result is overwritten: it doesn't move the search.)
	const float radius = fire.FireRadius();
	if (InBounds(centre))
	{
		bool search = true;
		if (traits::InHand(fire.object))
		{
			// held: only inside the holder's influence (0x72F61C: GetPlayerHoldingThis 0x63A190, then
			// CalculatePlayerInfluence 0x5CD170 with 1, 0, 0). (inferido: one local hand, PLAYER_ONE; openblack has no
			// holder-player accessor)
			search = influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, glm::vec3(centre.x, 0.0f, centre.z)) > 0.0f;
		}
		if (search)
		{
			const float reach = radius + 10.0f;
			// the spiral walks the fire's own MapCoords (copied at 0x72F5E1..0x72F608): the distance is
			// GetDistanceInMetres 0x74CD70 (0x72F674) between it and the centre, and the step is Spiral 0x74D7E0
			// (0x72F6C3) then operator+= 0x605470 (0x72F6D0), which only adds to the high words: the fraction is the
			// same in both, so the difference is a whole number of 10 m cells, and the 16-bit add wraps at the edge
			const auto start = map_coords::FromMetres(glm::vec2(centre.x, centre.z));
			auto coords = start;
			map_coords::Spiral spiral; // GUtils::Spiral 0x74D7E0, from dir = count = 1
			std::vector<entt::entity> objects;
			std::unordered_set<entt::entity> heated; // (inf) an object spanning several cells is heated once
			for (int steps = 99999; steps != 0; --steps)
			{
				const auto cell = map_coords::Cell(coords);
				// 0x72F674: GetDistanceInMetres 0x74CD70 (table hypotenuse, ConvertWholeDistanceToMeters 0x74DCC0)
				if (!(gutils::GetDistanceInMetres(coords, start) <= reach))
				{
					break;
				}
				CellObjects(glm::ivec2(cell), objects);
				for (const auto object : objects)
				{
					if (object != fire.object && heated.insert(object).second)
					{
						HeatTransfer(fire, object);
						if ((fire.flags & FireEffect::Deleted) != 0)
						{
							return;
						}
					}
				}
				map_coords::AddCells(coords, spiral.Next());
			}
		}
	}
	// the group root that no longer burns hands the group to the first member that does
	if (fire.root == &fire && !fire.IsAboveReactionTemperature() && fire.next != nullptr)
	{
		for (FireEffect* member = &fire; member != nullptr; member = member->next)
		{
			if (member->IsAboveReactionTemperature())
			{
				HandRootTo(fire, *member);
				break;
			}
		}
	}
	// the fire's reaction (0x72F729: not for objects in the hand or flying, Object +0x24 & 0x44; bit 0x40 is FLYING,
	// GScript::GetProperty 0x70DAE0. inf: PhysicsObjects::IsFlying stands for bit 0x40)
	if (!traits::InHand(fire.object) && !physics::PhysicsObjects::IsFlying(fire.object))
	{
		// a reaction removed with the rest of its object's (Pot::RemoveReaction 0x66D6A0 takes them all): the id is
		// forgotten, and a new one made while it burns (inf: the original keeps a pointer there)
		if (fire.reaction != 0 && effects::reactions::Find(fire.reaction) == nullptr)
		{
			fire.reaction = 0;
		}
		if (fire.reaction == 0)
		{
			if (fire.IsAboveReactionTemperature() && !traits::IsVillager(fire.object))
			{
				fire.reaction = effects::reactions::CreateReaction(fire.object, Reaction::ReactToFire,
				                                                   fire.hasPlayer ? fire.player : PlayerNames::NEUTRAL, true);
			}
		}
		else if (!fire.IsAboveReactionTemperature())
		{
			fire.reaction = 0;
			RemoveReactions(fire.object, Reaction::ReactToFire);
			RemoveReactions(fire.object, Reaction::ReactToBurningObjectInHand);
		}
	}
	// the fire sound: the 2 fires nearest the camera with a fraction above 0.1
	sound::Consider(fire, fire.FireFraction() > 0.1f);
	fire.previous = fire.temperature;
}
} // namespace

bool fire::TraceEnabled()
{
	static const bool enabled = [] {
		const char* value = std::getenv("OPENBLACK_FIRE_TRACE");
		return value != nullptr && value[0] != '\0' && value[0] != '0';
	}();
	return enabled;
}

float FireEffect::Tc() const
{
	if (object == entt::null)
	{
		return 0.0f;
	}
	return std::max(traits::CombustionTemperature(object), 40.0f);
}

float FireEffect::Ambient() const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return AmbientTemperature(transform != nullptr ? transform->position : glm::vec3(0.0f));
}

float FireEffect::Capacity() const
{
	return std::max(traits::HeatCapacity(object), 1.0f);
}

bool FireEffect::IsOnFire() const
{
	return !(Tc() > temperature);
}

bool FireEffect::IsAboveReactionTemperature() const
{
	return temperature >= 100.0f || !(Tc() > temperature);
}

float FireEffect::FireFraction() const
{
	const float tc = Tc();
	float fraction = (temperature - 0.8f * tc) / (Tmax() - 0.8f * tc);
	const float twiceLife = 2.0f * life::LifeOf(object);
	if (!(twiceLife > fraction))
	{
		fraction = twiceLife;
	}
	if (!(fraction > 0.0f))
	{
		return 0.0f;
	}
	return fraction < 1.0f ? fraction : 1.0f;
}

float FireEffect::FireRadius() const
{
	const float fraction = std::clamp(FireFraction(), 0.0f, 1.0f);
	return traits::DefaultFireRadius(object) * 1.25f * fraction;
}

float FireEffect::MaxFireRadius() const
{
	return object != entt::null ? traits::DefaultFireRadius(object) * 1.25f : 0.0f;
}

float FireEffect::SafeFireRadius() const
{
	if (object == entt::null)
	{
		return 0.0f;
	}
	const float maximum = MaxFireRadius();
	const float radius = FireRadius();
	return (radius > maximum ? maximum : radius) + 1.0f;
}

float FireEffect::FlameHeight() const
{
	float range = Tmax() - Ambient();
	if (range < 0.0001f)
	{
		range = 0.0001f;
	}
	float fraction = (temperature - Ambient()) / range;
	fraction = fraction > 0.0f ? (fraction < 1.0f ? fraction : 1.0f) : 0.0f;
	return traits::Height(object) * 1.25f * fraction;
}

float FireEffect::HeatContent() const
{
	return (temperature - Ambient()) * Capacity();
}

void FireEffect::AddHeat(float heat, float maxChange)
{
	float change = heat / Capacity();
	if (std::abs(maxChange) < std::abs(change))
	{
		change = maxChange;
	}
	temperature += change;
}

float FireEffect::GroupBurningRadius() const
{
	float sum = 0.0f;
	for (const auto* member = root; member != nullptr; member = member->next)
	{
		if (member->IsOnFire())
		{
			sum += traits::Radius(member->object);
		}
	}
	return sum;
}

float FireEffect::GroupBurningPriority() const
{
	float highest = 0.0f;
	for (const auto* member = root; member != nullptr; member = member->next)
	{
		if (member->object != entt::null)
		{
			const float priority = traits::BurningPriority(member->object);
			if (priority > highest)
			{
				highest = priority;
			}
		}
	}
	return highest;
}

FireEffect* FireEffect::NearestFireToFight(const glm::vec3& position) const
{
	if (root == nullptr)
	{
		return nullptr;
	}
	FireEffect* best = nullptr;
	float bestDistance = 1e7f; // 0x4B189680
	for (FireEffect* member = root; member != nullptr; member = member->next)
	{
		if (member->object == entt::null)
		{
			continue;
		}
		const auto centre = traits::FireCentre(member->object);
		const float objectRadius = traits::DefaultFireRadius(member->object);
		const float safe = member->SafeFireRadius();
		const float keep = safe < objectRadius ? objectRadius : safe;
		// fn_0074CD50 at 0x73010A (the symbol says ReactionInfo::GetInfo): the distance from the position to the fire
		// centre, the 16.16 x/z delta (fn_0074CCE0) through ConvertWholeDistanceToMeters 0x74DCC0
		const float distance = Distance2D(position, centre) - keep;
		if (distance < bestDistance && member->IsAboveReactionTemperature())
		{
			bestDistance = distance;
			best = member;
		}
	}
	return best;
}

void fire::AddToFireGroup(FireEffect& self, FireEffect& other)
{
	AddToMyFireGroup(self, other);
}

FireEffect* fire::Find(entt::entity object)
{
	const auto it = g_ByObject.find(object);
	return it != g_ByObject.end() ? it->second : nullptr;
}

FireEffect* fire::Get(uint32_t id)
{
	const auto it = g_ById.find(id);
	return it != g_ById.end() ? it->second : nullptr;
}

float fire::AmbientTemperature([[maybe_unused]] const glm::vec3& position)
{
	return 24.7f;
}

float fire::GetTemperature(entt::entity object)
{
	if (const auto* fire = Find(object))
	{
		return fire->temperature;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return AmbientTemperature(transform != nullptr ? transform->position : glm::vec3(0.0f));
}

bool fire::IsOnFire(entt::entity object)
{
	const auto* fire = Find(object);
	return fire != nullptr && fire->IsOnFire();
}

FireEffect* fire::Create(entt::entity object, bool hasPlayer, PlayerNames player, entt::entity source)
{
	auto& registry = Locator::entitiesRegistry::value();
	// (port) registry.Valid stands for the original's "being deleted" test (Object +0x0A bit 0)
	if (!registry.Valid(object) || !traits::IsBurnReceiver(object, 100.0f) || traits::CannotBeSetOnFire(object) ||
	    traits::CombustionTemperature(object) == 0.0f)
	{
		return nullptr;
	}
	if (auto* existing = Find(object))
	{
		return existing; // (the port) one fire per object, as Object +0x44
	}
	// fn_0072EB00
	auto owned = std::make_unique<FireEffect>();
	auto* fire = owned.get();
	fire->id = g_NextId++;
	fire->tag = g_CreateTag;
	g_CreateTag = 0;
	fire->object = object;
	fire->source = source;
	fire->hasPlayer = hasPlayer;
	fire->player = player;
	fire->temperature = GetTemperature(object);
	fire->previous = fire->temperature;
	g_ByObject[object] = fire;
	g_ById[fire->id] = fire;
	g_List.insert(g_List.begin(), fire);
	g_Pool.push_back(std::move(owned));
	graphic::Create(*fire); // CreateSprites 0x730AD0 -> fn_00731160
	fire->root = fire;
	fire->next = nullptr;
	traits::StartOnFire(object);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: new fire {} on object {} (T {:.1f}, Tc {:.0f}, cap {:.1f})", fire->id,
		                   static_cast<int>(object), fire->temperature, fire->Tc(), fire->Capacity());
	}
	return fire;
}

void fire::ToBeDeleted(FireEffect& fire)
{
	if ((fire.flags & FireEffect::Deleted) != 0)
	{
		return;
	}
	std::erase(g_List, &fire);
	RemoveFromFireGroup(fire);
	fire.flags |= FireEffect::Deleted;
	if (fire.object != entt::null)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(fire.object))
		{
			traits::EndOnFire(fire.object);
			if (fire.reaction != 0) // 0x72EC3E: only with a reaction (+0x28)
			{
				fire.reaction = 0;
				RemoveReactions(fire.object, Reaction::ReactToFire);
				RemoveReactions(fire.object, Reaction::ReactToBurningObjectInHand);
			}
		}
		g_ByObject.erase(fire.object);
		fire.object = entt::null;
	}
	graphic::Destroy(fire);
	sound::Free(fire);
	g_ById.erase(fire.id);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: fire {} deleted", fire.id);
	}
}

void fire::SetTemperature(entt::entity object, float temperature, entt::entity source)
{
	FireEffect* fire = Find(object);
	if (fire == nullptr)
	{
		if (!(GetTemperature(object) < temperature))
		{
			return;
		}
		PlayerNames player = PlayerNames::NEUTRAL;
		const bool hasPlayer = PlayerOfObject(object, player);
		fire = Create(object, hasPlayer, player, source);
		if (fire == nullptr)
		{
			return;
		}
	}
	fire->temperature = temperature;
}

void fire::SetOnFire(entt::entity object, float speed)
{
	FireEffect* fire = Find(object);
	if (fire == nullptr)
	{
		PlayerNames player = PlayerNames::NEUTRAL;
		const bool hasPlayer = PlayerOfObject(object, player);
		fire = Create(object, hasPlayer, player, entt::null);
		if (fire == nullptr)
		{
			return;
		}
	}
	fire->temperature = fire->Tmax() * speed + fire->Tc();
}

void fire::ApplyEffectToFireEffectIfNecessary(entt::entity object, const effects::EffectValues& values)
{
	// GetActualObjectToEffect(GetCausedPlayer, 1): the object itself
	const entt::entity target = object;
	if (!Locator::entitiesRegistry::value().Valid(target))
	{
		return;
	}
	const float burn = values.numbers[effects::EffectValues::Burn];
	if (burn == 0.0f)
	{
		return;
	}
	FireEffect* fire = Find(target);
	if (fire == nullptr)
	{
		if (!(burn > 0.0f))
		{
			return;
		}
		fire = Create(target, values.hasPlayer, values.player, values.appliedBy);
		if (fire == nullptr)
		{
			return;
		}
	}
	const float difference = fire->Ambient() + burn - fire->temperature;
	fire->AddHeat(HeatFromDifference(difference), difference);
	if (traits::IsVillager(target) && !villager_fire::IsInOnFireState(target))
	{
		villager_fire::SetupOnFire(target, 0);
	}
}

void fire::CheckToSeeIfObjectIsNearOnFireObject(entt::entity object)
{
	if (!traits::IsBurnReceiver(object, 100.0f))
	{
		return;
	}
	const auto centre = traits::FireCentre(object);
	if (!InBounds(centre))
	{
		return;
	}
	std::vector<entt::entity> objects;
	CellObjects(glm::ivec2(MapInterface::GetGridCell(glm::vec2(centre.x, centre.z))), objects);
	for (const auto other : objects)
	{
		if (other == object)
		{
			continue;
		}
		if (auto* fire = Find(other); fire != nullptr)
		{
			HeatTransfer(*fire, object);
		}
	}
}

void fire::CopyFire(entt::entity from, entt::entity to)
{
	auto* source = Find(from);
	if (source == nullptr || to == entt::null)
	{
		return;
	}
	FireEffect* fire = Find(to);
	if (fire == nullptr)
	{
		fire = Create(to, source->hasPlayer, source->player, source->source);
		if (fire == nullptr)
		{
			return;
		}
	}
	AddToMyFireGroup(*source, *fire);
	const float temperature = fire->temperature > source->temperature ? fire->temperature : source->temperature;
	fire->previous = temperature;
	fire->temperature = temperature;
}

void fire::MoveFire(entt::entity from, entt::entity to)
{
	auto* fire = Find(from);
	if (fire == nullptr || to == entt::null)
	{
		return;
	}
	g_ByObject.erase(from);
	g_ByObject[to] = fire;
	if (fire->reaction != 0)
	{
		// fn_006E4830: the reaction's initiator becomes the new object
		effects::reactions::SetInitiator(fire->reaction, to);
	}
	fire->object = to;
}

void fire::StartedMoving(entt::entity object, bool inHand)
{
	auto* fire = Find(object);
	if (fire == nullptr)
	{
		return;
	}
	RemoveFromFireGroup(*fire);
	if (fire->reaction != 0)
	{
		fire->reaction = 0;
		RemoveReactions(object, Reaction::ReactToFire);
	}
	if (inHand)
	{
		fire->reaction = effects::reactions::CreateReaction(object, Reaction::ReactToBurningObjectInHand,
		                                                    fire->hasPlayer ? fire->player : PlayerNames::NEUTRAL, true);
	}
}

void fire::SetOutMagicHand(entt::entity object)
{
	auto* fire = Find(object);
	if (fire == nullptr)
	{
		return;
	}
	fire->reaction = 0;
	RemoveReactions(object, Reaction::ReactToBurningObjectInHand);
}

void fire::ProcessList()
{
	g_ProcessTag = 0;
	sound::RefreshDistances();
	// the list as it is now: the fires this turn creates join the head and wait for the next turn
	const auto list = g_List;
	for (auto* fire : list)
	{
		if ((fire->flags & FireEffect::Deleted) != 0 || fire->tag != g_ProcessTag)
		{
			continue;
		}
		Process(*fire);
	}
	sound::StartSlots();
	// the deleted ones are freed (GameThing deletion is deferred in the original too)
	std::erase_if(g_Pool, [](const std::unique_ptr<FireEffect>& fire) { return (fire->flags & FireEffect::Deleted) != 0; });
}

const std::vector<FireEffect*>& fire::All()
{
	return g_List;
}

void fire::Clear()
{
	for (auto* fire : g_List)
	{
		graphic::Destroy(*fire);
	}
	sound::Clear();
	g_List.clear();
	g_ByObject.clear();
	g_ById.clear();
	g_Pool.clear();
	g_ProcessTag = 0;
	g_CreateTag = 0;
	traits::Clear();
}
