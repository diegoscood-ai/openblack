/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spot visuals' containers in a saved game's world file, each with the particle interface and the particle manager
// of its effect, and the managers' collections and atoms, in the original's order, written from the particle engine's
// containers and effects (docs/wiki/engine/saved-games.md#openblack, the particles). They are not entities: a
// container, its interface and its manager are named by the number of the container's effect, a collection and an atom
// by that number and their place in a walk of the effect.

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>

#include "Audio/Services/SpellSounds.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/ParticleCodecs.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Hand/HandMagicFX.h"
#include "Particles/BeamData.h"
#include "Particles/CreatureSpellData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/PlasmaData.h"
#include "Particles/Rules/Lightning.h"
#include "Particles/Rules/ManaPath.h"
#include "Particles/Rules/VortexAttract.h"
#include "Particles/SpellLink.h"
#include "Particles/SphereAnglesData.h"
#include "Particles/VolumeGlowData.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ParticleContainerRecord;
using gamesave::ParticleInterfaceRecord;
using gamesave::ParticleManagerRecord;

namespace
{
/// The manager's state bit of an effect closing down
constexpr uint32_t k_ClosingDown = 1;
/// The persistent index that names a particle file itself rather than one of its parts
constexpr uint32_t k_FileItself = 0xFFFFFFFF;
/// Below this length the effect's start velocity has no direction: it is taken as along x
constexpr double k_NoSpeed = 0.0001;

/// The particle engine's state, when there is one
particles::manager::State* EngineState()
{
	return Locator::particleSystem::has_value() ? &Locator::particleSystem::value().GetState() : nullptr;
}

/// The live container of the effect, when there is one
const particles::manager::Container* ContainerOf(uint64_t effect)
{
	auto* state = EngineState();
	if (state == nullptr)
	{
		return nullptr;
	}
	const auto found =
	    std::ranges::find(state->containers, static_cast<uint32_t>(effect), &particles::manager::Container::effect);
	return found != state->containers.end() ? &*found : nullptr;
}

/// The running effect of the number, when there is one
const particles::manager::Running* RunningOf(uint64_t effect)
{
	auto* state = EngineState();
	if (state == nullptr)
	{
		return nullptr;
	}
	const auto found = state->effects.find(static_cast<uint32_t>(effect));
	return found != state->effects.end() && found->second.effect != nullptr ? &found->second : nullptr;
}

/// An effect's collections and atoms, each collection before its atoms and each atom before its sub-collections; their
/// places in these lists name them in a save
struct EffectParts
{
	std::vector<const particles::Collection*> collections;
	std::vector<const particles::Atom*> atoms;
};

void AddParts(const particles::Collection& collection, EffectParts& parts)
{
	parts.collections.push_back(&collection);
	for (const auto& atom : collection.atoms)
	{
		parts.atoms.push_back(atom.get());
		for (const auto& sub : atom->subCollections)
		{
			AddParts(*sub, parts);
		}
	}
}

EffectParts PartsOf(const particles::Effect& effect)
{
	EffectParts parts;
	for (const auto& root : effect.GetRoots())
	{
		AddParts(*root, parts);
	}
	// then the lightning collections that hang from no cloud, which their cloud gathers' data own, and theirs
	for (size_t i = 0; i < parts.collections.size(); ++i)
	{
		for (const auto* rule : parts.collections[i]->dataMade)
		{
			if (const auto* detached = particles::storm::DetachedLightningOf(*parts.collections[i], *rule);
			    detached != nullptr && std::ranges::find(parts.collections, detached) == parts.collections.end())
			{
				AddParts(*detached, parts);
			}
		}
	}
	return parts;
}

/// The source id of an effect's part: the effect's number in the high 32 bits, its place in the low ones
uint64_t PartKey(uint64_t effect, size_t place)
{
	return (effect << 32U) | static_cast<uint32_t>(place);
}

/// A rule's data on a collection: the collection's id with the rule's place among its rules in the bits above its
/// place (an effect has far fewer than 2^24 collections)
constexpr uint32_t k_RuleShift = 24;
constexpr uint64_t k_PlaceMask = (uint64_t {1} << k_RuleShift) - 1;

uint64_t RuleDataKey(uint64_t collectionId, uint64_t rule)
{
	return (collectionId & ~uint64_t {0xFFFFFFFFU}) | (rule << k_RuleShift) | (collectionId & k_PlaceMask);
}

/// The collection's id and the rule's place of a rule's data id (the inverse of RuleDataKey)
std::pair<uint64_t, size_t> RuleDataOf(uint64_t id)
{
	const auto rule = static_cast<size_t>((id & 0xFFFFFFFFU) >> k_RuleShift);
	return {(id & ~uint64_t {0xFFFFFFFFU}) | (id & k_PlaceMask), rule};
}

/// A rule's place for its data on a part whose collection has not the rule among its own (the tornado's on its debris
/// and flying collections and on their atoms): past any place among a collection's own rules, by the rule's place among
/// the part's rules' data
constexpr uint64_t k_MadeRule = 128;

uint64_t MadeRuleIndex(const std::vector<const particles::Modifier*>& dataMade, const particles::Modifier* rule)
{
	return k_MadeRule + static_cast<uint64_t>(std::ranges::find(dataMade, rule) - dataMade.begin());
}

/// The rule of a place MadeRuleIndex gave; none for another place
const particles::Modifier* MadeRuleAt(const std::vector<const particles::Modifier*>& dataMade, size_t index)
{
	return index >= k_MadeRule && index - k_MadeRule < dataMade.size() ? dataMade[index - k_MadeRule] : nullptr;
}

/// The class a rule's data on an atom is written as; none for data not kept on atoms or not written from openblack yet
std::optional<uint32_t> AtomDataTypeOf(particles::RuleData saved)
{
	switch (saved)
	{
	case particles::RuleData::SoundAtom:
		return gamesave::SoundAtomDataRecord::k_Type;
	case particles::RuleData::SimpleBeam:
		return gamesave::BeamAtomDataRecord::k_Type;
	case particles::RuleData::VortexAttract:
		return gamesave::VortexAttractAtomDataRecord::k_Type;
	case particles::RuleData::ManaPath:
		return particle_codecs::k_ManaPathAtomDataType;
	case particles::RuleData::VolumeGlow:
		return particle_codecs::k_VolumeGlowAtomDataType;
	case particles::RuleData::Plasma:
		return particle_codecs::k_PlasmaAtomDataType;
	case particles::RuleData::BeliefSprite:
		return particle_codecs::k_BeliefSpriteAtomDataType;
	case particles::RuleData::CreatureSpellWisps:
		return particle_codecs::k_CreatureSpellWispDataType;
	case particles::RuleData::OrientSpriteWithVelocity:
		return particle_codecs::k_OrientWithVelocityAtomDataType;
	case particles::RuleData::CloudGather:
		return particle_codecs::k_CloudAtomDataType;
	case particles::RuleData::SphereSurfaceTracer:
		return particle_codecs::k_SphereTracerAtomDataType;
	case particles::RuleData::ForestPath:
		return particle_codecs::k_ForestPathAtomDataType;
	case particles::RuleData::Tornado:
		return particle_codecs::k_TornadoFlyingAtomDataType;
	case particles::RuleData::SoundRemoval:
		return particle_codecs::k_SoundRemovalAtomDataType;
	case particles::RuleData::SubCollectionsAdded:
		return particle_codecs::k_SubCollectionsAtomDataType;
	case particles::RuleData::FadeOut:
		return gamesave::FadeOutAtomDataRecord::k_Type;
	case particles::RuleData::FireBall:
		return particle_codecs::k_FireBallAtomDataType;
	case particles::RuleData::Explosion:
	case particles::RuleData::Lightning:
	case particles::RuleData::CreatureSpell:
	case particles::RuleData::CreatureSpellCompassion:
	case particles::RuleData::CreatureSpellFreeze:
	case particles::RuleData::CreatureSpellItch:
	case particles::RuleData::Flocking:
	case particles::RuleData::CloudMover:
	case particles::RuleData::LightningSprite:
	case particles::RuleData::None:
	case particles::RuleData::Emitter:
	case particles::RuleData::Trail:
	case particles::RuleData::HandSprinkle:
	case particles::RuleData::TargetAtoms:
	case particles::RuleData::WillowWisp:
		break;
	}
	return std::nullopt;
}

/// The number of a collection of the effect
gamesave::ObjectNumber CollectionNumber(WriteContext& context, uint64_t effect, const EffectParts& parts,
                                        const particles::Collection* collection)
{
	const auto found = std::ranges::find(parts.collections, collection);
	return found != parts.collections.end()
	           ? context.Reference(SourceKey {.type = gamesave::AtomCollectionRecord::k_Type,
	                                          .id = PartKey(effect, static_cast<size_t>(found - parts.collections.begin()))})
	           : 0;
}

/// The number of an atom of the effect
gamesave::ObjectNumber AtomNumber(WriteContext& context, uint64_t effect, const EffectParts& parts, const particles::Atom* atom)
{
	const auto found = std::ranges::find(parts.atoms, atom);
	return found != parts.atoms.end()
	           ? context.Reference(SourceKey {.type = gamesave::AtomCoreRecord::k_Type,
	                                          .id = PartKey(effect, static_cast<size_t>(found - parts.atoms.begin()))})
	           : 0;
}

/// The live containers' effects, the newest first, as the game's list of containers holds them
std::vector<uint64_t> ContainerKeys(const ecs::Registry& /*registry*/)
{
	std::vector<uint64_t> keys;
	if (auto* state = EngineState(); state != nullptr)
	{
		for (const auto& container : state->containers)
		{
			if (RunningOf(container.effect) != nullptr)
			{
				keys.push_back(container.effect);
			}
		}
	}
	return keys;
}

/// The spell whose effect it is, when one has it
entt::entity SpellOfEffect(const ecs::Registry& registry, uint64_t effect)
{
	for (const auto spell : magic::Spells())
	{
		const auto* made = registry.Valid(spell) ? registry.TryGet<ecs::components::Spell>(spell) : nullptr;
		if (made != nullptr && made->effect == effect)
		{
			return spell;
		}
	}
	return entt::null;
}

/// The particle type that names the effect's file in a save, as the original loads a file for each particle type: its
/// container's spot visual's, the type its spell's class plays, the physical shield's for a physical shield's effect, or
/// the mana path's for a worship site's mana path; none for a container whose spot visual's type openblack does not
/// know, or an effect of none of them
std::optional<uint32_t> FileIdOf(const ecs::Registry& registry, uint64_t effect)
{
	if (const auto* container = ContainerOf(effect); container != nullptr)
	{
		if (container->particleType == ParticleType::None)
		{
			return std::nullopt;
		}
		return static_cast<uint32_t>(container->particleType);
	}
	// the particles of the spell in the hand, by the file the hand's effects made them of
	if (const auto inHand = magic::hand_fx::InHandEffectOf(); inHand.has_value() && inHand->effect == effect)
	{
		if (inHand->type == ParticleType::None)
		{
			return std::nullopt;
		}
		return static_cast<uint32_t>(inHand->type);
	}
	if (const auto spell = SpellOfEffect(registry, effect); spell != entt::null)
	{
		const auto& made = registry.Get<ecs::components::Spell>(spell);
		return static_cast<uint32_t>(magic::OpsOf(made.spellClass).particleType(spell));
	}
	std::optional<uint32_t> shieldFile;
	registry.Each<const ecs::components::MagicShield>([&shieldFile, effect](const ecs::components::MagicShield& shield) {
		if (shield.fx != 0 && shield.fx == effect)
		{
			shieldFile = static_cast<uint32_t>(ParticleType::PhysicalShieldFx);
		}
	});
	if (shieldFile.has_value())
	{
		return shieldFile;
	}
	// a worship site's mana path, which the site makes for the mana path's particle type as its first mote of prayer
	// power flies
	std::optional<uint32_t> manaPathFile;
	registry.Each<const ecs::components::WorshipSite>([&manaPathFile, effect](const ecs::components::WorshipSite& site) {
		if (site.manaPathEffect != 0 && site.manaPathEffect == effect)
		{
			manaPathFile = static_cast<uint32_t>(ParticleType::ManaPath);
		}
	});
	if (manaPathFile.has_value())
	{
		return manaPathFile;
	}
	// a vortex's object mover, by the particle type of its vortex's row
	std::optional<uint32_t> moverFile;
	if (Locator::infoConstants::has_value())
	{
		registry.Each<const ecs::components::LandscapeVortex>(
		    [&moverFile, effect](const ecs::components::LandscapeVortex& vortex) {
			    if (vortex.objectMover != 0 && vortex.objectMover == effect)
			    {
				    moverFile = static_cast<uint32_t>(
				        Locator::infoConstants::value().vortex.at(static_cast<size_t>(vortex.type)).particleTypeObjectMover);
			    }
		    });
	}
	return moverFile;
}

/// A container of a spot visual: its place (the map coordinates its effect is kept at), whether its effect is drawn as
/// one, the thing it hangs on (while that is still there), its effect's interface, the turns it has left (-1 for
/// ever) and its strength (the value it gives its effect each turn)
std::optional<gamesave::SavedRecord> WriteContainer(WriteContext& context, uint64_t effect)
{
	const auto* container = ContainerOf(effect);
	const auto* running = RunningOf(effect);
	if (container == nullptr || running == nullptr)
	{
		context.Fail("the container of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	ParticleContainerRecord record;
	record.position.position = {.x = container->coords.x, .z = container->coords.z, .altitude = container->coords.altitude};
	record.byte = running->path == particles::DrawPath::Queued ? 1 : 0;
	const bool owned = container->hadOwner && container->owner != entt::null && context.GetRegistry().Valid(container->owner);
	record.object = {owned ? context.Reference(container->owner) : 0};
	record.system = {context.Reference(SourceKey {.type = ParticleInterfaceRecord::k_Type, .id = effect})};
	record.word = static_cast<uint32_t>(container->turns);
	record.lastWord = std::bit_cast<uint32_t>(container->strength);
	return record;
}

/// The particle interface of the effect: its manager
std::optional<gamesave::SavedRecord> WriteInterface(WriteContext& context, uint64_t effect)
{
	return ParticleInterfaceRecord {
	    .system = {context.Reference(SourceKey {.type = ParticleManagerRecord::k_Type, .id = effect})}};
}

/// The particle manager of the effect: its atoms' count, its player, its process info, its origin, alpha, age and
/// state (closing down), its start velocity with its direction and speed, its magnitude and the age it closed down at,
/// the spell whose effect it is, whether it is synced, its file (by its container's spot visual's or its spell's
/// particle type: a write without one fails) and the things it aims at. Its collections and their atoms, the word +0x78, the
/// second flag, the words of its targets and its target points are not written from openblack yet (empty, 0, the manager's
/// start value 1)
std::optional<gamesave::SavedRecord> WriteManager(WriteContext& context, uint64_t effect)
{
	const auto* running = RunningOf(effect);
	if (running == nullptr)
	{
		context.Fail("particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& made = *running->effect;
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (!file.has_value())
	{
		// the original names the file by the particle type of the container's spot visual or of its spell: an effect of
		// neither (a container made without the game's info, the debug hook's test effect) cannot be saved as the
		// original does
		context.Fail("the container of particle effect " + std::to_string(effect) + " (" + made.GetFile().name +
		             ") has no spot visual's particle type to name its file by");
		return std::nullopt;
	}
	ParticleManagerRecord record;
	record.word = static_cast<int32_t>(made.AtomCount());
	record.object = {PlayerObject(context, made.GetPlayer())};
	record.processInfo = ProcessInfoBytes(made.GetProcessInfo());
	const auto origin = made.GetOrigin();
	record.point = {origin.x, origin.y, origin.z};
	record.byte = static_cast<uint8_t>(std::clamp(made.GetGlobalAlpha(), 0.0f, 255.0f));
	record.value = made.GetAge();
	record.secondWord = made.Closing() ? k_ClosingDown : 0;
	// the start velocity, its direction (along x when it has none) and its speed, as the effect's start works them out
	const auto velocity = made.GetDirection();
	const auto speed = std::sqrt(static_cast<double>(velocity.x) * velocity.x + static_cast<double>(velocity.y) * velocity.y +
	                             static_cast<double>(velocity.z) * velocity.z);
	record.points[1] = {velocity.x, velocity.y, velocity.z};
	if (speed > k_NoSpeed)
	{
		const auto over = static_cast<float>(1.0 / speed);
		record.points[2] = {velocity.x * over, velocity.y * over, velocity.z * over};
		record.words[0] = std::bit_cast<uint32_t>(static_cast<float>(speed));
	}
	else
	{
		record.points[2] = {1.0f, 0.0f, 0.0f};
		record.words[0] = 0;
	}
	record.words[1] = std::bit_cast<uint32_t>(made.GetMagnitude());
	record.words[2] = std::bit_cast<uint32_t>(made.GetCloseAge());
	// the spell whose effect it is; none for a container's
	record.other = {context.Reference(SpellOfEffect(context.GetRegistry(), effect))};
	record.bytes = {static_cast<uint8_t>(made.NetType() == game_random::particles::NetGameType::Synced ? 1 : 0), 1};
	record.persistent = {.first = *file, .second = k_FileItself};
	// a loaded effect keeps what its manager's record holds that openblack does not: the point +0x78, the byte +0xAD,
	// its targets' points and word
	if (auto kept = context.KeptRecordOf(SourceKey {.type = ParticleManagerRecord::k_Type, .id = effect}); kept.has_value())
	{
		if (const auto* loaded = std::get_if<ParticleManagerRecord>(&*kept); loaded != nullptr)
		{
			record.points[0] = loaded->points[0];
			record.bytes[1] = loaded->bytes[1];
			record.targets.points = loaded->targets.points;
			record.targets.word = loaded->targets.word;
		}
	}
	const auto parts = PartsOf(made);
	for (const auto& root : made.GetRoots() | std::views::reverse)
	{
		record.systems.numbers.push_back(CollectionNumber(context, effect, parts, root.get()));
	}
	for (const auto target : made.GetTargets())
	{
		if (target != entt::null && context.GetRegistry().Valid(target))
		{
			record.targets.targets.elements.push_back({.word = 0, .object = {context.Reference(target)}});
		}
	}
	return record;
}
/// A collection of an effect: its values (particle_codecs::CollectionValues), its manager, its parent atom and its
/// atoms, the newest first, and its chain when its atoms are chain joints. Its rules' data are those openblack writes
std::optional<gamesave::SavedRecord> WriteCollection(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running == nullptr || !file.has_value())
	{
		context.Fail("the particle effect of collection " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	const auto parts = PartsOf(*running->effect);
	const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
	if (place >= parts.collections.size())
	{
		context.Fail("collection " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& collection = *parts.collections[place];
	auto record = particle_codecs::CollectionValues(collection, *file, running->effect->PassesFrame(collection.group));
	record.object = {context.Reference(SourceKey {.type = ParticleManagerRecord::k_Type, .id = effect})};
	for (const auto* modifier : collection.dataMade | std::views::reverse)
	{
		// a beam's data is its parent's rule's, kept on the beam's collection
		if (modifier->SavedData() == particles::RuleData::SimpleBeam)
		{
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = gamesave::BeamSubCollectionRecord::k_Type, .id = id}));
			continue;
		}
		// and the plasma's, the same way under its own class
		if (modifier->SavedData() == particles::RuleData::Plasma)
		{
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = gamesave::PlasmaSubCollectionRecord::k_Type, .id = id}));
			continue;
		}
		const auto slot = std::ranges::find(collection.modifiers, modifier, &particles::Collection::Slot::modifier);
		if (slot == collection.modifiers.end())
		{
			// the tornado's data on its debris and flying collections, which have not the tornado among their rules
			if (modifier->SavedData() == particles::RuleData::Tornado)
			{
				const auto key = RuleDataKey(id, MadeRuleIndex(collection.dataMade, modifier));
				record.systems.numbers.push_back(
				    context.Reference(SourceKey {.type = particle_codecs::k_TornadoEmitterDataType, .id = key}));
			}
			continue;
		}
		const auto index = static_cast<uint64_t>(slot - collection.modifiers.begin());
		switch (modifier->SavedData())
		{
		case particles::RuleData::Emitter:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = gamesave::EmitterCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::Trail:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = gamesave::TrailCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::WillowWisp:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = gamesave::WillowWispCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::HandSprinkle:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = gamesave::HandSprinkleCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::TargetAtoms:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = gamesave::TargetAtomsCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CreatureSpellWisps:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = particle_codecs::k_CreatureSpellWispsDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::Explosion:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = gamesave::ExplosionCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::Lightning:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = gamesave::LightningCollectionRecord::k_Type, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CreatureSpell:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = particle_codecs::k_CreatureSpellDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CreatureSpellCompassion:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = particle_codecs::k_CreatureSpellCompassionDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CreatureSpellFreeze:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = particle_codecs::k_CreatureSpellFreezeDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CreatureSpellItch:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = particle_codecs::k_CreatureSpellItchDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::Flocking:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = particle_codecs::k_FlockingDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CloudMover:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = particle_codecs::k_CloudMoverDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::CloudGather:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = particle_codecs::k_CloudGatherDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::Tornado:
			record.systems.numbers.push_back(
			    context.Reference(SourceKey {.type = particle_codecs::k_TornadoDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::LightningSprite:
			record.systems.numbers.push_back(context.Reference(
			    SourceKey {.type = particle_codecs::k_LightningSpriteDataType, .id = RuleDataKey(id, index)}));
			break;
		case particles::RuleData::SoundAtom:
		case particles::RuleData::SoundRemoval:
		case particles::RuleData::SubCollectionsAdded:
		case particles::RuleData::FadeOut:
		case particles::RuleData::FireBall:
		case particles::RuleData::SimpleBeam:
		case particles::RuleData::VortexAttract:
		case particles::RuleData::ManaPath:
		case particles::RuleData::VolumeGlow:
		case particles::RuleData::Plasma:
		case particles::RuleData::BeliefSprite:
		case particles::RuleData::OrientSpriteWithVelocity:
		case particles::RuleData::SphereSurfaceTracer:
		case particles::RuleData::ForestPath:
		case particles::RuleData::None:
			break;
		}
	}
	record.other = {AtomNumber(context, effect, parts, collection.parent)};
	for (const auto& atom : collection.atoms | std::views::reverse)
	{
		record.secondSystems.numbers.push_back(AtomNumber(context, effect, parts, atom.get()));
	}
	if (collection.chainCreator != nullptr)
	{
		record.third = {context.Reference(SourceKey {.type = gamesave::ChainRecord::k_Type, .id = id})};
	}
	return record;
}

/// An atom of an effect: its values (particle_codecs::AtomCoreValues), its sub-collections, the newest first, its rules'
/// data, its sounds, its collection, its draw offset and its particle
std::optional<gamesave::SavedRecord> WriteAtom(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	if (running == nullptr)
	{
		context.Fail("the particle effect of atom " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	const auto parts = PartsOf(*running->effect);
	const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
	if (place >= parts.atoms.size())
	{
		context.Fail("atom " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& atom = *parts.atoms[place];
	auto record = particle_codecs::AtomCoreValues(atom);
	for (const auto& sub : atom.subCollections | std::views::reverse)
	{
		record.atoms.numbers.push_back(CollectionNumber(context, effect, parts, sub.get()));
	}
	// its rules' data, the newest first
	for (const auto* rule : atom.dataMade | std::views::reverse)
	{
		const auto type = AtomDataTypeOf(rule->SavedData());
		if (atom.collection == nullptr || !type.has_value())
		{
			continue;
		}
		const auto slot = std::ranges::find(atom.collection->modifiers, rule, &particles::Collection::Slot::modifier);
		if (slot != atom.collection->modifiers.end())
		{
			const auto index = static_cast<uint64_t>(slot - atom.collection->modifiers.begin());
			record.modifiers.numbers.push_back(context.Reference(SourceKey {.type = *type, .id = RuleDataKey(id, index)}));
		}
		else if (rule->SavedData() == particles::RuleData::Tornado)
		{
			// the tornado's data on a flying atom, whose collection has not the tornado among its rules
			const auto key = RuleDataKey(id, MadeRuleIndex(atom.dataMade, rule));
			record.modifiers.numbers.push_back(context.Reference(SourceKey {.type = *type, .id = key}));
		}
	}
	// its sounds, the newest first as it keeps them
	for (size_t sound = 0; sound < atom.sounds.size(); ++sound)
	{
		if (atom.sounds[sound] != nullptr)
		{
			record.objects.numbers.push_back(
			    context.Reference(SourceKey {.type = gamesave::ParticleSoundRecord::k_Type, .id = RuleDataKey(id, sound)}));
		}
	}
	record.others[0] = {CollectionNumber(context, effect, parts, atom.collection)};
	// its draw offset: one that follows the hand is its own; a decaying one is its effect's offset of that id, which
	// the atoms that took it from their parents share
	if (atom.drawOffset.has_value())
	{
		record.others[1] = {context.Reference(SourceKey {.type = gamesave::DrawOffsetHandRecord::k_Type, .id = id})};
	}
	else if (atom.decayingOffset.has_value())
	{
		record.others[1] = {context.Reference(
		    SourceKey {.type = gamesave::DrawOffsetDecayRecord::k_Type, .id = PartKey(effect, atom.decayingOffset->id)})};
	}
	if (atom.creator != nullptr && atom.creator->kind == particles::Creator::Kind::Chain && atom.chainJoint >= 0)
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleChainJointRecord::k_Type, .id = id})};
	}
	else if (atom.creator != nullptr && atom.creator->kind == particles::Creator::Kind::Point)
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticlePointRecord::k_Type, .id = id})};
	}
	else if (atom.creator != nullptr && atom.creator->className == "ParticleSpriteCreator")
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleSpriteRecord::k_Type, .id = id})};
	}
	else if (atom.creator != nullptr && atom.creator->className == "ParticleMistCreator")
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleMistRecord::k_Type, .id = id})};
	}
	else if (atom.creator != nullptr && atom.creator->className == "ParticleMeshCreatorAnimTextured")
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleTexturedObjectRecord::k_Type, .id = id})};
	}
	else if (particles::vortex_attract::CarriedOf(atom).has_value())
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleGameObjectRecord::k_Type, .id = id})};
	}
	else if (particles::creature_spell::CreatureRefOf(atom).has_value())
	{
		record.others[2] = {context.Reference(SourceKey {.type = gamesave::ParticleCreatureRefRecord::k_Type, .id = id})};
	}
	return record;
}

/// An atom's draw offset that follows the hand
std::optional<gamesave::SavedRecord> WriteHandDrawOffset(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto parts = running != nullptr ? PartsOf(*running->effect) : EffectParts {};
	const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
	if (place >= parts.atoms.size() || !parts.atoms[place]->drawOffset.has_value())
	{
		context.Fail("the draw offset of atom " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	return particle_codecs::HandDrawOffsetValues(*parts.atoms[place]->drawOffset);
}

/// A decaying draw offset of an effect, taken by every atom of the effect that holds that offset
std::optional<gamesave::SavedRecord> WriteDecayingDrawOffset(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto offsetId = static_cast<uint32_t>(id & 0xFFFFFFFFU);
	const auto* running = RunningOf(effect);
	const auto parts = running != nullptr ? PartsOf(*running->effect) : EffectParts {};
	const particles::Atom::DecayingDrawOffset* offset = nullptr;
	uint16_t references = 0;
	for (const auto* atom : parts.atoms)
	{
		if (atom->decayingOffset.has_value() && atom->decayingOffset->id == offsetId)
		{
			offset = offset != nullptr ? offset : &*atom->decayingOffset;
			++references;
		}
	}
	if (offset == nullptr)
	{
		context.Fail("the decaying draw offset " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	return particle_codecs::DecayingDrawOffsetValues(*offset, references);
}

/// An emitter's data on a collection: the rule by its place in the effect's file, the next emission time, the count
/// emitted and whether its first update is still to come
std::optional<gamesave::SavedRecord> WriteEmitterData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running == nullptr || !file.has_value())
	{
		context.Fail("the particle effect of an emitter's data " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	const auto parts = PartsOf(*running->effect);
	const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
	if (place >= parts.collections.size() || index >= parts.collections[place]->modifiers.size())
	{
		context.Fail("an emitter's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& slot = parts.collections[place]->modifiers[index];
	return particle_codecs::EmitterDataValues(slot, *file);
}

/// The collection, its rule's slot and the effect's file of a rule's data on a collection; none, and the write fails, when
/// the effect, the collection or the rule is gone
struct CollectionRule
{
	const particles::Collection* collection {nullptr};
	const particles::Collection::Slot* slot {nullptr};
	uint32_t file {0};
};

std::optional<CollectionRule> CollectionRuleOf(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			return CollectionRule {
			    .collection = parts.collections[place], .slot = &parts.collections[place]->modifiers[index], .file = *file};
		}
	}
	context.Fail("the rule's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The explosion's data on its collection, with its targets while they are there and its beam's spot visual's container
std::optional<gamesave::SavedRecord> WriteExplosionData(WriteContext& context, uint64_t id)
{
	const auto rule = CollectionRuleOf(context, id);
	const auto* data = rule.has_value() && rule->slot->modifier != nullptr
	                       ? particles::explosion::DataOf(*rule->collection, *rule->slot->modifier)
	                       : nullptr;
	if (data == nullptr)
	{
		if (rule.has_value())
		{
			context.Fail("the explosion's data " + std::to_string(id) + " is gone");
		}
		return std::nullopt;
	}
	auto record = particle_codecs::ExplosionDataValues(*data, *rule->slot->modifier, rule->file);
	const auto& registry = context.GetRegistry();
	for (size_t i = 0; i < data->targets.size(); ++i)
	{
		const auto object = data->targets[i].object;
		record.targets.elements[i].object = {object != entt::null && registry.Valid(object) ? context.Reference(object) : 0};
	}
	record.beamFx = {SpotVisualObject(context, data->beamFx)};
	return record;
}

/// The lightning data of a bolt's collection of an effect, by the number its record gets; 0 for none or one gone
gamesave::ObjectNumber LightningDataNumber(WriteContext& context, uint32_t effect, const particles::Collection* collection)
{
	const auto* running = collection != nullptr ? RunningOf(effect) : nullptr;
	if (running == nullptr)
	{
		return 0;
	}
	const auto parts = PartsOf(*running->effect);
	const auto found = std::ranges::find(parts.collections, collection);
	const auto rule = std::ranges::find_if(collection->modifiers, [](const particles::Collection::Slot& slot) {
		return slot.modifier != nullptr && slot.modifier->SavedData() == particles::RuleData::Lightning;
	});
	if (found == parts.collections.end() || rule == collection->modifiers.end())
	{
		return 0;
	}
	const auto collectionId = PartKey(effect, static_cast<size_t>(found - parts.collections.begin()));
	return context.Reference(
	    SourceKey {.type = gamesave::LightningCollectionRecord::k_Type,
	               .id = RuleDataKey(collectionId, static_cast<uint64_t>(rule - collection->modifiers.begin()))});
}

/// The lightning's data on its collection: the bolts it is linked with, its places, its heading, its flags, the age since
/// its search, the distance to its targets' centroid, its caps, its targets, its forks' lists of targets, its forks
/// with whether each was left off the root, the fork it picked, its own collection, its forks' scale, the turn it was
/// made and its clash point
std::optional<gamesave::SavedRecord> WriteLightningData(WriteContext& context, uint64_t id)
{
	const auto rule = CollectionRuleOf(context, id);
	const auto saved = rule.has_value() && rule->slot->modifier != nullptr
	                       ? particles::lightning::SavedBoltOf(*rule->collection, *rule->slot->modifier)
	                       : std::nullopt;
	if (!saved.has_value())
	{
		if (rule.has_value())
		{
			context.Fail("the lightning's data " + std::to_string(id) + " is gone");
		}
		return std::nullopt;
	}
	const auto effect = static_cast<uint32_t>(RuleDataOf(id).first >> 32U);
	const auto parts = PartsOf(*RunningOf(effect)->effect);
	const auto& registry = context.GetRegistry();
	const auto point = [](const glm::vec3& v) { return gamesave::Point {v.x, v.y, v.z}; };
	gamesave::LightningCollectionRecord record;
	record.persistent = {.first = rule->file, .second = rule->slot->modifier->fileIndex};
	record.links = {gamesave::Ref {LightningDataNumber(context, saved->linkEffect, saved->link)},
	                gamesave::Ref {LightningDataNumber(context, saved->linkedByEffect, saved->linkedBy)}};
	record.points = {point(saved->origin), point(saved->searchOrigin), point(saved->centroid)};
	record.word = std::bit_cast<uint32_t>(saved->heading);
	record.bytes = {static_cast<uint8_t>(saved->first ? 1 : 0), static_cast<uint8_t>(saved->fromHand ? 1 : 0), 0};
	record.words = {std::bit_cast<uint32_t>(saved->life), std::bit_cast<uint32_t>(saved->centroidDistance),
	                static_cast<uint32_t>(saved->searchLimit), static_cast<uint32_t>(saved->atOnce)};
	for (const auto& target : saved->targets)
	{
		const bool object = target.object != entt::null && registry.Valid(target.object);
		record.targets.elements.push_back(
		    {.turn = target.turn,
		     .object = {object ? context.Reference(target.object) : 0},
		     .point = point(target.point),
		     .bytes = {static_cast<uint8_t>(target.isObject ? 1 : 0), static_cast<uint8_t>(target.active ? 1 : 0),
		               static_cast<uint8_t>(target.lightMapDone ? 1 : 0)},
		     .word = static_cast<uint32_t>(target.cooldown)});
	}
	for (const auto& list : saved->lists)
	{
		gamesave::LightningFork fork;
		for (const int index : list)
		{
			fork.words.values.push_back(static_cast<uint32_t>(index));
		}
		record.forks.elements.push_back(std::move(fork));
	}
	record.forkWord = static_cast<uint32_t>(saved->nextList);
	for (const auto& [fork, detached] : saved->forks)
	{
		record.held.elements.push_back(
		    {.collection = {CollectionNumber(context, effect, parts, fork)}, .owned = static_cast<uint8_t>(detached ? 1 : 0)});
	}
	record.heldWord = static_cast<uint32_t>(saved->nextFork);
	record.others = {gamesave::Ref {CollectionNumber(context, effect, parts, saved->pickedFork)},
	                 gamesave::Ref {CollectionNumber(context, effect, parts, saved->collection)}};
	record.lastWords = {std::bit_cast<uint32_t>(saved->forkScale), saved->createdTurn};
	record.lastPoint = point(saved->clash);
	return record;
}

/// The willow wisp's data on a collection
std::optional<gamesave::SavedRecord> WriteWillowWispData(WriteContext& context, uint64_t id)
{
	const auto rule = CollectionRuleOf(context, id);
	if (!rule.has_value())
	{
		return std::nullopt;
	}
	return particle_codecs::WillowWispDataValues(*rule->slot, rule->file);
}

/// The hand's sprinkle's data on a collection
std::optional<gamesave::SavedRecord> WriteHandSprinkleData(WriteContext& context, uint64_t id)
{
	const auto rule = CollectionRuleOf(context, id);
	if (!rule.has_value())
	{
		return std::nullopt;
	}
	return particle_codecs::HandSprinkleDataValues(*rule->slot, rule->file);
}

/// The atoms-at-a-target rule's data on a collection, with its target while it has one
std::optional<gamesave::SavedRecord> WriteTargetAtomsData(WriteContext& context, uint64_t id)
{
	const auto rule = CollectionRuleOf(context, id);
	if (!rule.has_value())
	{
		return std::nullopt;
	}
	auto record = particle_codecs::TargetAtomsDataValues(*rule->slot, rule->file);
	if (rule->slot->state.y != 0.0f)
	{
		const auto target = static_cast<entt::entity>(std::bit_cast<uint32_t>(rule->slot->state.x));
		if (context.GetRegistry().Valid(target))
		{
			record.target = {context.Reference(target)};
		}
	}
	return record;
}

/// An atom's sound rule's data
std::optional<gamesave::SavedRecord> WriteSoundAtomData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto* rule = parts.atoms[place]->collection->modifiers[index].modifier;
			if (rule != nullptr)
			{
				return particle_codecs::SoundAtomDataValues(*parts.atoms[place], *rule, *file);
			}
		}
	}
	context.Fail("the sound rule's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The simple beam's data on its atom, with the object it follows while that is there
std::optional<gamesave::SavedRecord> WriteBeamAtomData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			const auto data = rule != nullptr ? atom.modifierData.find(rule) : atom.modifierData.end();
			if (data != atom.modifierData.end())
			{
				const auto& target = *std::static_pointer_cast<const particles::beam::Target>(data->second);
				auto record = particle_codecs::BeamAtomDataValues(target, *rule, *file);
				if (target.object != entt::null && context.GetRegistry().Valid(target.object))
				{
					record.object = {context.Reference(target.object)};
				}
				return record;
			}
		}
	}
	context.Fail("the beam's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The fade-out rule's data on an atom
std::optional<gamesave::SavedRecord> WriteFadeOutData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto data = rule != nullptr ? particles::fade_out::DataOf(atom, *rule) : std::nullopt; data.has_value())
			{
				return particle_codecs::FadeOutDataValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the fade-out data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The vortex attract rule's data on a particle it carries, with its out vortex while that is there
std::optional<gamesave::SavedRecord> WriteVortexAttractData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::vortex_attract::DataOf(atom, *rule) : nullptr; data != nullptr)
			{
				auto record = particle_codecs::VortexAttractDataValues(*data, *rule, *file);
				if (data->outVortex != entt::null && context.GetRegistry().Valid(data->outVortex))
				{
					record.object = {context.Reference(data->outVortex)};
				}
				return record;
			}
		}
	}
	context.Fail("the vortex attract data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The mana path rule's data on a spark
std::optional<gamesave::SavedRecord> WriteManaPathData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::mana_path::DataOf(atom, *rule) : nullptr; data != nullptr)
			{
				return particle_codecs::ManaPathDataValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the mana path data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The glowing shell rule's data on an atom, with the object it is kept on while that is there
std::optional<gamesave::SavedRecord> WriteVolumeGlowData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::volume_glow::DataOf(atom, *rule) : nullptr; data != nullptr)
			{
				auto record = particle_codecs::VolumeGlowDataValues(*data, *rule, *file);
				if (data->object != entt::null && context.GetRegistry().Valid(data->object))
				{
					record.SetReference(particle_codecs::k_VolumeGlowObjectField, context.Reference(data->object));
				}
				return record;
			}
		}
	}
	context.Fail("the glowing shell's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The plasma rule's data on a carrier atom
std::optional<gamesave::SavedRecord> WritePlasmaData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* arc = rule != nullptr ? particles::plasma::DataOf(atom, *rule) : nullptr; arc != nullptr)
			{
				return particle_codecs::PlasmaDataValues(*arc, *rule, *file);
			}
		}
	}
	context.Fail("the plasma's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The belief sprite rule's data on a sprite
std::optional<gamesave::SavedRecord> WriteBeliefSpriteData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::belief_sprite::DataOf(atom, *rule) : nullptr; data != nullptr)
			{
				return particle_codecs::BeliefSpriteDataValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the belief sprite's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The creature spell wisps rule's data on its collection
std::optional<gamesave::SavedRecord> WriteCreatureSpellWispsData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto data = rule != nullptr ? particles::creature_spell::WispsOf(collection, *rule) : std::nullopt;
			    data.has_value())
			{
				return particle_codecs::CreatureSpellWispsValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the creature spell wisps' data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The creature spell wisps rule's data on a wisp
std::optional<gamesave::SavedRecord> WriteCreatureSpellWispData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::creature_spell::WispOf(atom, *rule) : nullptr; data != nullptr)
			{
				return particle_codecs::CreatureSpellWispValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the creature spell wisp's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The orient-with-velocity rule's data on a sprite
std::optional<gamesave::SavedRecord> WriteOrientWithVelocityData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::orient_velocity::DataOf(atom, *rule) : nullptr; data != nullptr)
			{
				return particle_codecs::OrientWithVelocityValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the orient-with-velocity data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// A creature spell rule's data on its collection, of the class asked for: the first rule's with its creature while
/// that is there, the hearts' or the steam's counts, the itch's first update
std::optional<gamesave::SavedRecord> WriteCreatureSpellCollectionData(WriteContext& context, uint64_t id, uint32_t type)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		const auto* rule = place < parts.collections.size() && index < parts.collections[place]->modifiers.size()
		                       ? parts.collections[place]->modifiers[index].modifier
		                       : nullptr;
		if (rule != nullptr)
		{
			namespace creature_spell = particles::creature_spell;
			const auto& collection = *parts.collections[place];
			if (const auto held = creature_spell::HeldOf(collection, *rule);
			    held.has_value() && type == particle_codecs::k_CreatureSpellDataType)
			{
				auto record = particle_codecs::CreatureSpellValues(*held, *rule, *file);
				if (held->creature != entt::null && context.GetRegistry().Valid(held->creature))
				{
					record.SetReference(particle_codecs::k_CreatureSpellCreatureField, context.Reference(held->creature));
				}
				return record;
			}
			if (const auto made = creature_spell::AtomsMadeOf(collection, *rule);
			    made.has_value() && (type == particle_codecs::k_CreatureSpellCompassionDataType ||
			                         type == particle_codecs::k_CreatureSpellFreezeDataType))
			{
				return particle_codecs::CreatureSpellAtomsMadeValues(type, *made, *rule, *file);
			}
			if (const auto first = creature_spell::ItchFirstOf(collection, *rule);
			    first.has_value() && type == particle_codecs::k_CreatureSpellItchDataType)
			{
				return particle_codecs::CreatureSpellItchValues(*first, *rule, *file);
			}
		}
	}
	context.Fail("the creature spell's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The sphere surface tracer's or the forest path's data on an atom, of the class asked for
std::optional<gamesave::SavedRecord> WriteSphereAnglesData(WriteContext& context, uint64_t id, uint32_t type)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (rule != nullptr)
			{
				const auto data = type == particle_codecs::k_SphereTracerAtomDataType
				                      ? particles::sphere_angles::SphereTracerOf(atom, *rule)
				                      : particles::sphere_angles::ForestPathOf(atom, *rule);
				if (data.has_value())
				{
					return particle_codecs::SphereAnglesValues(type, *data, *rule, *file);
				}
			}
		}
	}
	context.Fail("the sphere angles' data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The flocking rule's data on its collection
std::optional<gamesave::SavedRecord> WriteFlockingData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto velocity = rule != nullptr ? particles::flocking::FlockVelocityOf(collection, *rule) : std::nullopt;
			    velocity.has_value())
			{
				return particle_codecs::FlockingValues(*velocity, *rule, *file);
			}
		}
	}
	context.Fail("the flock's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The cloud gather's data on its collection, with its lightning collection and the cloud that hangs from
std::optional<gamesave::SavedRecord> WriteCloudGatherData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto values = rule != nullptr ? particles::storm::CloudGatherOf(collection, *rule) : std::nullopt;
			    values.has_value())
			{
				auto record = particle_codecs::CloudGatherValues(*values, *rule, *file);
				record.SetReference(particle_codecs::k_CloudGatherLightningField,
				                    CollectionNumber(context, effect, parts, particles::storm::LightningOf(collection, *rule)));
				record.SetReference(particle_codecs::k_CloudGatherCloudField,
				                    AtomNumber(context, effect, parts, particles::storm::LightningCloudOf(collection, *rule)));
				return record;
			}
		}
	}
	context.Fail("the cloud gather's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The cloud gather's data on a cloud
std::optional<gamesave::SavedRecord> WriteCloudData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto* data = rule != nullptr ? particles::storm::CloudOf(atom, *rule) : nullptr; data != nullptr)
			{
				return particle_codecs::CloudValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the cloud's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The cloud mover's data on its collection
std::optional<gamesave::SavedRecord> WriteCloudMoverData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto first = rule != nullptr ? particles::storm::CloudMoverFirstOf(collection, *rule) : std::nullopt;
			    first.has_value())
			{
				return particle_codecs::CloudMoverValues(*first, *rule, *file);
			}
		}
	}
	context.Fail("the cloud mover's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The tornado's data on its collection
std::optional<gamesave::SavedRecord> WriteTornadoData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto data = rule != nullptr ? particles::storm::TornadoOf(collection, *rule) : std::nullopt;
			    data.has_value())
			{
				return particle_codecs::TornadoValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the tornado's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// The sound removal's data on an atom
std::optional<gamesave::SavedRecord> WriteSoundRemovalData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto removed = rule != nullptr ? particles::sound_rules::SoundRemovedOf(atom, *rule) : std::nullopt;
			    removed.has_value())
			{
				return particle_codecs::SoundRemovalValues(*removed, *rule, *file);
			}
		}
	}
	context.Fail("the sound removal's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The sub-collections rule's data on an atom
std::optional<gamesave::SavedRecord> WriteSubCollectionsData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto added =
			        rule != nullptr ? particles::fireball_rules::SubCollectionsAddedOf(atom, *rule) : std::nullopt;
			    added.has_value())
			{
				return particle_codecs::SubCollectionsValues(*added, *rule, *file);
			}
		}
	}
	context.Fail("the sub-collections rule's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The fireball's rule's data on an atom, with its ball while that is there
std::optional<gamesave::SavedRecord> WriteFireBallData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr &&
		    index < parts.atoms[place]->collection->modifiers.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = atom.collection->modifiers[index].modifier;
			if (const auto data = rule != nullptr ? particles::fireball_rules::BallDataOf(atom, *rule) : std::nullopt;
			    data.has_value())
			{
				auto record = particle_codecs::FireBallDataValues(*data, *rule, *file);
				if (data->ball != entt::null && context.GetRegistry().Valid(data->ball))
				{
					record.SetReference(particle_codecs::k_FireBallBallField,
					                    context.Reference(data->ball, gamesave::MagicFireBallRecord::k_Type));
				}
				return record;
			}
		}
	}
	context.Fail("the fireball rule's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The lightning sprite emitter's data on its collection
std::optional<gamesave::SavedRecord> WriteLightningSpriteData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size() && index < parts.collections[place]->modifiers.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = collection.modifiers[index].modifier;
			if (const auto state = rule != nullptr ? particles::storm_flash::StateOf(collection, *rule) : std::nullopt;
			    state.has_value())
			{
				return particle_codecs::LightningSpriteValues(*state, *rule, *file);
			}
		}
	}
	context.Fail("the lightning sprite emitter's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The tornado's data on one of its debris and flying collections
std::optional<gamesave::SavedRecord> WriteTornadoEmitterData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
		if (place < parts.collections.size())
		{
			const auto& collection = *parts.collections[place];
			const auto* rule = MadeRuleAt(collection.dataMade, index);
			if (const auto data = rule != nullptr ? particles::storm::TornadoEmitterOf(collection, *rule) : std::nullopt;
			    data.has_value())
			{
				return particle_codecs::TornadoEmitterValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the tornado's emitter data " + std::to_string(id) + " of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// The tornado's data on a flying atom
std::optional<gamesave::SavedRecord> WriteTornadoFlyingData(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size())
		{
			const auto& atom = *parts.atoms[place];
			const auto* rule = MadeRuleAt(atom.dataMade, index);
			if (const auto data = rule != nullptr ? particles::storm::TornadoFlyingOf(atom, *rule) : std::nullopt;
			    data.has_value())
			{
				return particle_codecs::TornadoFlyingValues(*data, *rule, *file);
			}
		}
	}
	context.Fail("the tornado's data " + std::to_string(id) + " on a flying atom of particle effect " + std::to_string(effect) +
	             " is gone");
	return std::nullopt;
}

/// A particle drawn as the game object it carries: the thing while it has not been let go, and the effect's player
std::optional<gamesave::SavedRecord> WriteGameObject(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto parts = running != nullptr ? PartsOf(*running->effect) : EffectParts {};
	const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
	const auto carried = place < parts.atoms.size() ? particles::vortex_attract::CarriedOf(*parts.atoms[place]) : std::nullopt;
	if (!carried.has_value())
	{
		context.Fail("the game object particle " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	auto record = particle_codecs::GameObjectValues(*carried, parts.atoms[place]->savedDraw[1]);
	if (!carried->letGo && carried->object != entt::null && context.GetRegistry().Valid(carried->object))
	{
		record.objects[0] = {context.Reference(carried->object)};
	}
	record.objects[1] = {PlayerObject(context, carried->player)};
	return record;
}

/// A creature spell's first particle: enabled, the turn its creature was last found there, and its creature (none once
/// it has gone)
std::optional<gamesave::SavedRecord> WriteCreatureRef(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto parts = running != nullptr ? PartsOf(*running->effect) : EffectParts {};
	const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
	const auto ref = place < parts.atoms.size() ? particles::creature_spell::CreatureRefOf(*parts.atoms[place]) : std::nullopt;
	if (!ref.has_value())
	{
		context.Fail("the creature particle " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	auto record = particle_codecs::CreatureRefValues(*ref);
	if (ref->creature != entt::null && context.GetRegistry().Valid(ref->creature))
	{
		record.creature = {context.Reference(ref->creature)};
	}
	return record;
}

/// The simple beam's or the plasma's data on one of its beams' collections
std::optional<gamesave::SavedRecord> WriteRibbonData(WriteContext& context, uint64_t id, particles::RuleData kind)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.collections.size())
		{
			const auto& collection = *parts.collections[place];
			const auto rule =
			    std::ranges::find(collection.dataMade, kind, [](const particles::Modifier* made) { return made->SavedData(); });
			const auto data =
			    rule != collection.dataMade.end() ? collection.modifierData.find(*rule) : collection.modifierData.end();
			if (data != collection.modifierData.end())
			{
				const auto& ribbon = *std::static_pointer_cast<const particles::beam::Ribbon>(data->second);
				if (kind == particles::RuleData::Plasma)
				{
					return particle_codecs::PlasmaRibbonValues(ribbon, **rule, *file);
				}
				return particle_codecs::BeamRibbonValues(ribbon, **rule, *file);
			}
		}
	}
	context.Fail("the beam's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// A trail's data on a collection: the rule by its place in the effect's file, whether its first update is to come and
/// the places it keeps, the newest first
std::optional<gamesave::SavedRecord> WriteTrailData(WriteContext& context, uint64_t id)
{
	const auto [collectionId, index] = RuleDataOf(id);
	const auto effect = collectionId >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running == nullptr || !file.has_value())
	{
		context.Fail("the particle effect of a trail's data " + std::to_string(id) + " is gone");
		return std::nullopt;
	}
	const auto parts = PartsOf(*running->effect);
	const auto place = static_cast<size_t>(collectionId & 0xFFFFFFFFU);
	if (place >= parts.collections.size() || index >= parts.collections[place]->modifiers.size())
	{
		context.Fail("a trail's data " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
		return std::nullopt;
	}
	const auto& collection = *parts.collections[place];
	return particle_codecs::TrailDataValues(collection.modifiers[index], *file,
	                                        particles::fireball_rules::TrailPlaces(collection));
}

/// An atom's sprite
std::optional<gamesave::SavedRecord> WriteSprite(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.atoms.size())
		{
			return particle_codecs::SpriteValues(*parts.atoms[place], *file);
		}
	}
	context.Fail("the sprite " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// A chain joint's particle: its values and its chain, its collection's
std::optional<gamesave::SavedRecord> WriteChainJoint(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	if (running != nullptr)
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && parts.atoms[place]->collection != nullptr)
		{
			const auto& atom = *parts.atoms[place];
			auto record = particle_codecs::ChainJointValues(atom);
			const auto collection = std::ranges::find(parts.collections, atom.collection);
			if (collection != parts.collections.end())
			{
				const auto collectionId = PartKey(effect, static_cast<size_t>(collection - parts.collections.begin()));
				record.chain = {context.Reference(SourceKey {.type = gamesave::ChainRecord::k_Type, .id = collectionId})};
			}
			return record;
		}
	}
	context.Fail("the chain joint " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// A collection's chain
std::optional<gamesave::SavedRecord> WriteChain(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.collections.size() && parts.collections[place]->chainCreator != nullptr)
		{
			return particle_codecs::ChainValues(*parts.collections[place], *file);
		}
	}
	context.Fail("the chain " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// An atom's animated textured mesh
std::optional<gamesave::SavedRecord> WriteTexturedObject(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.atoms.size())
		{
			return particle_codecs::TexturedObjectValues(*parts.atoms[place], *file);
		}
	}
	context.Fail("the textured mesh " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// An atom's sound, with its atom
std::optional<gamesave::SavedRecord> WriteParticleSound(WriteContext& context, uint64_t id)
{
	const auto [atomId, index] = RuleDataOf(id);
	const auto effect = atomId >> 32U;
	if (const auto* running = RunningOf(effect); running != nullptr)
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(atomId & 0xFFFFFFFFU);
		if (place < parts.atoms.size() && index < parts.atoms[place]->sounds.size() &&
		    parts.atoms[place]->sounds[index] != nullptr)
		{
			auto record = particle_codecs::ParticleSoundValues(*parts.atoms[place]->sounds[index],
			                                                   static_cast<uint32_t>(audio::spell_sounds::Count()));
			record.object = {context.Reference(SourceKey {.type = gamesave::AtomCoreRecord::k_Type, .id = atomId})};
			return record;
		}
	}
	context.Fail("the sound " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// An atom's mist
std::optional<gamesave::SavedRecord> WriteMist(WriteContext& context, uint64_t id)
{
	const auto effect = id >> 32U;
	const auto* running = RunningOf(effect);
	const auto file = FileIdOf(context.GetRegistry(), effect);
	if (running != nullptr && file.has_value())
	{
		const auto parts = PartsOf(*running->effect);
		const auto place = static_cast<size_t>(id & 0xFFFFFFFFU);
		if (place < parts.atoms.size())
		{
			return particle_codecs::MistValues(*parts.atoms[place], *file);
		}
	}
	context.Fail("the mist " + std::to_string(id) + " of particle effect " + std::to_string(effect) + " is gone");
	return std::nullopt;
}

/// An atom's point particle: drawn (as every point is made)
std::optional<gamesave::SavedRecord> WritePoint(WriteContext& /*context*/, uint64_t /*id*/)
{
	return gamesave::ParticlePointRecord {.renderByte = 1};
}
} // namespace

std::array<uint8_t, 60> openblack::ecs::saved_world_write::ProcessInfoBytes(const particles::ProcessInfo& info)
{
	std::array<uint8_t, 60> bytes {};
	const auto put = [&bytes](size_t at, const auto& value) { std::memcpy(bytes.data() + at, &value, sizeof(value)); };
	const auto point = [&put](size_t at, const glm::vec3& value) {
		put(at, value.x);
		put(at + 4, value.y);
		put(at + 8, value.z);
	};
	point(0, info.interfacePos);
	point(12, info.handPos);
	point(24, info.cameraForward);
	point(36, info.direction);
	put(48, info.power);
	put(52, info.curl);
	bytes[56] = info.enabled ? 1 : 0;
	return bytes;
}

gamesave::ObjectNumber openblack::ecs::saved_world_write::SpotVisualObject(WriteContext& context, entt::entity object)
{
	auto* state = EngineState();
	if (object == entt::null || state == nullptr)
	{
		return 0;
	}
	const auto found = std::ranges::find(state->containers, object, &particles::manager::Container::object);
	if (found == state->containers.end() || RunningOf(found->effect) == nullptr)
	{
		return 0;
	}
	return context.Reference(SourceKey {.type = ParticleContainerRecord::k_Type, .id = found->effect});
}

gamesave::ObjectNumber openblack::ecs::saved_world_write::ScriptTableObject(WriteContext& context, entt::entity object)
{
	if (const auto effect = SpotVisualEffect(object); effect.has_value())
	{
		return context.Reference(SourceKey {.type = ParticleContainerRecord::k_Type, .id = *effect});
	}
	// a player's computer player, which CALL_COMPUTER_PLAYER hands the scripts (the player's entity stands for it): the
	// computer player's object of the player's number
	const auto& registry = context.GetRegistry();
	if (const auto* player = registry.Valid(object) ? registry.TryGet<const ecs::components::Player>(object) : nullptr;
	    player != nullptr && registry.AllOf<ecs::components::ComputerPlayer>(object))
	{
		return context.Reference(
		    SourceKey {.type = gamesave::k_ComputerPlayerType, .id = static_cast<uint64_t>(std::to_underlying(player->name))});
	}
	return context.Reference(object);
}

std::optional<uint64_t> openblack::ecs::saved_world_write::SpotVisualEffect(entt::entity object)
{
	auto* state = EngineState();
	if (object == entt::null || state == nullptr)
	{
		return std::nullopt;
	}
	const auto found = std::ranges::find(state->containers, object, &particles::manager::Container::object);
	if (found == state->containers.end() || RunningOf(found->effect) == nullptr)
	{
		return std::nullopt;
	}
	return found->effect;
}

void openblack::ecs::saved_world_write::RegisterParticleWriters(ClassWriters& writers)
{
	writers.RegisterSource(ParticleContainerRecord::k_Type, ContainerKeys, WriteContainer);
	writers.RegisterSource(ParticleInterfaceRecord::k_Type, ContainerKeys, WriteInterface);
	writers.RegisterSource(ParticleManagerRecord::k_Type, ContainerKeys, WriteManager);
	// the managers' collections and atoms and the atoms' points: reached only through their manager
	const auto none = [](const ecs::Registry& /*registry*/) { return std::vector<uint64_t> {}; };
	writers.RegisterSource(gamesave::AtomCollectionRecord::k_Type, none, WriteCollection);
	writers.RegisterSource(gamesave::AtomCoreRecord::k_Type, none, WriteAtom);
	writers.RegisterSource(gamesave::ParticlePointRecord::k_Type, none, WritePoint);
	writers.RegisterSource(gamesave::EmitterCollectionRecord::k_Type, none, WriteEmitterData);
	writers.RegisterSource(gamesave::TrailCollectionRecord::k_Type, none, WriteTrailData);
	writers.RegisterSource(gamesave::HandSprinkleCollectionRecord::k_Type, none, WriteHandSprinkleData);
	writers.RegisterSource(gamesave::WillowWispCollectionRecord::k_Type, none, WriteWillowWispData);
	writers.RegisterSource(gamesave::TargetAtomsCollectionRecord::k_Type, none, WriteTargetAtomsData);
	writers.RegisterSource(gamesave::ExplosionCollectionRecord::k_Type, none, WriteExplosionData);
	writers.RegisterSource(gamesave::LightningCollectionRecord::k_Type, none, WriteLightningData);
	writers.RegisterSource(gamesave::SoundAtomDataRecord::k_Type, none, WriteSoundAtomData);
	writers.RegisterSource(gamesave::DrawOffsetHandRecord::k_Type, none, WriteHandDrawOffset);
	writers.RegisterSource(gamesave::DrawOffsetDecayRecord::k_Type, none, WriteDecayingDrawOffset);
	writers.RegisterSource(gamesave::ParticleSpriteRecord::k_Type, none, WriteSprite);
	writers.RegisterSource(gamesave::ParticleMistRecord::k_Type, none, WriteMist);
	writers.RegisterSource(gamesave::ParticleTexturedObjectRecord::k_Type, none, WriteTexturedObject);
	writers.RegisterSource(gamesave::ParticleSoundRecord::k_Type, none, WriteParticleSound);
	writers.RegisterSource(gamesave::ParticleChainJointRecord::k_Type, none, WriteChainJoint);
	writers.RegisterSource(gamesave::ChainRecord::k_Type, none, WriteChain);
	writers.RegisterSource(gamesave::BeamAtomDataRecord::k_Type, none, WriteBeamAtomData);
	writers.RegisterSource(gamesave::VortexAttractAtomDataRecord::k_Type, none, WriteVortexAttractData);
	writers.RegisterSource(gamesave::FadeOutAtomDataRecord::k_Type, none, WriteFadeOutData);
	writers.RegisterSource(particle_codecs::k_ManaPathAtomDataType, none, WriteManaPathData);
	writers.RegisterSource(particle_codecs::k_VolumeGlowAtomDataType, none, WriteVolumeGlowData);
	writers.RegisterSource(particle_codecs::k_PlasmaAtomDataType, none, WritePlasmaData);
	writers.RegisterSource(particle_codecs::k_BeliefSpriteAtomDataType, none, WriteBeliefSpriteData);
	writers.RegisterSource(particle_codecs::k_CreatureSpellWispsDataType, none, WriteCreatureSpellWispsData);
	writers.RegisterSource(particle_codecs::k_CreatureSpellWispDataType, none, WriteCreatureSpellWispData);
	writers.RegisterSource(particle_codecs::k_OrientWithVelocityAtomDataType, none, WriteOrientWithVelocityData);
	writers.RegisterSource(particle_codecs::k_FlockingDataType, none, WriteFlockingData);
	writers.RegisterSource(particle_codecs::k_CloudMoverDataType, none, WriteCloudMoverData);
	writers.RegisterSource(particle_codecs::k_CloudAtomDataType, none, WriteCloudData);
	writers.RegisterSource(particle_codecs::k_CloudGatherDataType, none, WriteCloudGatherData);
	writers.RegisterSource(particle_codecs::k_TornadoDataType, none, WriteTornadoData);
	writers.RegisterSource(particle_codecs::k_TornadoEmitterDataType, none, WriteTornadoEmitterData);
	writers.RegisterSource(particle_codecs::k_TornadoFlyingAtomDataType, none, WriteTornadoFlyingData);
	writers.RegisterSource(particle_codecs::k_LightningSpriteDataType, none, WriteLightningSpriteData);
	writers.RegisterSource(particle_codecs::k_SoundRemovalAtomDataType, none, WriteSoundRemovalData);
	writers.RegisterSource(particle_codecs::k_SubCollectionsAtomDataType, none, WriteSubCollectionsData);
	writers.RegisterSource(particle_codecs::k_FireBallAtomDataType, none, WriteFireBallData);
	for (const auto type : {particle_codecs::k_SphereTracerAtomDataType, particle_codecs::k_ForestPathAtomDataType})
	{
		writers.RegisterSource(type, none,
		                       [type](WriteContext& context, uint64_t id) { return WriteSphereAnglesData(context, id, type); });
	}
	for (const auto type : {particle_codecs::k_CreatureSpellCompassionDataType, particle_codecs::k_CreatureSpellDataType,
	                        particle_codecs::k_CreatureSpellItchDataType, particle_codecs::k_CreatureSpellFreezeDataType})
	{
		writers.RegisterSource(type, none, [type](WriteContext& context, uint64_t id) {
			return WriteCreatureSpellCollectionData(context, id, type);
		});
	}
	writers.RegisterSource(gamesave::ParticleGameObjectRecord::k_Type, none, WriteGameObject);
	writers.RegisterSource(gamesave::ParticleCreatureRefRecord::k_Type, none, WriteCreatureRef);
	writers.RegisterSource(gamesave::BeamSubCollectionRecord::k_Type, none, [](WriteContext& context, uint64_t id) {
		return WriteRibbonData(context, id, particles::RuleData::SimpleBeam);
	});
	writers.RegisterSource(gamesave::PlasmaSubCollectionRecord::k_Type, none, [](WriteContext& context, uint64_t id) {
		return WriteRibbonData(context, id, particles::RuleData::Plasma);
	});
	// the game's containers: the live ones, the newest first, then those the load kept, as the loaded file listed them;
	// a kept one a load made live again (a sped-up pile's sparkle) is listed once, among the live ones
	writers.RegisterList(GlobalList::ParticleContainers, [](WriteContext& context) {
		std::vector<gamesave::ObjectNumber> numbers;
		for (const auto effect : context.Keys(ParticleContainerRecord::k_Type))
		{
			numbers.push_back(context.Reference(SourceKey {.type = ParticleContainerRecord::k_Type, .id = effect}));
		}
		if (const auto* game = std::get_if<gamesave::GameHead>(context.KeptWorldRecord(1)); game != nullptr)
		{
			const auto live = numbers.size();
			for (const auto number : game->lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers)
			{
				// a loaded container made live again and gone since (its turns run out, its owner's visual let go) has
				// left the list, as the original's container unlinks itself from the game's list as it is deleted
				const auto target = context.TargetOf(number);
				if (target.has_value() && target->type == ParticleContainerRecord::k_Type && target->id.has_value() &&
				    (ContainerOf(*target->id) == nullptr || RunningOf(*target->id) == nullptr))
				{
					continue;
				}
				if (std::find(numbers.begin(), numbers.begin() + static_cast<std::ptrdiff_t>(live), number) ==
				    numbers.begin() + static_cast<std::ptrdiff_t>(live))
				{
					numbers.push_back(number);
				}
			}
		}
		return numbers;
	});
}
