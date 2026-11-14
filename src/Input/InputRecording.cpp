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
#include <bit>
#include <map>
#include <string_view>
#include <utility>

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
	return kind >= static_cast<uint32_t>(Kind::MouseMotion) && kind <= static_cast<uint32_t>(Kind::Text);
}
} // namespace

std::vector<uint8_t> openblack::input::recording::Write(const Recording& recording)
{
	auto out = WriteHeader(recording.window);
	const auto events = WriteEvents(recording.events);
	out.insert(out.end(), events.begin(), events.end());
	return out;
}

std::vector<uint8_t> openblack::input::recording::WriteHeader(glm::ivec2 window)
{
	std::vector<uint8_t> out;
	out.reserve(k_HeaderSize);
	out.insert(out.end(), k_Magic.begin(), k_Magic.end());
	PutU32(out, k_Version);
	PutU32(out, static_cast<uint32_t>(window.x));
	PutU32(out, static_cast<uint32_t>(window.y));
	return out;
}

std::vector<uint8_t> openblack::input::recording::WriteEvents(std::span<const Event> events)
{
	std::vector<uint8_t> out;
	out.reserve(events.size() * k_EventSize);
	for (const auto& event : events)
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
		              .values = {event.wheel.x, event.wheel.y, static_cast<int32_t>(event.wheel.direction),
		                         std::bit_cast<int32_t>(event.wheel.preciseX), std::bit_cast<int32_t>(event.wheel.preciseY),
		                         0}};
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

std::vector<Event> openblack::input::recording::TextFromSdl(const SDL_Event& event, uint32_t frame)
{
	std::vector<Event> pieces;
	if (event.type != SDL_TEXTINPUT)
	{
		return pieces;
	}
	const std::string_view text(event.text.text, strnlen(event.text.text, sizeof(event.text.text)));
	size_t at = 0;
	while (at < text.size())
	{
		auto end = std::min(at + k_TextBytes, text.size());
		// a piece ends before a byte that goes on a character (10xxxxxx), so no character is cut in two
		while (end < text.size() && end > at + 1 && (static_cast<uint8_t>(text[end]) & 0xC0u) == 0x80u)
		{
			--end;
		}
		Event piece {.frame = frame, .kind = Kind::Text};
		for (size_t i = at; i < end; ++i)
		{
			const auto byte = i - at;
			auto& value = piece.values.at(byte / 4);
			value = static_cast<int32_t>(static_cast<uint32_t>(value) |
			                             (static_cast<uint32_t>(static_cast<uint8_t>(text[i])) << (8 * (byte % 4))));
		}
		pieces.push_back(piece);
		at = end;
	}
	return pieces;
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
		// The notches as turned, which the game's zoom reads; a recording made before they were kept, or crafted with
		// none, holds zero for both, and its whole notches stand for them
		if (v[3] == 0 && v[4] == 0)
		{
			out.wheel.preciseX = static_cast<float>(v[0]);
			out.wheel.preciseY = static_cast<float>(v[1]);
		}
		else
		{
			out.wheel.preciseX = std::bit_cast<float>(v[3]);
			out.wheel.preciseY = std::bit_cast<float>(v[4]);
		}
		break;
	case Kind::Key:
		out.type = v[4] != 0 ? SDL_KEYDOWN : SDL_KEYUP;
		out.key.state = v[4] != 0 ? SDL_PRESSED : SDL_RELEASED;
		out.key.keysym.scancode = static_cast<SDL_Scancode>(v[0]);
		out.key.keysym.sym = v[1];
		out.key.keysym.mod = static_cast<uint16_t>(v[2]);
		out.key.repeat = static_cast<uint8_t>(v[3]);
		break;
	case Kind::Logo:
		// not an event of the window: DueEvents leaves it out
		break;
	case Kind::Text:
		out.type = SDL_TEXTINPUT;
		for (size_t byte = 0; byte < k_TextBytes; ++byte)
		{
			out.text.text[byte] = static_cast<char>((static_cast<uint32_t>(v.at(byte / 4)) >> (8 * (byte % 4))) & 0xFFu);
		}
		out.text.text[k_TextBytes] = '\0';
		break;
	}
	return out;
}

std::vector<Event> openblack::input::recording::ReleasesOfHeld(std::span<const Event> events, uint32_t frame)
{
	std::map<int32_t, bool> buttons;                  // held or not, by button
	std::map<int32_t, std::pair<bool, int32_t>> keys; // held or not and its key code, by scancode
	std::array<int32_t, 2> pointer {0, 0};
	for (const auto& event : events)
	{
		const auto& v = event.values;
		switch (event.kind)
		{
		case Kind::MouseMotion:
			pointer = {v[0], v[1]};
			break;
		case Kind::MouseButton:
			buttons[v[0]] = v[1] != 0;
			pointer = {v[3], v[4]};
			break;
		case Kind::Key:
			keys[v[0]] = {v[4] != 0, v[1]};
			break;
		case Kind::MouseWheel:
		case Kind::Logo:
		case Kind::Text:
			break;
		}
	}
	std::vector<Event> ups;
	for (const auto& [button, held] : buttons)
	{
		if (held)
		{
			ups.push_back({.frame = frame, .kind = Kind::MouseButton, .values = {button, 0, 1, pointer[0], pointer[1], 0}});
		}
	}
	for (const auto& [scancode, key] : keys)
	{
		if (key.first)
		{
			ups.push_back({.frame = frame, .kind = Kind::Key, .values = {scancode, key.second, 0, 0, 0, 0}});
		}
	}
	return ups;
}

std::optional<uint32_t> openblack::input::recording::FrameAt(const Playhead& playhead, uint32_t frame)
{
	if (frame < playhead.startFrame)
	{
		return std::nullopt;
	}
	return frame - playhead.startFrame + playhead.played;
}

std::span<const Event> openblack::input::recording::Due(std::span<const Event> events, Playhead& playhead, uint32_t frame)
{
	const size_t first = std::min(playhead.next, events.size());
	const auto played = FrameAt(playhead, frame);
	if (!played.has_value())
	{
		return events.subspan(first, 0);
	}
	const uint32_t at = *played;
	size_t last = first;
	while (last < events.size() && events[last].frame <= at)
	{
		++last;
	}
	playhead.next = last;
	return events.subspan(first, last - first);
}

void openblack::input::recording::CountAgainFrom(Playhead& playhead, uint32_t frame)
{
	if (frame >= playhead.startFrame)
	{
		playhead.played += frame - playhead.startFrame;
		playhead.startFrame = 0;
	}
	else
	{
		playhead.startFrame -= frame;
	}
}
