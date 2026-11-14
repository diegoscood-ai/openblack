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

#include <algorithm>
#include <memory>
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
#include "Common/ProcessLogOnce.h"
#include "ECS/Systems/CreatureSpellBodiesInterface.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/CreatureSpellData.h"
#include "Particles/CreatureSpellMaths.h"
#include "Particles/CreatureSpellWispData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleObjectRules.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::particles;

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

/// The first rule's data on its collection beside its slot: the game turn its creature was last found available
struct CreatureTurn
{
	uint32_t validatedTurn {0};
};

/// The first rule's atom's particle, standing for its creature (the original's creature reference particle): it draws
/// nothing; after each step of its atom, while its creature is there, the turn it was last found there is stamped and
/// the atom is put where the creature's body stands, and once it is not the creature is let go
struct CreatureRef final: Creator
{
	mutable creature_spell::CreatureRefData data;

	CreatureRef()
	{
		kind = Kind::Other;
		className = "RenderParticleCreatureRef";
	}
	[[nodiscard]] int FramesPerAtom() const override { return 0; }
	void AfterAtomStep(Effect& /*effect*/, Atom& atom) const override
	{
		if (data.creature == entt::null)
		{
			return;
		}
		if (!Bodies().IsAvailable(data.creature))
		{
			data.creature = entt::null;
			return;
		}
		data.validatedTurn = magic::CurrentTurn();
		if (const auto body = Bodies().CreatureBody(data.creature); body.has_value())
		{
			atom.position = body->origin;
		}
	}
};

/// Gives the atom its creature particle, kept in the atom's data under the first rule, which answers for the points of
/// the creature's body the rules under it ask of the atom
void GiveCreatureRef(Atom& atom, const Modifier& rule, const creature_spell::CreatureRefData& data)
{
	auto ref = std::make_shared<CreatureRef>();
	ref->data = data;
	atom.creator = ref.get();
	atom.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(ref));
}

/// CreatureSpell. Flags 2 without 4: it makes no creator's atoms, but the effect waits for it until it closes.
/// The creature is taken once, on the first step: without one the rule lets go of its collection. Its atom is put
/// where the creature stands then, and its particle stands for the creature, which it follows (CreatureRef)
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

	[[nodiscard]] RuleData SavedData() const override { return RuleData::CreatureSpell; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto& bodies = Bodies();
		// its data on the collection is made on its first call: the turn its creature was last found available
		auto& held = CollectionDataOf<CreatureTurn>(collection, this);
		collection.NoteDataMade(this);
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
					held.validatedTurn = magic::CurrentTurn();
				}
			}
			const auto creature = SlotCreature(slot);
			const auto body = bodies.CreatureBody(creature);
			if (!body.has_value())
			{
				return false;
			}
			// made as the file's creator makes it, then its particle is the creature's
			auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
			GiveCreatureRef(atom, *this, {.creature = creature, .validatedTurn = magic::CurrentTurn()});
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
		// It lets go of a creature that has gone, and stamps the turn it finds it still there
		if (const auto creature = SlotCreature(slot); creature != entt::null)
		{
			if (bodies.IsAvailable(creature))
			{
				held.validatedTurn = magic::CurrentTurn();
			}
			else
			{
				slot.object = entt::to_integral(static_cast<entt::entity>(entt::null));
			}
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

	/// An emitter or the hearts under the atom ask for a point of the creature's body: one of its bones at random, as
	/// posed for the game turn, moved by a random point of the unit ball times 0.3 of its size
	/// (maths::NearBonePoint); a creature that has gone gives the atom's own place
	Result RandomSurfacePoint(Effect& effect, const Atom& atom, glm::vec3& out) const override
	{
		const auto* ref = dynamic_cast<const CreatureRef*>(atom.creator);
		const auto creature = ref != nullptr ? ref->data.creature : entt::entity {entt::null};
		const auto body =
		    creature != entt::null && Bodies().IsAvailable(creature) ? Bodies().CreatureBody(creature) : std::nullopt;
		// (port guard) a body not posed yet, which the original's always is: the atom's own place
		if (!body.has_value() || body->bones.empty())
		{
			out = effect.GlobalPosition(atom);
			return Result::Point;
		}
		const auto bone = body->bones[static_cast<size_t>(effect.Rand(static_cast<int32_t>(body->bones.size())))];
		out = maths::NearBonePoint(bone, effect.RandomInBall(), body->size);
		return Result::Point;
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

using WispData = creature_spell::WispData;

/// Where the creature stood and the caster's hand was as the wisps began, kept on the collection (the original keeps
/// both points whole)
struct WispsBegan
{
	glm::vec3 creature {0.0f};
	glm::vec3 hand {0.0f};
};

/// CreatureSpellGeneric. Flags 2 without 4, as the first rule. It lets go of its collection without a creator,
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
	[[nodiscard]] RuleData SavedData() const override { return RuleData::CreatureSpellWisps; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		// its data on the collection is made once it has a creator
		auto& began = CollectionDataOf<WispsBegan>(collection, this);
		collection.NoteDataMade(this);
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
		// Where the creature stood and the caster's hand was as the wisps began
		if (slot.first)
		{
			slot.first = false;
			began = {.creature = body->origin, .hand = hand};
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
			atom.NoteDataMade(this);
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
				const glm::vec2 then(began.creature.x - began.hand.x, began.creature.z - began.hand.z);
				const glm::vec2 now(body->origin.x - hand.x, body->origin.z - hand.z);
				if (maths::SwungTooFar(then, now, *limit))
				{
					sink->CloseDown();
				}
			}
		}
		const auto box = maths::WispBoxOf(body->bones);
		for (auto& atom : collection.atoms)
		{
			// a wisp with none gets the rule's data here
			auto& wisp = AtomDataOf<WispData>(*atom, this);
			atom->NoteDataMade(this);
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

/// CreatureSpellItch. Flags 6: a creator. Its flies are all made on the first step; it lets go of its collection
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
	[[nodiscard]] RuleData SavedData() const override { return RuleData::CreatureSpellItch; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		// its data on the collection, its first update, is made once it has a creator
		collection.NoteDataMade(this);
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
using AtomsMade = creature_spell::AtomsMade;

/// Whether another atom is due: the count made, a whole number, below the fraction due (or that fraction not a number)
bool StillDue(const AtomsMade& data)
{
	return !(static_cast<double>(data.made) >= data.due);
}

/// CreatureSpellCompassion, the hearts off a creature made nice. Flags 2 without 4, as the first rule. It lets go of
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
	[[nodiscard]] RuleData SavedData() const override { return RuleData::CreatureSpellCompassion; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		// its count is made once it has a creator
		auto& data = CollectionDataOf<AtomsMade>(collection, this);
		collection.NoteDataMade(this);
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value() || !maths::HasAtomRate(maxAtoms, dieAge))
		{
			return false;
		}
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
					// (pending) A parent whose point is not ported (an animated model's bones): the heart just made goes
					// again
					effect.NotifyAtomRemoved(*collection.atoms.back());
					collection.atoms.pop_back();
					// the game's logger is not there when a test runs on its own
					if (auto logger = spdlog::get("game");
					    logger != nullptr && log_once::First("game/psys-compassion-hearts-from-body"))
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

/// CreatureSpellFreeze, the steam off a frozen creature. Flags 2 without 4, as the first rule. It lets go of its
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
	[[nodiscard]] RuleData SavedData() const override { return RuleData::CreatureSpellFreeze; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* atomCreator = effect.FindCreator(creator);
		if (atomCreator == nullptr)
		{
			return false;
		}
		// its count is made once it has a creator
		auto& data = CollectionDataOf<AtomsMade>(collection, this);
		collection.NoteDataMade(this);
		const auto creature = CreatureOf(collection);
		const auto body = creature.has_value() ? Bodies().CreatureBody(*creature) : std::nullopt;
		if (!body.has_value())
		{
			return false;
		}
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

std::optional<creature_spell::WispsData> creature_spell::WispsOf(const Collection& collection, const Modifier& rule)
{
	if (dynamic_cast<const CreatureSpellGeneric*>(&rule) == nullptr)
	{
		return std::nullopt;
	}
	const auto found = collection.modifierData.find(&rule);
	const auto slot = std::ranges::find(collection.modifiers, &rule, &Collection::Slot::modifier);
	if (found == collection.modifierData.end() || slot == collection.modifiers.end())
	{
		return std::nullopt;
	}
	const auto& began = *static_cast<const WispsBegan*>(found->second.get());
	return WispsData {.creature = began.creature,
	                  .hand = began.hand,
	                  .first = slot->first,
	                  .due = slot->state.x,
	                  .made = static_cast<int>(slot->state.y)};
}

const creature_spell::WispData* creature_spell::WispOf(const Atom& atom, const Modifier& rule)
{
	if (dynamic_cast<const CreatureSpellGeneric*>(&rule) == nullptr)
	{
		return nullptr;
	}
	const auto found = atom.modifierData.find(&rule);
	return found != atom.modifierData.end() ? static_cast<const WispData*>(found->second.get()) : nullptr;
}

bool creature_spell::RestoreWisps(Collection& collection, const Modifier& rule, const WispsData& data)
{
	const auto slot = std::ranges::find(collection.modifiers, &rule, &Collection::Slot::modifier);
	if (dynamic_cast<const CreatureSpellGeneric*>(&rule) == nullptr || slot == collection.modifiers.end())
	{
		return false;
	}
	collection.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<WispsBegan>(
	                                                    WispsBegan {.creature = data.creature, .hand = data.hand})));
	slot->first = data.first;
	slot->state.x = data.due;
	slot->state.y = static_cast<float>(data.made);
	collection.NoteDataMade(&rule);
	return true;
}

bool creature_spell::RestoreWisp(Atom& atom, const Modifier& rule, const WispData& data)
{
	if (dynamic_cast<const CreatureSpellGeneric*>(&rule) == nullptr)
	{
		return false;
	}
	atom.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<WispData>(data)));
	atom.NoteDataMade(&rule);
	return true;
}

namespace
{
const Collection::Slot* SlotOfRule(const Collection& collection, const Modifier& rule)
{
	const auto slot = std::ranges::find(collection.modifiers, &rule, &Collection::Slot::modifier);
	return slot != collection.modifiers.end() ? &*slot : nullptr;
}

Collection::Slot* SlotOfRule(Collection& collection, const Modifier& rule)
{
	const auto slot = std::ranges::find(collection.modifiers, &rule, &Collection::Slot::modifier);
	return slot != collection.modifiers.end() ? &*slot : nullptr;
}

bool CountsAtoms(const Modifier& rule)
{
	return dynamic_cast<const CreatureSpellCompassion*>(&rule) != nullptr ||
	       dynamic_cast<const CreatureSpellFreeze*>(&rule) != nullptr;
}
} // namespace

std::optional<creature_spell::CreatureRefData> creature_spell::CreatureRefOf(const Atom& atom)
{
	const auto* ref = dynamic_cast<const CreatureRef*>(atom.creator);
	return ref != nullptr ? std::optional {ref->data} : std::nullopt;
}

bool creature_spell::RestoreCreatureRef(Atom& atom, const CreatureRefData& data)
{
	if (atom.collection == nullptr)
	{
		return false;
	}
	const auto rule = std::ranges::find_if(atom.collection->modifiers, [](const Collection::Slot& slot) {
		return dynamic_cast<const CreatureSpell*>(slot.modifier) != nullptr;
	});
	if (rule == atom.collection->modifiers.end())
	{
		return false;
	}
	GiveCreatureRef(atom, *rule->modifier, data);
	return true;
}

std::optional<creature_spell::CreatureHeld> creature_spell::HeldOf(const Collection& collection, const Modifier& rule)
{
	const auto* slot = SlotOfRule(collection, rule);
	const auto found = collection.modifierData.find(&rule);
	if (dynamic_cast<const CreatureSpell*>(&rule) == nullptr || slot == nullptr || found == collection.modifierData.end())
	{
		return std::nullopt;
	}
	return CreatureHeld {.first = slot->first,
	                     .creature = SlotCreature(*slot),
	                     .validatedTurn = static_cast<const CreatureTurn*>(found->second.get())->validatedTurn};
}

std::optional<creature_spell::AtomsMade> creature_spell::AtomsMadeOf(const Collection& collection, const Modifier& rule)
{
	const auto found = collection.modifierData.find(&rule);
	if (!CountsAtoms(rule) || found == collection.modifierData.end())
	{
		return std::nullopt;
	}
	return *static_cast<const AtomsMade*>(found->second.get());
}

std::optional<bool> creature_spell::ItchFirstOf(const Collection& collection, const Modifier& rule)
{
	const auto* slot = SlotOfRule(collection, rule);
	if (dynamic_cast<const CreatureSpellItch*>(&rule) == nullptr || slot == nullptr)
	{
		return std::nullopt;
	}
	return slot->first;
}

bool creature_spell::RestoreHeld(Collection& collection, const Modifier& rule, const CreatureHeld& held)
{
	auto* slot = SlotOfRule(collection, rule);
	if (dynamic_cast<const CreatureSpell*>(&rule) == nullptr || slot == nullptr)
	{
		return false;
	}
	slot->first = held.first;
	slot->object = entt::to_integral(held.creature);
	collection.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<CreatureTurn>(
	                                                    CreatureTurn {.validatedTurn = held.validatedTurn})));
	collection.NoteDataMade(&rule);
	return true;
}

bool creature_spell::RestoreAtomsMade(Collection& collection, const Modifier& rule, const AtomsMade& made)
{
	if (!CountsAtoms(rule) || SlotOfRule(collection, rule) == nullptr)
	{
		return false;
	}
	collection.modifierData.insert_or_assign(&rule, std::static_pointer_cast<void>(std::make_shared<AtomsMade>(made)));
	collection.NoteDataMade(&rule);
	return true;
}

bool creature_spell::RestoreItchFirst(Collection& collection, const Modifier& rule, bool first)
{
	auto* slot = SlotOfRule(collection, rule);
	if (dynamic_cast<const CreatureSpellItch*>(&rule) == nullptr || slot == nullptr)
	{
		return false;
	}
	slot->first = first;
	collection.NoteDataMade(&rule);
	return true;
}

void openblack::particles::RegisterCreatureSpellRules()
{
	RegisterModifier("UR_CreatureSpell", MakeModifierOf<CreatureSpell>);
	RegisterModifier("UR_CreatureSpellGeneric", MakeModifierOf<CreatureSpellGeneric>);
	RegisterModifier("UR_CreatureSpellCompassion", MakeModifierOf<CreatureSpellCompassion>);
	RegisterModifier("UR_CreatureSpellItch", MakeModifierOf<CreatureSpellItch>);
	RegisterModifier("UR_CreatureSpellFreeze", MakeModifierOf<CreatureSpellFreeze>);
}
