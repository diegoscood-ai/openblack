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

#include <chrono>
#include <optional>

namespace openblack::game_clock
{
/// Where the clock reads its milliseconds from (the wall clock unless a test or the fixed step sets another)
using TickSource = uint32_t (*)();
} // namespace openblack::game_clock

namespace openblack::ecs::systems
{
/// The state behind game_clock: the game timer, the engine timer, the turn and the frame clock
class TimeSystemInterface
{
public:
	virtual ~TimeSystemInterface() = default;

	/// The ticks the clock reads; the music thread reads them too, so implementations make this thread-safe
	[[nodiscard]] virtual uint32_t TickCount() const noexcept = 0;
	/// Tests and the fixed step: nullptr = the wall clock
	virtual void SetTickSource(game_clock::TickSource source) noexcept = 0;
	/// The fixed step (OPENBLACK_FIXED_FRAME_MS): every frame the ticks go up by `frameMs`; 0 = the wall clock
	virtual void SetFixedFrameMs(uint32_t frameMs) noexcept = 0;
	[[nodiscard]] virtual uint32_t FixedFrameMs() const noexcept = 0;
	/// The fixed step's ticks go up by one frame
	virtual void AdvanceFixedFrame() noexcept = 0;

	/// The fixed step by the names of the time system: each frame takes this long, whatever the wall clock says, until
	/// it is set back to none. As SetFixedFrameMs, the fixed step's ticks start again from 0 when it is set
	virtual void SetFixedFrameTime(std::optional<std::chrono::milliseconds> frameTime)
	{
		SetFixedFrameMs(frameTime.has_value() ? static_cast<uint32_t>(frameTime->count()) : 0u);
	}
	[[nodiscard]] virtual std::optional<std::chrono::milliseconds> GetFixedFrameTime() const
	{
		const auto frameMs = FixedFrameMs();
		return frameMs != 0 ? std::optional(std::chrono::milliseconds(frameMs)) : std::nullopt;
	}

	/// The machine's millisecond count as the game reads it: TickCount(), counted from when the clock was last
	/// restarted (never, unless a deterministic run restarts it)
	[[nodiscard]] virtual uint32_t GetTicks() const = 0;
	/// Seconds since 1970 as the game reads the date: the wall clock's, or once pinned the pinned date and the ticks
	/// since
	[[nodiscard]] virtual int64_t GetUnixTime() const = 0;
	/// For a deterministic run: the ticks count from 0 from now, and the date is pinned to this one (none for the wall
	/// clock's again)
	virtual void RestartClock(std::optional<int64_t> unixTime) = 0;
	/// The date pinned for a deterministic run, none while the wall clock's date is read
	[[nodiscard]] virtual std::optional<int64_t> GetPinnedDate() const = 0;

	/// The date a seeded run pins unless given another: 1 January 2001, 12:00 UTC
	static constexpr int64_t k_DeterministicDate = 978350400;
	/// The length of a turn the scheduler and the frame clock use (game_clock::k_SchedulerMsPerTurn)
	static constexpr auto k_TurnDuration = std::chrono::milliseconds(100);

	virtual void Reset() noexcept = 0;
	virtual void StartEngineTimer() noexcept = 0;
	[[nodiscard]] virtual uint32_t MsPerTurn() const noexcept = 0;
	virtual void SetMsPerTurn(uint32_t ms) noexcept = 0;
	[[nodiscard]] virtual uint32_t GetTurn() const noexcept = 0;
	virtual void SetTurn(uint32_t turn) noexcept = 0;
	[[nodiscard]] virtual bool IsTurnScheduled() noexcept = 0;
	[[nodiscard]] virtual uint32_t TurnsThisFrame() const noexcept = 0;
	virtual void StartTurn() noexcept = 0;
	virtual void ResetLocalTimer() noexcept = 0;
	virtual void Start(bool paused) noexcept = 0;
	virtual void OnLoad() noexcept = 0;
	virtual void SetPaused(bool paused) noexcept = 0;
	[[nodiscard]] virtual bool IsPaused() const noexcept = 0;
	[[nodiscard]] virtual int32_t SequenceMode() const noexcept = 0;
	virtual void SetSequenceMode(int32_t mode) noexcept = 0;
	virtual void SetSpeed(float speed) noexcept = 0;
	[[nodiscard]] virtual float GetSpeed() const noexcept = 0;
	virtual void UpdateFrame() noexcept = 0;
	virtual void UpdateRealClock() noexcept = 0;
	[[nodiscard]] virtual int32_t EngineMs() const noexcept = 0;
	/// The frame's game time in whole ms: 0 while paused, at most 199
	[[nodiscard]] virtual std::chrono::milliseconds GetFrameGameTime() const noexcept = 0;
	[[nodiscard]] virtual float GetTurnFraction() const noexcept = 0;
	[[nodiscard]] virtual uint32_t VisualMs() const noexcept = 0;
	[[nodiscard]] virtual uint32_t FrameRealMs() const noexcept = 0;
	[[nodiscard]] virtual uint32_t EngineFrameSampleMs() const noexcept = 0;
};
} // namespace openblack::ecs::systems
