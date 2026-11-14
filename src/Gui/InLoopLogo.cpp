/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InLoopLogo.h"

#include <cmath>

#include <algorithm>
#include <utility>

#include "Common/MsvcQsort.h"
#include "Common/X87Precision.h"

using namespace openblack;
using namespace openblack::in_loop_logo;

namespace x87 = openblack::game_math::x87;

namespace
{
/// 2 pi as the game's float
constexpr float k_TwoPi = 6.2831855f;
constexpr float k_Pi = 3.1415927f;
/// The pixels a picture's pixel spans: the screen's width over this
constexpr int32_t k_ScreenScale = 240;
/// A star's size is at least this (the sprite's own floor)
constexpr float k_MinimumSize = 0.0001f;
/// The FPU's log2(e), as it loads it: 0xB8AA3B295C17F0BC x 2^-63
constexpr x87::Extended k_FpuLog2E {0x1.71547652b82fep+0, 0x1.78p-56};
/// The camera's path: its three waves, each a double the game keeps
constexpr double k_WaveX = 0.352532;
constexpr double k_WaveY = 0.26793;
constexpr double k_WaveZ = 0.15161;
/// The game's float constants for its divisions: 1 / 12, 1 / 11, 1 / 15 and 1 / 1500
constexpr float k_TwelfthOf = 1.0f / 12.0f;
constexpr float k_EleventhOf = 1.0f / 11.0f;
constexpr float k_FifteenthOf = 1.0f / 15.0f;
constexpr float k_WheelScale = 1.0f / 1500.0f;

[[nodiscard]] float Clamp(float value, float low, float high) noexcept
{
	return value < low ? low : (value > high ? high : value);
}

[[nodiscard]] int32_t Clamp(int32_t value, int32_t low, int32_t high) noexcept
{
	return value < low ? low : (value > high ? high : value);
}

/// The sprite's own size floor
[[nodiscard]] float SpriteSize(float size) noexcept
{
	return size < k_MinimumSize ? k_MinimumSize : size;
}

/// 0 below 0, 1 above 1, else (1 - cos(t pi)) / 2, the cosine on the FPU's wider value
[[nodiscard]] float CosineEase(float t) noexcept
{
	if (t < 0.0f)
	{
		return 0.0f;
	}
	if (t > 1.0f)
	{
		return 1.0f;
	}
	const double angle = x87::Mul(static_cast<double>(t), static_cast<double>(k_Pi));
	const double eased = x87::Sub(1.0, x87::Cos(angle));
	return x87::Store(x87::Mul(eased, 0.5));
}

/// The wave t x k + c, its product rounded once from the exact one, then its sum
[[nodiscard]] double Wave(float t, double k, double c) noexcept
{
	const double product = x87::Mul(x87::Extended {k, 0.0}, static_cast<double>(t));
	return x87::Add(product, c);
}
} // namespace

std::optional<UserPicture> in_loop_logo::ReadUserPicture(std::span<const uint8_t> bytes)
{
	// the name's bytes, as many as the file holds, over a cleared name
	std::array<uint8_t, k_NameLength * 2> nameBytes {};
	if (bytes.size() > k_Stars)
	{
		const auto count = std::min(bytes.size() - k_Stars, nameBytes.size());
		std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(k_Stars), count, nameBytes.begin());
	}
	std::u16string name;
	for (size_t i = 0; i < k_NameLength; ++i)
	{
		const auto character = static_cast<char16_t>(nameBytes.at(i * 2) | (nameBytes.at(i * 2 + 1) << 8u));
		if (character == 0)
		{
			break;
		}
		name.push_back(character);
	}
	// a file with a name has all its grey bytes
	if (name.empty())
	{
		return std::nullopt;
	}
	UserPicture user;
	user.name = std::move(name);
	for (size_t i = 0; i < k_Stars; ++i)
	{
		const uint8_t grey = bytes[i];
		user.picture.at(i * 3) = grey;
		user.picture.at(i * 3 + 1) = grey;
		user.picture.at(i * 3 + 2) = grey;
	}
	return user;
}

State in_loop_logo::Start(const Picture& picture, bool defaultPicture, std::u16string name, glm::ivec2 screen,
                          const PointFn& pointAt, const RandomFn& random)
{
	State state;
	state.defaultPicture = defaultPicture;
	state.name = std::move(name);
	state.stars.resize(k_Stars);
	state.sizes.resize(k_Stars);
	// each column of the picture, each of its rows
	size_t index = 0;
	for (int32_t x = 0; x < k_PictureSize; ++x)
	{
		for (int32_t y = 0; y < k_PictureSize; ++y)
		{
			const float depth = random(70.0f, 130.0f);
			// whole-number divisions, cut towards zero
			const glm::ivec2 pixel {(screen.x >> 1) + (x - k_PictureSize / 2) * screen.x / k_ScreenScale,
			                        (screen.y >> 1) + (y - k_PictureSize / 2) * screen.x / k_ScreenScale};
			auto& star = state.stars.at(index);
			star.position = pointAt(pixel, depth);
			const size_t at = (static_cast<size_t>(y) * k_PictureSize + static_cast<size_t>(x)) * 3;
			const uint32_t red = picture.at(at);
			const uint32_t green = picture.at(at + 1);
			const uint32_t blue = picture.at(at + 2);
			star.argb = 0xFF000000u + (red << 16u) + (green << 8u) + blue;
			star.bright = green >= k_BrightGreen;
			star.angle = random(0.0f, k_TwoPi);
			state.sizes.at(index) = depth / random(200.0f, 320.0f);
			star.size = SpriteSize(state.sizes.at(index));
			++index;
		}
	}
	// by their blue byte, as the runtime's quicksort leaves them
	msvc_qsort::Sort(std::span<Star>(state.stars), [](const Star& a, const Star& b) {
		return static_cast<int32_t>(a.argb & 0xFFu) - static_cast<int32_t>(b.argb & 0xFFu);
	});
	state.home.resize(k_Stars);
	state.velocity.assign(k_Stars, glm::vec3(0.0f));
	for (size_t i = 0; i < k_Stars; ++i)
	{
		state.sizes.at(i) = state.stars.at(i).size;
		state.home.at(i) = state.stars.at(i).position;
	}
	return state;
}

float in_loop_logo::NextTime(float time, int32_t deltaMs, float speed) noexcept
{
	if (time == 0.0f)
	{
		return 0.001f;
	}
	const float scaled = static_cast<float>(deltaMs) * speed;
	const float step = scaled * 0.0011f;
	return time + step;
}

Frame in_loop_logo::FrameAt(float time) noexcept
{
	Frame frame;
	const float t = time;
	// the black fade, cut to a whole number and kept within 0 and 255
	int32_t fade = 0;
	if (t < 12.0f)
	{
		const float share = t * k_TwelfthOf;
		const float scaled = share * 255.0f;
		fade = static_cast<int32_t>(255.0f - scaled);
	}
	if (t > 31.0f)
	{
		const float left = 33.0f - t;
		const float scaled = left * 127.5f;
		fade = static_cast<int32_t>(255.0f - scaled);
	}
	frame.fade = static_cast<uint8_t>(Clamp(fade, 0, 255));

	// the fly into the logo from 15 to 26, eased
	const float approachShare = (t - 15.0f) * k_EleventhOf;
	const float approach = Clamp(approachShare, 0.0f, 1.0f);
	frame.zoom = CosineEase(approach);
	const float zoom = frame.zoom;
	// the distance: 25 coming down to 10 by 15, then out to 100 with the zoom
	const float near = Clamp((15.0f - t) * k_FifteenthOf, 0.0f, 1.0f);
	const float from = near * 15.0f + 10.0f;
	const float distance = from + (100.0f - from) * zoom;
	// the three waves; the first sine is scaled while still on the FPU, the others once stored
	const auto sineX = x87::Sin(Wave(t, k_WaveX, 2.0));
	const float waveY = x87::Store(x87::Cos(x87::Sub(3.0, x87::Mul(x87::Extended {k_WaveY, 0.0}, static_cast<double>(t)))));
	const float waveZ = x87::Store(x87::Sin(Wave(t, k_WaveZ, 7.0)));
	glm::vec3 eye {x87::Store(x87::Mul(sineX, static_cast<double>(distance))), waveY * distance, waveZ * distance};
	// towards the start camera by the zoom
	const glm::vec3 toStart {k_StartEye.x - eye.x, k_StartEye.y - eye.y, k_StartEye.z - eye.z};
	eye = {toStart.x * zoom + eye.x, toStart.y * zoom + eye.y, toStart.z * zoom + eye.z};
	// the flash: from 30 the camera comes in to the middle and its focus moves out past it
	const float flashRaw = 1.0f - (t - 30.0f) * 0.5f;
	glm::vec3 focus {0.0f};
	float flash = 1.0f;
	if (flashRaw <= -2.0f)
	{
		flash = -2.0f;
	}
	else if (flashRaw < 1.0f)
	{
		flash = flashRaw;
	}
	if (flash < 1.0f)
	{
		const float away = 1.0f - flash;
		focus = {focus.x - eye.x * away, focus.y - eye.y * away, focus.z - eye.z * away};
	}
	eye = {eye.x * flash, eye.y * flash, eye.z * flash};
	frame.eye = eye;
	frame.focus = focus;
	frame.flash = flash;

	// the name's fade: in from 22 to 23, out from 33 to 34
	const float fadeIn = t - 22.0f;
	const float fadeOut = 1.0f - (t - 33.0f);
	frame.nameFade = Clamp(t > 33.0f ? fadeOut : fadeIn, 0.0f, 1.0f);
	// the sky darkens from white from 22, its alpha rising from 160
	const int32_t skyFade = Clamp(static_cast<int32_t>(fadeIn * 50.0f), 0, 190);
	// each byte of white moved skyFade / 256 of the way to black, rounded down: the same for the three
	const auto grey = static_cast<uint32_t>(((-255 * skyFade) >> 8) + 255) & 0xFFu;
	const auto alpha = static_cast<uint32_t>(160 + 95 * skyFade / 256);
	frame.skyArgb = (alpha << 24u) | (grey << 16u) | (grey << 8u) | grey;

	// the stars' alpha and the end
	frame.starAlpha = static_cast<uint8_t>(Clamp(static_cast<int32_t>((flash + 1.0f) * 255.0f), 0, 255));
	frame.ends = flash <= -2.0f;
	const float squared = zoom * zoom;
	const float fourth = squared * squared;
	frame.sizeScale = fourth * fourth * 1.54f + 1.0f;
	return frame;
}

Frame in_loop_logo::Advance(State& state, int32_t deltaMs)
{
	state.time = NextTime(state.time, deltaMs, state.speed);
	const auto frame = FrameAt(state.time);
	if (frame.ends)
	{
		state.finished = true;
	}
	return frame;
}

double in_loop_logo::FpuExp(float x) noexcept
{
	const double power = x87::Mul(static_cast<double>(x), k_FpuLog2E);
	const auto scaled = x87::detail::TwoTo({power, 0.0});
	return std::ldexp(scaled.mantissa, static_cast<int>(scaled.exponent));
}

void in_loop_logo::Step(State& state, const Frame& frame, const Input& input, const ProjectFn& project, const PointFn& pointAt,
                        const RandomFn& random)
{
	// the mouse's move, smoothed by a tenth and cut to whole pixels
	const auto smooth = [](int32_t move, int32_t smoothed) {
		return static_cast<int32_t>(static_cast<float>(move - smoothed) * 0.1f + static_cast<float>(smoothed));
	};
	state.mouseMove = {smooth(input.move.x, state.mouseMove.x), smooth(input.move.y, state.mouseMove.y)};
	// the wheel slows or speeds the logo's time
	const int32_t wheel = Clamp(input.wheel, -240, 240);
	const double speed = x87::Mul(FpuExp(static_cast<float>(wheel) * k_WheelScale), static_cast<double>(state.speed));
	state.speed = x87::Store(speed);
	if (state.speed <= 0.1f)
	{
		state.speed = 0.1f;
	}
	else if (state.speed >= 1.0f)
	{
		state.speed = 1.0f;
	}
	const float dt = static_cast<float>(input.deltaMs) * 0.001f;
	const float damping = x87::Store(FpuExp(static_cast<float>(input.deltaMs) * -0.001f));

	// the push of the mouse on the stars near it
	if (state.mouseMove.x != 0 || state.mouseMove.y != 0)
	{
		const int32_t radius = input.screenWidth / 10;
		for (size_t i = 0; i < state.stars.size(); ++i)
		{
			auto& star = state.stars.at(i);
			const auto shown = project(star.position);
			if (!shown.has_value())
			{
				continue;
			}
			state.mouseMove = {Clamp(state.mouseMove.x, -20, 20), Clamp(state.mouseMove.y, -20, 20)};
			const int32_t dx = shown->pixel.x - input.mouse.x;
			if (std::abs(dx) >= radius)
			{
				continue;
			}
			const int32_t dy = shown->pixel.y - input.mouse.y;
			if (std::abs(dy) >= radius)
			{
				continue;
			}
			const int32_t distanceSquared = dy * dy + dx * dx;
			const float distance = std::sqrt(static_cast<float>(distanceSquared));
			float push = 1.0f - distance / static_cast<float>(radius);
			if (push <= 0.0f)
			{
				continue;
			}
			push *= 4.0f;
			const glm::ivec2 pushed {
			    static_cast<int32_t>(static_cast<float>(state.mouseMove.x) * push + static_cast<float>(shown->pixel.x)),
			    static_cast<int32_t>(static_cast<float>(state.mouseMove.y) * push + static_cast<float>(shown->pixel.y))};
			const auto point = pointAt(pushed, shown->depth);
			auto& velocity = state.velocity.at(i);
			const glm::vec3 moved {point.x - star.position.x, point.y - star.position.y, point.z - star.position.z};
			velocity = {moved.x * dt * 100.0f + velocity.x, moved.y * dt * 100.0f + velocity.y,
			            moved.z * dt * 100.0f + velocity.z};
			star.position = point;
		}
	}

	// each star springs back home, slows and moves, and takes the frame's alpha, size and a new angle
	const uint32_t alphaBits = static_cast<uint32_t>(frame.starAlpha) << 24u;
	for (size_t i = 0; i < state.stars.size(); ++i)
	{
		auto& star = state.stars.at(i);
		auto& velocity = state.velocity.at(i);
		const auto& home = state.home.at(i);
		const glm::vec3 away {star.position.x - home.x, star.position.y - home.y, star.position.z - home.z};
		velocity = {velocity.x - away.x * dt * 10.01f, velocity.y - away.y * dt * 10.01f, velocity.z - away.z * dt * 10.01f};
		velocity = {damping * velocity.x, damping * velocity.y, damping * velocity.z};
		star.position = {velocity.x * dt + star.position.x, velocity.y * dt + star.position.y,
		                 velocity.z * dt + star.position.z};
		star.argb = (star.argb & 0x00FFFFFFu) + alphaBits;
		star.size = SpriteSize(frame.sizeScale * state.sizes.at(i));
		star.angle = random(0.0f, k_TwoPi);
	}
}

TextPlace in_loop_logo::TrademarkPlace(glm::ivec2 screen) noexcept
{
	// (int)(W x 0.64) and (int)(H x 0.32), on the doubles the game keeps
	return {.position = {static_cast<float>(static_cast<int32_t>(static_cast<double>(screen.x) * 0.64)),
	                     static_cast<float>(static_cast<int32_t>(static_cast<double>(screen.y) * 0.32))},
	        .size = static_cast<float>(screen.x / 70)};
}

float in_loop_logo::NameSize(glm::ivec2 screen) noexcept
{
	return static_cast<float>(screen.y) / 20.0f;
}

TextPlace in_loop_logo::NamePlace(glm::ivec2 screen, float nameWidth) noexcept
{
	const float x = static_cast<float>(screen.x) * 0.5f - nameWidth * 0.5f;
	const float y = static_cast<float>(screen.y) * 0.71428573f;
	return {.position = {x, y}, .size = NameSize(screen)};
}
