/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
enum class TempleRoom
{
	ChallengeRoom,
	CreatureCave,
	CreditsRoom,
	MainRoom,
	MultiplayerRoom,
	OptionsRoom,
	SaveGameRoom
};

struct TempleInteriorPart
{
	TempleRoom room;
};

struct Temple
{
	PlayerNames owner;
};

/// The building state of a CitadelPart (the CitadelHeart, a WorshipSite): MultiMapFixed +0x58 / +0x5C / +0x74, as
/// components::Abode keeps them for the abodes (ecs::abodes and ecs::building_sites read both)
struct CitadelPartBuild
{
	/// +0x58 bit 1: under construction (MultiMapFixed ctor 0x52E1E0: (underConstruction & 1) << 1)
	static constexpr uint32_t k_UnderConstruction = 0x2;
	/// +0x58 bit 2: not repaired (a rebuild plan's heart, 0x467F90; cleared by MultiMapFixed::Repaired 0x52EC8D)
	static constexpr uint32_t k_NotRepaired = 0x4;
	/// +0x58 bit 3: the ctor without underConstruction (0x52E234), MultiMapFixed::Built 0x52EC3A
	static constexpr uint32_t k_Built = 0x8;
	uint32_t buildFlags {k_Built};
	/// +0x5C PercentBuilt (GetPercentBuilt 0x4014F0): the ctor's percent, 0 under construction
	float percentBuilt {1.0f};
	/// +0x74 the building site (components::BuildingSite's entity), null without one
	entt::entity buildingSite {entt::null};
};

/// CitadelHeart (0xE8 bytes, vtable 0x8C8D00; ctor 0x4649B0) on the temple entity, with components::Temple and
/// CitadelPartBuild. The first heart of a player's citadel also carries the Citadel (components::CitadelWorship)
struct CitadelHeart
{
	/// +0x80 CitadelPart::citadel (GetCitadel 0x464A80): the entity with the CitadelWorship
	entt::entity citadel {entt::null};
	/// +0x94 the town of the plan it was made from (0x467F83); its GetTown (vt +0x48, MultiMapFixed 0x4220A0) is still
	/// 0
	entt::entity town {entt::null};
	/// +0xDC the CitadelEntrance (CallVirtualFunctionsForCreation 0x4676CD)
	entt::entity entrance {entt::null};
	/// GetScale (vt +0x120): the plan's (or 1.0 for CREATE_CITADEL); it only feeds the influence (0x464A2C), the temple
	/// is drawn at scale 1 (0x46761F)
	float scale {1.0f};
	/// +0x40 +0x9C, the LH3DCitadel's percent (SetPercent 0x883120, from Citadel::Process 0x462D70): the partly built
	/// temple is drawn below 1 (0x882A40, abodes::RedrawConstruction)
	float drawPercent {0.0f};
};

/// CitadelEntrance (0x68 bytes, vtable 0x8CA294; ctor 0x468EB0): the temple's door, tapped to go inside
struct CitadelEntrance
{
	entt::entity heart {entt::null}; ///< +0x54
};
} // namespace openblack::ecs::components
