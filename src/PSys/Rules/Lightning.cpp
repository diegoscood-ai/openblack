/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's classes (SF_LightningBolt / PUOne / PUTwo, SF_LightningStrike, SF_LightningStorm;
// PSysLightning.cpp 0x68FD50-0x6941B0): UR_Lightning picks the objects inside a cone in front of the hand, builds one
// chain of joints per fork towards each of them (ParticleChainCreator, Creators/Chain.cpp), drops a light map where
// every fork ends (Creators/LightMap.cpp) and sends the spell a "landed" event at each tip, which is what burns and
// kills. UR_LightningStrike is the one-shot script / climate strike. Report: tmp_dis\miracles\destructive.md §4; wiki
// docs/bw1-notes/magic.md, "Rayo".

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <ranges>
#include <numbers>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Audio/SpellSounds.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "PSys/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// GUtils::Spiral 0x74D7E0 (table 0xDA59FC: +x, +z, -x, -z): the next 10 m cell of the outward spiral
glm::ivec2 SpiralStep(int& direction, int& count)
{
	constexpr std::array<glm::ivec2, 4> k_Steps = {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1)};
	const auto step = k_Steps[static_cast<size_t>(direction & 3)];
	if (--count == 0)
	{
		++direction;
		count = (direction % 2) == 0 ? direction / 2 + 1 : (direction + 1) / 2;
	}
	return step;
}

/// The objects of one 10 m map cell (MapCoords::FindType(-1) walks the cell's list)
void CellObjects(const glm::ivec2& cell, std::vector<entt::entity>& out)
{
	out.clear();
	if (!Locator::entitiesMap::has_value() || cell.x < 0 || cell.y < 0 || cell.x >= ecs::MapInterface::k_GridSize.x ||
	    cell.y >= ecs::MapInterface::k_GridSize.y)
	{
		return;
	}
	const auto& map = Locator::entitiesMap::value();
	const ecs::MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
	out.insert(out.end(), map.GetFixedInGridCell(id).begin(), map.GetFixedInGridCell(id).end());
	out.insert(out.end(), map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
	std::sort(out.begin(), out.end()); // (inf) the cell lists are unordered sets here: a stable order
}

/// fn_00690090: a spell seed is never struck (the bolt must not shoot at the miracle icons lying about)
bool CanBeStruck(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		return false;
	}
	// TODO(M4): a creature is also skipped when creature+0x12B0 < 0.5 (fn_0047ACC0; UNVERIFIED meaning)
	return !registry.AllOf<ecs::components::SpellSeed>(object);
}

/// LightningObjectInfo (0x1C bytes): one thing the bolt may strike this cast
struct Target
{
	entt::entity object {entt::null}; ///< +0x04 (null: a ground point)
	glm::vec3 ground {0.0f};          ///< +0x08
	bool isObject {false};            ///< +0x14
	bool active {false};              ///< +0x15 it may be struck
	bool lightMapDone {false};        ///< +0x16 its "impressive" report is in
	int cooldown {0};                 ///< +0x18 steps before it may be struck again

	/// fn_00691E00: an object's tip is its centre raised by its height, a ground point is itself
	[[nodiscard]] glm::vec3 Tip() const
	{
		if (!isObject)
		{
			return ground;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
		{
			return ground;
		}
		const auto& transform = registry.Get<const ecs::components::Transform>(object);
		return {transform.position.x, LandAt(transform.position.x, transform.position.z) + ecs::effects::ObjectHeight(object),
		        transform.position.z};
	}
};

/// UR_Lightning_CollectionData (0xC0 bytes): what the rule keeps for one collection between steps
struct Data
{
	glm::vec3 origin {0.0f};    ///< +0x30 where the forks start (the gesture position, or the parent atom)
	float heading {0.0f};       ///< +0x54 atan2(cameraForward.z, cameraForward.x), fn_00673650
	bool first {true};          ///< +0x58 the fork structure is not built yet
	float life {0.0f};          ///< +0x5C the collection's age in steps x dt
	std::vector<Target> targets; ///< +0x6C / +0x74
	std::vector<int> striking;   ///< +0x80 / +0x84 the targets picked this step
	float forkScale {1.0f};      ///< +0xAC ForkScale x FP_ForkScale
	glm::vec3 searchOrigin {0.0f}; ///< +0xB4 the origin of the last target search
	float sinceSearch {0.0f};
	Atom* root {nullptr}; ///< the single atom of the collection; its sub-collections are the forks
	std::chrono::steady_clock::time_point touched;
};

/// The data of a live collection, keyed as the fireball's balls are: a float in the slot names it, so nothing dangles
/// when the collection goes, and an entry no step touched for ten seconds is dropped (port bookkeeping, wall clock: a
/// pause longer than that resets a live bolt's targets and cooldowns)
uint32_t g_NextKey = 1;
std::unordered_map<uint32_t, Data> g_Data;

Data& DataFor(Collection::Slot& slot)
{
	const auto now = std::chrono::steady_clock::now();
	std::erase_if(g_Data, [now](const auto& entry) { return now - entry.second.touched > std::chrono::seconds(10); });
	auto key = static_cast<uint32_t>(slot.extra.x);
	if (key == 0 || !g_Data.contains(key))
	{
		key = g_NextKey++ & 0xFFFFFF;
		slot.extra.x = static_cast<float>(key);
		g_Data[key] = Data {};
	}
	auto& data = g_Data[key];
	data.touched = now;
	return data;
}

/// UR_Lightning (props 0x6B2500; ctor defaults 0x6900B0, ActualModify 0x6914C0)
class Lightning final: public Modifier
{
public:
	explicit Lightning(const Object& object)
	    : creator(object.String("PCreator"))
	    , lightMapCreator(object.String("PCreatorLightMapAtom"))
	    , forkGroup(object.Int("ForkGroup", -1))
	    , commonGlowGroup(object.Int("CommonGlowGroup", -1))
	    , lightMapGroup(object.Int("LightMapGroup", -1))
	    , maxObjects(std::max(1, object.Int("MaxLightningObjects", 50)))
	    , minObjects(std::max(1, object.Int("MinLightningObjects", 3)))
	    , atOnce(std::max(1, object.Int("MaxLightningObjectsAtOnce", 10)))
	    , maxJoints(std::max(2, object.Int("MaxJointsPerFork", 10)))
	    , splitAngle(object.Float("SplitAngle", std::numbers::pi_v<float> / 2.0f))
	    , randomFrac(object.Float("RandomFrac", 0.1f))
	    , forkScale(object.Float("ForkScale", 1.0f))
	    , forkScaleProvider(object.String("FP_ForkScale"))
	    , averageLightmapLife(object.Float("AverageLightmapLife", 0.5f))
	    , searchRadius(object.String("SearchRadius"))
	    , defaultSearchRadius(object.Float("DefaultSearchRadius", 20.0f))
	    , castingFromHand(object.Bool("CastingFromHand", true))
	    , takeTargetsFromManager(object.Bool("TakeTargetsFromManager", false))
	    , renewTargetsOnMove(object.Bool("RenewTargetsOnMove", false))
	    , renewTargetsOnMoveFrac(object.Float("RenewTargetsOnMoveFrac", 0.5f))
	    , renewSearchEvery(object.Float("RenewSearchEvery", 1.0f))
	    , sound(ReadSoundAction(object, "SoundLightning"))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& data = DataFor(slot);
		data.life += effect.GetDt();
		// ActualModify 0x6914C0: the origin is the gesture position when cast from the hand, else the parent atom
		data.origin = castingFromHand              ? effect.GetProcessInfo().handPos
		              : collection.parent != nullptr ? effect.GlobalPosition(*collection.parent)
		                                            : effect.GetOrigin();
		// fn_00673650: the heading the cone points at is the camera's forward, not the hand's velocity
		const auto& forward = effect.GetProcessInfo().cameraForward;
		if (forward.x != 0.0f || forward.z != 0.0f)
		{
			data.heading = std::atan2(forward.z, forward.x);
		}
		// ForkScale x FP_ForkScale (0x691BB0)
		data.forkScale = forkScale * (forkScaleProvider.empty() ? 1.0f : effect.FloatProvider(forkScaleProvider, 1.0f));
		if (effect.Closing())
		{
			// CloseDown: the forks go
			collection.atoms.clear();
			data.root = nullptr;
			data.first = true;
			return true;
		}
		if (data.first)
		{
			FindTargets(effect, data);
			CreateForkStructure(effect, collection, data);
			data.first = false;
			data.searchOrigin = data.origin;
			data.sinceSearch = 0.0f;
			audio::spell_sounds::StartSound(effect, *data.root, sound);
		}
		else if (Renew(effect, data))
		{
			FindTargets(effect, data);
			data.searchOrigin = data.origin;
			data.sinceSearch = 0.0f;
			// (inf) the original keeps the structure it built on the first step; here it is built again when the new
			// search found more targets than it has forks for, so that none of them is left without one
			if (data.root != nullptr && 2 * data.targets.size() > data.root->subCollections.size())
			{
				CreateForkStructure(effect, collection, data);
			}
		}
		// 0x691601..0x691691: UpdateForkStructure only while the manager IsInState(2) (PSysProcessInfo +0x38, enabled);
		// otherwise the forks stay as the last step left them
		if (effect.GetProcessInfo().enabled)
		{
			UpdateForkStructure(effect, collection, data);
		}
		return true;
	}

private:
	[[nodiscard]] float SearchRadius(const Effect& effect) const
	{
		return searchRadius.empty() ? defaultSearchRadius : effect.FloatProvider(searchRadius, defaultSearchRadius);
	}

	/// fn_006916B0 / fn_00691390: the targets are searched again when the origin moved more than
	/// RenewTargetsOnMoveFrac x SearchRadius, or every RenewSearchEvery seconds
	[[nodiscard]] bool Renew(const Effect& effect, Data& data) const
	{
		data.sinceSearch += effect.GetDt();
		if (renewTargetsOnMove &&
		    glm::distance(data.origin, data.searchOrigin) > renewTargetsOnMoveFrac * SearchRadius(effect))
		{
			return true;
		}
		return renewSearchEvery > 0.0f && data.sinceSearch >= renewSearchEvery;
	}

	/// fn_00690F50: the three target modes, tested in this order (0x690F88 CastingFromHand +0x75, 0x690F9E
	/// TakeTargetsFromManager +0x74): fn_006901E0 from the hand (the cone), fn_00690C70 from the manager's SpellTargets
	/// and fn_00690880 around the parent atom (the storm: no cone, a circle of the radius)
	void FindTargets(Effect& effect, Data& data) const
	{
		data.targets.clear();
		if (castingFromHand)
		{
			SearchAround(effect, data, true);
		}
		else if (takeTargetsFromManager)
		{
			for (const auto target : effect.GetTargets())
			{
				if (static_cast<int>(data.targets.size()) >= maxObjects)
				{
					break;
				}
				if (CanBeStruck(target))
				{
					data.targets.push_back({target, glm::vec3(0.0f), true, true, false, 0});
				}
			}
		}
		else
		{
			SearchAround(effect, data, false);
		}
		// (inferido) the manager mode gets the ground points too: fn_00690C70 was not read
		AddGroundPoints(effect, data, castingFromHand || takeTargetsFromManager);
	}

	/// fn_006901E0: the spiral of 4 ceil(R/10)^2 cells around the origin; an object counts when it is available, not a
	/// spell seed, and its horizontal direction is inside the cone of half-angle SplitAngle about the heading.
	/// fn_00690880 (`cone` false): the same spiral, but an object counts when dx^2 + dz^2 < R^2 (0x6909E5..0x690A1D)
	void SearchAround(Effect& effect, Data& data, bool cone) const
	{
		const float radius = SearchRadius(effect);
		const auto side = static_cast<int>(std::ceil(radius / 10.0f));
		const int cells = 4 * side * side;
		const glm::ivec2 start(static_cast<int>(data.origin.x * 0.1f), static_cast<int>(data.origin.z * 0.1f));
		const float limit = std::cos(splitAngle);
		const glm::vec2 heading(std::cos(data.heading), std::sin(data.heading));
		glm::ivec2 cell(start);
		int direction = 0;
		int count = 1;
		std::vector<entt::entity> objects;
		for (int i = 0; i < cells && static_cast<int>(data.targets.size()) < maxObjects; ++i)
		{
			CellObjects(cell, objects);
			for (const auto object : objects)
			{
				if (static_cast<int>(data.targets.size()) >= maxObjects)
				{
					break;
				}
				if (!CanBeStruck(object))
				{
					continue;
				}
				const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object);
				const glm::vec2 offset(transform.position.x - data.origin.x, transform.position.z - data.origin.z);
				const float length = glm::length(offset);
				const bool outside = cone ? length > 0.0f && glm::dot(offset / length, heading) <= limit
				                          : !(length * length < radius * radius);
				if (outside)
				{
					continue;
				}
				data.targets.push_back({object, transform.position, true, true, false, 0});
			}
			cell += SpiralStep(direction, count);
		}
	}

	/// fn_006901E0's tail: when fewer than MinLightningObjects were found, the rest are ground points at
	/// heading + rand(pi/4) and rand(0.6 R) away, two metres above the land. fn_00690880's tail (0x690B0B..0x690BF2,
	/// `aimed` false): at rand(2 pi) (0x40C90FDB) instead
	void AddGroundPoints(Effect& effect, Data& data, bool aimed) const
	{
		const float radius = SearchRadius(effect);
		while (static_cast<int>(data.targets.size()) < minObjects)
		{
			const float angle = aimed ? data.heading + effect.Random(std::numbers::pi_v<float> / 4.0f)
			                          : effect.Random(2.0f * std::numbers::pi_v<float>);
			const float distance = effect.Random(0.6f * radius);
			const glm::vec3 point(data.origin.x + distance * std::cos(angle), 0.0f,
			                      data.origin.z + distance * std::sin(angle));
			data.targets.push_back({entt::null, glm::vec3(point.x, LandAt(point.x, point.z) + 2.0f, point.z), false, true,
			                        false, 0});
		}
	}

	/// CreateForkStructure 0x691190: one atom in the collection, with two fork sub-collections per target, each holding
	/// MaxJointsPerFork chain joints
	void CreateForkStructure(Effect& effect, Collection& collection, Data& data) const
	{
		collection.atoms.clear();
		const std::vector<int> groups(2 * data.targets.size(), forkGroup);
		auto& root = effect.NewAtom(collection, effect.FindCreator(creator), {});
		root.visible = false; // (inferido) the root itself is not drawn: it only carries the forks
		effect.AddSubCollections(root, groups);
		const auto* joints = effect.FindCreator(creator);
		for (auto& fork : root.subCollections)
		{
			for (int i = 0; i < maxJoints; ++i)
			{
				effect.NewAtom(*fork, joints, {});
			}
		}
		data.root = &root;
	}

	/// UpdateForkStructure 0x691BB0: the cooldowns tick down, a set of targets is picked, and the recursion
	/// fn_00691F30 lays the joints of their forks out and fires the events
	void UpdateForkStructure(Effect& effect, Collection& collection, Data& data) const
	{
		if (data.root == nullptr || collection.atoms.empty() || collection.atoms.front().get() != data.root)
		{
			data.first = true; // the atoms went (a remove rule): build them again next step
			return;
		}
		for (auto& target : data.targets)
		{
			if (target.cooldown > 0)
			{
				--target.cooldown;
			}
		}
		data.striking.clear();
		const auto total = static_cast<int>(data.targets.size());
		if (total == 0)
		{
			return;
		}
		// each target with probability min(AtOnce, (N + 1) / 2) / N, at most that many (0x691C5B..0x691CF3)
		// TODO(M5): the other branch, 0x691C3F / 0x691CF5: when collection data +0x24 or +0x20 is set, ONE target
		// is picked, the first active one from PSysRand(N) on (a random rotation). Which renew path sets those flags
		// is not read, so every step takes this branch.
		const int limit = std::min(atOnce, (total + 1) / 2);
		const float probability = static_cast<float>(limit) / static_cast<float>(total);
		for (int i = 0; i < total && static_cast<int>(data.striking.size()) < limit; ++i)
		{
			if (data.targets[static_cast<size_t>(i)].active && effect.Random(1.0f) < probability)
			{
				data.striking.push_back(i);
			}
		}
		// every fork is hidden, then the struck ones are laid out (the atoms stay, as the original's do)
		for (auto& fork : data.root->subCollections)
		{
			for (auto& atom : fork->atoms)
			{
				atom->visible = false;
			}
		}
		size_t next = 0;
		for (const int index : data.striking)
		{
			auto& target = data.targets[static_cast<size_t>(index)];
			if (target.cooldown > 0 || next + 1 >= data.root->subCollections.size())
			{
				continue;
			}
			const auto tip = target.Tip();
			// (aproximado) the main fork reaches the target and a second one ends at a random point
			// tan(SplitAngle) x distance around the tip. The original's recursive fork tree (fn_00691F30) is not
			// ported, and these offsets (2 spread in x/z, + spread in y) are the port's (wiki_section.md m5).
			// TODO(M6): the segment shield test fn_006D0BC0(point, 2.5) -> SpellEvent 4 to the shield, fork cut there
			// (destructive.md §4.2; shields::FindShieldContainingPoint is the port's fn_006D0BC0)
			LayOutFork(effect, *data.root->subCollections[next++], data, data.origin, tip, true);
			const float spread = std::tan(splitAngle) * glm::distance(data.origin, tip);
			const glm::vec3 side(tip.x + effect.Random(2.0f * spread) - spread, tip.y + effect.Random(spread),
			                     tip.z + effect.Random(2.0f * spread) - spread);
			LayOutFork(effect, *data.root->subCollections[next++], data, data.origin, side, false);
			StrikeTarget(effect, data, target, tip);
		}
		if (next > 0 && std::getenv("OPENBLACK_SPELL_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Lightning: {} targets ({} objects), {} of {} forks struck from ({:.1f}, {:.1f}, {:.1f}) "
			                   "heading {:.2f} rad, fork scale {:.2f}",
			                   total,
			                   std::ranges::count_if(data.targets, [](const Target& t) { return t.isObject; }), next,
			                   data.root->subCollections.size(), data.origin.x, data.origin.y, data.origin.z, data.heading,
			                   data.forkScale);
		}
	}

	/// The joint loop of fn_00691F30 (0x69244d): the joints run from the origin to the tip, the inner ones jittered by
	/// +-RandomFrac of the length in Y and Z, the scale ramps with ForkScale and the alpha of a fork that has no target
	/// flickers between 128 and 255
	void LayOutFork(Effect& effect, Collection& fork, const Data& data, const glm::vec3& from, const glm::vec3& to,
	                bool hasTarget) const
	{
		const auto count = static_cast<int>(fork.atoms.size());
		if (count < 2)
		{
			return;
		}
		const auto direction = to - from;
		const float length = glm::length(direction);
		for (int i = 0; i < count; ++i)
		{
			auto& atom = *fork.atoms[static_cast<size_t>(i)];
			const float t = static_cast<float>(i) / static_cast<float>(count - 1);
			auto position = from + direction * t;
			if (i != 0 && i != count - 1)
			{
				position.y += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
				position.z += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
			}
			atom.position = position;
			atom.ruleScale = data.forkScale * (1.0f - t) + data.forkScale * t * 0.25f; // (inferido) the tip's 0.25
			atom.colour[3] = hasTarget ? 255 : static_cast<uint8_t>(128 + static_cast<int>(effect.Random(127.0f)));
			atom.visible = true;
		}
		if (commonGlowGroup >= 0 && fork.atoms.back()->subCollections.empty())
		{
			// (inferido) CommonGlowGroup: the glow sprite of the tip follows its joint (UR_FollowParent); where and
			// when the original attaches it is not read
			effect.AddSubCollections(*fork.atoms.back(), {commonGlowGroup});
		}
	}

	/// The tip of fn_00691F30 (0x692941): the cooldown, the light map atom (fn_00691E80), the "impressive" report once
	/// the collection is older than 0.2 s (fn_00691ED0) and the event that actually damages
	void StrikeTarget(Effect& effect, Data& data, Target& target, const glm::vec3& tip) const
	{
		// the target waits rand(AverageLightmapLife / dt) steps before it may be struck again
		// the max(dt, eps) is a port guard: the original multiplies by [0xD4E0F0] = 1/dt directly
		const auto steps = static_cast<int>(averageLightmapLife / std::max(effect.GetDt(), 1e-3f));
		target.cooldown = steps > 0 ? static_cast<int>(effect.Random(static_cast<float>(steps))) : 0;
		if (lightMapGroup >= 0)
		{
			if (auto* atom = effect.NewAtomInGroup(lightMapGroup, effect.FindCreator(lightMapCreator)); atom != nullptr)
			{
				// the original blits the light map into the landscape's light texture under the tip, so it always ends up
				// on the ground however high the tip is (part_render.md §8)
				atom->position = glm::vec3(tip.x, LandAt(tip.x, tip.z) + 0.1f, tip.z); // (inferido: port offset) +0.1
			}
		}
		if (!target.lightMapDone && data.life > 0.2f)
		{
			// TODO(M7): fn_00691ED0 -> fn_00692FA0, the "impressive" report of the struck object (GetImpressiveIntensity)
			target.lightMapDone = true;
		}
		SpellEventInfo event;
		event.type = SpellEventInfo::Landed;
		event.position = tip;
		event.strength = takeTargetsFromManager || !castingFromHand ? 2.0f : 1.0f;
		if (effect.GetSink() != nullptr)
		{
			effect.SendSpellEvent(event);
		}
		// TODO(M5): the event also goes to the struck object's own manager when it has one (fn_00690070,
		// destructive.md §4.2)
		// TODO(M5): without a spell (a script / climate strike, global 0xC029D0) the original applies the static
		// EffectValues of info 0xCC9704 at the tip. UNVERIFIED which GEffectInfo row that is.
	}

	std::string creator;
	std::string lightMapCreator;
	int forkGroup;
	int commonGlowGroup;
	int lightMapGroup;
	int maxObjects;
	int minObjects;
	int atOnce;
	int maxJoints;
	float splitAngle;
	float randomFrac;
	float forkScale;
	std::string forkScaleProvider;
	float averageLightmapLife;
	std::string searchRadius;
	float defaultSearchRadius;
	bool castingFromHand;
	bool takeTargetsFromManager;
	bool renewTargetsOnMove;
	float renewTargetsOnMoveFrac;
	float renewSearchEvery;
	SoundAction sound;
};

/// UR_LightningStrike 0x6937A0 (SF_LightningStrike / SF_LightningSingleStrike, the script and climate strike): one atom
/// with its NextGroups (where the UR_Lightning of the strike lives) and SOUND_SPELL_LIGHTNING, once
class LightningStrike final: public Modifier
{
public:
	explicit LightningStrike(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , sound(ReadSoundAction(object, "SoundLightning"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		audio::spell_sounds::StartSound(effect, atom, sound);
		return false;
	}
	std::string creator;
	std::vector<int> nextGroups;
	SoundAction sound;
};
} // namespace

void openblack::psys::RegisterLightningRules()
{
	RegisterModifier("UR_Lightning", MakeModifierOf<Lightning>);
	RegisterModifier("UR_LightningStrike", MakeModifierOf<LightningStrike>);
}
