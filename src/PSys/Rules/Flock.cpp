/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flock miracles' particle rules (SF_FlockFlyingRainGood / Evil, SF_FlockGroundDust; M4c): UR_FollowTargets, one
// atom on each target the spell gives its effect (every bird, every wolf), with the glow trail under it, and
// EventConditionAtomNearVillagers (declared by SF_FlockGroundDust, used by no rule of the shipped files); and the boids
// of the particle flocks, UR_Flocking (the butterflies of SF_Forest / SF_Butterflies*, the flies of SF_Flies*, the itch
// of SF_CreatureSpellItch*). Research: dev\tmp_dis\miracles\impl\m4c\followtargets.asm and flocking*.asm; wiki:
// docs/bw1-notes/magic.md ("Bandadas").

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>

#include "Flock.h"

#include "3D/Billboard.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCoords.h"
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
bool Available(entt::entity object)
{
	// GameThing::IsAvailable (vt 0x2C) == 1: here, the object still exists
	return object != entt::null && Locator::entitiesRegistry::has_value() &&
	       Locator::entitiesRegistry::value().Valid(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// UR_FollowTargets::AtomData (0x28 bytes, vtable 0x8FD3EC, ctor 0x560210): +0x24 the target, +0x20 the game turn it
/// was last seen (unused)
struct FollowData
{
	entt::entity target {entt::null};
};

/// UR_FollowTargets::ModifyAtomCollection 0x6A04B0 (ctor 0x6BFA20, DefineProperties UR_FollowTargets_vfunc3 0x6B1DD0 on
/// top of AtomCreateRule's). Flags 2 without 4 (0x6BFA4C / 0x6BFA7B): no creator, but the effect waits for its targets
/// until it closes.
class FollowTargets final: public Modifier
{
public:
	explicit FollowTargets(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , removeTarget(object.Bool("RemoveTargetFromManager", true))
	    , removeAtomWhenTargetDies(object.Bool("RemoveAtomWhenTargetDies", false))
	    , usePointTargets(object.Bool("UseLHPointTargets", false))
	    , soundOneOnly(object.Bool("SoundOneOnly", true))
	    , soundCreate(ReadSoundAction(object, "SoundCreate"))
	{
	}
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// no PCreator (+0x28): 0, the rule detaches
		if (creator.empty() || effect.FindCreator(creator) == nullptr)
		{
			return false;
		}
		// not closing (PSysManager::IsInState(1) 0x673050): one new atom per step, on a target object of the spell
		// (SpellTargets +0xB8) or, with UseLHPointTargets and no object left, on a point
		if (!effect.Closing())
		{
			if (!effect.GetTargets().empty())
			{
				// RemoveTargetFromManager: TakeTargetObject 0x671030 (the last one added); else fn_00671080, the first,
				// left in the list
				const auto target = removeTarget ? effect.TakeTarget() : effect.GetTargets().front();
				if (target != entt::null)
				{
					NewFollower(effect, collection, target);
				}
			}
			else if (usePointTargets && effect.TargetPointCount() > 0)
			{
				// fn_00670F00 (taken) / fn_00670E30 (the cyclic cursor): the point is read and not used; the atom
				// follows nothing
				glm::vec3 point;
				if (removeTarget)
				{
					effect.TakeTargetPoint(point);
				}
				NewFollower(effect, collection, entt::null);
			}
		}
		// every atom sits on its target (MapCoords +0x14 as a world point: the land + its height, GlobalToLocal); a
		// target that went is forgotten, and with RemoveAtomWhenTargetDies its atom goes too
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			auto& data = DataOf(atom);
			if (data.target == entt::null)
			{
				++i; // no target (a point's atom, or one already lost): left where it is
				continue;
			}
			if (!Available(data.target))
			{
				data.target = entt::null;
				if (removeAtomWhenTargetDies)
				{
					collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i)); // DeleteFromAtomList
					continue;
				}
				++i;
				continue;
			}
			const auto& position = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(data.target).position;
			atom.position = effect.GlobalToLocal(collection, position); // +0x80
			++i;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	bool removeTarget;             ///< +0x2C RemoveTargetFromManager (1)
	bool removeAtomWhenTargetDies; ///< +0x2D (0)
	bool usePointTargets;          ///< +0x2E UseLHPointTargets (0)
	bool soundOneOnly;             ///< +0x2F SoundOneOnly (1)
	SoundAction soundCreate;       ///< +0x30 SoundCreate

private:
	/// The AtomData of this rule on the atom (found in its list +0x24, or new)
	FollowData& DataOf(Atom& atom) const
	{
		auto& slot = atom.modifierData[this];
		if (slot == nullptr)
		{
			slot = std::make_shared<FollowData>();
		}
		return *std::static_pointer_cast<FollowData>(slot);
	}

	/// AtomCore::Create 0x6737F0, the creator's init (vt 0x10), fn_00674DD0 (into the collection with NextGroups), the
	/// AtomData with the target, scale +0x78 = the target's GetScale (vt 0x120); SoundCreate unless SoundOneOnly and this
	/// is not the collection's only atom (+0x44 == 1)
	void NewFollower(Effect& effect, Collection& collection, entt::entity target) const
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		DataOf(atom).target = target;
		if (target != entt::null)
		{
			if (Available(target))
			{
				atom.ruleScale = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(target).scale.x;
			}
			if (!soundOneOnly || collection.atoms.size() == 1)
			{
				audio::spell_sounds::StartSound(effect, atom, soundCreate);
			}
		}
	}
};

/// LHMatrix's 3 x 3 part as the original stores it (row-major; LH3D multiplies row vectors: M' = M x R)
using Rows = std::array<std::array<float, 3>, 3>;

Rows Multiply(const Rows& a, const Rows& b)
{
	Rows out {};
	for (size_t i = 0; i < 3; ++i)
	{
		for (size_t j = 0; j < 3; ++j)
		{
			out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
		}
	}
	return out;
}

/// AtomCore::SetAngleY 0x674360's layout: rows (c, 0, s), (0, 1, 0), (-s, 0, c); fn_0067A4A0 multiplies by the same
Rows RotationY(float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {{{c, 0.0f, s}, {0.0f, 1.0f, 0.0f}, {-s, 0.0f, c}}};
}

/// SetRotationMatrix 0x674120 (+0x44, the translation +0x68 cleared): openblack's columns are the LH rows
glm::mat3 ToAtomRotation(const Rows& m)
{
	return {glm::vec3(m[0][0], m[0][1], m[0][2]), glm::vec3(m[1][0], m[1][1], m[1][2]), glm::vec3(m[2][0], m[2][1], m[2][2])};
}

/// fn_006805F0: v set to that length (0 stays 0)
glm::vec3 SetLength(glm::vec3 v, float length)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	return v * (length / std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

/// The rule's normalisation (0x6837A4.., 0x6839B8..): a zero vector stays, its length 0
glm::vec3 Unit(glm::vec3 v, float& length)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		length = 0.0f;
		return v;
	}
	length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return v * (1.0f / length);
}

/// UR_Flocking::ModifyAtomCollection 0x683580 (ctor 0x683470, DefineProperties 0x6ABCD0, CollectionData 0x560EC0: the
/// flock's velocity +0x24, kept in the slot's state here), UpdateBanking 0x684160 and the sprite turn fn_006840E0
class Flocking final: public Modifier
{
public:
	explicit Flocking(const Object& object)
	    : velocityMatching(object.Float("K_VelocityMatching", 1.0f))
	    , centralAttraction(object.Float("K_CentralAttraction", 1.0f))
	    , neighbourAccn(object.Float("K_NeighbourAccn", 0.0f))
	    , neighbourAvoidance(object.Float("K_NeighbourAvoidance", 1.0f))
	    , damping(object.Float("K_Damping", 0.0f))
	    , flockDamping(object.Float("K_FlockDamping", 0.0f))
	    , idealVel(object.Float("K_IdealVel", 0.0f))
	    , maxAccn(object.Float("F_MaxAccn", 10.0f))
	    , maxVel(object.Float("F_MaxVel", 100.0f))
	    , gravityForBanking(object.Float("GravityForBanking", 10.0f))
	    , reducePitchBy(object.Float("ReducePitchBy", 1.0f))
	    , scaleModifier(object.Float("ScaleModifier", 1.0f))
	    , axisChosen(Type(object, "AxisChosen", 0))
	    , neighbourAccnType(Type(object, "NeghbourAccnType", 2))
	    , idealVelAccnType(Type(object, "IdealVelAccnType", 1))
	    , invertAccnIdealVel(object.Bool("F_InvertAccnIdealVel", false))
	    , invertAccn(object.Bool("F_InvertAccn", false))
	    , spriteRotation(object.Bool("SpriteRotation", true))
	    , neighbourAccnInvert(object.Bool("NeighbourAccnInvert", false))
	    , localScale(object.String("LocalScaleFP"))
	{
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const size_t n = collection.atoms.size();
		if (n == 0)
		{
			return true;
		}
		const float dt = effect.GetDt(); // [0xD4E0EC]
		const glm::vec3 oldFlock(slot.state);
		// fn_00674B10: the parent atom's position (the effect's origin without one) in this collection's frame
		const glm::vec3 target = effect.GlobalToLocal(
		    collection, collection.parent != nullptr ? effect.GlobalPosition(*collection.parent) : effect.GetOrigin());
		// the mean velocity and position of the atoms
		glm::vec3 meanVelocity(0.0f);
		glm::vec3 centre(0.0f);
		for (const auto& atom : collection.atoms)
		{
			meanVelocity += atom->velocity;
			centre += atom->position;
		}
		const float inverse = 1.0f / static_cast<float>(n);
		meanVelocity *= inverse;
		centre *= inverse;
		// the flock's velocity: V (1 - dt K_FlockDamping) + unit(target - centre) K_IdealVel f(|target - centre|) dt
		float length = 0.0f;
		const glm::vec3 toTarget = Unit(target - centre, length);
		const float ideal = Accn(length, idealVelAccnType, invertAccnIdealVel) * idealVel;
		const glm::vec3 newFlock = oldFlock * (1.0f - dt * flockDamping) + toTarget * ideal * dt;
		slot.state = glm::vec4(newFlock, 0.0f);
		const float scale = localScale.empty() ? 1.0f : effect.FloatProvider(localScale, 1.0f); // LocalScaleFP +0x60
		for (auto& atomPointer : collection.atoms)
		{
			auto& a = *atomPointer;
			// every other atom pushes (K_NeighbourAccn): unit(b - a) x f(|b - a| x LocalScale); the nearest is kept
			glm::vec3 neighbours(0.0f);
			const Atom* nearest = nullptr;
			float best = 0.0f;
			for (const auto& other : collection.atoms)
			{
				if (other.get() == &a)
				{
					continue;
				}
				const glm::vec3 r = other->position - a.position;
				const float d2 = r.x * r.x + r.y * r.y + r.z * r.z;
				float distance = 0.0f;
				const glm::vec3 unit = Unit(r, distance);
				neighbours += unit * Accn(distance * scale, neighbourAccnType, neighbourAccnInvert);
				if (nearest == nullptr || d2 < best)
				{
					best = d2;
					nearest = other.get();
				}
			}
			neighbours *= neighbourAccn / static_cast<float>(n);
			if (nearest == nullptr)
			{
				continue; // 0x683B18: a lone atom keeps its velocity
			}
			const glm::vec3 relative = a.velocity - oldFlock;
			// the avoidance of the nearest: unit(nearest - a) x f(|..| x LocalScale, type 2, not inverted: 1 / x^2) x K
			float nearestDistance = 0.0f;
			const glm::vec3 toNearest = Unit(nearest->position - a.position, nearestDistance);
			const glm::vec3 avoid = toNearest * (Accn(nearestDistance * scale, 2, false) * neighbourAvoidance);
			// the attraction to the centre: AxisChosen is its type, F_InvertAccn its inversion
			float centreDistance = 0.0f;
			const glm::vec3 toCentre = Unit(centre - a.position, centreDistance);
			const glm::vec3 attract = toCentre * (Accn(centreDistance, axisChosen, invertAccn) * centralAttraction);
			// velocity matching: (mean velocity - relative velocity) x K, x LocalScale
			glm::vec3 matching = (meanVelocity - relative) * velocityMatching;
			if (!localScale.empty())
			{
				matching *= scale;
			}
			glm::vec3 accn = neighbours + (matching + (attract - avoid));
			if (std::sqrt(accn.x * accn.x + accn.y * accn.y + accn.z * accn.z) > maxAccn)
			{
				accn = SetLength(accn, maxAccn);
			}
			glm::vec3 velocity = relative * (1.0f - dt * damping) + accn * dt;
			velocity += newFlock;
			if (maxVel * maxVel < velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)
			{
				velocity = SetLength(velocity, maxVel);
			}
			if (spriteRotation)
			{
				TurnSprite(a, velocity);
			}
			else
			{
				UpdateBanking(a, velocity, (velocity - a.velocity) * (1.0f / dt));
			}
			a.velocity = velocity; // +0x34
		}
		// then every atom moves: +0x80 += velocity x dt
		for (auto& atom : collection.atoms)
		{
			atom->position += atom->velocity * dt;
		}
		return true;
	}

	float velocityMatching;   ///< +0x20
	float centralAttraction;  ///< +0x24
	float neighbourAccn;      ///< +0x28
	float neighbourAvoidance; ///< +0x2C
	float damping;            ///< +0x30
	float flockDamping;       ///< +0x34
	float idealVel;           ///< +0x38
	float maxAccn;            ///< +0x3C
	float maxVel;             ///< +0x40
	float gravityForBanking;  ///< +0x44
	float reducePitchBy;      ///< +0x48
	float scaleModifier;      ///< +0x4C
	int axisChosen;           ///< +0x50 (the centre attraction's type)
	int neighbourAccnType;    ///< +0x54
	int idealVelAccnType;     ///< +0x58
	bool invertAccnIdealVel;  ///< +0x5C
	bool invertAccn;          ///< +0x5D
	bool spriteRotation;      ///< +0x5E
	bool neighbourAccnInvert; ///< +0x5F
	std::string localScale;   ///< +0x60

private:
	/// The GetSetIntegerProperty setters 0x6AC0D0 / 0x6AC100 / 0x6AC130 take 0..2 only (the files write them as FLOAT)
	static int Type(const Object& object, std::string_view key, int fallback)
	{
		const int value = static_cast<int>(object.Float(key, static_cast<float>(fallback)));
		return value >= 0 && value <= 2 ? value : fallback;
	}

	/// fn_00683520 (Flock.h) with the rule's ScaleModifier
	float Accn(float distance, int type, bool invert) const
	{
		return flocking::Accn(distance, type, invert, scaleModifier);
	}

	/// fn_006840E0: SetAngleY(atan2(-y, x) + pi / 2) of the velocity as the camera sees it (the LHMatrix at 0xEA1D28,
	/// its columns 0 and 1, the W2C rotation: x = v . right, y = v . up; billboard::ScreenVelocity)
	static void TurnSprite(Atom& atom, const glm::vec3& velocity)
	{
		if (!Locator::camera::has_value())
		{
			return;
		}
		const auto& camera = Locator::camera::value();
		atom.rotation = ToAtomRotation(RotationY(graphics::billboard::ScreenVelocity(velocity, camera.GetRight(), camera.GetUp())));
	}

	/// UpdateBanking 0x684160 (Flock.h)
	void UpdateBanking(Atom& atom, const glm::vec3& v, const glm::vec3& a) const
	{
		atom.rotation = flocking::BankingRotation(v, a, reducePitchBy, gravityForBanking);
	}
};

/// EventConditionAtomNearVillagers::ConditionTrueForAtom 0x67D8E0: the atom's +0x80 ((inferido) taken as metres x, z;
/// its local position) as MapCoords (ftol x 6553.6) is on the map and some object of that 10 m cell (MapCell::FindTypeOnMap
/// 0x6015E0 with any type) is a villager (vt 0x2C8 IsVillager). No ConditionTrueForCollection of its own: false.
bool AtomNearVillagers(const Effect& /*effect*/, const Object& /*object*/, const Atom* atom, const Collection& /*collection*/)
{
	if (atom == nullptr || !Locator::entitiesMap::has_value() || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto cell = ecs::map_coords::CellOf(atom->position); // ftol(x * 6553.6f), the high words
	if (!ecs::map_coords::InBounds(cell, Locator::terrainSystem::value().GetCellsPerSide()))
	{
		return false; // MapCoords::InBounds 0x6042C0
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto object : Locator::entitiesMap::value().GetMobileInGridCell(ecs::MapInterface::CellId(cell)))
	{
		if (registry.Valid(object) && registry.AllOf<ecs::components::Villager>(object))
		{
			return true;
		}
	}
	return false;
}
} // namespace

float openblack::psys::flocking::Accn(float distance, int type, bool invert, float scaleModifier)
{
	if (distance < 0.01f)
	{
		distance = 0.01f; // 0x8C5840 / 0x3C23D70A
	}
	const float x = distance * scaleModifier;
	float f = 1.0f;
	if (type == 1)
	{
		f = x;
	}
	else if (type == 2)
	{
		f = x * x;
	}
	// (another type would read the bool argument as a float: the property setters never allow it)
	return invert ? f : 1.0f / f;
}

glm::mat3 openblack::psys::flocking::BankingRotation(const glm::vec3& v, const glm::vec3& a, float reducePitchBy,
                                                     float gravityForBanking)
{
	const float horizontal = std::sqrt(v.x * v.x + v.z * v.z);
	const float yaw = std::atan2(v.z, v.x);
	const float pitch = std::atan2(v.y, horizontal) * reducePitchBy;
	const float turn = horizontal != 0.0f ? (a.z * v.x - a.x * v.z) / horizontal : 0.0f;
	const float bank = std::atan2(turn / gravityForBanking, 1.0f);
	const float cb = std::cos(bank);
	const float sb = std::sin(bank);
	const Rows rollX {{{1.0f, 0.0f, 0.0f}, {0.0f, cb, -sb}, {0.0f, sb, cb}}};
	const float cp = std::cos(-pitch);
	const float sp = std::sin(-pitch);
	const Rows pitchZ {{{cp, -sp, 0.0f}, {sp, cp, 0.0f}, {0.0f, 0.0f, 1.0f}}};
	// the double at 0x9361E8 is -pi / 2
	const auto m = Multiply(Multiply(Multiply(RotationY(-std::numbers::pi_v<float> * 0.5f), rollX), pitchZ), RotationY(yaw));
	return ToAtomRotation(m);
}

void openblack::psys::RegisterFlockRules()
{
	RegisterModifier("UR_FollowTargets", MakeModifierOf<FollowTargets>);
	RegisterModifier("UR_Flocking", MakeModifierOf<Flocking>);
	RegisterCondition("EventConditionAtomNearVillagers", &AtomNearVillagers);
}
