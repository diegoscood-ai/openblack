/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpProfileFile.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "Locator.h"

namespace openblack::help::profile
{

static_assert(helpstats::k_EventCount == k_EventCount);
static_assert(helpstats::k_TimesKept == k_TimesKept);

helpstats::HelpStats ToHelpStats(const HelpProfile& profile)
{
	helpstats::HelpStats stats {};
	for (uint32_t event = 0; event < k_EventCount; ++event)
	{
		const auto kept = profile.Count(event).GetKept();
		stats.at(event) = {
		    .total = kept.total,
		    .smoothedRate = kept.smoothedRate,
		    .timeHead = static_cast<uint8_t>(kept.head),
		    .timeCount = static_cast<uint8_t>(kept.kept),
		    .triggeredThisTurn = static_cast<uint8_t>(kept.triggeredThisTurn ? 1 : 0),
		    .padding = 0,
		    .times = kept.times,
		};
	}
	return stats;
}

void FromHelpStats(HelpProfile& profile, const helpstats::HelpStats& stats)
{
	for (uint32_t event = 0; event < k_EventCount; ++event)
	{
		const auto& record = stats.at(event);
		profile.Count(event).SetKept({
		    .total = record.total,
		    .smoothedRate = record.smoothedRate,
		    .head = record.timeHead,
		    .kept = record.timeCount,
		    .triggeredThisTurn = record.triggeredThisTurn != 0,
		    .times = record.times,
		});
	}
}

std::vector<uint8_t> FileOf(const HelpProfile& profile)
{
	return helpstats::Write(ToHelpStats(profile));
}

bool TakeFile(HelpProfile& profile, const std::optional<std::vector<uint8_t>>& file)
{
	if (!file.has_value())
	{
		return false;
	}
	// A shorter file replaces only the start of the record, a longer one gives the record's size
	auto bytes = FileOf(profile);
	std::copy_n(file->begin(), std::min(file->size(), bytes.size()), bytes.begin());
	if (const auto stats = helpstats::Read(bytes); stats.has_value())
	{
		FromHelpStats(profile, *stats);
	}
	return true;
}

namespace
{
bool HasRecordAndProfile()
{
	return Locator::playerProfileSystem::has_value() && Locator::helpProfileSystem::has_value() &&
	       Locator::scriptState::has_value();
}
} // namespace

void KeepInProfile()
{
	if (!HasRecordAndProfile())
	{
		return;
	}
	const auto bytes = FileOf(std::as_const(Locator::helpProfileSystem::value()).Get());
	if (Locator::playerProfileSystem::value().WritePlayerFile(k_FileName, bytes))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Help record: kept in the profile's {}", k_FileName);
	}
}

void TakeFromProfile()
{
	if (!HasRecordAndProfile())
	{
		return;
	}
	if (TakeFile(Locator::helpProfileSystem::value().Get(), Locator::playerProfileSystem::value().ReadPlayerFile(k_FileName)))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Help record: read from the profile's {}", k_FileName);
	}
}

} // namespace openblack::help::profile
