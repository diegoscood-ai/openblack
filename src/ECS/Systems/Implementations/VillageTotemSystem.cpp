/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillageTotemSystem.h"

#include <cstdint>

#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "3D/LandMorph.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/VillageTotem.h"
#include "InfoConstants.h"
#include "Input/GamePackets.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Worship/Citadel.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// How much the creature of the town's player thinks the player wanted to impress, as a totem is let go raised
constexpr float k_LetGoEmpathy = 0.5f;

VillageTotem* StatueOf(entt::entity totem)
{
	auto& registry = Locator::entitiesRegistry::value();
	return totem != entt::null && registry.Valid(totem) ? registry.TryGet<VillageTotem>(totem) : nullptr;
}

/// The totem's town: its town centre's
entt::entity TownOf(const VillageTotem& statue)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (statue.townCentre == entt::null || !registry.Valid(statue.townCentre))
	{
		return entt::null;
	}
	return ecs::abode_villagers::TownOf(statue.townCentre);
}

/// The totem's share is set: its town's (its own without a town), as given; the hand's share is then the one set,
/// which without a worship site stays as it was
void SetShareAndHold(entt::entity totem, VillageTotem& statue, float share)
{
	if (const auto town = TownOf(statue); town != entt::null)
	{
		worship::percentage::SetWorshipPercentage(town, share);
	}
	else
	{
		worship::percentage::SetStatuePercentage(totem, share);
	}
	statue.held = worship::percentage::GetStatuePercentage(totem);
}

/// The totems of town centres that have them, with their towns, in the order the registry keeps them
std::vector<std::pair<entt::entity, entt::entity>> TotemsAndTowns()
{
	std::vector<std::pair<entt::entity, entt::entity>> found;
	Locator::entitiesRegistry::value().Each<const VillageTotem>([&found](entt::entity totem, const VillageTotem& statue) {
		if (statue.onCentre)
		{
			found.emplace_back(totem, TownOf(statue));
		}
	});
	return found;
}

/// The totem icon of the species of the player's creature, by its place in the mesh pack; none without a creature
std::optional<int32_t> CreatureIconOf(PlayerNames player)
{
	if (!Locator::playerSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = creature.has_value() && registry.Valid(*creature) ? registry.TryGet<const Creature>(*creature) : nullptr;
	return ecs::village_totem::CreatureIcon(body != nullptr ? std::optional(body->species) : std::nullopt,
	                                        Locator::infoConstants::value().creature);
}
} // namespace

void VillageTotemSystem::AddToPlayer(entt::entity totem)
{
	const auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return;
	}
	const auto town = TownOf(*statue);
	if (town == entt::null)
	{
		worship::percentage::SetStatuePercentage(totem, 0.0f);
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// with a worship site it turns side on to it, its plinth and its icon alike
	const auto* magic = registry.TryGet<const TownMagic>(town);
	if (magic != nullptr && magic->worshipSite != entt::null && registry.Valid(magic->worshipSite))
	{
		const float angle =
		    ecs::village_totem::FacingSite(ecs::object::MapCoordsOf(totem), ecs::object::MapCoordsOf(magic->worshipSite));
		const auto rotation = affine::AngleY(angle);
		if (auto& plinth = registry.Get<Transform>(totem); plinth.rotation != rotation)
		{
			plinth.rotation = rotation;
			if (const auto icon = statue->icon; icon != entt::null && registry.Valid(icon))
			{
				registry.Get<Transform>(icon).rotation = rotation;
			}
			// turned, it goes to the head of its cells' lists
			ecs::map_cells::OnAnglesOrScaleChanged(totem);
			registry.SetDirty();
		}
	}
	SetPlayersCreature(totem);
	SetTownShare(town, 0.0f);
}

void VillageTotemSystem::SetPlayersCreature(entt::entity totem)
{
	auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto icon = statue->icon;
	auto* mesh = icon != entt::null && registry.Valid(icon) ? registry.TryGet<Mesh>(icon) : nullptr;
	const auto town = TownOf(*statue);
	if (mesh == nullptr)
	{
		return;
	}
	// the totem's player: its town's owner, the neutral player without a town
	const auto player = town != entt::null ? worship::town::OwnerOf(town) : PlayerNames::NEUTRAL;
	const auto index = ecs::village_totem::IconMesh(CreatureIconOf(player), static_cast<uint32_t>(MeshId::_COUNT));
	mesh->id = resources::HashIdentifier(static_cast<MeshId>(index));
	// the next frame places it again, without the share tooltip
	statue->quietOnce = true;
	statue->rise = ecs::village_totem::k_NeverPlaced;
	// it leaves its cells and goes back in at their head
	ecs::map_cells::OnAnglesOrScaleChanged(totem);
	registry.SetDirty();
}

void VillageTotemSystem::TownChangedPlayer(entt::entity town)
{
	if (town == entt::null)
	{
		return;
	}
	for (const auto& [totem, of] : TotemsAndTowns())
	{
		if (of == town)
		{
			AddToPlayer(totem);
		}
	}
}

void VillageTotemSystem::PlayersCreatureSet(PlayerNames player)
{
	// the totem of each of the player's towns' centre
	for (const auto& [totem, town] : TotemsAndTowns())
	{
		const auto* statue = StatueOf(totem);
		if (town != entt::null && worship::town::OwnerOf(town) == player && statue != nullptr &&
		    statue->townCentre == worship::town::TownCentreOf(town))
		{
			SetPlayersCreature(totem);
		}
	}
}

void VillageTotemSystem::WorldLoaded()
{
	// every totem, on a town centre or not, in the order the registry keeps them
	std::vector<entt::entity> totems;
	Locator::entitiesRegistry::value().Each<const VillageTotem>(
	    [&totems](entt::entity totem, const VillageTotem& /*statue*/) { totems.push_back(totem); });
	for (const auto totem : totems)
	{
		SetPlayersCreature(totem);
	}
}

void VillageTotemSystem::SetTownShare(entt::entity town, float share)
{
	worship::percentage::SetWorshipPercentage(town, share);
}

void VillageTotemSystem::Update(float /*gameMilliseconds*/)
{
	worship::percentage::UpdateTotems();
}

std::optional<entt::entity> VillageTotemSystem::TotemOf(entt::entity picked) const
{
	return worship::percentage::TotemOf(picked);
}

std::optional<entt::entity> VillageTotemSystem::TotemOfTownCentre(entt::entity centre) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (centre == entt::null || !registry.Valid(centre) || ecs::abodes::TypeOf(centre) != AbodeType::TownCentre)
	{
		return std::nullopt;
	}
	std::optional<entt::entity> found;
	registry.Each<const VillageTotem>([centre, &found](entt::entity totem, const VillageTotem& statue) {
		if (!found.has_value() && statue.townCentre == centre && statue.onCentre)
		{
			found = totem;
		}
	});
	return found;
}

std::optional<float> VillageTotemSystem::GetShare(entt::entity object) const
{
	if (StatueOf(object) == nullptr)
	{
		return std::nullopt;
	}
	return worship::percentage::GetStatuePercentage(object);
}

bool VillageTotemSystem::SetShareFromScript(entt::entity object, float share)
{
	auto* statue = StatueOf(object);
	if (statue == nullptr)
	{
		return false;
	}
	SetShareAndHold(object, *statue, share);
	return true;
}

std::optional<float> VillageTotemSystem::GetBuiltPercentage(entt::entity object) const
{
	const auto* statue = StatueOf(object);
	if (statue == nullptr)
	{
		return std::nullopt;
	}
	return statue->percentBuilt;
}

bool VillageTotemSystem::SetBuiltPercentageFromScript(entt::entity object, float percent)
{
	auto* statue = StatueOf(object);
	if (statue == nullptr)
	{
		return false;
	}
	statue->percentBuilt = ecs::village_totem::PercentBuiltSetTo(percent);
	return true;
}

bool VillageTotemSystem::SetLifeFromScript(entt::entity object, float life)
{
	if (StatueOf(object) == nullptr)
	{
		return false;
	}
	ecs::life::SetLife(object, life);
	return true;
}

bool VillageTotemSystem::ValidForLockedSelect(entt::entity totem, PlayerNames player) const
{
	const auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto town = TownOf(*statue);
	// the totem's player is its town's. (pending) with no town the original takes one of the game's players, not read
	const bool samePlayer = town != entt::null && worship::town::OwnerOf(town) == player;
	// functional: available, built (only a script makes it otherwise) and its life above the life it stops working at
	const bool functional = ecs::IsAvailable(totem) && ecs::abodes::IsBuilt(totem) &&
	                        ecs::abodes::GetPercentRepairedForNonFunctional(totem) < ecs::life::LifeOf(totem);
	const auto heart = worship::citadel::HeartOf(worship::citadel::Of(player));
	const bool heartBuilt = heart != entt::null && ecs::abodes::IsBuilt(heart);
	const auto* magic = town != entt::null ? registry.TryGet<const TownMagic>(town) : nullptr;
	const bool siteBuilt = magic != nullptr && magic->worshipSite != entt::null && registry.Valid(magic->worshipSite) &&
	                       ecs::abodes::IsBuilt(magic->worshipSite);
	return ecs::village_totem::ValidForLockedSelect(
	    {.samePlayer = samePlayer, .functional = functional, .heartBuilt = heartBuilt, .siteBuilt = siteBuilt});
}

bool VillageTotemSystem::IsGripped(entt::entity totem) const
{
	const auto* statue = StatueOf(totem);
	return statue != nullptr && statue->gripped;
}

void VillageTotemSystem::StartLockedSelect(entt::entity totem)
{
	auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return;
	}
	statue->held = worship::percentage::GetStatuePercentage(totem);
	// (not ported) the hand's immersion effect for the totem starts
	audio::guidance::StartTotemRaiseSound(statue->held);
}

void VillageTotemSystem::FriendlyStartLockedSelect(entt::entity totem, PlayerNames player)
{
	if (StatueOf(totem) == nullptr)
	{
		return;
	}
	// (not ported) the gripping player's creature, when it can see the player's hand, updates its desires as after
	// watching the player do something to impress it: openblack's creature mind has no such update yet
	// the living near it react to the hand using it, from this turn
	ecs::effects::reactions::CreateReaction(totem, openblack::Reaction::ReactToHandUsingTotem, player, true);
}

void VillageTotemSystem::SetGripped(entt::entity totem, bool gripped)
{
	if (auto* statue = StatueOf(totem); statue != nullptr)
	{
		statue->gripped = gripped;
	}
}

void VillageTotemSystem::Slide(entt::entity totem, float upPixels, float screenHeight)
{
	auto* statue = StatueOf(totem);
	if (statue == nullptr || !statue->gripped)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* icon =
	    statue->icon != entt::null && registry.Valid(statue->icon) ? registry.TryGet<const Transform>(statue->icon) : nullptr;
	if (icon == nullptr || !Locator::terrainSystem::has_value())
	{
		return; // (openblack guard) an icon always stands on the plinth
	}
	// the hand is where it holds the icon, over the totem's place, and the land is the land there
	const float iconHeight = ecs::object::GetHeight(statue->icon);
	const float handY = ecs::village_totem::GripY(icon->position.y, iconHeight);
	const auto place = map_coords::ToWorld(ecs::object::MapCoordsOf(totem));
	const float land = land_morph::CurrentAltitude()(glm::vec2(place.x, place.z));
	const float slide = ecs::village_totem::HandSlide(upPixels, screenHeight, iconHeight, handY - land);
	statue->held = ecs::village_totem::SlideShare(statue->held, slide);
}

std::optional<VillageTotemSystemInterface::HandHold> VillageTotemSystem::GetHandHold(entt::entity totem) const
{
	const auto* statue = StatueOf(totem);
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* icon = statue != nullptr && statue->icon != entt::null && registry.Valid(statue->icon)
	                       ? registry.TryGet<const Transform>(statue->icon)
	                       : nullptr;
	if (icon == nullptr)
	{
		return std::nullopt; // (openblack guard) an icon always stands on the plinth
	}
	// over the totem's place in the map, as high as the hand holds the icon now
	const auto place = map_coords::ToMetres(ecs::object::MapCoordsOf(totem));
	const float y = ecs::village_totem::GripY(icon->position.y, ecs::object::GetHeight(statue->icon));
	return HandHold {.position = {place.x, y, place.y}, .holdRadius = ecs::object::GetHoldRadius(statue->icon, true)};
}

void VillageTotemSystem::EndLockedSelect(entt::entity totem)
{
	const auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return;
	}
	game_packets::Packet packet {game_packets::Type::TotemShare, totem};
	packet.data[0] = statue->held;
	game_packets::Push(packet);
	audio::guidance::EndTotemRaiseSound(statue->held);
	// (not ported) the hand's immersion effect for the totem stops
}

void VillageTotemSystem::LetGo(entt::entity totem, float share)
{
	auto* statue = StatueOf(totem);
	if (statue == nullptr)
	{
		return;
	}
	if (const auto town = TownOf(*statue); town != entt::null)
	{
		// raised, the creature of the town's player ((inferred) a town of the neutral player has none) thinks the
		// player wanted to impress, if it can see the totem
		const auto owner = worship::town::OwnerOf(town);
		if (share > 0.0f && owner != PlayerNames::NEUTRAL && Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().EmpathiseWithPlayer(owner, CreatureDesires::ToImpress, k_LetGoEmpathy,
			                                                         map_coords::ToWorld(ecs::object::MapCoordsOf(totem)));
		}
	}
	SetShareAndHold(totem, *statue, share);
	// (not ported) the share floating up over the totem in grey, as a percentage
}
