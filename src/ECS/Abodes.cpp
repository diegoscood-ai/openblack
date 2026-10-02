/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Abodes.h"

#include "Audio/Audio.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<AbodeType> abodes::TypeOf(entt::entity abode)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const Abode>(abode);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	// Abode::GetAbodeType 0x4061F0 reads the info record the abode was made with. openblack keeps the abode number
	// (AbodeArchetype), so the record is looked up by it and by the mesh, as influence::AbodeInfoOf does; (inferido)
	// every tribe's record of one abode number carries the same ABODE_TYPE bits.
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const auto meshId = mesh != nullptr ? mesh->id : 0;
	std::optional<AbodeType> byNumber;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != component->type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == meshId)
		{
			return info.abodeType;
		}
		if (!byNumber.has_value())
		{
			byNumber = info.abodeType;
		}
	}
	return byNumber;
}

bool abodes::InterfaceValidToTap(entt::entity abode)
{
	// 0x406820: `mov eax, 1`
	return Locator::entitiesRegistry::value().AllOf<Abode>(abode);
}

void abodes::InterfaceTap(entt::entity abode, const glm::vec3& handPosition)
{
	// 0x406864..0x406870: only an abode whose ABODE_TYPE has the living-quarters bit (test al, 2) knocks; the houses A..F
	// and the windmill have it, the civic buildings (totem, storage pit, creche, workshop, wonder, graveyard, town
	// centre, football pitch, spell dispenser, field) do not.
	const auto type = TypeOf(abode);
	if (!type.has_value() || (static_cast<uint32_t>(*type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) == 0)
	{
		return;
	}
	// 0x4068E2..0x40694A: GAudio::PlaySoundEffect 0x429E30 with bank InGame (GAudio+0x3AC), sample 110 G_KnockRoofMulti
	// + the counter [0xC4CC7C] (0..8 in turn, 0x4068F4..0x406919), owner the abode (+0x20), is3D 1 (+0x08), track 0
	// (+0x0C), at the interface status' +0xC8 (the hand's point); mode and loops stay the ctor's (3 and 0), and the .sad
	// gives the sample 5 % pitch spread and min / max 100 / 150.
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), 110 + audio::NextCounter(audio::Counter::KnockRoof)};
	options.owner = audio::Owner::Thing(abode);
	options.is3D = true;
	options.track = false;
	options.position = handPosition;
	audio::PlaySoundEffect(options);
}
