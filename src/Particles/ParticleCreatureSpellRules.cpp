/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The particles of the spells cast on a creature. The first rule takes the creature the spell was cast on from the
// effect's targets and makes an atom where it stands, which hums and plays the casting sound; the others hang off
// that atom and read the creature from it. The wisps come out of the caster's hand one after another and wind round
// the box of the creature's bones, fading as an invisible creature fades; a creature casting it gives up once the way
// from its hands to the creature has swung round too far. The flies of an itchy creature wait at the hand, then circle
// its head. Hearts come off a creature made nice, from all over its body, and steam off a frozen creature's feet, each
// flung off and gone when old. Wiki: docs/wiki/creature/casting.md#the-creature-spells-particles.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <atomic>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Services/SpellSounds.h"
#include "ECS/Systems/CreatureSpellBodiesInterface.h"
#include "Locator.h"
#include "Particles/CreatureSpellMaths.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/ParticleObjectRules.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
namespace maths = particles::maths;

constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// The invisible spell's kind, whose wisps fade as the creature fades
constexpr int k_InvisibleKind = 7;
/// The kind after the last one, for which the wisps make nothing
constexpr int k_KindPastTheLast = 16;
/// The sound a creature's cast plays in place of the file's casting sound:
/// SOUND_SPELL_CREATURE_SPELL_CAST_BY_OTHER_CREATURE in Data\SoundAction.h
constexpr int32_t k_CastByCreatureSound = 152;
/// A wisp made by another rule turns at these rates and keeps this size until this rule has given it its own
constexpr float k_DefaultThetaRate = 0.0f;
constexpr float k_DefaultPhiRate = 1.0f;
constexpr float k_DefaultWispScale = 1.0f;

/// What the creature spells' particles read of their creature (Locator::creatureSpellBodies)
const ecs::systems::CreatureSpellBodiesInterface& Bodies()
{
	if (!Locator::creatureSpellBodies::has_value())
	{
		std::fputs("UR_CreatureSpell: no creature bodies in the locator (Locator::creatureSpellBodies)\n", stderr);
		std::abort();
	}
	return Locator::creatureSpellBodies::value();
}

/// The creature a collection slot keeps
entt::entity SlotCreature(const Collection::Slot& slot)
{
	return static_cast<entt::entity>(slot.object);
}

/// The creature the first rule's atom stands for, for the emitters under it
struct CreatureLink
{
	entt::entity creature {entt::null};
};

/// UR_CreatureSpell. Flags 2 without 4: it makes no creator's atoms, but the effect waits for it until it closes.
/// The creature is taken once, on the first step: without one the rule lets go of its collection. Its atom is put
/// where the creature stands then, and stays there
class CreatureSpell final: public Modifier, public SurfacePointSource
{
public:
	explicit CreatureSpell(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , hum(ReadSoundAction(object, "SoundCreatureSpell"))
	    , cast(ReadSoundAction(object, "SoundCreatureSpellCast"))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto& bodies = Bodies();
		if (slot.first)
		{
			slot.first = false;
			// One object target, kept when it is a creature
			if (!effect.GetTargets().empty())
			{
				const auto target = effect.TakeTarget();
				if (bodies.CreatureBody(target).has_value())
				{
					slot.object = entt::to_integral(target);
				}
			}
			const auto creature = SlotCreature(slot);
			const auto body = bodies.CreatureBody(creature);
			if (!body.has_value())
			{
				return false;
			}
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			AtomDataOf<CreatureLink>(atom, this).creature = creature;
			atom.position = body->origin;
			audio::spell_sounds::StartSound(effect, atom, hum);
			// A spell a script casts makes no casting sound; one a creature casts sounds as cast by a creature
			const auto* sink = effect.GetSink();
			if (sink == nullptr || !sink->IsScriptCasting())
			{
				auto sound = cast;
				if (sink != nullptr && sink->IsCreatureCasting())
				{
					sound.action = k_CastByCreatureSound;
				}
				audio::spell_sounds::StartSound(effect, atom, sound);
			}
		}
		// It lets go of a creature that has gone
		if (const auto creature = SlotCreature(slot); creature != entt::null && !bodies.IsAvailable(creature))
		{
			slot.object = entt::to_integral(static_cast<entt::entity>(entt::null));
		}
		// Closing down, every atom's sounds go
		if (effect.Closing())
		{
			for (auto& atom : collection.atoms)
			{
				audio::spell_sounds::StopAllSounds(*atom);
			}
		}
		return true;
	}

	/// An emitter under the atom asks for a point of the creature's body. (pending) The game walks the body mesh of the
	/// level of detail the creature was last drawn at, which is not ported; a creature that has gone gives the atom's
	/// own place
	Result RandomSurfacePoint(Effect& effect, const Atom& atom, glm::vec3& out) const override
	{
		const auto found = atom.modifierData.find(this);
		const auto creature =
		    found != atom.modifierData.end() ? std::static_pointer_cast<CreatureLink>(found->second)->creature : entt::null;
		if (creature == entt::null || !Bodies().IsAvailable(creature))
		{
			out = effect.GlobalPosition(atom);
			return Result::Point;
		}
		return Result::NotPorted;
	}

	std::string creator;
	std::vector<int> nextGroups;
	SoundAction hum;
	SoundAction cast;
};

/// The creature the first rule keeps, read through the atom the collection hangs off: the first rule of that atom's
/// own collection that is the first rule. None without a parent atom, without that rule or once its creature has gone
std::optional<entt::entity> CreatureOf(const Collection& collection)
{
	if (collection.parent == nullptr || collection.parent->collection == nullptr)
	{
		return std::nullopt;
	}
	for (const auto& slot : collection.parent->collection->modifiers)
	{
		if (dynamic_cast<const CreatureSpell*>(slot.modifier) != nullptr)
		{
			const auto creature = SlotCreature(slot);
			return creature != entt::null ? std::optional(creature) : std::nullopt;
		}
	}
	return std::nullopt;
}

/// A wisp's own data: its angles round and up the box, the rates they turn at, and the size it was made at
struct WispData
{
	float theta {0.0f};
	float phi {0.0f};
	float thetaRate {k_DefaultThetaRate};
	float phiRate {k_DefaultPhiRate};
	float scale {k_DefaultWispScale};
};

/// UR_CreatureSpellGeneric. Flags 2 without 4, as the first rule. It lets go of its collection without a creator,
/// without the first rule's creature, or when its effect's spell is no creature spell or is of the kind after the last
class CreatureSpellGeneric final: public Modifier
{
public:
	explicit CreatureSpellGeneric(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , numAtoms(object.Int("NumAtoms", 10))
	    , emitDuration(object.Float("EmitDuration", 2.0f))
	    , phiSpeed(object.Float("PhiSpeed", 1.0f))
	    , thetaSpeed(object.Float("ThetaSpeed", 2.0f))
	    , delayBeforeEmit(object.Float("DelayBeforeEmit", 1.0f))
	    , taperFrac(object.Float("TaperFrac", 1.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		const auto hand = effect.GetProcessInfo().handPos;
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value())
		{
			return false;
		}
		auto* sink = effect.GetSink();
		const auto kind = sink != nullptr ? sink->CreatureSpellKind() : std::nullopt;
		if (!kind.has_value() || *kind == k_KindPastTheLast)
		{
			return false;
		}
		// Where the creature and the caster's hand were, across the land, as the wisps began
		if (slot.first)
		{
			slot.first = false;
			slot.extra = glm::vec4(body->origin.x, body->origin.z, hand.x, hand.z);
		}
		const float dt = effect.GetDt();
		// The wisps come out one after another once the delay is over: the count made, a whole number, against the
		// fraction due, so the first comes as soon as any is due
		auto& due = slot.state.x;
		auto& made = slot.state.y;
		if (effect.CollectionAge(collection) > delayBeforeEmit)
		{
			due = maths::WispsDue(due, numAtoms, dt, emitDuration);
		}
		while (made < due)
		{
			made += 1.0f;
			auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
			auto& wisp = AtomDataOf<WispData>(atom, this);
			wisp.scale = atom.baseScale;
			wisp.thetaRate = effect.Random(0.5f, 1.5f) * thetaSpeed;
			if (effect.Random(1.0f) < 0.5f)
			{
				wisp.thetaRate = wisp.thetaRate * -1.0f;
			}
			wisp.phiRate = effect.Random(0.5f, 1.5f) * phiSpeed;
		}
		// As the creature goes invisible its wisps go with it, and come back as it ends
		if (*kind == k_InvisibleKind && !effect.Closing())
		{
			const auto alpha = static_cast<int32_t>((1.0f - body->invisible) * 255.0f);
			collection.alpha = static_cast<float>(static_cast<uint8_t>(alpha));
		}
		// A creature casting it gives up once the way from its hands to the creature has swung too far round
		if (!effect.Closing() && sink->IsCreatureCasting())
		{
			if (const auto limit = sink->MaxDirectionChange(); limit.has_value())
			{
				const glm::vec2 began(slot.extra.x - slot.extra.z, slot.extra.y - slot.extra.w);
				const glm::vec2 now(body->origin.x - hand.x, body->origin.z - hand.z);
				if (maths::SwungTooFar(began, now, *limit))
				{
					sink->CloseDown();
				}
			}
		}
		const auto box = maths::WispBoxOf(body->bones);
		for (auto& atom : collection.atoms)
		{
			auto& wisp = AtomDataOf<WispData>(*atom, this);
			const float age = effect.AtomAge(*atom);
			wisp.theta = std::fmod(dt * wisp.thetaRate + wisp.theta, k_TwoPi);
			wisp.phi = std::fmod(dt * wisp.phiRate + wisp.phi, k_TwoPi);
			const auto orbit = maths::WispOrbit(box, wisp.theta, wisp.phi, taperFrac, age);
			// The world point is the atom's own: the files have no hierarchy
			atom->position = maths::WispPosition(hand, orbit, age);
			atom->baseScale = wisp.scale * body->size;
			atom->colour[3] = maths::WispAlpha(age);
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	int numAtoms;
	float emitDuration;
	float phiSpeed;
	float thetaSpeed;
	float delayBeforeEmit;
	float taperFrac;
};

/// UR_CreatureSpellItch. Flags 6: a creator. Its flies are all made on the first step; it lets go of its collection
/// without a creator or without the first rule's creature
class CreatureSpellItch final: public Modifier
{
public:
	explicit CreatureSpellItch(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , numAtoms(object.Int("NumAtoms", 5))
	    , pauseBeforeGotoCreature(object.Float("PauseBeforeGotoCreature", 5.0f))
	    , orbitSpeed(object.Float("OrbitSpeed", 1.0f))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		if (slot.first)
		{
			for (int i = 0; i < numAtoms; ++i)
			{
				effect.NewAtom(collection, atomCreator, nextGroups);
			}
			slot.first = false;
		}
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value())
		{
			return false;
		}
		// (openblack) A species with no eyes to circle leaves its flies where they are
		const auto& bones = body->bones;
		if (!body->rightEye.has_value() || !body->leftEye.has_value() || *body->rightEye >= bones.size() ||
		    *body->leftEye >= bones.size())
		{
			return true;
		}
		// At the hand at first, then round the creature's head; the flies catch up as they flock
		const float age = effect.CollectionAge(collection);
		const auto point = age > pauseBeforeGotoCreature
		                       ? maths::ItchOrbit(bones[*body->rightEye], bones[*body->leftEye], age, orbitSpeed)
		                       : effect.GetProcessInfo().handPos;
		for (auto& atom : collection.atoms)
		{
			// The world point is the atom's own: the files have no hierarchy
			atom->position = point;
		}
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	int numAtoms;
	float pauseBeforeGotoCreature;
	float orbitSpeed;
};
/// The hearts' and the steam's own count: how many are due, and how many were made
struct AtomsMade
{
	float due {0.0f};
	int32_t made {0};
};

/// Whether another atom is due: the count made, a whole number, below the fraction due (or that fraction not a number)
bool StillDue(const AtomsMade& data)
{
	return !(static_cast<double>(data.made) >= data.due);
}

/// UR_CreatureSpellCompassion, the hearts off a creature made nice. Flags 2 without 4, as the first rule. It lets go of
/// its collection without a creator, without the first rule's creature, or when its hearts would come at no rate. Until
/// the effect closes the hearts come as they fall due, MaxAtoms over DieAge a second and no more than MaxAtoms at once,
/// each from a random point of what the atom it hangs off stands for, at its size times the creature's and flung off at
/// InitSpeed times the creature's size; each goes once it is older than DieAge
class CreatureSpellCompassion final: public Modifier
{
public:
	explicit CreatureSpellCompassion(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , maxAtoms(object.Int("MaxAtoms", 20))
	    , dieAge(object.Float("DieAge", 5.0f))
	    , initSpeed(object.Float("InitSpeed", 10.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value() || !maths::HasAtomRate(maxAtoms, dieAge))
		{
			return false;
		}
		auto& data = CollectionDataOf<AtomsMade>(collection, this);
		data.due = maths::AtomsDue(data.due, maxAtoms, dieAge, effect.GetDt());
		if (!effect.Closing())
		{
			auto& parent = *collection.parent;
			while (StillDue(data) && static_cast<int32_t>(collection.atoms.size()) < maxAtoms)
			{
				++data.made;
				auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
				atom.baseScale = atom.baseScale * body->size;
				// (pending) The game leaves the point unset when the atom it hangs off gives none; here at that atom
				glm::vec3 point = effect.GlobalPosition(parent);
				if (ParentSurfacePoint(effect, parent, point) == SurfacePointSource::Result::NotPorted)
				{
					// (pending) A point of the creature's body is not ported: the heart just made goes again
					effect.NotifyAtomRemoved(*collection.atoms.back());
					collection.atoms.pop_back();
					static std::atomic_bool s_logged {false};
					// the game's logger is not there when a test runs on its own
					if (auto logger = spdlog::get("game"); logger != nullptr && !s_logged.exchange(true))
					{
						SPDLOG_LOGGER_WARN(logger,
						                   "PSys: UR_CreatureSpellCompassion's hearts from a creature's body: not ported");
					}
					break;
				}
				atom.position = effect.GlobalToLocal(collection, point);
				atom.velocity = maths::FlungVelocity(effect.RandomInBall(), initSpeed, body->size);
			}
		}
		std::erase_if(collection.atoms, [&](const auto& atom) {
			if (!(effect.AtomAge(*atom) > dieAge))
			{
				return false;
			}
			effect.NotifyAtomRemoved(*atom);
			return true;
		});
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	int maxAtoms;
	float dieAge;
	float initSpeed;
};

/// UR_CreatureSpellFreeze, the steam off a frozen creature. Flags 2 without 4, as the first rule. It lets go of its
/// collection without a creator or without the first rule's creature. Until the effect closes the steam comes as it
/// falls due, MaxAtoms over DieAge a second and no more than MaxAtoms at once, each puff from about one foot or the
/// other, at its size times the creature's and flung off at InitSpeed times the creature's size. Every puff is kept at
/// least MinHeight times the creature's size above the land, and goes once it is older than DieAge
class CreatureSpellFreeze final: public Modifier
{
public:
	explicit CreatureSpellFreeze(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.IntArray("NextGroups"))
	    , maxAtoms(object.Int("MaxAtoms", 20))
	    , dieAge(object.Float("DieAge", 5.0f))
	    , initSpeed(object.Float("InitSpeed", 10.0f))
	    , minHeight(object.Float("MinHeight", 5.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value())
		{
			return false;
		}
		auto& data = CollectionDataOf<AtomsMade>(collection, this);
		data.due = maths::AtomsDue(data.due, maxAtoms, dieAge, effect.GetDt());
		// (openblack) A species with no feet, or a body not posed yet, lets no steam out
		const auto& bones = body->bones;
		const bool hasFeet = body->rightFoot.has_value() && body->leftFoot.has_value() && *body->rightFoot < bones.size() &&
		                     *body->leftFoot < bones.size();
		if (!effect.Closing() && hasFeet)
		{
			while (StillDue(data) && static_cast<int32_t>(collection.atoms.size()) < maxAtoms)
			{
				++data.made;
				auto& atom = effect.NewAtom(collection, atomCreator, nextGroups);
				atom.baseScale = atom.baseScale * body->size;
				// The right foot when the coin falls below a half, else the left
				const auto foot = !(effect.Random(1.0f) >= 0.5f) ? *body->rightFoot : *body->leftFoot;
				const auto start = maths::SteamStart(bones[foot], effect.RandomInBall(), body->size);
				atom.position = effect.GlobalToLocal(collection, start);
				atom.velocity = maths::FlungVelocity(effect.RandomInBall(), initSpeed, body->size);
			}
		}
		std::erase_if(collection.atoms, [&](const auto& atom) {
			// The land under the puff, at the map coordinates of its own position (the files have no hierarchy)
			const auto cell = object_surface::LandTestCell(atom->position);
			const float ground = map_coords::ToWorld(map_coords::MapCoords {cell.x, cell.y, 0.0f}).y;
			if (const auto raised = maths::SteamOffLand(atom->position.y, ground, minHeight, body->size); raised.has_value())
			{
				atom->position.y = *raised;
			}
			if (!(effect.AtomAge(*atom) > dieAge))
			{
				return false;
			}
			effect.NotifyAtomRemoved(*atom);
			return true;
		});
		return true;
	}

	std::string creator;
	std::vector<int> nextGroups;
	int maxAtoms;
	float dieAge;
	float initSpeed;
	float minHeight;
};
} // namespace

void openblack::psys::RegisterCreatureSpellRules()
{
	RegisterModifier("UR_CreatureSpell", MakeModifierOf<CreatureSpell>);
	RegisterModifier("UR_CreatureSpellGeneric", MakeModifierOf<CreatureSpellGeneric>);
	RegisterModifier("UR_CreatureSpellCompassion", MakeModifierOf<CreatureSpellCompassion>);
	RegisterModifier("UR_CreatureSpellItch", MakeModifierOf<CreatureSpellItch>);
	RegisterModifier("UR_CreatureSpellFreeze", MakeModifierOf<CreatureSpellFreeze>);
}
