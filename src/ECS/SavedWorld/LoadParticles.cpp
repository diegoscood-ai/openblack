/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadParticles.h"

#include <cstring>

#include <bit>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "Audio/Services/SpellSounds.h"
#include "ECS/SavedWorld/ParticleCodecs.h"
#include "Particles/BeamData.h"
#include "Particles/CreatureSpellData.h"
#include "Particles/CreatureSpellWispData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/ParticleTypes.h"
#include "Particles/PlasmaData.h"
#include "Particles/Rules/Fireball.h"
#include "Particles/Rules/Lightning.h"
#include "Particles/Rules/ManaPath.h"
#include "Particles/Rules/VortexAttract.h"
#include "Particles/SphereAnglesData.h"
#include "Particles/VolumeGlowData.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::saved_world_write;

namespace
{
const gamesave::SavedRecord* RecordIn(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*world.objects[number - 1];
}

/// Every record a part names, and theirs, copied into the plan by its number
void AddParts(const gamesave::WorldRead& world, gamesave::ObjectNumber number, saved_particles::PlannedEffect& planned)
{
	if (number == 0 || planned.parts.contains(number))
	{
		return;
	}
	const auto* record = RecordIn(world, number);
	if (record == nullptr)
	{
		return;
	}
	planned.parts.emplace(number, *record);
	const auto addAll = [&world, &planned](const gamesave::RefList& list) {
		for (const auto part : list.numbers)
		{
			AddParts(world, part, planned);
		}
	};
	if (const auto* collection = std::get_if<gamesave::AtomCollectionRecord>(record); collection != nullptr)
	{
		addAll(collection->systems);
		addAll(collection->secondSystems);
		AddParts(world, collection->third.number, planned);
	}
	else if (const auto* atom = std::get_if<gamesave::AtomCoreRecord>(record); atom != nullptr)
	{
		addAll(atom->atoms);
		addAll(atom->modifiers);
		addAll(atom->objects);
		AddParts(world, atom->others[1].number, planned);
		AddParts(world, atom->others[2].number, planned);
	}
	else if (const auto* joint = std::get_if<gamesave::ParticleChainJointRecord>(record); joint != nullptr)
	{
		AddParts(world, joint->chain.number, planned);
	}
	else if (const auto* data = std::get_if<gamesave::RuleDataRecord>(record); data != nullptr)
	{
		// what a rule's data names, a cloud gather's lightning collection among them, which may hang from no atom
		for (const auto reference : data->references)
		{
			AddParts(world, reference, planned);
		}
	}
}

/// The class of a record of a rule's data: a generic record's own
template <typename Data>
uint32_t RuleDataClassOf([[maybe_unused]] const Data& data)
{
	if constexpr (std::is_same_v<Data, gamesave::RuleDataRecord>)
	{
		return data.type;
	}
	else
	{
		return Data::k_Type;
	}
}

/// The record of a part, of the class asked for; none for another
template <typename Record>
const Record* PartOf(const saved_particles::PlannedEffect& planned, gamesave::ObjectNumber number)
{
	const auto found = planned.parts.find(number);
	return found != planned.parts.end() ? std::get_if<Record>(&found->second) : nullptr;
}

/// Makes the effect's parts again, collection by collection
class Restorer
{
public:
	Restorer(const saved_particles::PlannedEffect& planned, particles::Effect& effect,
	         const saved_particles::EffectLinks& links)
	    : _planned(planned)
	    , _effect(effect)
	    , _links(links)
	{
	}

	/// The collection of the number under the atom (none for a root); none, with the reason, when it is not read
	std::unique_ptr<particles::Collection> MakeCollection(gamesave::ObjectNumber number, particles::Atom* parent)
	{
		const auto* record = PartOf<gamesave::AtomCollectionRecord>(_planned, number);
		if (record == nullptr)
		{
			Fail("collection " + std::to_string(number) + " is not read");
			return nullptr;
		}
		auto collection = _effect.NewCollection(static_cast<int>(record->word), parent);
		_collections.insert_or_assign(number, collection.get());
		particle_codecs::ApplyCollection(*record, *collection);
		// its chain, with its creator and its joints as the last frame's draw left them
		if (const auto chainNumber = record->third.number; chainNumber != 0)
		{
			const auto* chain = PartOf<gamesave::ChainRecord>(_planned, chainNumber);
			if (chain == nullptr)
			{
				Fail("the chain " + std::to_string(chainNumber) + " of collection " + std::to_string(number) + " is not read");
				return nullptr;
			}
			collection->chainCreator = _effect.CreatorAtFileIndex(chain->persistent.second);
			particle_codecs::ApplyChain(*chain, *collection);
		}
		// its rules' data, the newest first in the file
		for (const auto data : record->systems.numbers | std::views::reverse)
		{
			RuleData(data, *collection);
		}
		// its atoms, the newest first in the file
		for (const auto atom : record->secondSystems.numbers | std::views::reverse)
		{
			auto made = MakeAtom(atom, *collection);
			if (made == nullptr)
			{
				return nullptr;
			}
			collection->atoms.push_back(std::move(made));
		}
		return collection;
	}

	[[nodiscard]] const std::string& Why() const { return _why; }

	/// Once every collection and atom of the effect is made: the rules' data that name others of them (a cloud gather's
	/// lightning collection, made here when it hangs from no cloud, and that cloud); false, with the reason, when one
	/// cannot be put back
	bool Finish()
	{
		// by place: a collection made here may hold a gather of its own
		for (size_t i = 0; i < _gathers.size(); ++i)
		{
			const auto gather = _gathers[i];
			const auto found = _collections.find(gather.lightning);
			auto* lightning = found != _collections.end() ? found->second : nullptr;
			std::unique_ptr<particles::Collection> detached;
			if (lightning == nullptr && gather.lightning != 0)
			{
				detached = MakeCollection(gather.lightning, nullptr);
				if (detached == nullptr)
				{
					return false;
				}
			}
			const auto cloud = _atoms.find(gather.cloud);
			if (!particles::storm::RestoreCloudGather(*gather.collection, *gather.rule, gather.values, lightning,
			                                          cloud != _atoms.end() ? cloud->second : nullptr, std::move(detached)))
			{
				Fail("the cloud gather's data of rule " + std::to_string(gather.rule->fileIndex) + " is of no cloud gather");
				return false;
			}
		}
		_gathers.clear();
		// the lightnings' data, once their atoms and forks are made
		for (const auto& bolt : _bolts)
		{
			auto saved = particle_codecs::LightningOf(bolt.record);
			for (size_t i = 0; i < saved.targets.size(); ++i)
			{
				const auto object = bolt.record.targets.elements[i].object.number;
				saved.targets[i].object = object != 0 && _links.entityOf ? _links.entityOf(object) : entt::null;
			}
			const auto collectionOf = [this](gamesave::ObjectNumber number) -> const particles::Collection* {
				const auto found = _collections.find(number);
				return found != _collections.end() ? found->second : nullptr;
			};
			saved.pickedFork = collectionOf(bolt.record.others[0].number);
			for (const auto& held : bolt.record.held.elements)
			{
				saved.forks.emplace_back(collectionOf(held.collection.number), held.owned != 0);
			}
			if (!particles::lightning::RestoreBolt(
			        *bolt.collection, *bolt.rule, saved,
			        {.number = bolt.number, .link = bolt.record.links[0].number, .linkedBy = bolt.record.links[1].number}))
			{
				Fail("the lightning's data names no lightning " + std::to_string(bolt.record.persistent.second));
				return false;
			}
		}
		_bolts.clear();
		return _why.empty();
	}

private:
	void Fail(std::string why)
	{
		if (_why.empty())
		{
			_why = std::move(why);
		}
	}

	/// The slot of the collection's rule at a place in the file
	static particles::Collection::Slot* SlotOf(particles::Collection& collection, uint32_t fileIndex)
	{
		const auto found = std::ranges::find_if(collection.modifiers, [fileIndex](const particles::Collection::Slot& slot) {
			return slot.modifier != nullptr && slot.modifier->fileIndex == fileIndex;
		});
		return found != collection.modifiers.end() ? &*found : nullptr;
	}

	/// A rule's data on the collection put back on its rule's slot, noted as made
	void RuleData(gamesave::ObjectNumber number, particles::Collection& collection)
	{
		const auto found = _planned.parts.find(number);
		if (found == _planned.parts.end())
		{
			Fail("rule data " + std::to_string(number) + " is not read");
			return;
		}
		std::visit(
		    [this, &collection, number](const auto& data) {
			    using Data = std::decay_t<decltype(data)>;
			    if constexpr (std::is_same_v<Data, gamesave::BeamSubCollectionRecord> ||
			                  std::is_same_v<Data, gamesave::PlasmaSubCollectionRecord>)
			    {
				    // a beam's data is its parent's rule's, kept on the beam's collection
				    const auto* rule = _effect.ModifierAtFileIndex(data.persistent.second);
				    if (rule == nullptr)
				    {
					    Fail("the beam's data names no rule " + std::to_string(data.persistent.second));
					    return;
				    }
				    collection.modifierData[rule] =
				        std::make_shared<particles::beam::Ribbon>(particle_codecs::BeamRibbonOf(data));
				    collection.NoteDataMade(rule);
			    }
			    else if constexpr (requires { data.persistent; })
			    {
				    if constexpr (std::is_same_v<Data, gamesave::RuleDataRecord>)
				    {
					    // the tornado's data on its debris and flying collections, which have not the tornado among
					    // their rules
					    if (const auto emitter = particle_codecs::TornadoEmitterOf(data); emitter.has_value())
					    {
						    const auto* rule = _effect.ModifierAtFileIndex(data.persistent.second);
						    if (rule == nullptr || !particles::storm::RestoreTornadoEmitter(collection, *rule, *emitter))
						    {
							    Fail("the tornado's emitter data names no tornado " + std::to_string(data.persistent.second));
						    }
						    return;
					    }
				    }
				    auto* slot = SlotOf(collection, data.persistent.second);
				    if (slot == nullptr)
				    {
					    Fail("rule data of rule " + std::to_string(data.persistent.second) + " has no rule on its collection");
					    return;
				    }
				    if constexpr (std::is_same_v<Data, gamesave::EmitterCollectionRecord>)
				    {
					    particle_codecs::ApplyEmitterData(data, *slot);
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::HandSprinkleCollectionRecord>)
				    {
					    particle_codecs::ApplyHandSprinkleData(data, *slot);
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::WillowWispCollectionRecord>)
				    {
					    particle_codecs::ApplyWillowWispData(data, *slot);
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::LightningCollectionRecord>)
				    {
					    // made once the collection's atoms and forks are (Finish)
					    _bolts.push_back({.number = number, .collection = &collection, .rule = slot->modifier, .record = data});
					    return;
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::ExplosionCollectionRecord>)
				    {
					    // its targets by the load's entities; its beam's spot visual once the containers are made
					    auto values = particle_codecs::ExplosionDataOf(data);
					    for (size_t i = 0; i < values.targets.size(); ++i)
					    {
						    const auto object = data.targets.elements[i].object.number;
						    values.targets[i].object = object != 0 && _links.entityOf ? _links.entityOf(object) : entt::null;
					    }
					    if (!particles::explosion::RestoreData(collection, *slot->modifier, values))
					    {
						    Fail("the explosion's data names no explosion " + std::to_string(data.persistent.second));
						    return;
					    }
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::TargetAtomsCollectionRecord>)
				    {
					    particle_codecs::ApplyTargetAtomsData(data, *slot);
					    const auto target = _links.entityOf ? _links.entityOf(data.target.number) : entt::null;
					    if (target != entt::null)
					    {
						    slot->state.x = std::bit_cast<float>(static_cast<uint32_t>(target));
						    slot->state.y = 1.0f;
					    }
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::RuleDataRecord>)
				    {
					    // the rules' data of a fixed row: the creature spells' (the wisps' where the creature and the hand
					    // were and the wisps due and made, the first rule's creature by the load's entities and its turn,
					    // the hearts' and the steam's counts, the itch's first update)
					    if (!RestoreCollectionRuleData(data, *slot->modifier, collection))
					    {
						    Fail("rule data of class " + std::to_string(data.type) + " is not loaded from openblack yet");
						    return;
					    }
				    }
				    else if constexpr (std::is_same_v<Data, gamesave::TrailCollectionRecord>)
				    {
					    slot->first = data.byte != 0;
					    particles::fireball_rules::RestoreTrailPlaces(collection, *slot->modifier,
					                                                  particle_codecs::TrailPlacesOf(data));
				    }
				    else
				    {
					    Fail("rule data of class " + std::to_string(RuleDataClassOf(data)) +
					         " is not loaded from openblack yet");
					    return;
				    }
				    collection.NoteDataMade(slot->modifier);
			    }
			    else
			    {
				    Fail("rule data " + std::to_string(RuleDataClassOf(data)) + " of a collection is of no rule's class");
			    }
		    },
		    found->second);
	}

	/// A rule's data of a fixed row put back on its collection, noted as made; false for a class or a rule not loaded
	/// from openblack yet
	[[nodiscard]] bool RestoreCollectionRuleData(const gamesave::RuleDataRecord& record, const particles::Modifier& rule,
	                                             particles::Collection& collection)
	{
		// the cloud gather's, once every part it names is made
		if (const auto values = particle_codecs::CloudGatherOf(record); values.has_value())
		{
			_gathers.push_back({.collection = &collection,
			                    .rule = &rule,
			                    .values = *values,
			                    .lightning = record.Reference(particle_codecs::k_CloudGatherLightningField).value_or(0),
			                    .cloud = record.Reference(particle_codecs::k_CloudGatherCloudField).value_or(0)});
			return true;
		}
		namespace creature_spell = particles::creature_spell;
		if (const auto wisps = particle_codecs::CreatureSpellWispsOf(record); wisps.has_value())
		{
			return creature_spell::RestoreWisps(collection, rule, *wisps);
		}
		if (auto held = particle_codecs::CreatureSpellOf(record); held.has_value())
		{
			const auto creature = record.Reference(particle_codecs::k_CreatureSpellCreatureField).value_or(0);
			held->creature = _links.entityOf ? _links.entityOf(creature) : entt::null;
			return creature_spell::RestoreHeld(collection, rule, *held);
		}
		if (const auto made = particle_codecs::CreatureSpellAtomsMadeOf(record); made.has_value())
		{
			return creature_spell::RestoreAtomsMade(collection, rule, *made);
		}
		if (const auto first = particle_codecs::CreatureSpellItchOf(record); first.has_value())
		{
			return creature_spell::RestoreItchFirst(collection, rule, *first);
		}
		if (const auto velocity = particle_codecs::FlockingOf(record); velocity.has_value())
		{
			return particles::flocking::RestoreFlockVelocity(collection, rule, *velocity);
		}
		if (const auto first = particle_codecs::CloudMoverOf(record); first.has_value())
		{
			return particles::storm::RestoreCloudMoverFirst(collection, rule, *first);
		}
		if (const auto tornado = particle_codecs::TornadoOf(record); tornado.has_value())
		{
			return particles::storm::RestoreTornado(collection, rule, *tornado);
		}
		if (const auto state = particle_codecs::LightningSpriteOf(record); state.has_value())
		{
			return particles::storm_flash::RestoreState(collection, rule, *state);
		}
		return false;
	}

	/// A rule's data of a fixed row put back on the atom, noted as made; false for a class or a rule not loaded from
	/// openblack yet
	[[nodiscard]] bool RestoreAtomRuleData(const gamesave::RuleDataRecord& record, const particles::Modifier& rule,
	                                       particles::Atom& atom) const
	{
		if (const auto values = particle_codecs::ManaPathDataOf(record); values.has_value())
		{
			return particles::mana_path::RestoreData(atom, rule, *values);
		}
		if (const auto flying = particle_codecs::TornadoFlyingOf(record); flying.has_value())
		{
			return particles::storm::RestoreTornadoFlying(atom, rule, *flying);
		}
		if (const auto removed = particle_codecs::SoundRemovalOf(record); removed.has_value())
		{
			return particles::sound_rules::RestoreSoundRemoved(atom, rule, *removed);
		}
		if (const auto added = particle_codecs::SubCollectionsOf(record); added.has_value())
		{
			return particles::fireball_rules::RestoreSubCollectionsAdded(atom, rule, *added);
		}
		if (auto values = particle_codecs::FireBallDataOf(record); values.has_value())
		{
			const auto ball = record.Reference(particle_codecs::k_FireBallBallField).value_or(0);
			values->ball = ball != 0 && _links.entityOf ? _links.entityOf(ball) : entt::null;
			return particles::fireball_rules::RestoreBallData(atom, rule, *values);
		}
		if (auto values = particle_codecs::VolumeGlowDataOf(record); values.has_value())
		{
			const auto object = record.Reference(particle_codecs::k_VolumeGlowObjectField).value_or(0);
			values->object = _links.entityOf ? _links.entityOf(object) : entt::null;
			return particles::volume_glow::RestoreData(atom, rule, *values);
		}
		if (const auto arc = particle_codecs::PlasmaDataOf(record); arc.has_value())
		{
			return particles::plasma::RestoreData(atom, rule, *arc);
		}
		if (const auto values = particle_codecs::BeliefSpriteDataOf(record); values.has_value())
		{
			return particles::belief_sprite::RestoreData(atom, rule, *values);
		}
		if (const auto values = particle_codecs::CreatureSpellWispOf(record); values.has_value())
		{
			return particles::creature_spell::RestoreWisp(atom, rule, *values);
		}
		if (const auto values = particle_codecs::OrientWithVelocityOf(record); values.has_value())
		{
			return particles::orient_velocity::RestoreData(atom, rule, *values);
		}
		if (const auto cloud = particle_codecs::CloudOf(record); cloud.has_value())
		{
			return particles::storm::RestoreCloud(atom, rule, *cloud);
		}
		if (const auto values = particle_codecs::SphereAnglesOf(record); values.has_value())
		{
			return record.type == particle_codecs::k_SphereTracerAtomDataType
			           ? particles::sphere_angles::RestoreSphereTracer(atom, rule, *values)
			           : particles::sphere_angles::RestoreForestPath(atom, rule, *values);
		}
		return false;
	}

	/// The atom of the number in the collection; none, with the reason, when it is not read
	std::unique_ptr<particles::Atom> MakeAtom(gamesave::ObjectNumber number, particles::Collection& collection)
	{
		const auto* record = PartOf<gamesave::AtomCoreRecord>(_planned, number);
		if (record == nullptr)
		{
			Fail("atom " + std::to_string(number) + " is not read");
			return nullptr;
		}
		auto atom = std::make_unique<particles::Atom>();
		_atoms.insert_or_assign(number, atom.get());
		atom->collection = &collection;
		particle_codecs::ApplyAtomCore(*record, *atom);
		// its particle's creator
		const auto particle = record->others[2].number;
		if (PartOf<gamesave::ParticlePointRecord>(_planned, particle) != nullptr)
		{
			atom->creator = _effect.PointCreatorOf(collection.group);
		}
		else if (const auto* sprite = PartOf<gamesave::ParticleSpriteRecord>(_planned, particle); sprite != nullptr)
		{
			atom->creator = _effect.CreatorAtFileIndex(sprite->persistent.second);
			// a stretched sprite draws the stretch of its last two steps, which it keeps
			if (atom->creator != nullptr && atom->creator->stretch != 1.0f && atom->steps >= 1)
			{
				atom->previous.stretch = std::bit_cast<float>(sprite->words[0]);
				atom->current.stretch = std::bit_cast<float>(sprite->words[1]);
			}
		}
		else if (const auto* mist = PartOf<gamesave::ParticleMistRecord>(_planned, particle); mist != nullptr)
		{
			atom->creator = _effect.CreatorAtFileIndex(mist->persistent.second);
			particle_codecs::ApplyMist(*mist, *atom);
		}
		else if (const auto* mesh = PartOf<gamesave::ParticleTexturedObjectRecord>(_planned, particle); mesh != nullptr)
		{
			atom->creator = _effect.CreatorAtFileIndex(mesh->persistent.second);
		}
		else if (const auto* joint = PartOf<gamesave::ParticleChainJointRecord>(_planned, particle); joint != nullptr)
		{
			// a chain joint: its chain's creator and its number in the chain
			const auto* chain = PartOf<gamesave::ChainRecord>(_planned, joint->chain.number);
			if (chain == nullptr)
			{
				Fail("the chain of the chain joint " + std::to_string(particle) + " is not read");
				return nullptr;
			}
			atom->creator = _effect.CreatorAtFileIndex(chain->persistent.second);
			atom->chainJoint = static_cast<int>(joint->words[0]);
		}
		else if (const auto* carried = PartOf<gamesave::ParticleGameObjectRecord>(_planned, particle); carried != nullptr)
		{
			// a thing a vortex carries: its thing (none once let go) and the player it was taken for, as its record
			// names it
			auto values = particle_codecs::CarriedObjectOf(*carried);
			values.object = _links.entityOf ? _links.entityOf(carried->objects[0].number) : entt::null;
			values.letGo = carried->objects[0].number == 0;
			values.player = _links.playerOf ? _links.playerOf(carried->objects[1].number) : -1;
			if (carried->objects[0].number != 0 && values.object == entt::null)
			{
				Fail("the thing " + std::to_string(carried->objects[0].number) + " the particle " + std::to_string(particle) +
				     " carries is not made");
				return nullptr;
			}
			particles::vortex_attract::RestoreCarrier(*atom, values);
		}
		else if (const auto* ref = PartOf<gamesave::ParticleCreatureRefRecord>(_planned, particle); ref != nullptr)
		{
			// a creature spell's first particle: its creature (none once it had gone), as its record names it
			auto values = particle_codecs::CreatureRefOf(*ref);
			values.creature = _links.entityOf ? _links.entityOf(ref->creature.number) : entt::null;
			if (ref->creature.number != 0 && values.creature == entt::null)
			{
				Fail("the creature " + std::to_string(ref->creature.number) + " the particle " + std::to_string(particle) +
				     " stands for is not made");
				return nullptr;
			}
			if (!particles::creature_spell::RestoreCreatureRef(*atom, values))
			{
				Fail("the creature particle " + std::to_string(particle) + " is not in a creature spell's first collection");
				return nullptr;
			}
		}
		else if (particle != 0)
		{
			Fail("the particle " + std::to_string(particle) + " of atom " + std::to_string(number) +
			     " is of a class not loaded from openblack yet");
			return nullptr;
		}
		// its draw offset: one that follows the hand, or a decaying one, made once for all the atoms that share it
		if (const auto offset = record->others[1].number; offset != 0)
		{
			if (const auto* hand = PartOf<gamesave::DrawOffsetHandRecord>(_planned, offset); hand != nullptr)
			{
				atom->drawOffset = particle_codecs::HandDrawOffsetOf(*hand);
			}
			else if (const auto* decay = PartOf<gamesave::DrawOffsetDecayRecord>(_planned, offset); decay != nullptr)
			{
				atom->decayingOffset = particle_codecs::DecayingDrawOffsetOf(*decay);
				auto [found, made] = _offsetIds.try_emplace(offset, 0U);
				if (made)
				{
					found->second = _effect.NewDrawOffsetId();
				}
				atom->decayingOffset->id = found->second;
			}
			else
			{
				Fail("the draw offset " + std::to_string(offset) + " of atom " + std::to_string(number) + " is not read");
				return nullptr;
			}
		}
		// its rules' data, the newest first in the file
		for (const auto data : record->modifiers.numbers | std::views::reverse)
		{
			// the simple beam's: its object, its turn and its two ends
			if (const auto* beam = PartOf<gamesave::BeamAtomDataRecord>(_planned, data); beam != nullptr)
			{
				const auto* rule = _effect.ModifierAtFileIndex(beam->persistent.second);
				if (rule == nullptr)
				{
					Fail("the beam's data " + std::to_string(data) + " of atom " + std::to_string(number) + " names no rule");
					return nullptr;
				}
				auto target = particle_codecs::BeamTargetOf(*beam);
				target.object = _links.entityOf ? _links.entityOf(beam->object.number) : entt::null;
				atom->modifierData[rule] = std::make_shared<particles::beam::Target>(target);
				atom->NoteDataMade(rule);
				continue;
			}
			// the vortex attract rule's: the thing's start height, its taken velocity, what the take found of it and its
			// out vortex
			if (const auto* attract = PartOf<gamesave::VortexAttractAtomDataRecord>(_planned, data); attract != nullptr)
			{
				const auto* rule = _effect.ModifierAtFileIndex(attract->persistent.second);
				if (rule == nullptr)
				{
					Fail("the vortex attract data " + std::to_string(data) + " of atom " + std::to_string(number) +
					     " names no rule");
					return nullptr;
				}
				auto values = particle_codecs::VortexAttractDataOf(*attract);
				values.outVortex = _links.entityOf ? _links.entityOf(attract->object.number) : entt::null;
				particles::vortex_attract::RestoreData(*atom, *rule, values);
				continue;
			}
			// the fade-out rule's: whether its condition has held, and the age, alpha and scale the atom started fading
			// from
			if (const auto* fade = PartOf<gamesave::FadeOutAtomDataRecord>(_planned, data); fade != nullptr)
			{
				const auto* rule = _effect.ModifierAtFileIndex(fade->persistent.second);
				if (rule == nullptr || !particles::fade_out::RestoreData(*atom, *rule, particle_codecs::FadeOutDataOf(*fade)))
				{
					Fail("the fade-out data " + std::to_string(data) + " of atom " + std::to_string(number) +
					     " names no fade-out rule");
					return nullptr;
				}
				continue;
			}
			// the rules' data of a fixed row: the mana path's on a spark (its path and its progress), the glowing shell's
			// (its object and the turn it was last found there), the plasma's on a carrier (its arc) and the belief
			// sprite's (its start and its alpha), the creature spell wisps' (its angles, their rates and its size)
			if (const auto* generic = PartOf<gamesave::RuleDataRecord>(_planned, data); generic != nullptr)
			{
				const auto* rule = _effect.ModifierAtFileIndex(generic->persistent.second);
				if (rule == nullptr || !RestoreAtomRuleData(*generic, *rule, *atom))
				{
					Fail("the rule data " + std::to_string(data) + " (" + gamesave::TypeName(generic->type) + ") of atom " +
					     std::to_string(number) + " is not loaded from openblack yet");
					return nullptr;
				}
				continue;
			}
			const auto* sound = PartOf<gamesave::SoundAtomDataRecord>(_planned, data);
			const auto* rule = sound != nullptr ? _effect.ModifierAtFileIndex(sound->persistent.second) : nullptr;
			if (rule == nullptr)
			{
				Fail("the rule data " + std::to_string(data) + " of atom " + std::to_string(number) +
				     " is not loaded from openblack yet");
				return nullptr;
			}
			particle_codecs::ApplySoundAtomData(*sound, *rule, *atom);
			atom->NoteDataMade(rule);
		}
		// its sounds, the newest first as it keeps them
		for (const auto soundNumber : record->objects.numbers)
		{
			if (const auto* sound = PartOf<gamesave::ParticleSoundRecord>(_planned, soundNumber); sound != nullptr)
			{
				audio::spell_sounds::RestoreSound(_effect, *atom, particle_codecs::SoundActionOf(sound->action),
				                                  std::bit_cast<float>(sound->secondWord), sound->byte != 0);
			}
		}
		// its sub-collections, the newest first in the file
		for (const auto sub : record->atoms.numbers | std::views::reverse)
		{
			auto made = MakeCollection(sub, atom.get());
			if (made == nullptr)
			{
				return nullptr;
			}
			atom->subCollections.push_back(std::move(made));
		}
		return atom;
	}

	const saved_particles::PlannedEffect& _planned;
	particles::Effect& _effect;
	const saved_particles::EffectLinks& _links;
	/// The collections and atoms made, by their numbers in the file
	std::unordered_map<gamesave::ObjectNumber, particles::Collection*> _collections;
	/// The lightnings' data, put back once the collections' atoms are made
	struct PendingBolt
	{
		gamesave::ObjectNumber number {0};
		particles::Collection* collection {nullptr};
		const particles::Modifier* rule {nullptr};
		gamesave::LightningCollectionRecord record;
	};
	std::vector<PendingBolt> _bolts;
	std::unordered_map<gamesave::ObjectNumber, particles::Atom*> _atoms;
	/// A cloud gather's data waiting for the parts it names
	struct PendingGather
	{
		particles::Collection* collection {nullptr};
		const particles::Modifier* rule {nullptr};
		particles::storm::CloudGatherValues values;
		gamesave::ObjectNumber lightning {0};
		gamesave::ObjectNumber cloud {0};
	};
	std::vector<PendingGather> _gathers;
	std::string _why;
	/// Each decaying draw offset made, by its number: the id its atoms share
	std::unordered_map<gamesave::ObjectNumber, uint32_t> _offsetIds;
};
} // namespace

std::optional<saved_particles::PlannedEffect> saved_particles::PlanEffect(const gamesave::WorldRead& world,
                                                                          gamesave::ObjectNumber interfaceNumber)
{
	const auto* face = RecordIn(world, interfaceNumber) != nullptr
	                       ? std::get_if<gamesave::ParticleInterfaceRecord>(RecordIn(world, interfaceNumber))
	                       : nullptr;
	const auto* manager = face != nullptr && RecordIn(world, face->system.number) != nullptr
	                          ? std::get_if<gamesave::ParticleManagerRecord>(RecordIn(world, face->system.number))
	                          : nullptr;
	if (manager == nullptr)
	{
		return std::nullopt;
	}
	PlannedEffect planned {.interfaceNumber = interfaceNumber, .managerNumber = face->system.number, .manager = *manager};
	for (const auto collection : manager->systems.numbers)
	{
		AddParts(world, collection, planned);
	}
	return planned;
}

std::unique_ptr<particles::Effect> saved_particles::RestoreEffect(const PlannedEffect& planned,
                                                                  std::shared_ptr<const particles::File> file,
                                                                  const EffectLinks& links, std::string& why)
{
	const auto& manager = planned.manager;
	if (file == nullptr)
	{
		why = "its file (particle type " + std::to_string(manager.persistent.first) + ") is not loaded";
		return nullptr;
	}
	const auto magnitude = std::bit_cast<float>(manager.words[1]);
	auto effect = std::make_unique<particles::Effect>(
	    file, glm::vec3(manager.point[0], manager.point[1], manager.point[2]), magnitude,
	    manager.bytes[0] != 0 ? game_random::particles::NetGameType::Synced : game_random::particles::NetGameType::Local);
	particles::Effect::SavedState state;
	state.origin = glm::vec3(manager.point[0], manager.point[1], manager.point[2]);
	state.magnitude = magnitude;
	state.age = manager.value;
	state.closeAge = std::bit_cast<float>(manager.words[2]);
	state.closing = (manager.secondWord & 1U) != 0;
	state.globalAlpha = static_cast<float>(manager.byte);
	state.direction = glm::vec3(manager.points[1][0], manager.points[1][1], manager.points[1][2]);
	state.info = particle_codecs::ProcessInfoOf(manager.processInfo);
	state.player = links.playerOf ? links.playerOf(manager.object.number) : planned.player;
	state.atomCount = static_cast<size_t>(manager.word);
	for (const auto& target : manager.targets.targets.elements)
	{
		const auto entity = links.entityOf ? links.entityOf(target.object.number) : entt::null;
		if (entity != entt::null)
		{
			state.targets.push_back(entity);
		}
	}
	Restorer restorer(planned, *effect, links);
	std::vector<std::unique_ptr<particles::Collection>> roots;
	// the manager's collections, the newest first in the file
	for (const auto collection : manager.systems.numbers | std::views::reverse)
	{
		auto made = restorer.MakeCollection(collection, nullptr);
		if (made == nullptr)
		{
			why = restorer.Why();
			return nullptr;
		}
		roots.push_back(std::move(made));
	}
	if (!restorer.Finish() || !restorer.Why().empty())
	{
		why = restorer.Why();
		return nullptr;
	}
	effect->Restore(state, std::move(roots));
	return effect;
}

std::vector<saved_particles::PlannedContainer>
saved_particles::PlanContainers(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list,
                                const std::function<int(gamesave::ObjectNumber)>& playerOf)
{
	std::vector<PlannedContainer> containers;
	for (const auto number : list)
	{
		const auto* record = RecordIn(world, number);
		const auto* container = record != nullptr ? std::get_if<gamesave::ParticleContainerRecord>(record) : nullptr;
		if (container == nullptr)
		{
			continue;
		}
		if (auto effect = PlanEffect(world, container->system.number); effect.has_value())
		{
			effect->player = playerOf ? playerOf(effect->manager.object.number) : -1;
			containers.push_back({.number = number, .record = *container, .effect = std::move(*effect)});
		}
	}
	return containers;
}

saved_particles::MadeContainers
saved_particles::MakeContainers(const std::vector<PlannedContainer>& containers, const EffectLinks& links,
                                const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                                const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep)
{
	MadeContainers made;
	// the oldest first, so that the newest ends at the head of the engine's containers, as the game's list holds them
	for (const auto& planned : containers | std::views::reverse)
	{
		const auto type = static_cast<ParticleType>(planned.effect.manager.persistent.first);
		const auto fileName = particles::ParticleTypeFile(type);
		const auto& record = planned.record;
		const auto owner = record.object.number != 0 && links.entityOf ? links.entityOf(record.object.number) : entt::null;
		if (record.object.number != 0 && owner == entt::null)
		{
			// The thing it hangs on was kept as read, not made: the container stays as read with it, as the file holds
			// the two together. Made, it would close at once with no thing to hang on, and the thing's kept record
			// would name a container gone
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger,
				                   "Load: particle container {} is kept as read: the object {} it hangs on was not made",
				                   planned.number, record.object.number);
			}
			continue;
		}
		std::string why;
		auto effect = fileName.empty()
		                  ? nullptr
		                  : RestoreEffect(planned.effect, particles::File::Load(std::string(fileName)), links, why);
		if (effect == nullptr)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Load: particle container {} is kept as read: {}", planned.number,
				                   fileName.empty() ? std::string("its particle type has no file") : why);
			}
			continue;
		}
		const auto id = particles::manager::Adopt(std::move(effect));
		const auto object =
		    particles::manager::RestoreContainer(id,
		                                         map_coords::MapCoords {.x = record.position.position.x,
		                                                                .z = record.position.position.z,
		                                                                .altitude = record.position.position.altitude},
		                                         owner, record.object.number != 0, static_cast<int>(record.word),
		                                         std::bit_cast<float>(record.lastWord), type, record.byte);
		if (object == entt::null)
		{
			continue;
		}
		made.emplace(planned.number, object);
		if (keepSource)
		{
			keepSource(planned.number, gamesave::ParticleContainerRecord::k_Type, id);
			keepSource(planned.effect.interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, id);
			keepSource(planned.effect.managerNumber, gamesave::ParticleManagerRecord::k_Type, id);
		}
		if (keep)
		{
			keep(planned.number, object, gamesave::ParticleContainerRecord::k_Type);
		}
	}
	return made;
}
