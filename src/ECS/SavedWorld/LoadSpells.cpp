/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadSpells.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <variant>

#include <CreatureRecords.h>
#include <InterfaceRecords.h>
#include <SpellIconRecords.h>
#include <WorldReader.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/Components/FlockSpell.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/ParticleCodecs.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Magic/Objects/MapShield.h"
#include "Magic/Spells/SpellCreature.h"
#include "Magic/Spells/SpellFlock.h"
#include "Magic/Spells/SpellForest.h"
#include "Magic/Spells/SpellShield.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "Magic/Spells/SpellWater.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManager.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/ParticleTypes.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The places in a spell's record of its six words and its four objects (WriteSpells.cpp writes them so)
constexpr size_t k_Chants = 0;
constexpr size_t k_InitialChants = 1;
constexpr size_t k_ClosedDown = 2;
constexpr size_t k_MyInterfaceCasting = 3;
constexpr size_t k_CreatureCasting = 4;
constexpr size_t k_HumanPlayerCasting = 5;
constexpr size_t k_Creator = 0;
constexpr size_t k_Player = 1;
constexpr size_t k_Status = 2;
constexpr size_t k_Seed = 3;
constexpr size_t k_Age = 0;
constexpr size_t k_Magnitude = 1;
constexpr size_t k_StrengthMultiplier = 2;
constexpr size_t k_ManaPathPerTurn = 0;
constexpr size_t k_ManaPathPerEvent = 1;

const gamesave::SavedRecord* RecordIn(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
	{
		return nullptr;
	}
	return &*world.objects[number - 1];
}

/// What kind of creator a spell's creator object is, by its record's class
components::SpellCreator::Kind CreatorKindOf(const gamesave::SavedRecord* record)
{
	using Kind = components::SpellCreator::Kind;
	if (record == nullptr)
	{
		return Kind::None;
	}
	if (std::holds_alternative<gamesave::PlayerRecord>(*record))
	{
		return Kind::Player;
	}
	if (std::holds_alternative<gamesave::CreatureRecord>(*record))
	{
		return Kind::Creature;
	}
	if (std::holds_alternative<gamesave::WorshipSpellIconRecord>(*record))
	{
		return Kind::WorshipSpellIcon;
	}
	return Kind::Thing;
}

/// A place the game keeps as map coordinates, in metres with the altitude as it is
glm::vec3 MetresOf(const gamesave::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z)};
}

/// Map coordinates written as their 12 bytes, read back
gamesave::MapCoords CoordsOfRaw(const gamesave::Point& point)
{
	return {.x = std::bit_cast<int32_t>(point[0]), .z = std::bit_cast<int32_t>(point[1]), .altitude = point[2]};
}

glm::vec3 VectorOf(const gamesave::Point& point)
{
	return {point[0], point[1], point[2]};
}

/// The values every spell writes, of a planned spell's record
const gamesave::SpellFields& FieldsOf(const saved_spells::PlannedSpell& planned)
{
	return std::visit([](const auto& record) -> const gamesave::SpellFields& { return record.spell; }, planned.record);
}

/// A flock spell's own as its load keeps them: its flock, the animals it has made, its count of animals to emit and where
/// it last let them out; its animals, the newest first, each naming it as its spell (an animal the load did not make is
/// left out). The flying spell's cast effect is not saved: it has none
template <typename EntityOf>
void RestoreFlock(ecs::Registry& registry, entt::entity spell, const gamesave::SpellFlockFields& fields,
                  const gamesave::RefList& objects, const EntityOf& entityOf)
{
	auto& data = registry.AssignOrReplace<components::FlockSpell>(spell);
	data.flock = entityOf(fields.object.number);
	data.created = static_cast<int>(fields.words[0]);
	data.emitted = std::bit_cast<float>(fields.words[1]);
	data.lastSpawn = {fields.lastSpawn.x, fields.lastSpawn.z};
	data.lastSpawnHeight = fields.lastSpawn.altitude;
	for (const auto number : objects.numbers | std::views::reverse)
	{
		const auto animal = entityOf(number);
		if (animal == entt::null)
		{
			continue;
		}
		magic::spell_objects::Add(spell, animal);
		if (auto* own = registry.TryGet<magic::SpellFlockAnimal>(animal); own != nullptr)
		{
			own->spell = spell;
		}
	}
}

/// A physical shield's matrix from its 52 bytes: its rows, its translation and its scale, each cell a float
components::ShieldMatrix MatrixOf(const std::array<uint8_t, 52>& bytes)
{
	std::array<float, 13> cells {};
	std::memcpy(cells.data(), bytes.data(), bytes.size());
	components::ShieldMatrix matrix;
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t column = 0; column < 3; ++column)
		{
			matrix.rows[row][column] = cells.at(static_cast<size_t>(row * 3 + column));
		}
	}
	matrix.translation = {cells[9], cells[10], cells[11]};
	matrix.scale = cells[12];
	return matrix;
}
} // namespace

std::vector<saved_spells::PlannedSpell>
saved_spells::PlanSpells(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list,
                         const std::function<std::optional<PlayerNames>(gamesave::ObjectNumber)>& playerOf)
{
	std::vector<PlannedSpell> spells;
	for (const auto number : list)
	{
		const auto* record = RecordIn(world, number);
		std::optional<PlannedSpell> planned;
		if (const auto* water = record != nullptr ? std::get_if<gamesave::SpellWaterRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *water};
		}
		else if (const auto* shield = record != nullptr ? std::get_if<gamesave::SpellShieldRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *shield};
		}
		else if (const auto* flying = record != nullptr ? std::get_if<gamesave::SpellFlockFlyingRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *flying};
		}
		else if (const auto* ground = record != nullptr ? std::get_if<gamesave::SpellFlockGroundRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *ground};
		}
		else if (const auto* creature = record != nullptr ? std::get_if<gamesave::SpellCreatureRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *creature};
		}
		else if (const auto* plain = record != nullptr ? std::get_if<gamesave::SpellRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *plain};
		}
		else if (const auto* heal = record != nullptr ? std::get_if<gamesave::SpellHealRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *heal};
		}
		else if (const auto* storm = record != nullptr ? std::get_if<gamesave::SpellStormAndTornadoRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *storm};
		}
		else if (const auto* forest = record != nullptr ? std::get_if<gamesave::SpellForestRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *forest};
		}
		else if (const auto* teleport = record != nullptr ? std::get_if<gamesave::SpellTeleportRecord>(record) : nullptr)
		{
			planned = PlannedSpell {.number = number, .record = *teleport};
		}
		if (!planned.has_value())
		{
			continue;
		}
		const auto& fields = FieldsOf(*planned);
		if (fields.object.number != 0)
		{
			planned->effect = saved_particles::PlanEffect(world, fields.object.number);
		}
		planned->creatorKind = CreatorKindOf(RecordIn(world, fields.objects[k_Creator].number));
		if (fields.objects[k_Player].number != 0 && playerOf)
		{
			planned->player = playerOf(fields.objects[k_Player].number);
		}
		spells.push_back(std::move(*planned));
	}
	return spells;
}

std::vector<saved_spells::PlannedPhysicalShield>
saved_spells::PlanPhysicalShields(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list)
{
	std::vector<PlannedPhysicalShield> shields;
	for (size_t place = 0; place < list.size(); ++place)
	{
		const auto number = list[place];
		const auto* record = RecordIn(world, number);
		const auto* shield = record != nullptr ? std::get_if<gamesave::PhysicalShieldRecord>(record) : nullptr;
		if (shield == nullptr)
		{
			continue;
		}
		PlannedPhysicalShield planned {.number = number, .record = *shield, .place = place};
		if (shield->object.number != 0)
		{
			planned.effect = saved_particles::PlanEffect(world, shield->object.number);
		}
		shields.push_back(std::move(planned));
	}
	return shields;
}

std::vector<saved_spells::PlannedSpiritualShield>
saved_spells::PlanSpiritualShields(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list)
{
	std::vector<PlannedSpiritualShield> shields;
	for (size_t place = 0; place < list.size(); ++place)
	{
		const auto* record = RecordIn(world, list[place]);
		if (const auto* shield = record != nullptr ? std::get_if<gamesave::SpiritualShieldRecord>(record) : nullptr)
		{
			shields.push_back({.number = list[place], .record = *shield, .place = place});
		}
	}
	return shields;
}

components::MagicShield saved_spells::SpiritualShieldOf(const gamesave::SpiritualShieldRecord& record)
{
	components::MagicShield shield;
	shield.kind = components::MagicShield::Kind::Spiritual;
	shield.magicType = MagicType::Shield;
	shield.position = MetresOf(record.base.position.position);
	shield.objectScale = record.base.object.scale;
	return shield;
}

components::MagicShield saved_spells::PhysicalShieldOf(const gamesave::PhysicalShieldRecord& record)
{
	components::MagicShield shield;
	shield.kind = components::MagicShield::Kind::Physical;
	shield.magicType = MagicType::PhysicalShield;
	shield.position = MetresOf(record.base.position.position);
	shield.objectScale = record.base.object.scale;
	shield.creationTurn = record.words[1];
	shield.matrix = MatrixOf(record.first);
	shield.previousMatrix = MatrixOf(record.second);
	shield.startScale = std::bit_cast<float>(record.secondWords[0]);
	shield.finalScale = std::bit_cast<float>(record.secondWords[1]);
	shield.startSpin = std::bit_cast<float>(record.secondWords[2]);
	shield.endSpin = std::bit_cast<float>(record.secondWords[3]);
	shield.angle = std::bit_cast<float>(record.secondWords[4]);
	shield.bob = std::bit_cast<float>(record.secondWords[5]);
	shield.dying = record.bytes[0] != 0;
	shield.alpha = record.bytes[1];
	shield.dieTime = std::bit_cast<float>(record.lastWord);
	return shield;
}

components::Spell saved_spells::SpellOf(const gamesave::SpellFields& fields, components::SpellCreator::Kind creatorKind,
                                        std::optional<PlayerNames> player, entt::entity creator, entt::entity seed)
{
	using Kind = components::SpellCreator::Kind;
	components::Spell spell;
	spell.magicType = static_cast<MagicType>(fields.thirdWord);
	spell.position = MetresOf(fields.position.position);
	spell.movementDirection = VectorOf(fields.points[0]);
	spell.castPos = MetresOf(CoordsOfRaw(fields.points[1]));
	spell.direction = VectorOf(fields.points[2]);
	spell.chants = std::bit_cast<float>(fields.words[k_Chants]);
	spell.initialChants = std::bit_cast<float>(fields.words[k_InitialChants]);
	spell.closedDown = fields.words[k_ClosedDown] != 0;
	spell.isMyInterfaceCasting = fields.words[k_MyInterfaceCasting] != 0;
	spell.isCreatureCasting = fields.words[k_CreatureCasting] != 0;
	spell.isHumanPlayerCasting = fields.words[k_HumanPlayerCasting] != 0;
	spell.processInfo = saved_world_write::particle_codecs::ProcessInfoOf(fields.values);
	spell.duration = fields.value;
	spell.free = fields.flag != 0;
	spell.hasPlayer = player.has_value();
	spell.player = player.value_or(PlayerNames::NEUTRAL);
	// the creator's player is the spell's (a spell takes its creator's player as it is made)
	spell.creator = {.kind = creatorKind, .player = spell.player, .entity = creatorKind == Kind::Player ? entt::null : creator};
	spell.castFromInterface = fields.objects[k_Status].number != 0;
	spell.seed = seed;
	spell.age = fields.floats[k_Age];
	spell.magnitude = fields.floats[k_Magnitude];
	spell.strengthMultiplier = fields.floats[k_StrengthMultiplier];
	spell.originalCastPos = MetresOf(CoordsOfRaw(fields.place));
	spell.manaPathPerTurn = fields.lastFloats[k_ManaPathPerTurn];
	spell.manaPathPerEvent = fields.lastFloats[k_ManaPathPerEvent];
	return spell;
}

saved_spells::MadeSpells
saved_spells::MakePhysicalShields(const std::vector<PlannedPhysicalShield>& shields, const SpellLinks& links,
                                  const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                                  const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep,
                                  const std::vector<PlannedSpiritualShield>& spiritual)
{
	MadeSpells made;
	auto& registry = Locator::entitiesRegistry::value();
	const auto file = particles::ParticleTypeFile(ParticleType::PhysicalShieldFx);
	// the list's last first, both kinds together: the newest gets the newest serial
	struct Next
	{
		size_t place;
		size_t index;
		bool spiritual;
	};
	std::vector<Next> order;
	order.reserve(shields.size() + spiritual.size());
	for (size_t index = 0; index < shields.size(); ++index)
	{
		order.push_back({.place = shields[index].place, .index = index, .spiritual = false});
	}
	for (size_t index = 0; index < spiritual.size(); ++index)
	{
		order.push_back({.place = spiritual[index].place, .index = index, .spiritual = true});
	}
	std::ranges::sort(order, [](const Next& left, const Next& right) {
		return left.place != right.place ? left.place > right.place : left.index > right.index;
	});
	for (const auto& next : order)
	{
		if (next.spiritual)
		{
			const auto& planned = spiritual[next.index];
			const auto entity = magic::map_shield::RestoreSpiritual(SpiritualShieldOf(planned.record));
			registry.AssignOrReplace<components::ObjectCreationIndex>(entity, planned.record.base.object.creationIndex);
			registry.Assign<components::LoadedRecord<gamesave::SpiritualShieldRecord>>(entity, planned.record);
			if (keep)
			{
				keep(planned.number, entity, gamesave::SpiritualShieldRecord::k_Type);
			}
			made.emplace(planned.number, entity);
			continue;
		}
		const auto& planned = shields[next.index];
		const auto entity = magic::map_shield::RestorePhysical(PhysicalShieldOf(planned.record));
		// its creation number is the one its record holds, as every loaded object's
		registry.AssignOrReplace<components::ObjectCreationIndex>(entity, planned.record.base.object.creationIndex);
		registry.Assign<components::LoadedRecord<gamesave::PhysicalShieldRecord>>(entity, planned.record);
		// known before its effect is made, which follows it as its target
		if (keep)
		{
			keep(planned.number, entity, gamesave::PhysicalShieldRecord::k_Type);
		}
		made.emplace(planned.number, entity);
		if (!planned.effect.has_value())
		{
			continue;
		}
		std::string why = file.empty() ? std::string("its particle type has no file") : std::string();
		auto effect = file.empty() ? nullptr
		                           : saved_particles::RestoreEffect(*planned.effect, particles::File::Load(std::string(file)),
		                                                            {.entityOf = links.entityOf}, why);
		if (effect == nullptr)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Load: physical shield {} has no effect: {}", planned.number, why);
			}
			continue;
		}
		// stepped by the shield, as its own
		const auto id = particles::manager::AdoptForSpell(std::move(effect), nullptr);
		registry.Get<components::MagicShield>(entity).fx = id;
		if (keepSource && id != 0)
		{
			keepSource(planned.effect->interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, id);
			keepSource(planned.effect->managerNumber, gamesave::ParticleManagerRecord::k_Type, id);
		}
	}
	return made;
}

saved_spells::MadeSpells
saved_spells::MakeSpells(const std::vector<PlannedSpell>& spells, const SpellLinks& links,
                         const std::function<void(gamesave::ObjectNumber, uint32_t, uint64_t)>& keepSource,
                         const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep)
{
	MadeSpells made;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entityOf = [&links](gamesave::ObjectNumber number) {
		return number != 0 && links.entityOf ? links.entityOf(number) : entt::entity {entt::null};
	};
	const auto forestOf = [&links](gamesave::ObjectNumber number) {
		return number != 0 && links.forestOf ? links.forestOf(number) : 0U;
	};
	// the list's last first: each is pushed at the front of the game's spells, so the newest ends first again
	for (const auto& planned : spells | std::views::reverse)
	{
		const auto& fields = FieldsOf(planned);
		// a creature spell on a creature the load did not make is not made: its record stays as the load kept it
		if (const auto* onCreature = std::get_if<gamesave::SpellCreatureRecord>(&planned.record);
		    onCreature != nullptr && onCreature->object.number != 0 && entityOf(onCreature->object.number) == entt::null)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Load: spell {} is kept as read: its creature is not made", planned.number);
			}
			continue;
		}
		// so is a forest spell naming a forest the load did not make
		if (const auto* forest = std::get_if<gamesave::SpellForestRecord>(&planned.record);
		    forest != nullptr && forest->object.number != 0 && forestOf(forest->object.number) == 0)
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_INFO(logger, "Load: spell {} is kept as read: its forest is not made", planned.number);
			}
			continue;
		}
		const auto seed = entityOf(fields.objects[k_Seed].number);
		auto values = SpellOf(fields, planned.creatorKind, planned.player, entityOf(fields.objects[k_Creator].number), seed);
		std::unique_ptr<particles::Effect> effect;
		if (planned.effect.has_value())
		{
			const auto type = static_cast<ParticleType>(planned.effect->manager.persistent.first);
			const auto fileName = particles::ParticleTypeFile(type);
			std::string why = fileName.empty() ? std::string("its particle type has no file") : std::string();
			if (!fileName.empty())
			{
				effect = saved_particles::RestoreEffect(*planned.effect, particles::File::Load(std::string(fileName)),
				                                        {.entityOf = entityOf}, why);
			}
			if (effect == nullptr)
			{
				// without its effect the spell is not made: its record stays as the load kept it
				if (auto logger = spdlog::get("game"); logger != nullptr)
				{
					SPDLOG_LOGGER_INFO(logger, "Load: spell {} is kept as read: its effect: {}", planned.number, why);
				}
				continue;
			}
		}
		const auto entity = magic::RestoreSpell(values, std::move(effect));
		if (const auto* water = std::get_if<gamesave::SpellWaterRecord>(&planned.record); water != nullptr)
		{
			registry.Assign<magic::water::SpellWaterData>(
			    entity, magic::water::SpellWaterData {.lastRipple = std::bit_cast<float>(water->word)});
			registry.Assign<components::LoadedRecord<gamesave::SpellWaterRecord>>(entity, *water);
		}
		else if (const auto* shield = std::get_if<gamesave::SpellShieldRecord>(&planned.record); shield != nullptr)
		{
			// its world shields, the newest first, each naming it as its spell
			for (const auto number : shield->objects.numbers | std::views::reverse)
			{
				const auto object = entityOf(number);
				if (object == entt::null)
				{
					continue;
				}
				magic::spell_objects::Add(entity, object);
				if (auto* world = registry.TryGet<components::MagicShield>(object); world != nullptr)
				{
					world->spell = entity;
					world->magicType = values.magicType;
				}
			}
			registry.Assign<components::LoadedRecord<gamesave::SpellShieldRecord>>(entity, *shield);
		}
		else if (const auto* onCreature = std::get_if<gamesave::SpellCreatureRecord>(&planned.record); onCreature != nullptr)
		{
			// the creature it was cast on and whether the hand has poured onto it, as the game's load reads them
			if (onCreature->object.number != 0 || onCreature->byte != 0)
			{
				registry.Assign<magic::SpellCreatureData>(
				    entity, magic::SpellCreatureData {.creature = entityOf(onCreature->object.number),
				                                      .handPoured = onCreature->byte != 0});
			}
			registry.Assign<components::LoadedRecord<gamesave::SpellCreatureRecord>>(entity, *onCreature);
		}
		else if (const auto* plain = std::get_if<gamesave::SpellRecord>(&planned.record); plain != nullptr)
		{
			// a plain spell keeps nothing of its own: what it does lives in its effect's rules
			registry.Assign<components::LoadedRecord<gamesave::SpellRecord>>(entity, *plain);
		}
		else if (const auto* heal = std::get_if<gamesave::SpellHealRecord>(&planned.record); heal != nullptr)
		{
			// a healing spell keeps nothing of its own either: its targets live in its effect
			registry.Assign<components::LoadedRecord<gamesave::SpellHealRecord>>(entity, *heal);
		}
		else if (const auto* storm = std::get_if<gamesave::SpellStormAndTornadoRecord>(&planned.record); storm != nullptr)
		{
			// its own (the cast swirl, the water reaction) is not saved: it joins the storm spells below with none
			registry.Assign<components::LoadedRecord<gamesave::SpellStormAndTornadoRecord>>(entity, *storm);
		}
		else if (const auto* forest = std::get_if<gamesave::SpellForestRecord>(&planned.record); forest != nullptr)
		{
			// whether it has made its forest, the forest it made and the most trees it makes, as the game's load reads them
			registry.Assign<magic::SpellForestData>(entity,
			                                        magic::SpellForestData {.forestId = forestOf(forest->object.number),
			                                                                .forestCreated = forest->byte != 0,
			                                                                .maxTrees = static_cast<int>(forest->word)});
			registry.Assign<components::LoadedRecord<gamesave::SpellForestRecord>>(entity, *forest);
		}
		else if (const auto* teleport = std::get_if<gamesave::SpellTeleportRecord>(&planned.record); teleport != nullptr)
		{
			// its stones, in the order its list holds them (the newest first); each is given the spell its own record
			// names once the spells are made (LinkTeleportStones)
			for (const auto number : teleport->objects.numbers | std::views::reverse)
			{
				if (const auto object = entityOf(number); object != entt::null)
				{
					magic::spell_objects::Add(entity, object);
				}
			}
			registry.Assign<components::LoadedRecord<gamesave::SpellTeleportRecord>>(entity, *teleport);
		}
		else
		{
			std::visit(
			    [&registry, &entityOf, entity](const auto& record) {
				    if constexpr (requires { record.flock; })
				    {
					    RestoreFlock(registry, entity, record.flock, record.objects, entityOf);
					    using Record = std::decay_t<decltype(record)>;
					    registry.Assign<components::LoadedRecord<Record>>(entity, record);
				    }
			    },
			    planned.record);
		}
		if (seed != entt::null)
		{
			if (auto* component = registry.TryGet<components::SpellSeed>(seed); component != nullptr)
			{
				component->spell = entity;
			}
		}
		made.emplace(planned.number, entity);
		const auto id = registry.Get<const components::Spell>(entity).effect;
		if (keepSource && id != 0 && planned.effect.has_value())
		{
			keepSource(planned.effect->interfaceNumber, gamesave::ParticleInterfaceRecord::k_Type, id);
			keepSource(planned.effect->managerNumber, gamesave::ParticleManagerRecord::k_Type, id);
		}
		if (keep)
		{
			keep(planned.number, entity,
			     std::visit([](const auto& record) { return std::decay_t<decltype(record)>::k_Type; }, planned.record));
		}
	}
	// the shield spells join the shield spells' list as the game's load makes them, in their objects' order (each at
	// the list's head), each with its anti-influence rings, the newest first; its town is not saved, so it has none
	std::vector<std::pair<gamesave::ObjectNumber, const gamesave::SpellShieldRecord*>> shields;
	for (const auto& planned : spells)
	{
		if (const auto* shield = std::get_if<gamesave::SpellShieldRecord>(&planned.record);
		    shield != nullptr && made.contains(planned.number))
		{
			shields.emplace_back(planned.number, shield);
		}
	}
	std::ranges::sort(shields, {}, &std::pair<gamesave::ObjectNumber, const gamesave::SpellShieldRecord*>::first);
	for (const auto& [number, shield] : shields)
	{
		auto& data = magic::spell_shield::AddLoaded(made.at(number));
		for (const auto ring : shield->shields.numbers)
		{
			if (const auto entity = entityOf(ring); entity != entt::null)
			{
				data.rings.push_back(entity);
			}
		}
	}
	// the storm spells join the storm spells' list the same way, in their objects' order (each at the list's head), the
	// newest first, each with no cast swirl and no water reaction
	std::vector<gamesave::ObjectNumber> storms;
	for (const auto& planned : spells)
	{
		if (std::holds_alternative<gamesave::SpellStormAndTornadoRecord>(planned.record) && made.contains(planned.number))
		{
			storms.push_back(planned.number);
		}
	}
	std::ranges::sort(storms);
	for (const auto number : storms)
	{
		magic::spell_storm::AddLoaded(made.at(number));
	}
	return made;
}

std::vector<saved_spells::PlannedTeleportStone>
saved_spells::PlanTeleportStones(const gamesave::WorldRead& world, const std::vector<gamesave::ObjectNumber>& list,
                                 const std::function<std::optional<PlayerNames>(gamesave::ObjectNumber)>& playerOf)
{
	std::vector<PlannedTeleportStone> stones;
	for (const auto number : list)
	{
		const auto* record = RecordIn(world, number);
		const auto* stone = record != nullptr ? std::get_if<gamesave::MagicTeleportRecord>(record) : nullptr;
		if (stone == nullptr)
		{
			continue;
		}
		PlannedTeleportStone planned {.number = number, .record = *stone};
		if (stone->player.number != 0 && playerOf)
		{
			planned.player = playerOf(stone->player.number);
		}
		// its place in its player's list of stones, which the player's record holds
		const auto* owner = RecordIn(world, stone->player.number);
		if (const auto* player = owner != nullptr ? std::get_if<gamesave::PlayerRecord>(owner) : nullptr)
		{
			if (const auto found = std::ranges::find(player->teleports, number); found != player->teleports.end())
			{
				planned.place = static_cast<size_t>(std::distance(player->teleports.begin(), found));
			}
		}
		stones.push_back(std::move(planned));
	}
	return stones;
}

saved_spells::MadeSpells
saved_spells::MakeTeleportStones(const std::vector<PlannedTeleportStone>& stones,
                                 const std::function<void(gamesave::ObjectNumber, entt::entity, uint32_t)>& keep)
{
	MadeSpells made;
	auto& registry = Locator::entitiesRegistry::value();
	// those no list names first, then the lists' last first: each made is the newest of its player's stones
	std::vector<const PlannedTeleportStone*> order;
	order.reserve(stones.size());
	for (const auto& planned : stones)
	{
		order.push_back(&planned);
	}
	std::ranges::stable_sort(order, [](const PlannedTeleportStone* left, const PlannedTeleportStone* right) {
		const auto rank = [](const PlannedTeleportStone* stone) {
			return stone->place.has_value() ? static_cast<int64_t>(*stone->place) : std::numeric_limits<int64_t>::max();
		};
		return rank(left) > rank(right);
	});
	for (const auto* planned : order)
	{
		const auto& base = planned->record.base;
		const auto entity = magic::teleport::Restore(MetresOf(base.position.position), planned->player);
		// its X, Y and Z angles, as read (a stone is made upright)
		const glm::vec3 angles {std::bit_cast<float>(planned->record.mobileStatic.words[0]), base.object.heading,
		                        std::bit_cast<float>(planned->record.mobileStatic.words[1])};
		if (angles != glm::vec3(0.0f))
		{
			ecs::placed_turn::SetAngles(registry, entity, angles);
		}
		registry.AssignOrReplace<components::ObjectCreationIndex>(entity, base.object.creationIndex);
		registry.Assign<components::LoadedRecord<gamesave::MagicTeleportRecord>>(entity, planned->record);
		if (keep)
		{
			keep(planned->number, entity, gamesave::MagicTeleportRecord::k_Type);
		}
		made.emplace(planned->number, entity);
	}
	return made;
}

void saved_spells::LinkTeleportStones(const std::vector<PlannedTeleportStone>& stones, const MadeSpells& made,
                                      const MadeSpells& spells,
                                      const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& planned : stones)
	{
		const auto stone = made.find(planned.number);
		if (stone == made.end())
		{
			continue;
		}
		auto& component = registry.Get<components::TeleportStone>(stone->second);
		const auto spell = planned.record.spell.number != 0 ? spells.find(planned.record.spell.number) : spells.end();
		component.spell = spell != spells.end() ? spell->second : entt::entity {entt::null};
		const auto reaction =
		    planned.record.reaction.number != 0 ? reactionIds.find(planned.record.reaction.number) : reactionIds.end();
		component.reaction = reaction != reactionIds.end() ? reaction->second : 0U;
	}
}

void saved_spells::LinkReactions(const std::vector<PlannedSpell>& spells, const MadeSpells& made,
                                 const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto idOf = [&reactionIds](gamesave::ObjectNumber number) {
		const auto found = number != 0 ? reactionIds.find(number) : reactionIds.end();
		return found != reactionIds.end() ? found->second : 0U;
	};
	for (const auto& planned : spells)
	{
		const auto spell = made.find(planned.number);
		if (spell == made.end())
		{
			continue;
		}
		registry.Get<components::Spell>(spell->second).reaction = idOf(FieldsOf(planned).other.number);
		if (const auto* shield = std::get_if<gamesave::SpellShieldRecord>(&planned.record); shield != nullptr)
		{
			auto& data = registry.Get<magic::SpellShieldData>(spell->second);
			data.struckReaction = idOf(shield->others[0].number);
			data.shieldReaction = idOf(shield->others[1].number);
		}
	}
}
