/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Profiler.h"

#include <cassert>
#include <cstdlib>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

void openblack::Profiler::Begin(Stage stage)
{
	assert(_currentLevel < 255);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	entry.level = _currentLevel;
	_currentLevel++;
	entry.start = std::chrono::system_clock::now();
	entry.finalized = false;
}

void openblack::Profiler::End(Stage stage)
{
	assert(_currentLevel > 0);
	auto& entry = _entries.at(_currentEntry).stages.at(static_cast<uint8_t>(stage));
	assert(!entry.finalized);
	_currentLevel--;
	assert(entry.level == _currentLevel);
	entry.end = std::chrono::system_clock::now();
	entry.finalized = true;
}

void openblack::Profiler::Frame()
{
	auto& prevEntry = _entries.at(_currentEntry);
	_currentEntry = (_currentEntry + 1) % k_BufferSize;
	prevEntry.frameEnd = _entries.at(_currentEntry).frameStart = std::chrono::system_clock::now();
	if (_summaryInterval > 0.0f && prevEntry.frameStart.time_since_epoch().count() != 0)
	{
		Accumulate(prevEntry);
	}
}

void openblack::Profiler::Accumulate(const Entry& entry)
{
	using Ms = std::chrono::duration<double, std::milli>;
	if (_summaryStart.time_since_epoch().count() == 0)
	{
		_summaryStart = entry.frameStart;
	}
	const double frame = Ms(entry.frameEnd - entry.frameStart).count();
	_framesTotal += frame;
	_framesWorst = std::max(_framesWorst, frame);
	++_frames;
	for (size_t i = 0; i < _totals.size(); ++i)
	{
		// The entries are a ring buffer: a stage that did not run this frame still holds an old time.
		const auto& stage = entry.stages.at(i);
		if (!stage.finalized || stage.start < entry.frameStart || stage.end > entry.frameEnd)
		{
			continue;
		}
		const double ms = Ms(stage.end - stage.start).count();
		_totals.at(i).total += ms;
		_totals.at(i).worst = std::max(_totals.at(i).worst, ms);
		++_totals.at(i).runs;
	}

	const double elapsed = Ms(entry.frameEnd - _summaryStart).count() / 1000.0;
	if (elapsed < _summaryInterval || _frames == 0)
	{
		return;
	}
	std::string text = fmt::format("Profile over {:.1f} s: {} frames, {:.1f} fps, frame avg {:.3f} ms, worst {:.3f} ms", elapsed,
	                               _frames, _frames / elapsed, _framesTotal / _frames, _framesWorst);
	for (size_t i = 0; i < _totals.size(); ++i)
	{
		const auto& totals = _totals.at(i);
		if (totals.runs == 0)
		{
			continue;
		}
		// avg per frame (the share of the frame budget), how often it ran, and its worst single run
		text += fmt::format("\n  {:<22} {:8.3f} ms/frame {:5.1f}%  ran {:5} x  worst {:8.3f} ms", k_StageNames.at(i),
		                    totals.total / _frames, 100.0 * totals.total / std::max(_framesTotal, 1e-9), totals.runs,
		                    totals.worst);
	}
	if (auto logger = spdlog::get("game"); logger)
	{
		SPDLOG_LOGGER_INFO(logger, "{}", text);
	}
	_totals = {};
	_framesTotal = 0.0;
	_framesWorst = 0.0;
	_frames = 0;
	_summaryStart = entry.frameEnd;
}
