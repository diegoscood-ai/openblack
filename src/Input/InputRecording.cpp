/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputRecording.h"

#include <cstring>

#include <algorithm>

using namespace openblack::input::recording;

namespace
{
void PutU32(std::vector<uint8_t>& out, uint32_t value)
{
	for (size_t i = 0; i < 4; ++i)
	{
		out.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

[[nodiscard]] uint32_t GetU32(std::span<const uint8_t> bytes, size_t at)
{
	uint32_t value = 0;
	for (size_t i = 0; i < 4; ++i)
	{
		value |= static_cast<uint32_t>(bytes[at + i]) << (8 * i);
	}
	return value;
}

[[nodiscard]] bool KnownKind(uint32_t kind)
{
	return kind >= static_cast<uint32_t>(Kind::MouseMotion) && kind <= static_cast<uint32_t>(Kind::Key);
}
} // namespace

std::vector<uint8_t> openblack::input::recording::Write(const Recording& recording)
{
	std::vector<uint8_t> out;
	out.reserve(k_HeaderSize + (recording.events.size() * k_EventSize));
	out.insert(out.end(), k_Magic.begin(), k_Magic.end());
	PutU32(out, k_Version);
	PutU32(out, static_cast<uint32_t>(recording.window.x));
	PutU32(out, static_cast<uint32_t>(recording.window.y));
	for (const auto& event : recording.events)
	{
		PutU32(out, event.frame);
		PutU32(out, static_cast<uint32_t>(event.kind));
		for (const auto value : event.values)
		{
			PutU32(out, static_cast<uint32_t>(value));
		}
	}
	return out;
}

std::optional<Recording> openblack::input::recording::Read(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_HeaderSize || !std::equal(k_Magic.begin(), k_Magic.end(), bytes.begin()) ||
	    GetU32(bytes, 4) != k_Version)
	{
		return std::nullopt;
	}
	Recording recording;
	recording.window = {static_cast<int32_t>(GetU32(bytes, 8)), static_cast<int32_t>(GetU32(bytes, 12))};
	for (size_t at = k_HeaderSize; at + k_EventSize <= bytes.size(); at += k_EventSize)
	{
		const auto kind = GetU32(bytes, at + 4);
		if (!KnownKind(kind))
		{
			return std::nullopt;
		}
		Event event {.frame = GetU32(bytes, at), .kind = static_cast<Kind>(kind)};
		for (size_t i = 0; i < event.values.size(); ++i)
		{
			event.values.at(i) = static_cast<int32_t>(GetU32(bytes, at + 8 + (4 * i)));
		}
		recording.events.push_back(event);
	}
	return recording;
}

std::optional<Event> openblack::input::recording::FromSdl(const SDL_Event& event, uint32_t frame)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		return Event {.frame = frame,
		              .kind = Kind::MouseMotion,
		              .values = {event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel,
		                         static_cast<int32_t>(event.motion.state), 0}};
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		return Event {.frame = frame,
		              .kind = Kind::MouseButton,
		              .values = {event.button.button, event.type == SDL_MOUSEBUTTONDOWN ? 1 : 0, event.button.clicks,
		                         event.button.x, event.button.y, 0}};
	case SDL_MOUSEWHEEL:
		return Event {.frame = frame,
		              .kind = Kind::MouseWheel,
		              .values = {event.wheel.x, event.wheel.y, static_cast<int32_t>(event.wheel.direction), 0, 0, 0}};
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return Event {.frame = frame,
		              .kind = Kind::Key,
		              .values = {static_cast<int32_t>(event.key.keysym.scancode), event.key.keysym.sym, event.key.keysym.mod,
		                         event.key.repeat, event.type == SDL_KEYDOWN ? 1 : 0, 0}};
	default:
		return std::nullopt;
	}
}

SDL_Event openblack::input::recording::ToSdl(const Event& event)
{
	SDL_Event out {};
	const auto& v = event.values;
	switch (event.kind)
	{
	case Kind::MouseMotion:
		out.type = SDL_MOUSEMOTION;
		out.motion.x = v[0];
		out.motion.y = v[1];
		out.motion.xrel = v[2];
		out.motion.yrel = v[3];
		out.motion.state = static_cast<uint32_t>(v[4]);
		break;
	case Kind::MouseButton:
		out.type = v[1] != 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		out.button.button = static_cast<uint8_t>(v[0]);
		out.button.state = v[1] != 0 ? SDL_PRESSED : SDL_RELEASED;
		out.button.clicks = static_cast<uint8_t>(v[2]);
		out.button.x = v[3];
		out.button.y = v[4];
		break;
	case Kind::MouseWheel:
		out.type = SDL_MOUSEWHEEL;
		out.wheel.x = v[0];
		out.wheel.y = v[1];
		out.wheel.direction = static_cast<uint32_t>(v[2]);
		break;
	case Kind::Key:
		out.type = v[4] != 0 ? SDL_KEYDOWN : SDL_KEYUP;
		out.key.state = v[4] != 0 ? SDL_PRESSED : SDL_RELEASED;
		out.key.keysym.scancode = static_cast<SDL_Scancode>(v[0]);
		out.key.keysym.sym = v[1];
		out.key.keysym.mod = static_cast<uint16_t>(v[2]);
		out.key.repeat = static_cast<uint8_t>(v[3]);
		break;
	}
	return out;
}

std::span<const Event> openblack::input::recording::Due(std::span<const Event> events, Playhead& playhead, uint32_t frame)
{
	const size_t first = std::min(playhead.next, events.size());
	if (frame < playhead.startFrame)
	{
		return events.subspan(first, 0);
	}
	const uint32_t at = frame - playhead.startFrame;
	size_t last = first;
	while (last < events.size() && events[last].frame <= at)
	{
		++last;
	}
	playhead.next = last;
	return events.subspan(first, last - first);
}
