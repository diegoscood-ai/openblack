/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicModel.h"

#include <cstring>

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "InfoConstants.h"
#include "Particles/ParticleTypes.h"

namespace openblack::debug::magic_window
{

namespace
{
constexpr std::array<std::string_view, static_cast<size_t>(magic::MagicInfoSection::_COUNT)> k_SectionNames {
    "General", "Heal", "Teleport", "Forest",       "Food",         "Storm and tornado",
    "Shield",  "Wood", "Water",    "Flying flock", "Ground flock", "Creature spell",
};

std::string_view NameOf(const std::array<char, 0x30>& name)
{
	return {name.data(), strnlen(name.data(), name.size())};
}
} // namespace

std::string_view SectionName(magic::MagicInfoSection section)
{
	const auto index = static_cast<size_t>(section);
	return index < k_SectionNames.size() ? k_SectionNames.at(index) : std::string_view("?");
}

std::string MagicName(const InfoConstants& info, MagicType type)
{
	const auto name = NameOf(magic::GetMagicEffectInfo(info, type).debugString);
	return name.empty() ? fmt::format("Magic type {}", static_cast<uint32_t>(type)) : std::string(name);
}

std::string SeedName(const InfoConstants& info, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return "none";
	}
	const auto name = NameOf(magic::GetSpellSeedInfo(info, seed).debugString);
	return name.empty() ? fmt::format("Seed {}", static_cast<int>(seed)) : std::string(name);
}

std::string Seconds(float seconds)
{
	return seconds < 0.0f ? std::string("no limit") : fmt::format("{:.1f} s", seconds);
}

std::string PowerUpLevelName(int level)
{
	return level < 0 ? std::string("base") : fmt::format("{}", level);
}

std::string ParticleTypeLabel(ParticleType type)
{
	const auto file = particles::ParticleTypeFile(type);
	return fmt::format("{} {} ({})", static_cast<uint32_t>(type), particles::ParticleTypeName(type),
	                   file.empty() ? std::string_view("no file") : file);
}

std::vector<entt::entity> TargetsInGivingOrder(std::vector<TargetCandidate> candidates, size_t count)
{
	std::ranges::stable_sort(candidates, {}, &TargetCandidate::distance);
	const auto taken = std::min(candidates.size(), count);
	std::vector<entt::entity> order;
	order.reserve(taken);
	for (size_t i = taken; i > 0; --i)
	{
		order.push_back(candidates.at(i - 1).entity);
	}
	return order;
}

std::string_view DrawPathName(particles::DrawPath path)
{
	switch (path)
	{
	case particles::DrawPath::Sorted:
		return "Sorted";
	case particles::DrawPath::Queued:
		return "Queued";
	case particles::DrawPath::Immediate:
		return "Immediate";
	}
	return "?";
}

} // namespace openblack::debug::magic_window
