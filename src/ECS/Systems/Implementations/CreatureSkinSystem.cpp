/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureSkinSystem.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DSkinFormat.h"
#include "3D/LowResolutionTextures.h"
#include "3D/PlayerOwnSymbols.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/BloodTrails.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Visual/DetailLevel.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
/// A creature mesh's file, for its skins' texels, as the creature loaders read it at start-up; nothing when it was
/// not read
const l3d::L3DFile* MeshFile(const std::string& name)
{
	if (name.empty())
	{
		return nullptr;
	}
	const auto& files = Locator::resources::value().GetL3DFiles();
	const auto id = entt::hashed_string(("creature/skins/" + name).c_str()).value();
	return files.Contains(id) ? &*files.Handle(id) : nullptr;
}

std::span<const uint16_t> TexelsOf(const l3d::L3DTexture& skin)
{
	return {reinterpret_cast<const uint16_t*>(skin.texels.data()), skin.texels.size()};
}

/// The skins as the body is drawn: the base mesh's, and the evil or good mesh's it is blended towards, the base's
/// where the species has none
struct SkinSources
{
	const l3d::L3DFile* base;
	const l3d::L3DFile* variant;
	entt::id_type baseId;
	entt::id_type variantId;
};

std::optional<SkinSources> SourcesOf(const CreatureRig& rig, float evilGood)
{
	const auto variantMesh = evilGood < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
	const auto& baseName = rig.meshNames.at(static_cast<size_t>(CreatureRig::Mesh::Base));
	const auto& variantName = rig.meshNames.at(static_cast<size_t>(variantMesh));
	const auto* base = MeshFile(baseName);
	if (base == nullptr)
	{
		return std::nullopt;
	}
	const auto* variant = rig.hasMesh.at(static_cast<size_t>(variantMesh)) ? MeshFile(variantName) : nullptr;
	return SkinSources {
	    .base = base,
	    .variant = variant != nullptr ? variant : base,
	    .baseId = entt::hashed_string(baseName.c_str()).value(),
	    .variantId = entt::hashed_string((variant != nullptr ? variantName : baseName).c_str()).value(),
	};
}

void Paint(CreatureSkin& skin, const SkinSources& sources, uint8_t weight, const CreatureTattoos& tattoos,
           const creature_tattoo::Masks* masks, const CreatureMarks& marks, const CreatureRig& rig,
           const creature_skin::Art* art)
{
	const auto& baseSkins = sources.base->GetSkins();
	const auto& variantSkins = sources.variant->GetSkins();
	std::vector<uint32_t> baseOrder;
	std::vector<uint32_t> variantOrder;
	std::ranges::transform(baseSkins, std::back_inserter(baseOrder), &l3d::L3DTexture::id);
	std::ranges::transform(variantSkins, std::back_inserter(variantOrder), &l3d::L3DTexture::id);

	// At the lowest detail levels the game keeps the models' skins at half size, as its system-memory textures copy a
	// skin into a half-size surface: each texel the mean of a square of four, each channel rounded down (an opaque skin
	// the mean of its 5-bit colours, kept opaque); the creature's skin is then blended and painted at that size
	const bool halfSize = Locator::config::has_value() && Locator::config::value().halfSizePictures;
	const auto loaded = [halfSize](const l3d::L3DFile& file, std::span<const uint16_t> texels) -> std::vector<uint16_t> {
		if (halfSize)
		{
			const auto flags = static_cast<uint32_t>(file.GetHeader().flags);
			auto half = l3d_skin::HasOpaqueSkins(flags) ? low_resolution_textures::HalveByMean555Opaque(texels)
			                                            : low_resolution_textures::HalveByMean4444(texels);
			if (half.has_value())
			{
				return *std::move(half);
			}
		}
		return {texels.begin(), texels.end()};
	};
	skin.skins.resize(baseSkins.size());
	for (size_t i = 0; i < baseSkins.size(); ++i)
	{
		const auto& base = baseSkins[i];
		auto& painted = skin.skins[i];
		painted.id = base.id;
		const auto baseTexels = loaded(*sources.base, TexelsOf(base));
		painted.texels.resize(baseTexels.size());
		// A neutral creature, or one whose evil or good mesh lacks the skin, starts from its base skin
		std::vector<uint16_t> variant;
		if (sources.variant != sources.base)
		{
			if (const auto paired = creature_skin::PairedSkin(baseOrder, variantOrder, base.id); paired.has_value())
			{
				const auto found = std::ranges::find(variantSkins, *paired, &l3d::L3DTexture::id);
				if (found != variantSkins.end())
				{
					variant = loaded(*sources.variant, TexelsOf(*found));
				}
			}
		}
		creature_skin::Compose(painted.texels, baseTexels, variant, weight,
		                       {.skinIndex = static_cast<uint8_t>(i),
		                        .tattoos = tattoos.slots,
		                        .sites = rig.tattooSites,
		                        .marks = marks.marks,
		                        .art = art,
		                        .masks = masks});
	}
	++skin.revision;
}

/// The number of the player whose masks a creature's tattoos are painted with: its own player's, the local player's for
/// a creature of no player
std::optional<size_t> MasksPlayerOf(PlayerNames owner)
{
	const auto player = static_cast<size_t>(owner == PlayerNames::NEUTRAL ? creature::LocalPlayer() : owner);
	return player < static_cast<size_t>(PlayerNames::_COUNT) ? std::optional(player) : std::nullopt;
}

static_assert(std::is_same_v<creature_tattoo::Pictures, player_own_symbols::Pictures>,
              "the own pictures are kept as the tattoos' pictures");
} // namespace

void CreatureSkinSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& resources = Locator::resources::value();
	const auto& rigs = resources.GetCreatureRigs();
	const auto& arts = resources.GetCreatureSkinArt();
	const creature_skin::Art* art = arts.Contains(creature_skin::k_ArtId) ? &*arts.Handle(creature_skin::k_ArtId) : nullptr;

	// The tattoos a creature is painted with: the local player's creature's for the local player's creatures, and for
	// every creature while the local player is inside the temple; its own otherwise
	const auto local = creature::LocalPlayer();
	const auto localCreature = Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(local)
	                                                              : std::optional<entt::entity> {};
	const auto* localTattoos = localCreature.has_value() && registry.Valid(*localCreature)
	                               ? std::as_const(registry).TryGet<const CreatureTattoos>(*localCreature)
	                               : nullptr;
	const bool insideTemple = Locator::time::has_value() && game_clock::IsInsideCitadel();

	// The creatures are found through the const registry, which makes no storage when there is none
	std::as_const(registry)
	    .Each<const Creature, const CreatureMorph, const CreatureTattoos, const CreatureMarks, const CreatureSkin>(
	        [&](entt::entity entity, const Creature& creature, const CreatureMorph& morph, const CreatureTattoos& tattoos,
	            const CreatureMarks& marks, const CreatureSkin&) {
		        auto& skin = registry.Get<CreatureSkin>(entity);
		        const auto rigId = creature::GetRigId(creature.species);
		        if (!rigs.Contains(rigId))
		        {
			        return;
		        }
		        const auto& rig = *rigs.Handle(rigId);
		        // The skins follow the alignment the body is drawn with, which moves on only past the morph threshold
		        const auto sources = SourcesOf(rig, morph.drawn.evilGood);
		        if (!sources)
		        {
			        return;
		        }
		        const CreatureSkin::Painted wanted {
		            .baseMesh = sources->baseId,
		            .variantMesh = sources->variantId,
		            .weight = creature_skin::BlendWeight(morph.drawn.evilGood),
		            .tattoos = tattoos.revision,
		            .marks = marks.revision,
		            .art = art != nullptr,
		        };
		        if (skin.painted == wanted)
		        {
			        return;
		        }
		        const bool shown =
		            creature_skin::ShowsLocalCreaturesTattoos(creature.owner == local, insideTemple, localTattoos != nullptr);
		        // painted with its own player's masks, also when it shows the local player's creature's tattoos
		        const auto player = MasksPlayerOf(creature.owner);
		        const auto* masks = player.has_value() ? &_tattooMasks.at(*player) : nullptr;
		        Paint(skin, *sources, wanted.weight, shown ? *localTattoos : tattoos, masks, marks, rig, art);
		        skin.painted = wanted;
	        },
	        entt::exclude<Unavailable>);
}

void CreatureSkinSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> marked;
	std::as_const(registry).Each<const CreatureMarks>(
	    [&marked](entt::entity entity, const CreatureMarks&) { marked.push_back(entity); });
	BeginCreatureTurns();
	for (const auto entity : marked)
	{
		ProcessCreatureTurn(entity);
	}
	EndCreatureTurns();
}

void CreatureSkinSystem::BeginCreatureTurns() {}

void CreatureSkinSystem::ProcessCreatureTurn(entt::entity creature)
{
	// its marks heal a little
	auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature);
	if (marks != nullptr && creature_marks::Heal(marks->marks, creature_marks::k_CountsPerTurn))
	{
		++marks->revision;
	}
}

void CreatureSkinSystem::EndCreatureTurns() {}

void CreatureSkinSystem::SetTattoo(entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo)
{
	auto* tattoos = Locator::entitiesRegistry::value().TryGet<CreatureTattoos>(creature);
	if (tattoos == nullptr || slot >= tattoos->slots.size() || tattoos->slots.at(slot) == tattoo)
	{
		return;
	}
	tattoos->slots.at(slot) = tattoo;
	++tattoos->revision;
}

void CreatureSkinSystem::RemakeTattooMasks(entt::entity creature, creature_tattoo::Sheet sheet)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	const auto* self = registry.TryGet<const Creature>(creature);
	auto* tattoos = registry.TryGet<CreatureTattoos>(creature);
	if (self == nullptr || tattoos == nullptr)
	{
		return;
	}
	const auto& arts = Locator::resources::value().GetCreatureSkinArt();
	const auto player = MasksPlayerOf(self->owner);
	// a creature with no block of pictures makes no masks
	if (player.has_value() && tattoos->pictures.has_value() && arts.Contains(creature_skin::k_ArtId))
	{
		const auto& art = *arts.Handle(creature_skin::k_ArtId);
		creature_tattoo::RemakeMasks(_tattooMasks.at(*player), tattoos->slots, *tattoos->pictures,
		                             sheet == creature_tattoo::Sheet::Shipped ? art.shippedDesigns : art.ownDesigns);
	}
	++tattoos->revision;
}

void CreatureSkinSystem::PutOnTattoos(entt::entity creature, const creature_tattoo::Slots& slots)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tattoos = registry.Valid(creature) ? registry.TryGet<CreatureTattoos>(creature) : nullptr;
	if (tattoos == nullptr)
	{
		return;
	}
	tattoos->slots = slots;
	tattoos->pictures = creature_tattoo::PicturesBlock(slots, player_own_symbols::Kept());
	RemakeTattooMasks(creature, creature_tattoo::Sheet::OwnPictures);
}

const creature_tattoo::Masks* CreatureSkinSystem::TattooMasks(PlayerNames player) const
{
	const auto index = static_cast<size_t>(player);
	return index < _tattooMasks.size() ? &_tattooMasks.at(index) : nullptr;
}

void CreatureSkinSystem::AddWound(entt::entity creature, const creature_marks::Mark& wound)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature); marks != nullptr)
	{
		creature_marks::Add(marks->marks.wounds, wound);
		++marks->revision;
	}
}

void CreatureSkinSystem::AddBlood(entt::entity creature, const creature_marks::Mark& blood)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature); marks != nullptr)
	{
		creature_marks::Add(marks->marks.blood, blood);
		++marks->revision;
	}
}

void CreatureSkinSystem::Heal(entt::entity creature, uint32_t counts)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature);
	    marks != nullptr && creature_marks::Heal(marks->marks, counts))
	{
		++marks->revision;
	}
}

void CreatureSkinSystem::Bleed(entt::entity creature, const creature_marks::MeshCut& cut)
{
	if (auto* marks = Locator::entitiesRegistry::value().TryGet<CreatureMarks>(creature); marks != nullptr)
	{
		creature_marks::StartTrail(marks->trails, cut);
		// (openblack, a readout at debug) a bleeding cut starts a trail of blood down the skin
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			log->debug("Creature {} bleeds: a trail of blood starts down its skin, turn {}", entt::to_integral(creature),
			           game_clock::Turn());
		}
	}
}

void CreatureSkinSystem::BleedTurnOf(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto* marks = lookup.Valid(creature) ? lookup.TryGet<const CreatureMarks>(creature) : nullptr;
	if (marks == nullptr ||
	    !std::ranges::any_of(marks->trails, [](const auto& trail) { return trail.ms < creature_marks::k_TrailRunsMs; }))
	{
		return;
	}
	const auto* played = lookup.TryGet<const CreatureAnimation>(creature);
	const auto lived = creature_turn_pose::LivedMs(played != nullptr ? played->playbackScale : 1.0f,
	                                               static_cast<int32_t>(game_clock::MsPerTurn()));
	auto& trails = registry.Get<CreatureMarks>(creature).trails;
	for (const auto& drop : blood_trails::Step(lookup, creature, trails, lived))
	{
		AddBlood(creature, drop);
	}
}

void CreatureSkinSystem::BleedTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> bleeding;
	lookup.Each<const CreatureMarks>([&bleeding](entt::entity entity, const CreatureMarks& marks) {
		if (std::ranges::any_of(marks.trails, [](const auto& trail) { return trail.ms < creature_marks::k_TrailRunsMs; }))
		{
			bleeding.push_back(entity);
		}
	});
	const auto msPerTurn = static_cast<int32_t>(game_clock::MsPerTurn());
	for (const auto entity : bleeding)
	{
		const auto* played = lookup.TryGet<const CreatureAnimation>(entity);
		const auto lived = creature_turn_pose::LivedMs(played != nullptr ? played->playbackScale : 1.0f, msPerTurn);
		auto& trails = registry.Get<CreatureMarks>(entity).trails;
		for (const auto& drop : blood_trails::Step(lookup, entity, trails, lived))
		{
			AddBlood(entity, drop);
		}
	}
}
