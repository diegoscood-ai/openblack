/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OneOffSpellSeed.h"

#include <cmath>

#include <glm/geometric.hpp>

#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Players.h"
#include "Resources/ResourcesInterface.h"
#include "SpellSeed.h"
#include "Worship/PlayerSpellIcons.h"
#include "Worship/SpellSeedGraphic.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

entt::entity one_off::Create(const glm::vec3& worldPosition, SpellSeedType seedType, int powerUp, float scale)
{
	const auto orb = ecs::archetypes::OneOffSpellSeedArchetype::Create(worldPosition, seedType, powerUp, scale);
	if (orb != entt::null)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic: one-shot orb {} (seed {}, pu {}) at ({:.1f}, {:.1f}, {:.1f})",
		                   static_cast<uint32_t>(orb), static_cast<int>(seedType), powerUp, worldPosition.x,
		                   worldPosition.y, worldPosition.z);
	}
	return orb;
}

entt::entity one_off::CreateSpellIntoHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier)
{
	if (!Locator::handSystem::has_value() || static_cast<int>(seedType) < 0 || static_cast<int>(seedType) >= 30)
	{
		return entt::null;
	}
	auto& hand = Locator::handSystem::value();
	// GInterfaceStatus::IsHandReadyForObject 0x5DC890 (inf: nothing held)
	if (hand.GetHeldObject().has_value())
	{
		return entt::null;
	}
	// at the interface's hand position (+0xC8, +0xD0; altitude 0): GPlayer::FindBestSpellIconForSpellSeed 0x64BF40, and
	// with an icon of that seed fn_007282A0 (a seed of the icon, its creator the icon: Worship/WorshipSpellIcon.cpp),
	// else fn_00728300 (a free seed of that type)
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto hands = hand.GetPlayerHandPositions();
	const glm::vec3 handPosition = hands[static_cast<size_t>(Side::Right)].value_or(glm::vec3(0.0f));
	const auto icon = worship::player::FindBestSpellIconForSpellSeed(player, seedType);
	const auto entity = icon != entt::null ? worship::icon::CreateSeed(icon, handPosition, player, powerUp, multiplier)
	                                       : seed::Create(handPosition, seedType, player, powerUp, multiplier);
	auto& registry = Locator::entitiesRegistry::value();
	auto& component = registry.Get<SpellSeed>(entity);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seedType);
	players::SetMagicTypeEverBeenEnabled(player, GetMagicTypeFromPULevel(info, powerUp));
	component.fromOneShot = true;
	seed::AddToChantStore(component, seed::GetChantNeeded(component, powerUp));
	// GInterfaceStatus::PlaceObjectInMagicHand 0x5DC870 -> the hand holds it, SpellSeed::InterfaceSetInMagicHand
	hand.PlaceObjectInMagicHand(entity);
	if (seed::InterfaceSetInMagicHand(entity) != 1)
	{
		return entt::null;
	}
	seed::SetInactive(registry.Get<SpellSeed>(entity), false);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic: seed {} ({}, pu {}) in the hand with {:.0f} chants, ready {}, icon {}",
	                   static_cast<uint32_t>(entity), info.debugString.data(), powerUp,
	                   registry.Get<SpellSeed>(entity).chantStore, registry.Get<SpellSeed>(entity).ready,
	                   icon == entt::null ? -1 : static_cast<int>(icon));
	return entity;
}

int one_off::InterfaceTap(entt::entity orb, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto component = registry.Get<const OneOffSpellSeed>(orb); // a copy: the seed's creation adds entities
	if (CreateSpellIntoHand(player, component.seedType, component.powerUp, component.scale) == entt::null)
	{
		return 0;
	}
	// TODO(M2): GInterface::StartImmersion(0xE, 0x80000000)
	if (Locator::audio::has_value())
	{
		Locator::audio::value().PlaySound(static_cast<entt::id_type>(audio::SoundId::G_SpellBubblePop_04),
		                                  audio::PlayType::Once); // sample 0x6D
	}
	worship::seed_graphic::Delete(component.graphic); // ToBeDeleted 0x72A420: the seed graphic inside goes with it
	registry.Destroy(orb);
	registry.SetDirty();
	return 3;
}

void one_off::InterfaceSetInMagicHand(entt::entity orb, PlayerNames player)
{
	const auto& component = Locator::entitiesRegistry::value().Get<OneOffSpellSeed>(orb);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), component.seedType);
	players::SetMagicTypeEverBeenEnabled(player, GetMagicTypeFromPULevel(info, component.powerUp));
}

void one_off::UpdateFrames(float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	bool any = false;
	registry.Each<OneOffSpellSeed, UvScroll>([&](entt::entity /*orb*/, OneOffSpellSeed& orb, UvScroll& scroll) {
		orb.phase = std::fmod(orb.phase + milliseconds * 18.0f * 0.001f, 16.0f);
		const int frame = static_cast<int>(orb.phase);
		scroll.u = static_cast<float>(frame % 4) * 0.25f;
		scroll.v = static_cast<float>(frame / 4) * 0.25f;
		any = true;
	});
	// Draw 0x518E90 -> fn_00518720 (on while the byte [0xBE8E8D] is set, 1): the mesh is turned about the centre c of
	// its box (LH3DMesh::ComputeBoundingBox, all the submeshes) so that its +Y points at the camera. D = normalize(centre
	// - camera), U = normalize(Y - (Y.D) D), and the axes x, y, z go to U x D, -D, U. The visible submesh is the half
	// sphere above c, so the bubble looks round from every side. The object's position does not move (only its 3D
	// matrix does) and the physics sphere is the same after the turn.
	const auto camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	registry.Each<OneOffSpellSeed, const Transform, const Mesh>(
	    [&camera, hasCamera = Locator::camera::has_value()](OneOffSpellSeed& orb, const Transform& transform, const Mesh& mesh) {
		    if (!hasCamera)
		    {
			    return;
		    }
		    const auto l3d = Locator::resources::value().GetMeshes().Handle(mesh.id);
		    const glm::vec3 centre = l3d ? l3d->GetBoundingBox().Center() : glm::vec3(0.0f);
		    const glm::vec3 toOrb = transform.position + centre - camera;
		    if (glm::dot(toOrb, toOrb) <= 0.0f)
		    {
			    return;
		    }
		    const glm::vec3 d = glm::normalize(toOrb);
		    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f) - glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), d) * d;
		    if (glm::dot(up, up) <= 0.0f)
		    {
			    return;
		    }
		    const glm::vec3 u = glm::normalize(up);
		    orb.facing = glm::mat3(glm::cross(u, d), -d, u);
		    orb.facingOffset = centre - orb.facing * centre;
	    });
	if (any)
	{
		registry.SetDirty();
	}
}
