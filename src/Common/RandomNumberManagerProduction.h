/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <random>

#include "RandomNumberManager.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class RandomNumberManagerProduction final: public RandomNumberManagerInterface
{
public:
	/// The run's seed is the time the game starts at, as the one generator was seeded before
	RandomNumberManagerProduction();
	RandomNumberManagerProduction(const RandomNumberManagerProduction&) = delete;
	RandomNumberManagerProduction& operator=(const RandomNumberManagerProduction&) = delete;

	[[nodiscard]] uint32_t GetRunSeed() const override { return _runSeed; }
	void SetRunSeed(uint32_t seed) override;
	[[nodiscard]] std::mt19937& Stream(RandomStream stream) override;

private:
	std::mt19937& Generator() override;
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override;

	/// Only the main thread draws from this service, so the one generator a thread_local gave each thread is this one
	uint32_t _runSeed {0};
	std::mt19937 _generator;
	std::array<std::mt19937, static_cast<size_t>(RandomStream::Count)> _streams;
};
} // namespace openblack
