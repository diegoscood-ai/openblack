/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// SimpleBeam: wiggling ribbons of light from the effect's origin to each object or point it is given. Each target
// the effect is given while it is not closing down becomes an atom of its own carrying a few beams, each a collection of
// ribbon joints. Every step each beam is laid afresh from the origin, wherever it is now, to its target, an object being
// followed for as long as it is available: a few key points along the way are pushed aside by noise drifting along the
// beam, most at its middle, and kept above the land, then the joints are spread evenly along a smooth curve through
// them, thickest at the middle. A chain's texture slides along it. Wiki: docs/wiki/physics/particle-rules.md#the-simple-beam.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "3D/LandMorph.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/BeamData.h"
#include "Particles/BeamMaths.h"
#include "Particles/Noise.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/PlasmaCommand.h"
#include "Particles/PlasmaData.h"
#include "Particles/Rules/SurfRevol.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// The bits of the particle allocator's fill, read as a float (-431602080)
constexpr uint32_t k_UnsetTarget = beam::k_Unset;

using BeamTarget = beam::Target;
using BeamNumber = beam::Ribbon;

/// An available object with a place
bool Available(entt::entity object)
{
	return object != entt::null && Locator::entitiesRegistry::has_value() && ecs::IsAvailable(object) &&
	       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object);
}

/// Where a beam on an object ends: half the object's height above its place. (pending) The game starts from the object's
/// map coordinates, and ends a beam on a creature at one of its bones
glm::vec3 BeamEnd(entt::entity object)
{
	auto end = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object).position;
	end.y = (ecs::object::GetHeight(object) * 0.5f) + end.y;
	return end;
}

/// SimpleBeam. Flag 2 without 4: it makes no atoms of its own accord, but the effect waits for its targets until it
/// closes down.
class SimpleBeam final: public Modifier
{
public:
	// The game makes the rule zeroed, so MaxJointsPerFork and MinHeight are 0 when a file does not give them (every file
	// does). The properties' ranges are only the editor's: the values are kept as the file gives them
	explicit SimpleBeam(const Object& object)
	    : creator(object.String("PCreator"))
	    , joints(object.Int("MaxJointsPerFork", 0))
	    , keyPoints(object.Int("NumSplinePoints", 5))
	    , beams(object.Int("NumBeams", 3))
	    , beamGroup(object.Int("BeamGroup", -1))
	    , wiggle(particles::maths::BeamWiggle {
	          .frequency = object.Float("WiggleFreq", 4.0f),
	          .speed = object.Float("WiggleSpeed", 1.0f),
	          .amount = object.Float("RandomFrac", 0.1f),
	          .minHeight = object.Float("MinHeight", 0.0f),
	      })
	    , speedV(object.Float("SpeedV", 1.0f))
	    , forkScaleMin(object.Float("ForkScaleMin", 1.0f))
	    , forkScaleMax(object.Float("ForkScaleMax", 1.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }
	[[nodiscard]] RuleData SavedData() const override { return RuleData::SimpleBeam; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// Where the beams start this step: the parent atom, or the effect's origin
		const auto origin = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		if (!effect.Closing())
		{
			while (effect.GetTargets().size() + effect.TargetPointCount() > 0)
			{
				Begin(effect, collection, origin);
			}
		}
		// The original's lists grow at their heads, so it walks the newest atom and the newest beam first
		const float magnitude = effect.GetMagnitude();
		for (auto& atom : std::ranges::reverse_view(collection.atoms))
		{
			const auto found = atom->modifierData.find(this);
			// (port guard) an atom without the beam's data crashes the game; only carriers are in this group
			if (found == atom->modifierData.end())
			{
				continue;
			}
			auto& data = *std::static_pointer_cast<BeamTarget>(found->second);
			Follow(data);
			data.origin = origin;
			for (auto& ribbon : std::ranges::reverse_view(atom->subCollections))
			{
				Lay(effect, *ribbon, data, magnitude);
			}
		}
		return true;
	}

private:
	/// A new atom for the next target, objects before points, carrying its beams and their joints
	void Begin(Effect& effect, Collection& collection, const glm::vec3& origin) const
	{
		// (port guard) no sub-collections for a negative NumBeams
		const std::vector<int> groups(static_cast<size_t>(std::max(0, beams)), beamGroup);
		// The atom has no creator: it is never drawn, it only carries the beams
		auto& atom = effect.NewAtom(collection, nullptr, groups);
		auto& data = AtomDataOf<BeamTarget>(atom, this);
		atom.NoteDataMade(this);
		data.origin = origin;
		if (!effect.GetTargets().empty())
		{
			// An object's end is set when it is followed, the same step. Until then it keeps what the game's particle
			// allocator fills every block with, 0xCDCDCDCD, which an object unavailable the step it is given keeps
			data.target = glm::vec3(std::bit_cast<float>(k_UnsetTarget));
			data.object = effect.TakeTarget();
			// the object's turn is stamped as it is taken
			if (data.object != entt::null)
			{
				data.validatedTurn = static_cast<uint32_t>(openblack::magic::CurrentTurn());
			}
		}
		else
		{
			effect.TakeTargetPoint(data.target);
		}
		atom.position = origin;
		// (port guard) the game calls through a missing creator and crashes; every file gives one. Here the joints have none
		const auto* jointCreator = effect.FindCreator(creator);
		const bool chain = jointCreator != nullptr && jointCreator->kind == Creator::Kind::Chain;
		int number = 0;
		for (auto& ribbon : std::ranges::reverse_view(atom.subCollections))
		{
			auto& kept = CollectionDataOf<BeamNumber>(*ribbon, this);
			kept.number = number++;
			kept.beams = beams;
			ribbon->NoteDataMade(this);
			if (chain)
			{
				ribbon->chainScrollRate = speedV;
			}
			for (int j = 0; j < joints; ++j)
			{
				effect.NewAtom(*ribbon, jointCreator, {});
			}
		}
	}

	/// An object's beam ends at its middle for as long as it is available, and stays where it last was after
	static void Follow(BeamTarget& data)
	{
		if (data.object == entt::null)
		{
			return;
		}
		if (Available(data.object))
		{
			// the turn it was found available is stamped
			data.validatedTurn = static_cast<uint32_t>(openblack::magic::CurrentTurn());
			data.target = BeamEnd(data.object);
		}
		else
		{
			data.object = entt::null;
		}
	}

	/// A beam laid from the origin to the target, its joints from the newest at the origin to the first made at the
	/// target
	void Lay(const Effect& effect, Collection& ribbon, const BeamTarget& data, float magnitude) const
	{
		auto& kept = CollectionDataOf<BeamNumber>(ribbon, this);
		const int number = kept.number;
		const auto keys =
		    particles::maths::BeamKeyPoints(data.origin, data.target, keyPoints, wiggle, effect.CollectionAge(ribbon), number,
		                                    noise::SignedValueNoise, land_morph::CurrentAltitude());
		// the beam keeps the key points its curve goes through, for a save
		kept.keys = keys;
		const auto laid =
		    particles::maths::BeamJoints(keys, ribbon.atoms.size(), magnitude * forkScaleMin, magnitude * forkScaleMax);
		for (size_t j = 0; j < laid.size(); ++j)
		{
			auto& joint = *ribbon.atoms[ribbon.atoms.size() - 1 - j];
			joint.position = laid[j].position;
			joint.ruleScale = laid[j].scale;
		}
	}

	std::string creator;
	int joints;
	int keyPoints;
	int beams;
	int beamGroup;
	particles::maths::BeamWiggle wiggle;
	/// How fast a chain's texture slides along it
	float speedV;
	float forkScaleMin;
	float forkScaleMax;
};
/// What a plasma's carrier atom keeps: the arc it was asked to fire
struct PlasmaArcData
{
	PlasmaCommand plasma;
};

/// Plasma. Flag 2 without 4, as SimpleBeam: it waits for its plasmas until the effect closes, and makes no atoms
/// of its own accord. Each plasma becomes a carrier atom of its creator with its beams, chains of joints laid along an
/// arc from the plasma's start to its end, faded in and out over the plasma's life, and gone a fifth of a second after
class Plasma final: public Modifier
{
public:
	explicit Plasma(const Object& object)
	    : creator(object.String("PCreator"))
	    , chainCreator(object.String("ChainCreator"))
	    , joints(object.Int("JointsPerArc", 19))
	    , keyPoints(object.Int("NumSplinePoints", 5))
	    , beams(object.Int("NumBeams", 3))
	    , beamGroup(object.Int("BeamGroup", -1))
	    , maxAlpha(object.Int("MaxAlpha", 255))
	    , shape(maths::PlasmaShape {
	          .scaleTangents = object.Float("ScaleTangents", 1.0f),
	          .randomTangents = object.Float("RandomTangents", 0.2f),
	          .frequency = object.Float("WiggleFreq", 4.0f),
	          .speed = object.Float("WiggleSpeed", 1.0f),
	          .amount = object.Float("RandomFrac", 0.1f),
	      })
	    , speedV(object.Float("SpeedV", 1.0f))
	    , forkScaleMin(object.Float("ForkScaleMin", 1.0f))
	    , forkScaleMax(object.Float("ForkScaleMax", 1.0f))
	{
	}

	[[nodiscard]] bool KeepsAlive() const override { return true; }
	[[nodiscard]] RuleData SavedData() const override { return RuleData::Plasma; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// Without its creator the rule leaves the collection
		const auto* carrierCreator = effect.FindCreator(creator);
		if (carrierCreator == nullptr)
		{
			return false;
		}
		// Every plasma asked for, closing or not: a carrier atom each, with its beams and their joints
		while (const auto plasma = effect.TakePlasma())
		{
			Begin(effect, collection, *carrierCreator, *plasma);
		}
		// Every carrier, the newest first: its beams laid while it lives, then it goes
		for (size_t i = collection.atoms.size(); i-- > 0;)
		{
			auto& atom = *collection.atoms[i];
			const auto& data = AtomDataOf<PlasmaArcData>(atom, this).plasma;
			if (effect.AtomAge(atom) <= data.life + k_LingerSeconds)
			{
				for (auto& ribbon : std::ranges::reverse_view(atom.subCollections))
				{
					Lay(effect, *ribbon, data);
				}
			}
			else
			{
				effect.NotifyAtomRemoved(atom);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
			}
		}
		return true;
	}

private:
	/// A carrier is laid until a fifth of a second past its plasma's life
	static constexpr float k_LingerSeconds = 0.2f;

	/// A new carrier for the plasma, with NumBeams beams of BeamGroup, each a chain of JointsPerArc joints in the effect's
	/// player's colour (white with none, or the neutral one)
	void Begin(Effect& effect, Collection& collection, const Creator& carrierCreator, const PlasmaCommand& plasma) const
	{
		// (port guard) no sub-collections for a negative NumBeams
		const std::vector<int> groups(static_cast<size_t>(std::max(0, beams)), beamGroup);
		auto& atom = effect.NewAtom(collection, &carrierCreator, groups);
		AtomDataOf<PlasmaArcData>(atom, this).plasma = plasma;
		atom.NoteDataMade(this);
		const auto* jointCreator = effect.FindCreator(chainCreator);
		if (jointCreator == nullptr || jointCreator->kind != Creator::Kind::Chain)
		{
			return;
		}
		const int player = effect.GetPlayer();
		const uint32_t colour =
		    player < 0 || player == static_cast<int>(PlayerNames::NEUTRAL) ? 0xFFFFFFFFu : surf_revol::PlayerColour(player);
		int number = 0;
		for (auto& ribbon : std::ranges::reverse_view(atom.subCollections))
		{
			// its data, made as its chain is: its number and the count of its atom's beams
			auto& kept = CollectionDataOf<BeamNumber>(*ribbon, this);
			kept.number = number++;
			kept.beams = beams;
			ribbon->NoteDataMade(this);
			ribbon->chainScrollRate = plasma.speed * speedV;
			for (int j = 0; j < joints; ++j)
			{
				auto& joint = effect.NewAtom(*ribbon, jointCreator, {});
				// the player's colour under the joint's own alpha
				joint.colour[0] = static_cast<uint8_t>((colour >> 16) & 0xFFu);
				joint.colour[1] = static_cast<uint8_t>((colour >> 8) & 0xFFu);
				joint.colour[2] = static_cast<uint8_t>(colour & 0xFFu);
			}
		}
	}

	/// One beam of a plasma this step: its alpha by the beam's age, its arc with the two tangents each pushed by a random
	/// point of the unit ball (drawn whatever RandomTangents is), and its joints along it, thickest at the middle
	void Lay(Effect& effect, Collection& ribbon, const PlasmaCommand& plasma) const
	{
		const float age = effect.CollectionAge(ribbon);
		ribbon.alpha = static_cast<float>(maths::PlasmaAlpha(age, plasma.life, plasma.alpha, maxAlpha));
		auto& kept = CollectionDataOf<BeamNumber>(ribbon, this);
		const int number = kept.number;
		const auto startPush = effect.RandomInBall();
		const auto endPush = effect.RandomInBall();
		const maths::PlasmaArc arc {
		    .start = plasma.start, .end = plasma.end, .startTangent = plasma.startTangent, .endTangent = plasma.endTangent};
		const auto keys =
		    maths::PlasmaKeyPoints(arc, startPush, endPush, shape, keyPoints,
		                           maths::PlasmaDrift(age, plasma.speed, shape.speed), number, noise::SignedValueNoise);
		// the beam keeps the key points its curve goes through, for a save
		kept.keys = keys;
		const auto laid = maths::BeamJoints(keys, ribbon.atoms.size(), forkScaleMin, forkScaleMax);
		for (size_t j = 0; j < laid.size(); ++j)
		{
			auto& joint = *ribbon.atoms[ribbon.atoms.size() - 1 - j];
			joint.position = laid[j].position;
			joint.ruleScale = laid[j].scale;
		}
	}

	std::string creator;
	std::string chainCreator;
	int joints;
	int keyPoints;
	int beams;
	int beamGroup;
	int32_t maxAlpha;
	maths::PlasmaShape shape;
	/// How fast a chain's texture slides along it, times the plasma's own speed
	float speedV;
	float forkScaleMin;
	float forkScaleMax;
};
} // namespace

const PlasmaCommand* plasma::DataOf(const Atom& atom, const Modifier& rule)
{
	if (dynamic_cast<const Plasma*>(&rule) == nullptr)
	{
		return nullptr;
	}
	const auto found = atom.modifierData.find(&rule);
	return found != atom.modifierData.end() ? &static_cast<const PlasmaArcData*>(found->second.get())->plasma : nullptr;
}

bool plasma::RestoreData(Atom& atom, const Modifier& rule, const PlasmaCommand& arc)
{
	if (dynamic_cast<const Plasma*>(&rule) == nullptr)
	{
		return false;
	}
	atom.modifierData.insert_or_assign(&rule,
	                                   std::static_pointer_cast<void>(std::make_shared<PlasmaArcData>(PlasmaArcData {arc})));
	atom.NoteDataMade(&rule);
	return true;
}

void openblack::particles::RegisterBeamRules()
{
	RegisterModifier("UR_SimpleBeam", MakeModifierOf<SimpleBeam>);
	RegisterModifier("UR_Plasma", MakeModifierOf<Plasma>);
}
