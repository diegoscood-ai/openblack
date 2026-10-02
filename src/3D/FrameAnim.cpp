/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameAnim.h"

#include <cmath>

#include <algorithm>

#include <stb_image.h>

#include "3D/Billboard.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_Milli = 0.001f;              ///< [0x8AA3B0] / [0x8AC418]
constexpr float k_OneOffRate = 18.0f;          ///< [0x981FB4]
constexpr float k_OneOffFrames = 16.0f;        ///< double [0x982820]
constexpr float k_OneOffCell = 0.25f;          ///< [0x981FB8]
constexpr float k_SpellIconRate = -15.0f;      ///< [0x8D86F0]
constexpr float k_ThirtyTwo = 32.0f;           ///< double [0x8D8740] / float [0x8CF134]
constexpr float k_Texel = 0.00390625f;         ///< 1 / 256: [0x8D86CC], [0x9357A8], [0x938EBC]
constexpr float k_Eighth = 0.125f;             ///< [0x8AB620]
constexpr float k_PSysMaxLerp = 5.0f;          ///< [0x9357B8]
constexpr float k_MistRate = 0.255f;           ///< [0x9A2BA8]
constexpr int k_MistWrap = 900;                ///< 0x384
constexpr float k_MistEffectRow = 0.25f;       ///< [0x8AB3D4]
constexpr float k_SmokeMaxDt = 100.0f;         ///< [0x8AB41C]
constexpr float k_SmokeAgeRate = 255.0f;       ///< [0x8AB270], fn_007F8E00: ftol(dt x 255)
constexpr float k_FireRate = -25.0f;           ///< [0x999668]
constexpr float k_SteamRate = 25.0f;           ///< [0x99966C]
constexpr float k_FishMaxDt = 0.1f;            ///< [0x8AB22C]
constexpr float k_FishRate = 25.0f;            ///< [0x8C7BD0]
constexpr float k_Fifteen = 15.0f;             ///< [0x8C2C40]
constexpr float k_InverseFifteen = 0.0666667f; ///< [0x8C9D38]
constexpr int k_LanternLoopMs = 700;           ///< [0xC383C8]
constexpr float k_LeashRate = 10.0f;           ///< [0x8C8404]
constexpr float k_LeashScrollRate = 0.5f;      ///< [0x8C8400]
constexpr float k_JCSpecialRate = 0.01f;       ///< [0x8C4B10]
constexpr std::array<float, 2> k_PlayerSymbolRate = {0.02f, 0.023f}; ///< [0x937538], [0x937534]
constexpr float k_PlayerSymbolSpinRate = 0.002f; ///< [0x92A544]
constexpr float k_TwoPi = 6.28318548f;           ///< [0x8AB210]
constexpr float k_WaterfallRate = 0.5f;        ///< [0x8AA3B4]
constexpr float k_GoolooMs = 500.0f;           ///< [0xBF3588]
constexpr double k_GoolooV = static_cast<double>(1.7f); ///< double [0x92B338] = 1.7000000476837158
constexpr double k_GoolooRate = 0.7;           ///< double [0x900AE0]
constexpr float k_GoolooU = 2.0f;              ///< fn_005E6390
} // namespace

std::array<glm::vec2, 4> frame_anim::SpriteCellUv(int cell, uint8_t cellsPerRow)
{
	return billboard::CellUv(SpriteCell(cell), cellsPerRow);
}

glm::vec2 frame_anim::OffsetUv(const glm::vec2& uv, const UvOffset& offset, bool fixedMaterial) noexcept
{
	if (!IsAnimatedUv(offset) || fixedMaterial)
	{
		return uv;
	}
	return {uv.x + offset.x, uv.y + offset.y};
}

float frame_anim::PackUvOffset(float u, float v) noexcept
{
	const float steps = std::round((u - std::floor(u)) * 256.0f);
	return v + 4.0f * (steps >= 256.0f ? 0.0f : steps);
}

frame_anim::UvOffset frame_anim::AnimTexturedCell(int frame, const AnimTexturedSheet& sheet) noexcept
{
	if (!sheet.slideU && !sheet.slideV)
	{
		// 0x67A570..0x67A5CC: cols = 256 / W (idiv), f / cols and f % cols unsigned (div); v pushed first
		const int cols = std::max(1, sheet.width > 0 ? 256 / sheet.width : 1);
		const auto f = static_cast<uint32_t>(frame);
		const auto c = static_cast<uint32_t>(cols);
		return {static_cast<float>(sheet.width) * k_Texel * static_cast<float>(f % c),
		        static_cast<float>(sheet.height) * k_Texel * static_cast<float>(f / c)};
	}
	// 0x67A5FF..0x67A644: fild qword (W f) / (fild N x 256 [0x8D45CC])
	const float n = static_cast<float>(sheet.frames) * 256.0f;
	return {sheet.slideU ? static_cast<float>(sheet.width * frame) / n : 0.0f,
	        sheet.slideV ? static_cast<float>(sheet.height * frame) / n : 0.0f};
}

frame_anim::UvOffset frame_anim::OneOffFrame(float& phase, float milliseconds) noexcept
{
	phase = std::fmod(phase + milliseconds * k_OneOffRate * k_Milli, k_OneOffFrames);
	const int frame = static_cast<int>(phase);
	return {static_cast<float>(frame % 4) * k_OneOffCell, static_cast<float>(frame / 4) * k_OneOffCell};
}

frame_anim::UvOffset frame_anim::SpellIconFrame(float& phase, float seconds) noexcept
{
	phase = std::fmod(phase + k_SpellIconRate * seconds, k_ThirtyTwo);
	if (phase < 0.0f)
	{
		phase += k_ThirtyTwo;
	}
	const int frame = static_cast<int>(phase);
	return {static_cast<float>(frame % 8) * k_Texel * k_ThirtyTwo, static_cast<float>(frame / 8) * k_Texel * k_ThirtyTwo};
}

frame_anim::UvOffset frame_anim::HandFlowFrame(float& phase, float seconds, float rate, int frames) noexcept
{
	phase += seconds * rate;
	const float wrap = static_cast<float>(frames + frames);
	if (rate > 0.0f)
	{
		if (phase > wrap)
		{
			phase = std::fmod(phase, wrap);
		}
	}
	else if (phase < 0.0f)
	{
		phase = std::fmod(phase, wrap) + wrap;
	}
	// fistp 0x68D323: rounded to the nearest, ties to even (the FPU's default mode)
	const int frame = static_cast<int>(std::nearbyint(phase)) % std::max(1, frames);
	return {static_cast<float>(frame % 8) * k_Eighth, static_cast<float>(frame / 8) * k_Eighth};
}

void frame_anim::PSysFrameAdvance(float& previous, float& current, float dt, float rate, int frames, bool play) noexcept
{
	previous = current;
	// 0x673FC6..0x673FDE: [0xC029DC] (1) and PlayAnim == 1, else straight to 0x67406A
	if (!play)
	{
		return;
	}
	current = previous + dt * rate;
	if (frames <= 0)
	{
		return;
	}
	const auto n = static_cast<float>(frames);
	const float twoN = n + n;
	// (openblack guard) a frame too far out for the steps of N to move it (2^24) is wrapped first, so the loops end
	if (!std::isfinite(current) || std::abs(current) > 16777216.0f || std::abs(previous) > 16777216.0f)
	{
		current = std::isfinite(current) ? std::fmod(current, n) : 0.0f;
		previous = current;
	}
	if (rate > 0.0f)
	{
		while (current > twoN && previous > twoN)
		{
			current -= n;
			previous -= n;
		}
	}
	else
	{
		while (current < 0.0f || previous < 0.0f)
		{
			current += twoN;
			previous += twoN;
		}
	}
}

float frame_anim::PSysFrameLerp(float previous, float current, float t, bool loop) noexcept
{
	const float limit = loop ? k_PSysMaxLerp : 1.0f;
	const float k = t <= 0.0f ? 0.0f : (t < limit ? t : limit);
	return (current - previous) * k + previous;
}

int frame_anim::PSysFrameIndex(float frame, int frames, bool loop) noexcept
{
	const int n = std::max(1, frames);
	if (loop)
	{
		float r = std::fmod(frame, static_cast<float>(n));
		if (r < 0.0f)
		{
			r += static_cast<float>(n);
		}
		return static_cast<int>(r);
	}
	const int f = static_cast<int>(frame);
	return f <= 0 ? 0 : std::min(f, n - 1);
}

frame_anim::UvOffset frame_anim::MistCellUv(int cell, bool effect) noexcept
{
	return {static_cast<float>(cell & 7) * k_Eighth,
	        static_cast<float>((cell >> 3) & 7) * k_Eighth + (effect ? k_MistEffectRow : 0.0f)};
}

void frame_anim::MistAdvanceExact(MistClock& clock, uint32_t gameTimeIncMs) noexcept
{
	clock.counter += static_cast<int>(static_cast<float>(gameTimeIncMs) * k_MistRate);
	if (clock.counter > k_MistWrap)
	{
		clock.counter %= k_MistWrap;
	}
}

void frame_anim::MistAdvance(MistClock& clock, float milliseconds) noexcept
{
	clock.remainder += milliseconds * k_MistRate;
	const int step = static_cast<int>(clock.remainder);
	clock.remainder -= static_cast<float>(step);
	clock.counter += step;
	if (clock.counter > k_MistWrap)
	{
		clock.counter %= k_MistWrap;
	}
}

int frame_anim::SmokeAgeStep(float& remainder, float milliseconds) noexcept
{
	const float dt = std::min(milliseconds * k_Milli, k_SmokeMaxDt);
	remainder += dt * k_SmokeAgeRate;
	const auto step = static_cast<int>(remainder);
	remainder -= static_cast<float>(step);
	return step;
}

int frame_anim::FireCell(float age) noexcept
{
	return static_cast<int>(std::fmod(k_FireRate * age, k_ThirtyTwo) + k_ThirtyTwo);
}

int frame_anim::SteamCell(float age) noexcept
{
	return static_cast<int>(std::fmod(k_SteamRate * age, k_ThirtyTwo));
}

float frame_anim::FishDt(float seconds) noexcept
{
	return seconds < k_FishMaxDt ? seconds : k_FishMaxDt;
}

uint8_t frame_anim::FishFrame(float& frame, float seconds, float speed) noexcept
{
	frame += FishDt(seconds) * speed * k_FishRate;
	const uint8_t cell = SpriteCell(8 + (static_cast<int>(frame) & 15));
	frame -= static_cast<float>(static_cast<int>(frame * k_InverseFifteen)) * k_Fifteen;
	return cell;
}

int frame_anim::LanternAdvance(int& clockMs, uint32_t gameTimeIncMs) noexcept
{
	clockMs += static_cast<int>(gameTimeIncMs);
	if (clockMs > k_LanternLoopMs)
	{
		clockMs %= k_LanternLoopMs;
	}
	return clockMs * 31 / k_LanternLoopMs;
}

uint8_t frame_anim::LanternCell(int a, int flame, const LanternStarts& starts) noexcept
{
	const int start = starts.at(static_cast<size_t>(std::clamp(flame, 0, 1)));
	return static_cast<uint8_t>((10 * flame + 31 - ((start + a) & 31)) & 31);
}

uint8_t frame_anim::LeashCell(float& phase, float seconds) noexcept
{
	phase += k_LeashRate * seconds;
	phase -= static_cast<float>(static_cast<int>(phase * k_InverseFifteen)) * k_Fifteen;
	return SpriteCell(static_cast<int>(phase));
}

float frame_anim::LeashScroll(float& u, float seconds) noexcept
{
	u += k_LeashScrollRate * seconds;
	u -= static_cast<float>(static_cast<int>(u));
	return u;
}

uint8_t frame_anim::GoldenShowerCell(int32_t t, int32_t base, uint8_t dropOffset) noexcept
{
	return SpriteCell((t / 50 + base + static_cast<int32_t>(dropOffset)) % 32);
}

uint8_t frame_anim::CreatureRoomCell(uint32_t tickCount, int sprite) noexcept
{
	return static_cast<uint8_t>(31 - ((static_cast<int>(tickCount >> 5u) + sprite) & 31));
}

uint8_t frame_anim::CursorCell(uint32_t tickCount) noexcept
{
	return static_cast<uint8_t>((static_cast<int32_t>(tickCount) / 50) & 15);
}

uint8_t frame_anim::HelpSystemCell(int32_t& clockMs, int32_t stepMs) noexcept
{
	clockMs += stepMs;
	return static_cast<uint8_t>((clockMs / 200) & 15);
}

uint8_t frame_anim::JCSpecialCell(float& frame, float milliseconds) noexcept
{
	frame += milliseconds * k_JCSpecialRate;
	if (frame > k_Fifteen)
	{
		frame = 0.0f;
	}
	return static_cast<uint8_t>(static_cast<int>(frame) & 15);
}

uint8_t frame_anim::PlayerSymbolCell(float& phase, float milliseconds, int layer) noexcept
{
	phase -= milliseconds * k_PlayerSymbolRate.at(static_cast<size_t>(std::clamp(layer, 0, 1)));
	while (phase < 0.0f)
	{
		phase += k_ThirtyTwo;
	}
	return SpriteCell(static_cast<int>(phase));
}

float frame_anim::PlayerSymbolSpin(float& angle, float milliseconds) noexcept
{
	angle += milliseconds * k_PlayerSymbolSpinRate;
	while (angle > k_TwoPi)
	{
		angle -= k_TwoPi;
	}
	return angle;
}

uint8_t frame_anim::SmokyStuffCell(float life) noexcept
{
	return SpriteCell(static_cast<int32_t>(life * k_Fifteen));
}

uint8_t frame_anim::DustCell(uint32_t seed, float age) noexcept
{
	return static_cast<uint8_t>(16 + ((seed + static_cast<uint32_t>(2.0f * age)) & 15u));
}

float frame_anim::WaterfallScroll(float& v, float seconds) noexcept
{
	v -= seconds * k_WaterfallRate;
	v -= std::trunc(v);
	return v;
}

frame_anim::Gooloo frame_anim::GoolooFrame(float t) noexcept
{
	// the game thread runs the x87 at 24 bits (fn_007DEE00, and cw 0xFCFF at 0x7DEE0D): every fmul / fdiv rounds to
	// float even against the double constants; fcos / fsin are not affected by the precision control
	const float x = t / k_GoolooMs;
	const float rateX = static_cast<float>(k_GoolooRate * static_cast<double>(x));
	Gooloo frame;
	frame.uv = {static_cast<float>(k_GoolooU * static_cast<double>(static_cast<float>(std::cos(static_cast<double>(x))))),
	             static_cast<float>(k_GoolooV * static_cast<double>(static_cast<float>(std::sin(static_cast<double>(rateX)))))};
	frame.materialByte = static_cast<uint8_t>(255 - static_cast<int>(255.0f * t / k_GoolooMs));
	return frame;
}

frame_anim::UvOffset frame_anim::RotatingUv(const UvOffset& previous, const UvOffset& current, float t,
                                            const glm::vec2& period) noexcept
{
	UvOffset uv {(current.x - previous.x) * t + previous.x, (current.y - previous.y) * t + previous.y};
	for (int k = 0; k < 2; ++k)
	{
		if (period[k] > 0.0f)
		{
			// (openblack guard) far beyond the period, where taking it off would not move it
			if (uv[k] > period[k] * 16777216.0f)
			{
				uv[k] = std::fmod(uv[k], period[k]);
			}
			while (uv[k] > period[k])
			{
				uv[k] -= period[k];
			}
		}
	}
	return uv;
}

void frame_anim::RotatingUvClock::GameUpdate() noexcept
{
	for (int k = 0; k < 2; ++k)
	{
		if (period[k] <= 0.0f)
		{
			continue; // (openblack guard) the original would turn its two loops into endless ones
		}
		const float two = period[k] + period[k]; // 0x6C8BC3..0x6C8BD1: the period doubled, once per axis
		// 0x6C8BDF..0x6C8C5A: both under -2 period -> both up a period (the pair keeps its difference)
		while (destination[k] < -two && current[k] < -two)
		{
			destination[k] += period[k];
			current[k] += period[k];
		}
		// 0x6C8C5B..0x6C8CCC: both over +2 period -> both down a period
		while (destination[k] > two && current[k] > two)
		{
			destination[k] -= period[k];
			current[k] -= period[k];
		}
	}
	previous = current;    // 0x6C8CCD / 0x6C8CD3 / 0x6C8CD9
	current = destination; // 0x6C8CD6 / 0x6C8CDC..0x6C8CE2
}

float frame_anim::ChainScroll(float& scroll, float milliseconds, float rate, int frameHeight) noexcept
{
	scroll = milliseconds * rate * k_Milli + scroll;
	const float height = static_cast<float>(frameHeight) * k_Texel;
	if (height > 0.0f)
	{
		scroll = std::fmod(scroll, height);
		if (scroll < 0.0f)
		{
			scroll += height;
		}
	}
	return scroll;
}

std::array<glm::vec2, 4> frame_anim::ChainSegmentUv(int segment, int segments, const ChainSheet& sheet, float scroll) noexcept
{
	// (openblack guard) S and T at least 1: fn_006C8920 divides by both unchecked (idiv 0x6C8936, 0x6C893E, 0x6C8949)
	const int s = std::max(1, segments);
	const int t = std::max(1, sheet.textures);
	// 0x6C8928..0x6C8957: the texture k of this segment, its first segment b and its count n
	const int k = ((segment + 1) * t - 1) / s;
	const int b = k * s / t;
	const int n = std::max(1, (k + 1) * s / t - b); // (openblack guard) the original divides by n in float
	const int j = segment - b;
	// 0x6C8964..0x6C8989
	const int frame = sheet.fileOffset + (k == t - 1 ? sheet.frameOfHead : (k == 0 ? sheet.frameOfTail : 0));
	const float v0 = static_cast<float>(sheet.frameHeight) * (static_cast<float>(j) / static_cast<float>(n));
	const float v1 = static_cast<float>(sheet.frameHeight) * (static_cast<float>(j + 1) / static_cast<float>(n));
	const float u0 = static_cast<float>(frame * sheet.frameWidth);
	const float u1 = static_cast<float>(sheet.frameWidth) + u0;
	// 0x6C89CE..0x6C8A47: x 1/256, then the scroll on the four v
	return {glm::vec2(u0 * k_Texel, v0 * k_Texel + scroll), glm::vec2(u1 * k_Texel, v0 * k_Texel + scroll),
	        glm::vec2(u0 * k_Texel, v1 * k_Texel + scroll), glm::vec2(u1 * k_Texel, v1 * k_Texel + scroll)};
}

std::optional<frame_anim::StackedFrames> frame_anim::LoadBitmapFromFile(std::span<const uint8_t> bytes, int pitch,
                                                                        int bpp, int framesInFile, int framesInUse)
{
	if (pitch <= 0 || bpp <= 0 || framesInFile <= 0)
	{
		return std::nullopt;
	}
	const auto frameSize = static_cast<size_t>(pitch) * static_cast<size_t>(pitch) * static_cast<size_t>(bpp);
	// 0x57CAC7..0x57CAD4: the file must be bpp x pitch^2 x framesInFile bytes exactly
	if (bytes.size() != frameSize * static_cast<size_t>(framesInFile))
	{
		return std::nullopt;
	}
	StackedFrames bitmap;
	bitmap.pitch = pitch;
	bitmap.channels = bpp;
	bitmap.frames = std::max(std::min(framesInUse, framesInFile), 0); // 0x57CADB..0x57CADF
	bitmap.data.resize(frameSize * static_cast<size_t>(bitmap.frames));
	// fn_0057CB40: n = ftol(sqrt(framesInFile)) frames per row of the file; frame f at column f % n, row f / n
	const auto perRow = std::max(1, static_cast<int>(std::sqrt(static_cast<float>(framesInFile))));
	size_t out = 0;
	for (int frame = 0; frame < bitmap.frames; ++frame)
	{
		const int column = frame % perRow;
		const int row = frame / perRow;
		for (int y = 0; y < pitch; ++y)
		{
			for (int x = 0; x < pitch; ++x)
			{
				const auto source = ((static_cast<size_t>(row) * static_cast<size_t>(pitch) + static_cast<size_t>(y)) *
				                         static_cast<size_t>(perRow) +
				                     static_cast<size_t>(column)) *
				                        static_cast<size_t>(pitch) +
				                    static_cast<size_t>(x);
				for (int c = 0; c < bpp; ++c)
				{
					const auto at = source * static_cast<size_t>(bpp) + static_cast<size_t>(c);
					bitmap.data[out++] = at < bytes.size() ? bytes[at] : 0;
				}
			}
		}
	}
	return bitmap;
}

const uint8_t* frame_anim::FrameTexels(const StackedFrames& bitmap, int frame) noexcept
{
	// 0x6CA2E6: no data -> null
	if (bitmap.frames <= 0 || bitmap.data.empty())
	{
		return nullptr;
	}
	// 0x6CA2E3..0x6CA30E: frame % frames (unsigned word); bpp x that x pitch x pitch
	const int index = static_cast<int>(static_cast<uint32_t>(frame) % static_cast<uint32_t>(bitmap.frames));
	return bitmap.data.data() + static_cast<size_t>(index) * static_cast<size_t>(bitmap.pitch) *
	                                static_cast<size_t>(bitmap.pitch) * static_cast<size_t>(bitmap.channels);
}

const uint8_t* frame_anim::GifFrames::Frame(int frame) const noexcept
{
	return rgba.data() + static_cast<size_t>(frame) * static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
}

std::optional<frame_anim::GifFrames> frame_anim::LoadGif(std::span<const uint8_t> bytes)
{
	if (bytes.empty())
	{
		return std::nullopt;
	}
	int* delays = nullptr;
	GifFrames gif;
	int channels = 0;
	auto* pixels = stbi_load_gif_from_memory(bytes.data(), static_cast<int>(bytes.size()), &delays, &gif.width, &gif.height,
	                                         &gif.frames, &channels, 4);
	if (pixels == nullptr || gif.frames <= 0)
	{
		if (pixels != nullptr)
		{
			stbi_image_free(pixels);
		}
		stbi_image_free(delays); // STBI_FREE, free() by default
		return std::nullopt;
	}
	const auto size = static_cast<size_t>(gif.frames) * static_cast<size_t>(gif.width) * static_cast<size_t>(gif.height) * 4;
	gif.rgba.assign(pixels, pixels + size);
	gif.delaysMs.reserve(static_cast<size_t>(gif.frames));
	for (int frame = 0; frame < gif.frames; ++frame)
	{
		gif.delaysMs.push_back(delays != nullptr ? delays[frame] : 0);
	}
	stbi_image_free(pixels);
	stbi_image_free(delays);
	return gif;
}

frame_anim::DelayClock frame_anim::DelayClock::FromDelays(std::span<const int> storedDelaysMs)
{
	DelayClock clock;
	float time = 0.0f;
	for (const int stored : storedDelaysMs)
	{
		time += static_cast<float>(GifDelayMs(stored)) / 1000.0f;
		clock.ends.push_back(time);
	}
	return clock;
}

frame_anim::DelayClock frame_anim::DelayClock::FixedRate(int frames, float framesPerSecond)
{
	DelayClock clock;
	const float step = framesPerSecond > 0.0f ? 1.0f / framesPerSecond : 0.0f;
	for (int i = 0; i < frames; ++i)
	{
		clock.ends.push_back(step * static_cast<float>(i + 1));
	}
	return clock;
}

frame_anim::DelayClock::Sample frame_anim::DelayClock::At(float seconds) const noexcept
{
	Sample sample;
	if (ends.empty())
	{
		return sample;
	}
	const float length = ends.back();
	const float at = length > 0.0f ? std::fmod(seconds, length) : 0.0f;
	const auto frames = ends.size();
	sample.frame = std::min(static_cast<size_t>(std::ranges::upper_bound(ends, at) - ends.begin()), frames - 1);
	sample.next = (sample.frame + 1) % frames;
	const float start = sample.frame > 0 ? ends[sample.frame - 1] : 0.0f;
	sample.fraction = std::clamp((at - start) / std::max(ends[sample.frame] - start, 1e-3f), 0.0f, 1.0f);
	return sample;
}

frame_anim::AnimatedSprite::Frame frame_anim::AnimatedSprite::At(float seconds) const noexcept
{
	const auto sample = clock.At(seconds);
	Frame frame;
	frame.cell = static_cast<uint16_t>(first + sample.frame);
	frame.nextCell = static_cast<uint16_t>(first + sample.next);
	frame.weight = blend ? sample.fraction : 0.0f;
	return frame;
}
