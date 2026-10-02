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
// "landed" event at each struck tip, which is what burns and kills. Two bolts cast from the hand that meet are linked
// (fn_006916B0): both trunks end at the clash point, where a glow sits, and the older one goes on from there, three
// times as thick, to its target. UR_LightningStrike is the one-shot script / climate strike. Report:
// tmp_dis\miracles\destructive.md §4; wiki docs/bw1-notes/miracles.md, "Rayo".

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <memory>
#include <ranges>
#include <numbers>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "Locator.h"
#include "PSys/PSys.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"
#include "PSys/Rules/Shield.h"
#include "PSys/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// UR_Lightning +0x60: 3.0 from the ctor (0x690183) and set by no property: the scale of the fork after a clash point
/// (0x692162)
constexpr float k_ClashScale = 3.0f;
/// fn_00691F30 0x69226C / 0x69228B: the margin of the shield test fn_006D0BC0 and of FindIntersect (push 0x40200000)
constexpr float k_ShieldMargin = 2.5f;

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The objects of one 10 m map cell in the inline iterator's order (0x690344 / 0x69094B: the fixed list +4, then the
/// mobile one +0, each from its head; ecs::map_cells); MapCoords::InBounds 0x6042C0 first (0x690333 / 0x690939)
void CellObjects(const ecs::map_coords::MapCoords& coords, std::vector<entt::entity>& out)
{
	out.clear();
	if (!ecs::map_coords::InBounds(coords))
	{
		return;
	}
	out = ecs::map_cells::ObjectsInCell(ecs::map_coords::Cell(coords));
}

/// PSysManager::PSysRand 0x6729E0: the function at [0xD4E0BC] (fn_00673340 sets 0x672AF0 GRand::GameRand or 0x672B40
/// GRand::LocalRand): 0 for n == 0 without a draw (GData::Rand 0x510693 / LocalRand 0x6DE574), else LHRand 0x7DB600
/// modulo n as an UNSIGNED number (`div` 0x7DB62B): a negative n draws from 0 .. 2^32 - |n| - 1, kept in a signed int
/// (0x80000000, the ftol of an infinite step count, gives 0 .. 2^31 - 1)
int32_t PSysRand(Effect& effect, int32_t n)
{
	return effect.Rand(n);
}

/// GameThing::IsAvailable 0x401810 (vt +0x2C): not being deleted (+0xA & 1; openblack's entity is gone instead), and
/// for a villager Villager::IsAvailable 0x751D50: its final state is not DYING (ecs::villager::IsAvailable). The other
/// vt +0x2C overrides of the image (GGame 0x54B9A0 and the rooms of the front end) are not things on the map
bool IsAvailable(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	return !registry.AllOf<ecs::components::Villager>(object) || ecs::villager::IsAvailable(object);
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

	/// fn_00691E00: an object's tip is its MapCoords as a point (x, z = fild x 10 / 65536 0x691E30..0x691E43, y =
	/// GetAltitude 0x803090 + its altitude +0x1C 0x691E22..0x691E2D) raised by GetHeight (vt +0x42C, 0x691E48); a
	/// ground point is itself
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
		auto tip = ecs::map_coords::ToWorld(ecs::object::MapCoordsOf(object));
		tip.y = ecs::object::GetHeight(object) + tip.y; // fadd [edi + 4] (0x691E4E)
		return tip;
	}
};

/// What fn_00691AD0 does to the bolt's fork 0, done by that bolt's own next step here (see Link)
enum class GlowChange
{
	None,
	Add,    ///< AddSubCollection(CommonGlowGroup) on its last joint when it has none (0x691B28..0x691B42)
	Remove, ///< fn_00673B20 on its last joint: every sub-collection goes (0x691AF6..0x691B03)
};

/// UR_Lightning_CollectionData (0xC0 bytes, ctor 0x68FE20): what the rule keeps for one collection between steps
struct Data
{
	uint32_t key {0};           ///< the port's name of it (DataFor)
	uint32_t link {0};          ///< +0x20 the bolt this one is linked to (it aims at that one's +0xB4), 0 none
	uint32_t linkedBy {0};      ///< +0x24 the bolt linked to this one, 0 none
	glm::vec3 origin {0.0f};    ///< +0x30 where the forks start (the gesture position, or the parent atom)
	glm::vec3 searchOrigin {0.0f}; ///< +0x3C the origin of the last target search (0x690F72..0x690F85)
	glm::vec3 centroid {0.0f};  ///< +0x48 the targets' tips summed over their number (fn_00690F50, 0x6910BD..0x691177)
	float heading {0.0f};       ///< +0x54 atan2(cameraForward.z, cameraForward.x), fn_00673650
	bool first {true};          ///< +0x58 the fork structure is not built yet
	bool fromHand {false};      ///< +0x59 the rule's CastingFromHand at the ctor (0x68FECA..0x68FECD)
	float life {0.0f};          ///< +0x5C the age since the last target search (set to 0 by fn_00690F50 at 0x690F66)
	/// +0x60 |centroid - origin| (fn_00690F50 0x69114D..0x691174), 1.0 from the ctor (0x68FE7F) until the first search
	float centroidDistance {1.0f};
	std::vector<Target> targets; ///< +0x6C / +0x74
	std::vector<int> striking;   ///< +0x80 / +0x84 the targets picked this step
	float forkScale {1.0f};      ///< +0xAC ForkScale x FP_ForkScale
	uint32_t createdTurn {0};    ///< +0xB0 g_game +0x205A40 at the ctor (0x68FE9F..0x68FEB2)
	glm::vec3 clash {0.0f};      ///< +0xB4 the clash point, when another bolt is linked to this one (fn_006916B0)
	Atom* root {nullptr}; ///< the single atom of the collection; its sub-collections are the forks
	// port bookkeeping: the effect that steps it (to send the linked bolt's events, PSysManager +0x30 of fn_00690070),
	// the turn of its last step, the glow fn_00691AD0 asked for, and the wall clock of its last step
	Effect* effect {nullptr};
	uint32_t effectId {0};
	uint32_t steppedTurn {0};
	GlowChange glow {GlowChange::None};
	int glowGroup {-1};
	std::chrono::steady_clock::time_point touched;
};

/// The data of a live collection, keyed as the fireball's balls are: a float in the slot names it, so nothing dangles
/// when the collection goes. The original keeps them in a global list, newest first (0xD4EC60 / count 0xD4EC64, the
/// ctor puts each at the head, 0x68FED0..0x68FEEE), and a collection's data leaves it in its dtor (fn_0068FF50). Here an
/// entry leaves when its collection's step stops coming (no step this turn nor the last, or ten seconds of wall clock
/// without one: port bookkeeping, a pause longer than that resets a live bolt's targets)
uint32_t g_NextKey = 1;
std::unordered_map<uint32_t, Data> g_Data;
std::vector<uint32_t> g_Order; ///< the keys, newest first

Data* Find(uint32_t key)
{
	if (key == 0)
	{
		return nullptr;
	}
	const auto it = g_Data.find(key);
	return it != g_Data.end() ? &it->second : nullptr;
}

/// fn_00691AD0(bolt, other, group): `bolt` (+0x20) is linked to `other`, or unlinked with 0. The old partner forgets
/// it (+0x24 = 0, 0x691ADF..0x691AE3); a new link gives the other +0x24 = bolt (0x691B11). Linking from none puts a
/// CommonGlowGroup collection on the last joint of the bolt's fork 0 (fn_00691B50, 0x691B1B..0x691B42), unlinking
/// takes that joint's sub-collections off (fn_00673B20, 0x691AEF..0x691B03). The original changes the other bolt's
/// atoms at once; here that waits for the bolt's own step (ApplyGlow), so the glow may come or go one turn late
/// (aproximado): the other bolt's atoms are not safe to touch from this one's step
void Link(Data& bolt, uint32_t other, int group)
{
	if (bolt.link == other)
	{
		return;
	}
	if (auto* old = Find(bolt.link); old != nullptr)
	{
		old->linkedBy = 0;
	}
	if (other == 0)
	{
		if (bolt.link != 0)
		{
			bolt.glow = GlowChange::Remove;
		}
		bolt.link = 0;
		return;
	}
	if (auto* target = Find(other); target != nullptr)
	{
		target->linkedBy = bolt.key;
	}
	if (bolt.link == 0)
	{
		bolt.glow = GlowChange::Add;
		bolt.glowGroup = group;
	}
	bolt.link = other;
}

/// fn_0068FF50, the data's dtor: a bolt linked to this one is unlinked (0x68FF55..0x68FF68) and the one this is linked
/// to forgets it (0x68FF6D..0x68FF74); then it leaves the list (0x68FFAE..0x68FFDC)
void Destroy(Data& data)
{
	if (auto* by = Find(data.linkedBy); by != nullptr)
	{
		Link(*by, 0, -1);
	}
	if (auto* to = Find(data.link); to != nullptr)
	{
		to->linkedBy = 0;
	}
	std::erase(g_Order, data.key);
}

Data& DataFor(Effect& effect, Collection::Slot& slot, bool fromHand)
{
	const auto now = std::chrono::steady_clock::now();
	const auto turn = game_clock::Turn();
	std::vector<uint32_t> gone;
	for (const auto& [key, entry] : g_Data)
	{
		if (now - entry.touched > std::chrono::seconds(10) || entry.steppedTurn + 1 < turn)
		{
			gone.push_back(key);
		}
	}
	for (const auto key : gone)
	{
		Destroy(g_Data[key]);
		g_Data.erase(key);
	}
	auto key = static_cast<uint32_t>(slot.extra.x);
	if (key == 0 || !g_Data.contains(key))
	{
		key = g_NextKey++ & 0xFFFFFF;
		slot.extra.x = static_cast<float>(key);
		auto& created = g_Data[key];
		created = Data {};
		created.key = key;
		created.fromHand = fromHand;
		created.createdTurn = turn;
		g_Order.insert(g_Order.begin(), key);
	}
	auto& data = g_Data[key];
	data.touched = now;
	data.steppedTurn = turn;
	data.effect = &effect;
	data.effectId = manager::IdOf(&effect);
	return data;
}

/// The effect of a linked bolt, for its events (fn_00690070: the collection's manager, +0xA8 -> +0x30). A running
/// effect is checked against the manager; one the manager does not run (the tests) only by its step this turn
Effect* EffectOf(const Data& data)
{
	if (data.effect == nullptr)
	{
		return nullptr;
	}
	if (data.effectId != 0)
	{
		return manager::Find(data.effectId) == data.effect ? data.effect : nullptr;
	}
	return data.steppedTurn == game_clock::Turn() ? data.effect : nullptr;
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
	    , numTexturesToTile(object.Int("NumTexturesToTile", -1))
	    , sound(ReadSoundAction(object, "SoundLightning"))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& data = DataFor(effect, slot, castingFromHand);
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
		}
		ApplyGlow(effect, collection, data);
		// 0x6915E5: fn_006916B0, the clash with another bolt
		FindClash(effect, data);
		// 0x6915EE: fn_00691390: the targets are searched again but the fork structure built on the first step stays;
		// when there are more targets than forks the recursion simply stops (0x692786)
		if (Renew(effect, data))
		{
			FindTargets(effect, data);
		}
		// 0x691601..0x69164C: with SoundLightning (+0x80) not NO_SOUND, every step the root atom's sound of that action
		// (GetSoundOfAction 0x674550) is started when it has none while the manager IsInState(2) (StartSound 0x6745D0),
		// and stopped when it has one otherwise (StopSound 0x674500)
		if (sound.action != -1 && Valid(collection, data))
		{
			auto* playing = audio::spell_sounds::GetSoundOfAction(*data.root, sound.action);
			if (effect.GetProcessInfo().enabled)
			{
				if (playing == nullptr)
				{
					audio::spell_sounds::StartSound(effect, *data.root, sound);
				}
			}
			else if (playing != nullptr)
			{
				audio::spell_sounds::StopSound(*data.root, *playing);
			}
		}
		// 0x691651..0x691681: every fork is detached from the root (fn_00674A30(0), +4 = 1) at every step, whatever the
		// state: the port hides their joints. The recursion attaches the ones it uses again (0x691F7D)
		if (Valid(collection, data))
		{
			for (auto& fork : data.root->subCollections)
			{
				for (auto& atom : fork->atoms)
				{
					atom->visible = false;
				}
			}
		}
		// 0x691601..0x691691: UpdateForkStructure only while the manager IsInState(2) (PSysProcessInfo +0x38, enabled)
		if (effect.GetProcessInfo().enabled)
		{
			UpdateForkStructure(effect, collection, data);
		}
		return true;
	}

private:
	/// The root atom is still the collection's (a remove rule may have taken it)
	[[nodiscard]] static bool Valid(const Collection& collection, const Data& data)
	{
		return data.root != nullptr && !collection.atoms.empty() && collection.atoms.front().get() == data.root;
	}

	/// What fn_00691AD0 asked for this bolt's fork 0 (Link). The last joint of the first fork, fn_00691B50 (data +0x8C
	/// [0], its atom list +0x40 to the end)
	void ApplyGlow(Effect& effect, const Collection& collection, Data& data) const
	{
		const auto change = data.glow;
		data.glow = GlowChange::None;
		if (change == GlowChange::None || !Valid(collection, data) || data.root->subCollections.empty() ||
		    data.root->subCollections.front()->atoms.empty())
		{
			return;
		}
		auto& joint = *data.root->subCollections.front()->atoms.back();
		if (change == GlowChange::Remove)
		{
			joint.subCollections.clear(); // fn_00673B20
			return;
		}
		// 0x691B28..0x691B42: only on a joint with no sub-collection; AddSubCollection 0x673BB0 skips -1. The new
		// collection is drawn without interpolation (`and byte [edi+0x38], 0xFD`, 0x691B42)
		if (joint.subCollections.empty() && data.glowGroup != -1)
		{
			effect.AddSubCollections(joint, {data.glowGroup});
			if (!joint.subCollections.empty())
			{
				joint.subCollections.back()->flags &= static_cast<uint8_t>(~2u);
			}
		}
	}

	/// fn_006916B0(collection, data): a bolt cast from the hand that is not linked yet looks for an older one (+0xB0
	/// smaller) in the list, also cast from the hand and with targets, not linked or linked to it. With d1 = that one's
	/// centroid - its origin and d2 = its centroid - this origin, the clash point is T = its centroid - 0.7 x 0.5 x
	/// (normalize(d1) + (cos h, 0, sin h)) x min(|d1|, |d2|), h this heading (0x6918E8..0x6919F3). They are linked
	/// (fn_00691AD0(older, this, CommonGlowGroup), this +0xB4 = T, 0x691A9B..0x691AB6) when their headings agree
	/// (cos h' cos h + sin h' sin h > 0, 0x6918AF..0x6918E2), T is closer than SearchRadius (fn_006901D0,
	/// 0x691A25..0x691A45) and normalize(T - this origin) lies within 1.0367 rad [0x936D90] of h (0x691A47..0x691A72).
	/// An older bolt that fails the test is unlinked if it was linked (0x691A74..0x691A81). The first link ends the search
	void FindClash(Effect& effect, Data& data) const
	{
		if (!data.fromHand || data.link != 0) // 0x6916BE..0x6916D0
		{
			return;
		}
		data.clash = glm::vec3(0.0f); // 0x6916D6..0x691707
		const auto order = g_Order; // Link does not change the list, but a copy keeps the walk simple
		for (const auto key : order)
		{
			auto* other = Find(key);
			// 0x691718..0x691737: cast from the hand, with targets, not linked to a third one
			if (other == nullptr || !other->fromHand || other->targets.empty() ||
			    (other->link != data.key && other->link != 0))
			{
				continue;
			}
			if (other->createdTurn < data.createdTurn) // 0x69173D..0x691749 (`jae`: the same turn does not clash)
			{
				if (const auto clash = ClashPoint(effect, *other, data); clash.has_value())
				{
					data.clash = *clash;
					Link(*other, data.key, commonGlowGroup);
					return;
				}
			}
			if (other->link != 0)
			{
				Link(*other, 0, -1);
			}
		}
	}

	/// The geometry of fn_006916B0 (see FindClash); nullopt when the two do not meet
	[[nodiscard]] std::optional<glm::vec3> ClashPoint(const Effect& effect, const Data& older, const Data& data) const
	{
		// LHPoint::Normalise inline (0x69179D..0x6918AD): a zero vector stays zero with length 0
		const auto normalise = [](glm::vec3& v) {
			if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
			{
				return 0.0f;
			}
			const float length = std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
			const float inverse = 1.0f / length;
			v = glm::vec3(v.x * inverse, v.y * inverse, v.z * inverse);
			return length;
		};
		auto d1 = older.centroid - older.origin;
		auto d2 = older.centroid - data.origin;
		const float length1 = normalise(d1);
		const float length2 = normalise(d2);
		const float c = std::cos(data.heading);
		const float s = std::sin(data.heading);
		if (!(std::cos(older.heading) * c + std::sin(older.heading) * s > 0.0f))
		{
			return std::nullopt;
		}
		// 0x6918E8..0x6919C0: (d1 + (c, 0, s)) x 0.5 [0x8AA3B4] x 0.7 [0x8AB238] x the shorter length (`fcomp; test ah, 1`:
		// length1 when it is the smaller, else length2)
		const glm::vec3 sum(d1.x + c, d1.y, d1.z + s);
		const glm::vec3 half = sum * 0.5f;
		const glm::vec3 scaled = half * 0.7f;
		const float shorter = length1 < length2 ? length1 : length2;
		const glm::vec3 clash = older.centroid - scaled * shorter;
		auto toClash = clash - data.origin;
		const float distance = normalise(toClash); // fn_00460710: normalises and returns the length
		if (!(SearchRadius(effect) > distance))
		{
			return std::nullopt;
		}
		// 0x691A47..0x691A72: fld [0x936D90] (double 1.036725640296936); fcos; fcompp: cos < dot links
		if (!(static_cast<float>(std::cos(1.036725640296936)) < toClash.z * s + toClash.x * c))
		{
			return std::nullopt;
		}
		return clash;
	}

	/// 0x691072..0x691091 / 0x6928FD..0x69292F: ftol(AverageLightmapLife / (fild [0xD01A38] x 0.001 [0x8AA3B0])), the
	/// turns of AverageLightmapLife seconds, passed to PSysRand as it is (a 0 ms turn gives +inf, ftol 0x80000000)
	[[nodiscard]] int LightmapSteps() const
	{
		return ecs::map_coords::FtoL(averageLightmapLife /
		                             (static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs));
	}

	[[nodiscard]] float SearchRadius(const Effect& effect) const
	{
		return searchRadius.empty() ? defaultSearchRadius : effect.FloatProvider(searchRadius, defaultSearchRadius);
	}

	/// fn_00691390: the targets are searched again when RenewSearchEvery > 0 and the age since the last search (+0x5C)
	/// is past it, or with RenewTargetsOnMove when the origin moved more than RenewTargetsOnMoveFrac x SearchRadius
	/// from where it was searched (+0x3C). (fn_006916B0 is the clash of two bolts: FindClash)
	[[nodiscard]] bool Renew(const Effect& effect, const Data& data) const
	{
		if (renewSearchEvery > 0.0f && data.life > renewSearchEvery)
		{
			return true;
		}
		if (!renewTargetsOnMove)
		{
			return false;
		}
		// 0x6913BD..0x69140B: d = +0x3C - +0x30 squared as (dz dz + dy dy) + dx dx, against (R x frac)^2: no root
		const auto d = data.searchOrigin - data.origin;
		const float distanceSq = (d.z * d.z + d.y * d.y) + d.x * d.x;
		const float limit = SearchRadius(effect) * renewTargetsOnMoveFrac;
		return limit * limit < distanceSq;
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
		// 0x691072..0x6910CE: every target starts with +0x18 = PSysRand(LightmapSteps())
		// 0x6910BD..0x691177: with it, +0x48 = the tips of every target (fn_00691E00) summed, x 1 / their number, and
		// +0x60 = sqrt((dz dz + dy dy) + dx dx) from the origin (+0x30) to it
		const auto steps = LightmapSteps();
		glm::vec3 sum(0.0f);
		for (auto& target : data.targets)
		{
			target.cooldown = PSysRand(effect, steps); // 0x6910C9
			sum += target.Tip();
		}
		const float inverse = 1.0f / static_cast<float>(data.targets.size());
		data.centroid = glm::vec3(inverse * sum.x, sum.y * inverse, sum.z * inverse);
		const auto d = data.centroid - data.origin;
		data.centroidDistance = std::sqrt((d.z * d.z + d.y * d.y) + d.x * d.x);
	}

	/// fn_006901E0: the spiral of 4 ceil(R/10)^2 cells around the origin (0x6902A1..0x6902A4); an object counts when it
	/// is available, not a spell seed, and its horizontal direction is inside the cone of half-angle SplitAngle about
	/// the heading. fn_00690880 (`cone` false): ceil(R/10)^2 cells only (0x690902..0x690906, no x4), and an object
	/// counts when dx^2 + dz^2 < R^2 (0x6909E5..0x690A1D). dx, dz are the object's MapCoords in metres minus the
	/// origin (fild x 10 / 65536, 0x6903DF / 0x6909E5)
	void SearchAround(Effect& effect, Data& data, bool cone) const
	{
		const float radius = SearchRadius(effect);
		const auto side = ecs::map_coords::FtoL(std::ceil(radius / 10.0f)); // fdiv [0x936C60]; _ceil; __ftol
		const int cells = cone ? 4 * side * side : side * side;
		const float limit = std::cos(splitAngle);
		const glm::vec2 heading(std::cos(data.heading), std::sin(data.heading));
		// the origin's MapCoords (ToFixed of x and z, 0x69024A..0x690286 / 0x6908B7..0x6908EA), walked by GUtils::Spiral
		// 0x74D7E0 from dir = count = 1 (0x6902A7..0x6902BB / 0x69090D..0x69091E) and MapCoords += JustMapXZ 0x605470
		auto cell = ecs::map_coords::FromMetres(glm::vec2(data.origin.x, data.origin.z));
		ecs::map_coords::Spiral spiral;
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
				// 0x69038E / 0x690997: IsAvailable (vt +0x2C) == 1, then the own cell (fn_00604F40) and fn_00690090
				if (!IsAvailable(object) || !CanBeStruck(object))
				{
					continue;
				}
				const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object);
				// fn_00604F40 (0x6903AE / 0x6909B4): only in the cell its own MapCoords (+0x14) is in, so an object
				// listed in several cells is found once
				const auto own = ecs::object::MapCoordsOf(object);
				if (!ecs::map_cells::IsOwnCell(own, ecs::map_coords::Cell(cell)))
				{
					continue;
				}
				float dx = ecs::map_coords::ToMetres(own.x) - data.origin.x;
				float dz = ecs::map_coords::ToMetres(own.z) - data.origin.z;
				bool outside = false;
				if (cone)
				{
					// 0x690410..0x6904A8: LHPoint(dx, 0, dz) normalised unless it is zero (x 1 / sqrt((x x + z z) + y y)),
					// then counted when dx cos(heading) + sin(heading) dz > cos(SplitAngle)
					if (dx != 0.0f || dz != 0.0f)
					{
						const float inverse = 1.0f / std::sqrt(dx * dx + dz * dz);
						dx = dx * inverse;
						dz = dz * inverse;
					}
					outside = !(dx * heading.x + heading.y * dz > limit);
				}
				else
				{
					// 0x690A06..0x690A1D: dz dz + dx dx < R R ([esp + 0x18] = R x R, 0x690902..0x69091A)
					outside = !(dz * dz + dx * dx < radius * radius);
				}
				if (outside)
				{
					continue;
				}
				data.targets.push_back({object, transform.position, true, true, false, 0});
			}
			ecs::map_coords::AddCells(cell, spiral.Next());
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
			// 0x6906A1..0x6906AF: GetAltitude 0x803090 at the MapCoords fn_004427B0(x, z) = ftol(x x 65536 x 0.1), which is
			// ToFixed (65536 x 0.1f is 6553.6f exactly), + 2 [0x8AB478]; x and z stay the float point
			const float ground = ecs::map_coords::ToWorld(ecs::map_coords::FromMetres(glm::vec2(point.x, point.z))).y;
			data.targets.push_back({entt::null, glm::vec3(point.x, ground + 2.0f, point.z), false, true, false, 0});
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
		// 0x69130C..0x69134C: cast from the player's own hand (fn_00691B80), every joint of every fork gets a
		// DrawOffsetLT (SetDrawOffset 0x673AF0), so the bolt's start follows the hand between the steps
		const bool offset = HandOffset(effect);
		for (auto& fork : root.subCollections)
		{
			// 0x6912A1 `and byte [edi+0x38], 0xFD`: the forks are drawn as the last step left them, not interpolated
			// (fn_00679920 0x67999E), so each step the bolt jumps to its new shape
			fork->flags &= static_cast<uint8_t>(~2u);
			for (int i = 0; i < maxJoints; ++i)
			{
				auto& joint = effect.NewAtom(*fork, joints, {});
				if (offset)
				{
					joint.drawOffset.emplace();
				}
			}
		}
		data.root = &root;
	}

	/// fn_00691B80: CastingFromHand (+0x75) and PSysManager::NetUnsafeIsMyInterfaceCasting 0x673540 (the spell's +0x44,
	/// 1 without a spell)
	[[nodiscard]] bool HandOffset(const Effect& effect) const { return castingFromHand && effect.IsMyInterfaceCasting(); }

	/// UpdateForkStructure 0x691BB0: the cooldowns tick down, a set of targets is picked, and the recursion
	/// fn_00691F30 lays the joints of their forks out and fires the events
	void UpdateForkStructure(Effect& effect, Collection& collection, Data& data) const
	{
		if (!Valid(collection, data))
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
		const auto total = static_cast<int>(data.targets.size());
		if (data.linkedBy != 0 || data.link != 0)
		{
			// 0x691C3F..0x691C55 -> 0x691CF5..0x691D56: a bolt in a clash strikes ONE target, the first active one from
			// PSysRand(N) on
			const int start = effect.Rand(total); // UpdateForkStructure 0x691CF9
			for (int i = 0; i < total; ++i)
			{
				const int index = (start + i) % total;
				if (data.targets[static_cast<size_t>(index)].active)
				{
					data.striking.push_back(index);
					break;
				}
			}
		}
		else if (total > 0)
		{
			// each active target with probability min(+0x68, (N + 1) / 2) / N, until that many are picked
			// (0x691C5B..0x691CF3)
			const int limit = std::min(atOnce, (total + 1) / 2);
			const float probability = static_cast<float>(limit) / static_cast<float>(total);
			for (int i = 0; i < total && static_cast<int>(data.striking.size()) < limit; ++i)
			{
				if (data.targets[static_cast<size_t>(i)].active && effect.Random(1.0f) < probability)
				{
					data.striking.push_back(i);
				}
			}
		}
		// 0x691D59..0x691D6E: no forks or no target picked, no recursion (and so nothing drawn this step)
		if (data.root->subCollections.empty() || data.striking.empty())
		{
			return;
		}
		// 0x691D70..0x691DE6: fork 0 is the trunk, the next free fork is 1 (+0xA0), depth 0, scale 1.0. +0xA4, the
		// collection of fork PSysRand(2 x forks) or none (0x691D98..0x691DC3), is set but not used by the recursion:
		// only its draw from the random stream is kept
		static_cast<void>(effect.Rand(static_cast<int32_t>(2 * data.root->subCollections.size()))); // 0x691DA0
		size_t nextFork = 1;
		size_t used = 0;
		Fork(effect, data, 0, *data.root->subCollections.front(), data.origin, data.striking, 1.0f, nextFork, used);
		if (std::getenv("OPENBLACK_SPELL_TRACE") != nullptr)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Lightning {}: {} targets ({} objects), {} struck, {} of {} forks drawn from ({:.1f}, {:.1f}, "
			                   "{:.1f}) heading {:.2f} rad, fork scale {:.2f}, link {} linked by {} clash ({:.1f}, {:.1f}, "
			                   "{:.1f})",
			                   data.key, total, std::ranges::count_if(data.targets, [](const Target& t) { return t.isObject; }),
			                   data.striking.size(), used, data.root->subCollections.size(), data.origin.x, data.origin.y,
			                   data.origin.z, data.heading, data.forkScale, data.link, data.linkedBy, data.clash.x,
			                   data.clash.y, data.clash.z);
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
		// 0x691F6D..0x691F7D: the fork is attached to the root again (+4 = 0, fn_00674A30): from here it is drawn, with
		// the joints the last step that laid it out left, even when this one returns before laying it out (a missing
		// DrawOffsetLT; in the original also the land)
		for (auto& atom : fork.atoms)
		{
			atom->visible = true;
		}
		++used;
		// 0x691F9C..0x69203E: cast from the player's own hand (fn_00691B80), each joint's DrawOffsetLT gets SetRefPos
		// (vt 0x100, 0x6C7600) with this step's origin (data +0x30) and the weight w - w i / (n - 1), w = 1 on the trunk
		// (depth 0) and 0 on the child forks (0x691FC7..0x691FD1); a joint without one ends the fork (0x692007)
		if (HandOffset(effect))
		{
			const float w = depth == 0 ? 1.0f : 0.0f;
			const float inverse = 1.0f / (static_cast<float>(joints) - 1.0f); // fild [eax+0x44]; fsub 1; fdivr 1
			for (int i = 0; i < joints; ++i)
			{
				auto& atom = *fork.atoms[static_cast<size_t>(i)];
				if (!atom.drawOffset.has_value())
				{
					return;
				}
				atom.drawOffset->SetRefPos(data.origin, -w * static_cast<float>(i) * inverse + w);
			}
		}

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
		// [esp+0x13]: the fork ends where it is laid out (no children, no strike); [esp+0x2B]: the clash fork follows;
		// [esp+0x58]: the scale the children get
		bool stop = false;
		bool clashChild = false;
		float childScale = scale;
		glm::vec3 split = centroid;
		const auto* linked = Find(data.link);
		if (depth == 0 && data.linkedBy != 0)
		{
			// 0x692110..0x692136: a bolt another one is linked to ends its trunk at its own clash point (+0xB4)
			split = data.clash;
			stop = true;
		}
		else if (depth == 0 && linked != nullptr)
		{
			// 0x69213B..0x692172: a bolt linked to another one ends its trunk at that one's clash point, and the fork
			// after it gets the scale x this +0x60 (3.0, set by the ctor 0x690183 and by no property)
			split = linked->clash;
			childScale = scale * k_ClashScale;
			clashChild = true;
		}
		else if (targets.size() >= 2)
		{
			// 0x692177..0x692208: with two or more targets the fork stops at origin + (centroid - origin) x
			// (0.2 + rand(0.4)) (0x3ECCCCCD, 0x8AB244); with one it goes all the way to it
			const float f = 0.2f + effect.Random(0.4f);
			split = origin + (centroid - origin) * f;
		}
		// 0x69220C..0x692262: LH3DIsland::RayCast fn_00802550(origin, split) (LandIslandInterface::RayCast: the RAY from
		// the origin through the split point to the map's edge, or its y = 0 point within 7500 m of the camera). When it
		// meets the land closer to the origin in x z than the split point, `(hz - oz)^2 + (hx - ox)^2 < (sz - oz)^2 +
		// (sx - ox)^2` (fcompp, `test ah, 1`), the whole of fn_00691F30 ends (jne 0x692ABE): the fork is not laid out
		// again (it keeps the joints of the last step), no children, no strike, no shield test
		if (Locator::terrainSystem::has_value())
		{
			// LH3DTech::g_camera 0xEA1DB8 for the y = 0 point; (openblack) the origin when there is no camera (tests)
			const glm::vec3 camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : origin;
			glm::vec2 land(0.0f); // [esp + 0x2C], [esp + 0x50]
			if (Locator::terrainSystem::value().RayCast(origin, split, land, camera))
			{
				const float splitX = split.x - origin.x;
				const float splitZ = split.z - origin.z;
				const float landX = land.x - origin.x;
				const float landZ = land.y - origin.z;
				if (landZ * landZ + landX * landX < splitZ * splitZ + splitX * splitX)
				{
					if (std::getenv("OPENBLACK_SPELL_TRACE") != nullptr)
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"),
						                   "Lightning {}: fork at depth {} from ({:.1f}, {:.1f}, {:.1f}) cut by the land at "
						                   "({:.1f}, {:.1f}) before its split point ({:.1f}, {:.1f}, {:.1f})",
						                   data.key, depth, origin.x, origin.y, origin.z, land.x, land.y, split.x, split.y,
						                   split.z);
					}
					return;
				}
			}
		}

		// 0x692268..0x692361: fn_006D0BC0(split, 2.5): inside a shield, the segment's way in (vt 0xFC FindIntersect from
		// the origin), a SpellEvent 4 there {no movement, strength 1 (2 when linked, data +0x20), no shield test,
		// target = the shield's spell (fn_006D0B10)} to this bolt's spell; a 0 (the shield held) ends the fork there
		// (split = the hit, [esp+0x13]). The shield gets its spark either way (fn_006D0AF0)
		if (const auto* sphere = shields::FindShieldContainingPoint(split, k_ShieldMargin); sphere != nullptr)
		{
			glm::vec3 hit = split;
			shields::FindIntersect(*sphere, origin, split, k_ShieldMargin, hit);
			SpellEventInfo event;
			event.type = SpellEventInfo::HitSpell;
			event.position = hit;
			event.velocity = glm::vec3(0.0f);
			event.strength = (data.link != 0 ? 2.0f : 1.0f) * 1.0f; // 0x692308..0x69231D
			event.checkShields = false;
			event.target = shields::SpellOf(*sphere);
			if (effect.SendSpellEvent(event) == 0)
			{
				split = hit;
				stop = true;
			}
			shields::AddImpactTarget(*sphere, hit);
		}

		// 0x692366..0x69239E: the fork's scale runs from S / (depth + 1) to S / (depth + 2), S = scale x ForkScale x
		// FP_ForkScale (+0xAC)
		const float s = scale * data.forkScale;
		const float scaleFrom = s / static_cast<float>(depth + 1);
		const float scaleTo = s / static_cast<float>(depth + 2);
		const auto direction = split - origin;
		// 0x6923A0..0x6923C6: sqrt((dz dz + dy dy) + dx dx), inline (not a GUtils call)
		const float length = std::sqrt((direction.z * direction.z + direction.y * direction.y) + direction.x * direction.x);
		// 0x6923D0..0x6923FC: with NumTexturesToTile (this +0x48) != -1 the fork's chain (+0x48) is cut in
		// max(1, ftol(NumTexturesToTile x length / data +0x60)) repeats (chain +0x30); data +0x60 is the distance from
		// the origin to the targets' centroid (fn_00690F50 0x691174). SF_LightningStrike / SF_LightningStormPush give 15
		if (numTexturesToTile != -1)
		{
			fork.chainTextures = std::max(
			    1, ecs::map_coords::FtoL(static_cast<float>(numTexturesToTile) * length / data.centroidDistance));
		}
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
			// +0x8F, the colour's alpha byte (0x692548..0x69257C): 0xFF, or PSysRand(0x7F) + 0x80 (0x692551..0x69255D)
			atom.colour[3] = opaque ? 255 : static_cast<uint8_t>(effect.Rand(0x7F) + 0x80);
		}
		const glm::vec3 end = fork.atoms.back()->position; // the last joint, `lea ebx, [ecx+0x80]` (0x69279A)
		if (targets.size() >= 2 && !stop)
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
			// 0x69276D..0x6927DA: each list gets the next free fork (+0xA0), depth first, with the scale [esp+0x58]; when
			// the forks run out the recursion stops there (0x692786, 0x6927C6)
			for (const auto* list : {&ahead, &behind})
			{
				if (nextFork >= data.root->subCollections.size())
				{
					return;
				}
				auto& child = *data.root->subCollections[nextFork++];
				Fork(effect, data, depth + 1, child, end, *list, childScale, nextFork, used);
			}
			return;
		}
		if (targets.size() == 1 && clashChild)
		{
			// 0x6927EC..0x6928EA: the linked bolt goes on from the clash point to its one target: a new list with it, the
			// next free fork (none: stop, 0x6928B6), depth + 1, the scale x 3 (so opaque and thick)
			if (nextFork >= data.root->subCollections.size())
			{
				return;
			}
			auto& child = *data.root->subCollections[nextFork++];
			Fork(effect, data, depth + 1, child, end, targets, childScale, nextFork, used);
			return;
		}
		// 0x6928ED..0x6928F3: ended by a clash or a shield
		if (stop)
		{
			return;
		}
		// 0x6928F9..: one target: the strike
		StrikeTarget(effect, data, data.targets[static_cast<size_t>(targets.front())], centroid);
	}

	/// The tip of fn_00691F30 (0x6928F9..0x692ABE): the cooldown, the light map atom at the list's centroid
	/// (fn_00691E80), the "impressive" report once 0.2 s passed since the search (fn_00691ED0) and the event that
	/// actually damages
	void StrikeTarget(Effect& effect, Data& data, Target& target, const glm::vec3& centroid) const
	{
		// 0x6928FD..0x692949: +0x18 = PSysRand(LightmapSteps()); data only
		const auto steps = LightmapSteps();
		target.cooldown = PSysRand(effect, steps); // 0x692935
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
		// 0x692976..0x6929F4: SpellEventInfo type 3 at the target's tip (fn_00691E00), no movement, no shield test;
		// strength 1, 2 when the bolt is linked to another one (data +0x20, 0x6929DE..0x6929EE)
		SpellEventInfo event;
		event.type = SpellEventInfo::Landed;
		event.position = target.Tip();
		event.velocity = glm::vec3(0.0f);
		event.strength = data.link != 0 ? 2.0f : 1.0f;
		event.checkShields = false;
		if (effect.GetSink() != nullptr)
		{
			effect.SendSpellEvent(event);
		}
		// TODO(M5): without a spell (a script / climate strike, global 0xC029D0) the original applies the static
		// EffectValues of info 0xCC9704 at the tip (0x692A10..0x692A9B). UNVERIFIED which GEffectInfo row that is.

		// 0x692AA0..0x692AB9: the same event also goes to the manager of the bolt this one is linked to (fn_00690070)
		if (const auto* linked = Find(data.link); linked != nullptr)
		{
			if (auto* other = EffectOf(*linked); other != nullptr)
			{
				other->SendSpellEvent(event);
			}
		}
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
	int numTexturesToTile; ///< +0x48 (ctor 0x690180: -1)
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
