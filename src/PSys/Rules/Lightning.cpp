/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's classes (SF_LightningBolt / PUOne / PUTwo, SF_LightningStrike, SF_LightningStorm;
// PSysLightning.cpp 0x68FD50-0x6941B0): UR_Lightning picks the objects inside a cone in front of the hand and each step
// grows a tree of forks (chains of joints, ParticleChainCreator, Creators/Chain.cpp) to the ones it strikes: a trunk
// that splits in two until every branch holds one target (fn_00691F30). It drops a light map and sends the spell a
// "landed" event at each struck tip, which is what burns and kills. UR_LightningStrike is the one-shot script / climate strike. Report: tmp_dis\miracles\destructive.md §4; wiki
// docs/bw1-notes/miracles.md, "Rayo".

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
#include "Audio/Services/SpellSounds.h"
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

/// GUtils::Spiral 0x74D7E0 (table 0xDA59FC: +x, +z, -x, -z, filled at start-up; the same walk as Explosion.cpp's
/// Spiral): `--count == 0` -> ++direction, count = direction / 2, and only then the step table[direction & 3] is
/// returned. fn_006901E0 / fn_00690880 start it at direction = count = 1 (0x6902A7..0x6902BB, 0x69090D..0x69091E), so
/// the first step is -x
glm::ivec2 SpiralStep(int& direction, int& count)
{
	constexpr std::array<glm::ivec2, 4> k_Steps = {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1)};
	if (--count == 0)
	{
		++direction;
		count = direction / 2;
	}
	return k_Steps[static_cast<size_t>(direction & 3)];
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
	/// +0x18: written when the targets are found (0x6910CE) and at each strike (0x692949), ticked down every step
	/// (0x691C26..0x691C31) but never read to skip a target: kept as data only
	int cooldown {0};

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
	float life {0.0f};          ///< +0x5C the age since the last target search (set to 0 by fn_00690F50 at 0x690F66)
	std::vector<Target> targets; ///< +0x6C / +0x74
	std::vector<int> striking;   ///< +0x80 / +0x84 the targets picked this step
	float forkScale {1.0f};      ///< +0xAC ForkScale x FP_ForkScale
	glm::vec3 searchOrigin {0.0f}; ///< +0x3C the origin of the last target search (0x690F72..0x690F85)
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
			audio::spell_sounds::StartSound(effect, *data.root, sound);
		}
		else if (Renew(effect, data))
		{
			// fn_00691390: the targets are searched again but the fork structure built on the first step stays; when
			// there are more targets than forks the recursion simply stops (0x692786)
			FindTargets(effect, data);
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

	/// fn_00691390: the targets are searched again when RenewSearchEvery > 0 and the age since the last search (+0x5C)
	/// is past it, or with RenewTargetsOnMove when the origin moved more than RenewTargetsOnMoveFrac x SearchRadius
	/// from where it was searched (+0x3C). (fn_006916B0 is the clash of two bolts, not ported: TODO(M5))
	[[nodiscard]] bool Renew(const Effect& effect, const Data& data) const
	{
		if (renewTargetsOnMove &&
		    glm::distance(data.origin, data.searchOrigin) > renewTargetsOnMoveFrac * SearchRadius(effect))
		{
			return true;
		}
		return renewSearchEvery > 0.0f && data.life > renewSearchEvery;
	}

	/// fn_00690F50: the three target modes, tested in this order (0x690F88 CastingFromHand +0x75, 0x690F9E
	/// TakeTargetsFromManager +0x74): fn_006901E0 from the hand (the cone), fn_00690C70 from the manager's SpellTargets
	/// and fn_00690880 around the parent atom (the storm: no cone, a circle of the radius)
	void FindTargets(Effect& effect, Data& data) const
	{
		data.targets.clear();
		data.life = 0.0f;               // 0x690F66
		data.searchOrigin = data.origin; // 0x690F72..0x690F85
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
		// 0x691072..0x6910CE: every target starts with +0x18 = PSysRand(AverageLightmapLife / (ms per turn [0xD01A38]
		// x 0.001)); the max(dt, eps) is a port guard
		const auto steps = static_cast<int>(averageLightmapLife / std::max(effect.GetDt(), 1e-3f));
		for (auto& target : data.targets)
		{
			target.cooldown = steps > 0 ? static_cast<int>(effect.Random(static_cast<float>(steps))) : 0;
		}
	}

	/// fn_006901E0: the spiral of 4 ceil(R/10)^2 cells around the origin (0x6902A1..0x6902A4); an object counts when it
	/// is available, not a spell seed, and its horizontal direction is inside the cone of half-angle SplitAngle about
	/// the heading. fn_00690880 (`cone` false): ceil(R/10)^2 cells only (0x690902..0x690906, no x4), and an object
	/// counts when dx^2 + dz^2 < R^2 (0x6909E5..0x690A1D)
	void SearchAround(Effect& effect, Data& data, bool cone) const
	{
		const float radius = SearchRadius(effect);
		const auto side = static_cast<int>(std::ceil(radius / 10.0f));
		const int cells = cone ? 4 * side * side : side * side;
		const glm::ivec2 start(static_cast<int>(data.origin.x * 0.1f), static_cast<int>(data.origin.z * 0.1f));
		const float limit = std::cos(splitAngle);
		const glm::vec2 heading(std::cos(data.heading), std::sin(data.heading));
		glm::ivec2 cell(start);
		int direction = 1;
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
				// fn_00604F40 (0x6903AE / 0x6909B4): only in the cell its own position is in, so an object listed in
				// several cells is found once
				const auto own = ecs::MapInterface::GetGridCell(glm::vec2(transform.position.x, transform.position.z));
				if (static_cast<int>(own.x) != cell.x || static_cast<int>(own.y) != cell.y)
				{
					continue;
				}
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

	/// CreateForkStructure 0x691190: one atom in the collection, with two fork sub-collections per target (0x69119B..
	/// 0x6911A6, the ForkGroup list of 2N), each holding MaxJointsPerFork chain joints (0x6912C3..0x6912FA)
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
			// 0x6912A1 `and byte [edi+0x38], 0xFD`: the forks are drawn as the last step left them, not interpolated
			// (fn_00679920 0x67999E), so each step the bolt jumps to its new shape
			fork->flags &= static_cast<uint8_t>(~2u);
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
		// 0x691C16..0x691C3D: +0x18 counts down; nothing reads it to skip a target
		for (auto& target : data.targets)
		{
			if (target.cooldown > 0)
			{
				--target.cooldown;
			}
		}
		data.striking.clear();
		// every fork is hidden (the original detaches the unused ones from the root, 0x691651..0x691681), then the
		// recursion shows the ones it lays out (fn_00674A30 at 0x691F7D)
		for (auto& fork : data.root->subCollections)
		{
			for (auto& atom : fork->atoms)
			{
				atom->visible = false;
			}
		}
		const auto total = static_cast<int>(data.targets.size());
		if (total == 0)
		{
			return;
		}
		// each active target with probability min(+0x68, (N + 1) / 2) / N, until that many are picked
		// (0x691C5B..0x691CF3). TODO(M5): the other branch, 0x691C3F / 0x691CF5, is taken when the bolt is linked to
		// another one (collection data +0x20 / +0x24, the clash of fn_006916B0 / fn_00691AD0, not ported): ONE target,
		// the first active one from PSysRand(N) on
		const int limit = std::min(atOnce, (total + 1) / 2);
		const float probability = static_cast<float>(limit) / static_cast<float>(total);
		for (int i = 0; i < total && static_cast<int>(data.striking.size()) < limit; ++i)
		{
			if (data.targets[static_cast<size_t>(i)].active && effect.Random(1.0f) < probability)
			{
				data.striking.push_back(i);
			}
		}
		// 0x691D59..0x691D6E: no forks or no target picked, no recursion (and so nothing drawn this step)
		if (data.root->subCollections.empty() || data.striking.empty())
		{
			return;
		}
		// 0x691D70..0x691DE6: fork 0 is the trunk, the next free fork is 1 (+0xA0), depth 0, scale 1.0. (+0xA4, a random
		// fork's collection, is set here but not used by the recursion)
		size_t nextFork = 1;
		size_t used = 0;
		Fork(effect, data, 0, *data.root->subCollections.front(), data.origin, data.striking, 1.0f, nextFork, used);
		if (std::getenv("OPENBLACK_SPELL_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Lightning: {} targets ({} objects), {} struck, {} of {} forks drawn from ({:.1f}, {:.1f}, "
			                   "{:.1f}) heading {:.2f} rad, fork scale {:.2f}",
			                   total, std::ranges::count_if(data.targets, [](const Target& t) { return t.isObject; }),
			                   data.striking.size(), used, data.root->subCollections.size(), data.origin.x, data.origin.y,
			                   data.origin.z, data.heading, data.forkScale);
		}
	}

	/// fn_00691F30(depth, data, fork, origin, targets, scale): one fork of the tree, from `origin` to the split point of
	/// its targets, then two child forks from there, or the strike when it has a single target
	void Fork(Effect& effect, Data& data, int depth, Collection& fork, const glm::vec3& origin,
	          const std::vector<int>& targets, float scale, size_t& nextFork, size_t& used) const
	{
		// 0x691F42..0x691F60: no fork (the structure ran out) or no target: nothing. The `joints < 2` is a port guard
		// (MaxJointsPerFork < 2): the original goes on with 0 atoms (0x69243F -> 0x692598) and would read the child
		// origin from a null atom, and divides by n - 1 = 0 with 1; no spell file gives fewer than 2
		const auto joints = static_cast<int>(fork.atoms.size());
		if (targets.empty() || joints < 2)
		{
			return;
		}
		++used;
		// TODO(M5): DrawOffsetLT (0x691F9C..0x69203E, set up at 0x69131C when cast from the player's own hand,
		// fn_00691B80): each joint of the trunk (depth 0) gets SetRefPos 0x6C7600(the origin of this step, weight
		// clamp(1 - i / (n - 1), 0, 1)); the child forks get weight 0. DrawOffsetLT::GetOffset 0x6C7690 adds
		// (the interface's hand position now, GInterface +0x3A0 +0x78, - that origin) x weight to the drawn position
		// every frame (fn_00679920, atom +0x124). Needs a per-atom draw offset in PSys.h: not ported, so the start of
		// the bolt follows the hand at the step rate only

		// 0x692040..0x69210C: the centroid of the list: the tips of its active targets summed, over the list's size
		glm::vec3 centroid(0.0f);
		for (const int index : targets)
		{
			const auto& target = data.targets[static_cast<size_t>(index)];
			if (target.active)
			{
				centroid += target.Tip();
			}
		}
		centroid /= static_cast<float>(targets.size());
		// 0x692177..0x692208: with two or more targets the fork stops at origin + (centroid - origin) x
		// (0.2 + rand(0.4)) (0x3ECCCCCD, 0x8AB244); with one it goes all the way to it.
		// TODO(M5): at depth 0 a bolt linked to another one aims at the clash point (+0xB4) instead (0x692110..0x692172)
		glm::vec3 split = centroid;
		if (targets.size() >= 2)
		{
			const float f = 0.2f + effect.Random(0.4f);
			split = origin + (centroid - origin) * f;
		}
		// TODO(M5): 0x69220C..0x692262: fn_00802550, (inferido) the segment against the island: the fork is dropped when
		// the land is hit before the split point (not read)
		// TODO(M6): 0x692268..0x692361: fn_006D0BC0(split, 2.5) -> a shield on the way gets SpellEvent 4 (strength 1, 2
		// when linked) at the point where the fork meets it and the fork ends there without children or strike
		// (shields::FindShieldContainingPoint is the port's fn_006D0BC0)

		// 0x692366..0x69239E: the fork's scale runs from S / (depth + 1) to S / (depth + 2), S = scale x ForkScale x
		// FP_ForkScale (+0xAC)
		const float s = scale * data.forkScale;
		const float scaleFrom = s / static_cast<float>(depth + 1);
		const float scaleTo = s / static_cast<float>(depth + 2);
		const auto direction = split - origin;
		const float length = glm::length(direction);
		// TODO(M5): 0x6923D0..0x6923FC: with NumTexturesToTile (this +0x48) != -1 the chain's repeats become
		// max(1, ftol(NumTexturesToTile x length / data +0x60)); data +0x60 is 1.0 from the data ctor (0x68FE7F) and no
		// other writer was found, so it is left out (SF_LightningStrike / SF_LightningStormPush give 15)
		// 0x6923FF..0x692418: the alpha is 255 only when the scale is > 1 (a clash), else 128 + PSysRand(127)
		const bool opaque = scale > 1.0f;
		const float step = 1.0f / static_cast<float>(joints - 1);
		for (int i = 0; i < joints; ++i)
		{
			auto& atom = *fork.atoms[static_cast<size_t>(i)];
			const float t = static_cast<float>(i) * step;
			// 0x69244D..0x6924C3: origin + (split - origin) x i / (n - 1)
			auto position = origin + direction * t;
			if (i != 0 && i != joints - 1)
			{
				// 0x6924D5..0x692511: the inner joints move by (rand(2 RandomFrac) - RandomFrac) x length in X and Z
				position.x += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
				position.z += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
			}
			atom.position = position;                                // +0x80
			atom.ruleScale = scaleFrom + (scaleTo - scaleFrom) * t; // +0x78 (0x692515..0x692545)
			// +0x8F, the colour's alpha byte (0x692548..0x69257C)
			atom.colour[3] = opaque ? 255 : static_cast<uint8_t>(128 + static_cast<int>(effect.Random(127.0f)));
			atom.visible = true;
		}
		if (commonGlowGroup >= 0 && fork.atoms.back()->subCollections.empty())
		{
			// (inferido) CommonGlowGroup: the glow sprite of the tip follows its joint (UR_FollowParent); where and
			// when the original attaches it is not read
			effect.AddSubCollections(*fork.atoms.back(), {commonGlowGroup});
		}
		const glm::vec3 end = fork.atoms.back()->position; // the last joint, `lea ebx, [ecx+0x80]` (0x69279A)
		if (targets.size() >= 2)
		{
			// 0x6925B9..0x6926F4: the targets are split by the sign of dot((tip - split).xz, (centroid - origin).xz):
			// > 0 to the first list, else to the second
			std::vector<int> ahead;
			std::vector<int> behind;
			const glm::vec2 axis(centroid.x - origin.x, centroid.z - origin.z);
			for (const int index : targets)
			{
				const auto tip = data.targets[static_cast<size_t>(index)].Tip();
				const float dot = (tip.x - split.x) * axis.x + (tip.z - split.z) * axis.y;
				(dot > 0.0f ? ahead : behind).push_back(index);
			}
			// 0x6926FA..0x69276A: an empty list takes the last target of the other
			if (ahead.empty())
			{
				ahead.push_back(behind.back());
				behind.pop_back();
			}
			else if (behind.empty())
			{
				behind.push_back(ahead.back());
				ahead.pop_back();
			}
			// 0x69276D..0x6927DA: each list gets the next free fork (+0xA0), depth first; when the forks run out the
			// recursion stops there (0x692786, 0x6927C6)
			for (const auto* list : {&ahead, &behind})
			{
				if (nextFork >= data.root->subCollections.size())
				{
					return;
				}
				auto& child = *data.root->subCollections[nextFork++];
				Fork(effect, data, depth + 1, child, end, *list, scale, nextFork, used);
			}
			return;
		}
		// 0x6928ED..: one target: the strike
		StrikeTarget(effect, data, data.targets[static_cast<size_t>(targets.front())], centroid);
	}

	/// The tip of fn_00691F30 (0x6928F9..0x692ABE): the cooldown, the light map atom at the list's centroid
	/// (fn_00691E80), the "impressive" report once 0.2 s passed since the search (fn_00691ED0) and the event that
	/// actually damages
	void StrikeTarget(Effect& effect, Data& data, Target& target, const glm::vec3& centroid) const
	{
		// 0x6928FD..0x692949: +0x18 = PSysRand(AverageLightmapLife / (ms per turn x 0.001)); data only
		// the max(dt, eps) is a port guard
		const auto steps = static_cast<int>(averageLightmapLife / std::max(effect.GetDt(), 1e-3f));
		target.cooldown = steps > 0 ? static_cast<int>(effect.Random(static_cast<float>(steps))) : 0;
		if (lightMapGroup >= 0)
		{
			if (auto* atom = effect.NewAtomInGroup(lightMapGroup, effect.FindCreator(lightMapCreator)); atom != nullptr)
			{
				// the original blits the light map into the landscape's light texture under the point, so it always
				// ends up on the ground however high the tip is (part_render.md §8); the +0.1 is the port's (inferido)
				atom->position = glm::vec3(centroid.x, LandAt(centroid.x, centroid.z) + 0.1f, centroid.z);
			}
		}
		// 0x692951..0x692971: once per target, when +0x5C > 0.2
		if (!target.lightMapDone && data.life > 0.2f)
		{
			// TODO(M7): fn_00691ED0 -> fn_00692FA0, the "impressive" report of the struck object (GetImpressiveIntensity)
			target.lightMapDone = true;
		}
		// 0x692976..0x6929F4: SpellEventInfo type 3 at the target's tip (fn_00691E00); strength 1, 2 only when the bolt
		// is linked to another one (data +0x20, the clash, not ported)
		SpellEventInfo event;
		event.type = SpellEventInfo::Landed;
		event.position = target.Tip();
		event.strength = 1.0f;
		if (effect.GetSink() != nullptr)
		{
			effect.SendSpellEvent(event);
		}
		// TODO(M5): 0x692AA0..0x692AB9: the same event also goes to the manager of the linked bolt (data +0x20,
		// fn_00690070) when there is one (the clash, not ported)
		// TODO(M5): without a spell (a script / climate strike, global 0xC029D0) the original applies the static
		// EffectValues of info 0xCC9704 at the tip (0x692A10..0x692A9B). UNVERIFIED which GEffectInfo row that is.
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
