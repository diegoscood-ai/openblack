/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Spirits.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <string>
#include <utility>

#include "3D/ObjectMatrix.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "Help/HelpDudeFile.h"

namespace openblack::help::spirits
{

namespace
{
// pi [0x8C36A0], 2 pi [0x8AB210] and pi / 2 [0x8C78D8]: the shared constants of Camera/ScriptCamera.h
using script_camera::k_HalfPi;
using script_camera::k_Pi;
using script_camera::k_TwoPi;
constexpr float k_HoverClampX = 0.75f;   ///< [0x8AB274] / -[0x900C44]
constexpr float k_HoverClampY = 0.6f;    ///< [0x8C7BDC] / -[0x900C48]
constexpr float k_Third = 0.333333343f;  ///< 0x3EAAAAAB (Update1 0x5BDF42)

/// The anim names at 0xBF029C (0..9 static, 10..17 copied from the emotion names 0xBF027C, 18.. written at 0x5BAD6A)
constexpr std::array<const char*, k_AnimSlots> k_AnimNames = {
    "Stand",       "Hover",       "Hover left",  "Hover right",     "Wingflap",        "L Eye Shut",  "R Eye Shut",
    "Vowel E",     "Vowel O",     "Vowel S",     "Normal",          "Pleased",         "Displeased",  "Sad",
    "Sarcastic",   "Afraid",      "Confused",    "Furious",         "Look L/R",        "Look Up/Dn",  "NodHead",
    "ShakeHead",   "CockHead",    "HeadInHands", "Laugh",           "CrossArms",       "Spare",       "PointL In",
    "Point L",     "PointR In",   "Point R",     "HoverStable",     "Avoid L",         "Avoid R",     "Avoid U",
    "Avoid D",     "HandsOnHips", "Spare!",      "ShakeFinger",     "ScratchHead",     "ScratchChin", "PickNose",
    "PointAtCamera", "HangHead",  "Sulk",        "Cry",             "GoInvisible",     "Dance",       "PressFace",
    "StrokeBeard", "CoverEyes",   "Pray",        "PunchAir",        "WaveNo",          "Dodgy",       "Dismiss",
    "Rude",        "HeadSlap",    "KnockScreen", "Wanker",          "HandGun",         "Burp",        "Fart",
    "Triumph",     "Shrug",       "ClingL",      "ClingR",          "ClingU",          "ClingD",      "GimmeFive",
    "Look L/R Stable", "Look U/D Stable", "Chuckle", "Spare 4",     "Spare 5",         "Spare 6",     "Spare 7",
    "Spare 8",     "Spare 9",     "Spare 10",
};
/// 0xBF027C
constexpr std::array<const char*, 8> k_EmotionNames = {"Normal",    "Pleased", "Displeased", "Sad",
                                                       "Sarcastic", "Afraid",  "Confused",   "Furious"};

/// Row-vector v x rows (the LHMatrix 3x3 products of 0x5BA4DD, 0x5BBA19, 0x5BEF99)
glm::vec3 ByRows(const glm::vec3& v, const glm::mat3& rows)
{
	return v.x * rows[0] + v.y * rows[1] + v.z * rows[2];
}

bool IsSpace(char c)
{
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}

int StrNICmp(const char* a, const char* b, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		const int ca = std::toupper(static_cast<unsigned char>(a[i]));
		const int cb = std::toupper(static_cast<unsigned char>(b[i]));
		if (ca != cb || ca == 0)
		{
			return ca - cb;
		}
	}
	return 0;
}

/// [0xC5716C], the static buffer of fn_0042A6D0's sscanf: it keeps its word from one call to the next
std::string& TagWordBuffer()
{
	static std::string s_Word;
	return s_Word;
}

/// fn_0042A6D0 on `s` from `pos`: one tag, the position after it (and the spaces after it)
size_t ParseOneTag(const std::string& s, size_t pos, AudioTag& tag, int& errors)
{
	auto skipSpaces = [&s](size_t p) {
		while (p < s.size() && IsSpace(s[p]))
		{
			++p;
		}
		return p;
	};
	auto at = [&s](size_t p) { return p < s.size() ? s[p] : '\0'; };

	// the talker (table 0x42AC04 / 0x42AC1C on toupper(c) - 0x21)
	tag.who = 0;
	switch (std::toupper(static_cast<unsigned char>(at(pos))))
	{
	case 'T':
		tag.who = 0;
		break;
	case '!':
	case 'O':
		tag.who = 1;
		break;
	case 'G':
		tag.who = 2;
		break;
	case 'E':
		tag.who = 3;
		break;
	case '*':
	case 'B':
		tag.who = 4;
		break;
	default:
		++errors; // "unrecognised talker in audio tag [%s]"
		break;
	}
	pos = skipSpaces(pos + 1);

	// the type (table 0x42AC50 / 0x42AC64 on toupper(c) - 'A')
	tag.action = 0;
	const int next = std::toupper(static_cast<unsigned char>(at(pos + 1)));
	switch (std::toupper(static_cast<unsigned char>(at(pos))))
	{
	case 'A':
		if (next == 'S')
		{
			++pos;
			tag.action = 2;
		}
		else if (next == 'R')
		{
			++pos;
			tag.action = 3;
		}
		else
		{
			tag.action = 1;
		}
		break;
	case 'E':
		tag.action = 4;
		break;
	case 'L':
		if (next == 'S')
		{
			++pos;
			tag.action = 6;
		}
		else if (next == 'R')
		{
			++pos;
			tag.action = 7;
		}
		else
		{
			tag.action = 5;
		}
		break;
	case 'R':
		tag.action = 3;
		break;
	default:
		++errors; // "unrecognised tag type in audio tag [%s]"
		break;
	}
	pos = skipSpaces(pos + 1);

	// sscanf(rest, "%s " (0x9CB580), [0xC5716C]) (0x42A989): after the space skip the rest is either empty, which gives
	// EOF (-1) and leaves the buffer's previous word, or starts with a word; the error test at 0x42A9A7 (test edi, edi;
	// jne) counts only a result of 0, which cannot happen here. Then the position moves on by the buffer's strlen
	// (0x42A9B1..0x42A9C0), the previous word's length too
	std::string& buffer = TagWordBuffer();
	if (pos < s.size())
	{
		size_t end = pos;
		while (end < s.size() && !IsSpace(s[end]))
		{
			++end;
		}
		buffer = s.substr(pos, end - pos);
	}
	const std::string word = buffer;
	// (openblack) guard: the original's pointer can pass the label's NUL when the previous word is kept
	pos = std::min(pos + word.size(), s.size());

	// the trailing digits (0x42A9C2..0x42AA22): from the last character back over digits, never testing the first
	tag.value = 100;
	size_t digits = word.size();
	if (digits > 0)
	{
		--digits;
		while (digits > 0 && std::isdigit(static_cast<unsigned char>(word[digits])) != 0)
		{
			--digits;
		}
	}
	if (digits + 1 < word.size() && std::isdigit(static_cast<unsigned char>(word[digits + 1])) != 0)
	{
		tag.value = std::atoi(word.c_str() + digits + 1);
	}
	// the letters at the start (0x42AA24..0x42AA62): counted up to the first non-letter after the first character, at
	// most up to the first trailing digit (edi after its inc at 0x42AA0B); the count goes up before each isalpha
	const size_t bound = word.empty() ? 1 : digits + 1;
	size_t letters = 0;
	if (!word.empty() && std::isalpha(static_cast<unsigned char>(word[0])) != 0)
	{
		do
		{
			if (letters >= bound)
			{
				break;
			}
			++letters;
		} while (std::isalpha(static_cast<unsigned char>(word[letters])) != 0);
	}
	if (tag.value <= 0 || tag.value >= 100)
	{
		tag.value = 100; // 0x42AA64..0x42AA71
	}

	tag.index = 0;
	bool found = true;
	switch (tag.action)
	{
	case 1:
	case 2:
	case 3: // strnicmp over min(strlen(name), letters) against the 80 anim names (0x42AB04..0x42AB57)
		found = false;
		for (size_t i = 0; i < k_AnimNames.size(); ++i)
		{
			const size_t n = std::min(std::strlen(k_AnimNames[i]), letters);
			if (StrNICmp(word.c_str(), k_AnimNames[i], n) == 0)
			{
				tag.index = static_cast<int32_t>(i);
				found = true;
				break;
			}
		}
		break;
	case 4: // the same against the 8 emotions (0x42AB97..0x42ABEA)
		found = false;
		for (size_t i = 0; i < k_EmotionNames.size(); ++i)
		{
			const size_t n = std::min(std::strlen(k_EmotionNames[i]), letters);
			if (StrNICmp(word.c_str(), k_EmotionNames[i], n) == 0)
			{
				tag.index = static_cast<int32_t>(i);
				found = true;
				break;
			}
		}
		break;
	case 5:
	case 6:
	case 7: // the first letter (table 0x42AC94 / 0x42ACAC on toupper(c) - 'C')
		switch (std::toupper(static_cast<unsigned char>(word.empty() ? '\0' : word[0])))
		{
		case 'O':
			tag.index = 1;
			break;
		case 'C':
			tag.index = 2;
			break;
		case 'P':
			tag.index = 3;
			break;
		case 'H':
		case 'M':
			tag.index = 4;
			break;
		case 'D':
		case 'N':
			tag.index = 0;
			break;
		default:
			found = false;
			break;
		}
		break;
	default:
		found = false;
		break;
	}
	if (!found)
	{
		++errors; // "unrecognised name in audio tag [%s]" ([0xC5836C] += 1 at 0x42AB59)
	}
	return skipSpaces(pos);
}

/// The inline exp of HelpDudeControl::Process 0x5C3AB8..0x5C3ACA (and 0x5C3AE3..0x5C3AF5) with the FPU at 24 bits
/// (fn_007DEE00): fldl2e; fmulp (rounded to a float: the 64-bit log2(e) as the double one, equal but at a tie); frndint
/// (to nearest); fsub (exact); f2xm1 (not rounded by the precision); fld1; faddp (rounded to a float); fscale (exact)
float Exp24(float x)
{
	const auto t = static_cast<float>(static_cast<double>(x) * 1.4426950408889634);
	const float whole = std::nearbyint(t);
	const float fraction = t - whole;
	const auto power = static_cast<float>(std::exp2(static_cast<double>(fraction)));
	return std::ldexp(power, static_cast<int>(whole));
}
} // namespace

float Smooth(float t)
{
	// fn_005BD250
	if (t < 0.0f)
	{
		return 0.0f;
	}
	if (t > 1.0f)
	{
		return 1.0f;
	}
	return (1.0f - std::cos(t * k_Pi)) * 0.5f;
}

std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, int* errors)
{
	int count = 0;
	std::string s(label);
	// ';' and '/' end the label (0x42A6F1..0x42A70F)
	if (const auto cut = s.find_first_of(";/"); cut != std::string::npos)
	{
		s.resize(cut);
	}
	auto trim = [](std::string& t) {
		size_t b = 0;
		while (b < t.size() && IsSpace(t[b]))
		{
			++b;
		}
		t.erase(0, b);
		while (!t.empty() && IsSpace(t.back()))
		{
			t.pop_back();
		}
	};
	trim(s);
	if (!s.empty() && s[0] == '[')
	{
		s.erase(0, 1);
		if (const auto close = s.find(']'); close != std::string::npos)
		{
			s.resize(close);
		}
		else
		{
			++count; // 0x42A78F
		}
		trim(s);
	}
	std::vector<AudioTag> tags;
	size_t pos = 0;
	while (pos < s.size())
	{
		AudioTag tag;
		tag.time = time;
		const size_t next = ParseOneTag(s, pos, tag, count);
		tags.push_back(tag);
		if (next <= pos)
		{
			break;
		}
		pos = next;
	}
	if (errors != nullptr)
	{
		*errors += count;
	}
	return tags;
}

DudeData DudeData::FromFile(const HelpDudeFile& file)
{
	DudeData data;
	for (size_t i = 0; i < k_AnimSlots; ++i)
	{
		data.clips[i] = file.Clip(i);
		if (i < file.animEvents.size())
		{
			data.loopStart[i] = file.animEvents[i].loopStart;
			data.loopEnd[i] = file.animEvents[i].loopEnd;
		}
	}
	data.flags = file.animFlags;
	// +0x2C38: 8 records of 0x40 bytes (fn_005C2090 x 8, 0x5C2AA5; the 0x200 bytes read at 0x5C2307)
	const size_t records = std::min<size_t>(file.block2C38.size() / 0x40, data.faces.size());
	for (size_t r = 0; r < records; ++r)
	{
		std::memcpy(data.faces[r].data(), file.block2C38.data() + r * 0x40, 0x40);
	}
	data.startEmotion = file.startEmotion;
	data.modelSize = file.restHeight;
	data.nearDepth = file.nearDepth;
	data.farDepth = file.farDepth * 1.2f; // fmul [0x8C6C98] (0x5C2CA5 / 0x5C2CC3)
	data.scale = file.scale35B4;
	data.pitchOffset = file.value35C0;
	data.fingerR0 = file.value35C4;
	data.fingerR2 = file.value35C8;
	return data;
}

// ---------------------------------------------------------------------------------------------------------------------
// HelpDude

float HelpDude::Zone::Feel(float px, float py) const
{
	// HoverZone::Feel 0x5B9620
	if (strength == 0.0f)
	{
		return 0.0f;
	}
	const float dx = px - x;
	if (outer < std::abs(dx))
	{
		return 0.0f;
	}
	const float dy = py - y;
	if (outer < std::abs(dy))
	{
		return 0.0f;
	}
	const float d2 = dy * dy + dx * dx;
	if (d2 > outer * outer)
	{
		return 0.0f;
	}
	if (d2 < inner * inner)
	{
		return strength;
	}
	return (1.0f - (std::sqrt(d2) - inner) / (outer - inner)) * strength;
}

HelpDude::HelpDude(int index, const DudeData& data, HelpDudeControl& control)
    : _index(index)
    , _data(data)
    , _control(control)
{
	// fn_005C17D0: everything 0 but the values below; the .hd's +0x2C28 (Load) is the starting emotion
	_slotLast.fill(-1.0f); // 0x5C194F
	_lastMouse = control.Frame().mouse; // 0x5C1911
	_emotion = data.startEmotion;
	_face = data.faces[0]; // (pending) the record fn_005C2090 builds before the first UpdateFace
	_hoverX.SetPosition(0.0f);
	_hoverY.SetPosition(0.0f);
	_depth.SetPosition(0.0f);
	_worldTarget.SetPosition(glm::vec3(0.0f));
	SetState(dude_state::Hover, false); // 0x5C1C76
}

void HelpDude::SetPosition(glm::ivec2 pixel)
{
	// fn_005BBBD0
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	_hoverX.SetPosition(static_cast<float>(pixel.x - screen.HalfWidth()) / halfW);
	_hoverY.SetPosition(static_cast<float>(pixel.y - screen.HalfHeight()) / halfW);
	ResetTrail();
	SetState(dude_state::Hover, false);
}

void HelpDude::FlyTo(glm::ivec2 pixel, float seconds, bool clamp)
{
	// fn_005BBDD0
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const float hx = static_cast<float>(pixel.x - screen.HalfWidth()) / halfW;
	const float hy = static_cast<float>(pixel.y - screen.HalfHeight()) / halfW;
	SetHoverX(hx, seconds, clamp);
	SetHoverY(hy, seconds, clamp);
	_clingX = hx; // 0x5BBE4B: the unclamped target
	_clingY = hy;
	SnapClingEdge();
	SetState(dude_state::Hover, false);
}

void HelpDude::SetHoverX(float target, float seconds, bool clamp)
{
	if (_puffRunning) // 0x5B96D6
	{
		return;
	}
	if (clamp)
	{
		if (target <= -k_HoverClampX)
		{
			target = -k_HoverClampX;
		}
		else if (!(target < k_HoverClampX))
		{
			target = k_HoverClampX;
		}
	}
	_hoverX.SetDestinationWithSpeedAndTime(target, 0.0f, seconds); // inline 0x5B971E..0x5B98BD
}

void HelpDude::SetHoverY(float target, float seconds, bool clamp)
{
	if (_puffRunning) // 0x5B98D6
	{
		return;
	}
	if (clamp)
	{
		if (target <= -k_HoverClampY)
		{
			target = -k_HoverClampY;
		}
		else if (!(target < k_HoverClampY))
		{
			target = k_HoverClampY;
		}
	}
	_hoverY.SetDestinationWithSpeedAndTime(target, 0.0f, seconds); // inline 0x5B991E..0x5B9ABD
}

void HelpDude::SetEmotion(uint32_t emotion, float peak)
{
	_emotionTime = 0.0f;
	_emotionPeak = peak > 0.0f ? peak : 0.0f; // test ah, 0x41 at 0x5BD226
	_emotionTarget = emotion;
}

void HelpDude::ClearAnims()
{
	_slotMode.fill(0);
}

void HelpDude::SnapClingEdge()
{
	// fn_005BBD20
	if (_state == dude_state::ClingLeave)
	{
		return;
	}
	if (std::abs(_clingY * 1.28205f) < std::abs(_clingX)) // [0x900C64]
	{
		if (_clingX > 0.0f)
		{
			_edge = Edge::Right;
			_clingX = 1.04f; // 0x3F851EB8
		}
		else
		{
			_edge = Edge::Left;
			_clingX = -1.04f;
		}
	}
	else if (_clingY > 0.0f)
	{
		_edge = Edge::Bottom;
		_clingY = 0.78f; // 0x3F47AE14
	}
	else
	{
		_edge = Edge::Top;
		_clingY = -0.78f;
	}
}

void HelpDude::Cling(float hx, float hy, bool fromHome)
{
	// fn_005BBE70
	_clingX = hx;
	_targetX = hx;
	_clingY = hy;
	_targetY = hy;
	SnapClingEdge();
	if (fromHome)
	{
		SetPosition(_control.Anchor(static_cast<int>(_edge)));
		ResetTrail();
	}
	SetState(dude_state::Cling, false);
}

bool HelpDude::IsPlayingAnim() const
{
	// fn_005BBEF0
	if ((_state == dude_state::FlyToAnim || _queuedState == dude_state::FlyToAnim) &&
	    _afterFly == dude_state::ScriptedAnim)
	{
		return true;
	}
	return _state == dude_state::ScriptedAnim || _queuedState == dude_state::ScriptedAnim;
}

void HelpDude::PlayAnim(float hx, float hy, uint32_t anim, float speed)
{
	// fn_005BBF30
	if (anim == 0 && IsPlayingAnim())
	{
		SetState(dude_state::Hover, false);
		return;
	}
	_targetY = hy;
	_targetX = hx;
	_afterFly = dude_state::ScriptedAnim;
	_animSpeed = speed;
	_scriptAnim = anim;
	SetState(dude_state::FlyToAnim, false);
}

void HelpDude::ScreenPoint(glm::ivec2 pixel)
{
	// fn_005BBFA0
	_pointOffScreen = false;
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	_pointX = static_cast<float>(pixel.x - screen.HalfWidth()) / halfW;
	_pointY = static_cast<float>(pixel.y - screen.HalfHeight()) / halfW;
	SetState(dude_state::Point, false);
}

void HelpDude::PointAt(const glm::vec3& position, bool inWorld, float side, float height)
{
	// fn_005BC4A0
	if (_inWorld == 0.0f)
	{
		_worldTarget.SetPosition(position); // 0x5BC4BF..0x5BC564
	}
	else
	{
		_worldTarget.SetDestinationWithTime(position, 0.5f); // 0x5BC56B..0x5BC5B0
	}
	_worldSide = side;
	_inWorldTarget = inWorld ? 1.0f : 0.0f;
	_worldHeight = height;
	_pointOffScreen = false;
	const Queries& q = _control.GetQueries();
	std::optional<ProjectedPoint> projected;
	if (q.projectPoint)
	{
		projected = q.projectPoint(position);
	}
	else
	{
		projected = ProjectedPoint {static_cast<int32_t>(position.x), static_cast<int32_t>(position.y), position.z};
	}
	const float nearClip = q.nearClip ? q.nearClip() : 0.0f;
	// off screen: ProjectPoint fails, depth < 1.1 near (0x5BC607, spec_spirits_motion section 0) or y > H (0x5BC62C)
	if (!projected || nearClip * 1.1f > projected->depth || projected->y > _control.GetScreen().height)
	{
		_pointOffScreen = true;
	}
	const auto hover = WorldToHover(position, true);
	if (hover && !_pointOffScreen)
	{
		_pointY = hover->y;
		_pointX = hover->x;
		SetState(dude_state::Point, false);
		_lookTarget = position; // fn_005BC6C0(pos)
		return;
	}
	_pointX = 0.0f; // 0x5BC684..0x5BC6A7
	_pointY = 1.0f;
	SetState(dude_state::Point, false);
	_lookTarget.reset();
	_pointOffScreen = true;
}

void HelpDude::SetState(uint32_t next, bool force)
{
	// SetState 0x5BD4A0
	if (next == 0)
	{
		next = _queuedState != 0 ? _queuedState : dude_state::Hover;
	}
	if (_state == next)
	{
		return;
	}
	if (!force && (_state & 0x30) != 0)
	{
		_queuedState = next; // 0x5BD4D8
		return;
	}
	_queuedState = 0;
	bool enter = true;
	if (_state > dude_state::Cling)
	{
		if (_state == dude_state::ClingLeave)
		{
			_state = dude_state::Hover; // 0x5BD535
		}
	}
	else if (_state == dude_state::Cling)
	{
		if ((next & 0x100) == 0)
		{
			next = dude_state::ClingLeave; // 0x5BD524
			enter = false;
		}
	}
	else if (_state == dude_state::PointHoldL || _state == dude_state::PointHoldR)
	{
		if ((next & 8) == 0 && (next & 0x100) == 0)
		{
			next = _state | 0x20; // 0x5BD515: the outro
			enter = false;
		}
	}
	else if (_state == dude_state::PointOutroL || _state == dude_state::PointOutroR)
	{
		_state = dude_state::Hover; // 0x5BD535 (table 0x5BD9AC: 0x29 / 0x2A -> 1)
	}

	if (enter)
	{
		switch (next)
		{
		case dude_state::Hover: // 0x5BD8BB: re-sync the hover from the model's 3D point
		{
			const auto hover = WorldToHover(_position, true);
			const glm::vec2 h = hover ? *hover : glm::vec2(0.0f);
			_hoverX.SetPosition(h.x);
			_hoverY.SetPosition(h.y);
			break;
		}
		case dude_state::Point: // 0x5BD777: the arm
		{
			uint32_t side = _state & 3;
			if (side == 0)
			{
				side = _control.LocalRand(2) != 0 ? 1 : 2; // 0x5BD784 (neg / sbb / add 2)
			}
			const float hx = _hoverX.value;
			if (hx < _pointX || _pointX > 0.25f) // [0x8AB3D4]
			{
				side = 1;
			}
			if (hx > _pointX || _pointX < -0.25f) // [0x8C79F0]
			{
				side = 2;
			}
			if (_inWorldTarget != 0.0f)
			{
				side = 1;
			}
			float x;
			if (side == 1)
			{
				next = dude_state::PointIntroL;
				x = _pointX - 0.2f; // [0x8AB244]
			}
			else
			{
				side = 2;
				next = dude_state::PointIntroR;
				x = _pointX + 0.2f;
			}
			if ((_state & 8) == 0)
			{
				SetHoverY(_pointY, 1.0f, true);
				SetHoverX(x, 1.0f, true);
			}
			else if ((_state & 3) == side)
			{
				next = _state; // the same arm: no change
			}
			else
			{
				SetHoverY(_pointY, 1.0f, true);
				SetHoverX(x, 1.0f, true);
				next = _state | 0x20; // the other arm: outro, then point again
				_queuedState = dude_state::Point;
			}
			break;
		}
		case dude_state::Avoid: // 0x5BD6F7
			_avoidDir &= 3;
			if (_state != dude_state::Hover)
			{
				next = _state;
				break;
			}
			_inWorldTarget = 0.0f;
			_hoverX.SetPosition(_hoverX.value);
			_hoverY.SetPosition(_hoverY.value);
			break;
		case dude_state::FlyToAnim: // 0x5BD5A9
		{
			if (_state == dude_state::FlyToAnim)
			{
				break;
			}
			const float dx = _hoverX.value - _targetX;
			const float dy = _hoverY.value - _targetY;
			float seconds = std::sqrt(dy * dy + dx * dx);
			if (seconds > 0.05f) // [0x8AC3F4]
			{
				if (seconds > 1.5f)
				{
					seconds = 1.5f;
				}
				else if (seconds < 0.5f)
				{
					seconds = 0.5f;
				}
			}
			_inWorldTarget = 0.0f;
			_hoverX.SetPosition(_hoverX.value);
			_hoverY.SetPosition(_hoverY.value);
			SetHoverX(_targetX, seconds, true);
			SetHoverY(_targetY, seconds, true);
			break;
		}
		case dude_state::Cling: // 0x5BD55C
			if ((_state & 0x100) != 0)
			{
				next = dude_state::Cling;
				break;
			}
			_clingClock = 0.0f;
			next = dude_state::ClingArrive;
			SetHoverX(_clingX, 1.0f, false);
			SetHoverY(_clingY, 1.0f, false);
			break;
		default:
			break;
		}
	}
	if (_state != next)
	{
		_stateTime = 0.0f; // 0x5BD98A
	}
	_state = next;
}

void HelpDude::RandomisePuffParticle(PuffParticle& particle) const
{
	// Random 0x81D180 (the CRT stream) six times: the grey first (0x5C1D4C / 0x5C0B6C), then +0, +4, +8, +0xC, +0x14
	const auto g = static_cast<uint32_t>(static_cast<int32_t>(_control.Random(116.0f, 250.0f))); // __ftol
	particle.grey = ((g << 8u | g) << 8u) | g;
	particle.velocity.x = _control.Random(-0.8f, 0.8f); // 0xBF4CCCCD / 0x3F4CCCCD
	particle.velocity.y = _control.Random(-1.9f, 1.5f); // 0xBFF33333 / 0x3FC00000
	particle.velocity.z = _control.Random(-1.0f, 1.0f);
	particle.k = (1.0f - particle.velocity.y) + 1.0f; // fsubr 1, fadd 1 (0x5C1D9A..0x5C1DB3)
	particle.sizeBase = _control.Random(4.0f, 8.0f);
	particle.spin = _control.Random(-2.0f, 2.0f);
	particle.age = 0.0f; // 0x5C1DD9 / 0x5C0BEF
}

void HelpDude::StartPuff()
{
	// fn_005C1D20: the 16 particles drawn again with Random only once the draw has made them (+0x2C10, 0x5C1D2E); the
	// first puff of a dude draws them when fn_005C0700 makes them (UpdateDraw)
	if (_puffParticles)
	{
		for (auto& particle : *_puffParticles)
		{
			RandomisePuffParticle(particle);
		}
	}
	_puffRunning = true; // 0x5C1DEA
	_puffTime = 0.0f;    // 0x5C1DF7
	_alphaTarget = _alpha > 0.5f ? 0.0f : 1.0f; // 0x5C1DF1
}

glm::vec3 HelpDude::HoverTo3D(float hx, float hy, float k, bool nearFlag) const
{
	// fn_005BD2A0
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const auto px = static_cast<int32_t>((hx + 1.0f) * halfW);
	const auto py = static_cast<int32_t>(hy * halfW + static_cast<float>(screen.HalfHeight()));
	const Queries& q = _control.GetQueries();
	float depth;
	if (nearFlag)
	{
		const float nearClip = q.nearClip ? q.nearClip() : 0.0f;
		depth = nearClip + nearClip;
	}
	else
	{
		depth = ((_data.farDepth - _data.nearDepth) * Smooth(_closeness) + _data.nearDepth) * (k * 0.3f + 1.0f);
	}
	const glm::vec2 pixel(static_cast<float>(px), static_cast<float>(py));
	return q.pointFromScreen ? q.pointFromScreen(pixel, depth) : glm::vec3(pixel, depth);
}

std::optional<glm::vec2> HelpDude::WorldToHover(const glm::vec3& p, bool force) const
{
	// Convert3DToHover 0x5BD390
	const Queries& q = _control.GetQueries();
	std::optional<glm::vec2> pixel;
	if (q.worldToPixel)
	{
		pixel = q.worldToPixel(p, force);
	}
	else
	{
		pixel = glm::vec2(p.x, p.y);
	}
	if (!pixel)
	{
		return std::nullopt;
	}
	const Screen& screen = _control.GetScreen();
	const auto halfW = static_cast<float>(screen.HalfWidth());
	return glm::vec2((pixel->x - halfW) / halfW, (pixel->y - static_cast<float>(screen.HalfHeight())) / halfW);
}

float HelpDude::Feel(float x, float y) const
{
	// HelpDude::Feel 0x5B9AD0
	const float limit = _control.Frame().wideScreen ? 0.45f : 0.6f; // 0x3EE66667 / 0x3F19999A
	float e = 0.0f;
	const float ax = std::abs(x);
	const float ay = std::abs(y) * 1.33333337f; // [0x900C50]
	if (ax > 0.75f) // 0x5B9B18
	{
		e = ax - 0.75f;
	}
	if (ay > limit)
	{
		e += (ay - limit) + (ay - limit);
	}
	float sum = e * e * -250.0f; // [0x900C4C]
	for (const auto& zone : _zones)
	{
		sum = zone.Feel(x, y) + sum;
	}
	return sum;
}

void HelpDude::ApplyAnim(uint32_t anim, float phase, float referencePhase, bool wrap)
{
	const CAnim* clip = Clip(anim);
	if (clip == nullptr) // 0x5BB98E
	{
		return;
	}
	const ApplyAnimArgs args = ApplyAnimArguments(*clip, phase, referencePhase, wrap);
	if (anim == anim::GoInvisible) // 0x5BB9D6 -> fn_005BB8F0
	{
		const float p = args.phase;
		if (_index == k_EvilDude)
		{
			_flicker = _flicker || (p > 0.13f && p < 0.25f) || (p > 0.55f && p < 0.7f); // [0x900C60], [0x8D3E80]
		}
		else
		{
			_flicker = _flicker || (p > 0.16f && p < 0.25f); // 0x5BB94B..0x5BB969
		}
	}
	_position += ByRows(args.rootMove, _rows); // 0x5BBA19..0x5BBAD6
	_layers.push_back({AnimLayer::Kind::Add, anim, args.milliseconds, args.referenceKey});
}

void HelpDude::AddAt(uint32_t anim, float weight, uint32_t referenceKey)
{
	const CAnim* clip = Clip(anim);
	if (clip == nullptr)
	{
		return;
	}
	auto ms = static_cast<int32_t>(static_cast<float>(clip->durationMs) * weight);
	if (ms < 0)
	{
		ms = 0;
	}
	if (ms >= clip->durationMs)
	{
		ms = clip->durationMs - 1;
	}
	_layers.push_back({AnimLayer::Kind::Add, anim, ms, referenceKey});
}

void HelpDude::Sound(uint32_t anim, float phase, bool sfx)
{
	if (sfx)
	{
		_sounds.push_back({anim, phase});
	}
}

void HelpDude::UpdateHoverPosition(float probability)
{
	// UpdateHoverPos 0x5BA350
	if (_state == dude_state::Avoid || _state == dude_state::FlyToAnim || (_state & 0x100) != 0 ||
	    _state == dude_state::ScriptedAnim)
	{
		return;
	}
	if (_hoverX.time != _hoverX.duration || _hoverY.time != _hoverY.duration || _puffRunning)
	{
		return;
	}
	const float x0 = _hoverX.value;
	const float y0 = _hoverY.value;
	const float f0 = Feel(x0, y0);
	// 0x5BA3E6..0x5BA401: fabs, fadd qword 1.0 [0x8AB680], fmul 0.3f [0x8AB23C], each rounded to a float (FPU at 24 bits;
	// the double 1.0 is exact in float)
	float radius = (std::abs(f0) + 1.0f) * 0.3f;
	int tries = 1;
	bool urgent = false;
	if (f0 < -1.0f) // [0x8AB678]
	{
		urgent = true;
		radius = 2.0f;
		tries = 25;
	}
	float best = f0;
	glm::vec2 bestPos(x0, y0);
	int32_t hop = -1;
	bool keepHop = false;

	if (partner != nullptr && _partnerDistance < 0.9f) // [0x900C5C]
	{
		// the Avoid hop away from the partner
		const float dx = _hoverX.value - partner->_hoverX.value;
		const float dy = _hoverY.value - partner->_hoverY.value;
		uint32_t dir;
		if (std::abs(dy) < std::abs(dx))
		{
			dir = dx < 0.0f ? 0 : 1;
		}
		else
		{
			dir = dy < 0.0f ? 2 : 3;
		}
		if (const CAnim* clip = Clip(anim::AvoidL + dir); clip != nullptr)
		{
			const glm::vec3 landing = ByRows(clip->displacement, _rows) + _position;
			if (const auto h = WorldToHover(landing, true))
			{
				const float f1 = (Feel(h->x, h->y) - f0) * 6.0f + f0; // [0x8AB35C]
				if (f1 > f0)
				{
					best = f1;
					bestPos = *h;
					hop = static_cast<int32_t>(dir);
					if (_control.LocalRand(10) >= 5) // 0x5BA5EA
					{
						keepHop = true;
					}
					else
					{
						hop = -1; // the search goes on from the hop's score (sic)
					}
				}
			}
		}
	}
	if (!keepHop)
	{
		for (int i = 0; i < tries; ++i)
		{
			const float span = radius + radius;
			float cx = _control.LocalFloatRand(span) + x0 - radius;
			float cy = _control.LocalFloatRand(span) + y0 - radius;
			if (cx < -1.0f)
			{
				cx = -1.0f;
			}
			else if (!(cx <= 1.0f))
			{
				cx = 1.0f;
			}
			if (cy < -0.75f)
			{
				cy = -0.75f;
			}
			else if (!(cy <= 0.75f))
			{
				cy = 0.75f;
			}
			const float f = Feel(cx, cy);
			if (f > best)
			{
				best = f;
				bestPos = {cx, cy};
				hop = -1;
			}
		}
	}
	if (!urgent)
	{
		float gain = 0.0f;
		if (best > f0)
		{
			gain = (best - f0) * probability;
		}
		if (_control.LocalFloatRand(1.0f) > gain) // 0x5BA71D..0x5BA72E
		{
			return;
		}
	}
	if (hop >= 0)
	{
		_avoidDir = static_cast<uint32_t>(hop);
		SetState(dude_state::Avoid, false);
		return;
	}
	SetHoverX(bestPos.x, 1.0f, true);
	SetHoverY(bestPos.y, 1.0f, true);
}

void HelpDude::UpdateDepthSpacing(float zMin)
{
	// fn_005BA010
	if (_gimme || (_activeFlags & 0x40) != 0)
	{
		_depth.SetDestinationWithSpeedAndTime(0.0f, 0.0f, 0.5f);
		return;
	}
	if (partner != nullptr)
	{
		const float dy = _hoverY.value - partner->_hoverY.value;
		const float dx = _hoverX.value - partner->_hoverX.value;
		const float d2 = dx * dx + dy * dy;
		const float d = std::sqrt(d2);
		_partnerDistance = d;
		partner->_partnerDistance = d;
		if (d2 < 0.81f) // [0x900C58]
		{
			const float k = d * 1.11111116f; // [0x900C54]
			partner->_depth.SetDestinationWithSpeedAndTime((1.0f - k * k) * -0.5f, 0.0f, 1.0f);
			const float own = dy < zMin ? zMin : dy;
			_depth.SetDestinationWithSpeedAndTime(own, 0.0f, 1.0f);
			return;
		}
	}
	// the inline SetDestinationWithSpeedAndTime(0, 0, 1) (0x5BA113..0x5BA210 / 0x5BA21D..0x5BA31A)
	_depth.SetDestinationWithSpeedAndTime(0.0f, 0.0f, 1.0f);
}

void HelpDude::UpdateZones(float dt, bool focus)
{
	// fn_005BDAF0
	const FrameInput& frame = _control.Frame();
	Zone& partnerZone = _zones[1];
	if (partner != nullptr && partner->_inWorld == 0.0f)
	{
		partnerZone.strength = focus ? -0.5f : -5.0f; // [0x8CEFCC] / [0x8C79A8]
		const float r = partner->_modelScale * partner->_data.scale * partner->_data.modelSize /
		                partner->_data.nearDepth;
		partnerZone.outer = r + r;
		partnerZone.inner = partnerZone.outer * 0.1f;
		partnerZone.x = partner->_hoverX.value;
		partnerZone.y = partner->_hoverY.value;
	}
	else
	{
		partnerZone.strength = 0.0f;
	}
	if ((_state & 8) != 0)
	{
		_zones[2] = {4.0f, _pointX, _pointY, 0.2f, 0.6f};      // 0x5BDB91..0x5BDBB5
		_zones[3] = {-1.0f, _pointX, _pointY, 0.08f, 0.16f};  // 0x5BDBBB..0x5BDBD9
	}
	else
	{
		_zones[2].strength = 0.0f;
		_zones[3].strength = 0.0f;
	}
	const Screen& screen = frame.screen;
	const auto halfW = static_cast<float>(screen.HalfWidth());
	const auto halfH = static_cast<float>(screen.HalfHeight());
	Zone& mouse = _zones[4];
	mouse.inner = 0.16f; // 0x3E23D70A
	mouse.outer = 0.6f;
	const float mdx = static_cast<float>(frame.mouse.x - _lastMouse.x) / halfW;
	const float mdy = static_cast<float>(frame.mouse.y - _lastMouse.y) / halfW;
	_lastMouse = frame.mouse;
	// (openblack) a frame of dt 0 gives speed 0 instead of the original's division by zero
	_mouseSpeed = dt > 0.0f ? std::sqrt(mdy * mdy + mdx * mdx) / dt : 0.0f;
	const float candidate = _mouseSpeed * -1.5f - 2.0f; // [0x900C6C], [0x8AB478]
	if (candidate < mouse.strength)
	{
		mouse.strength = candidate;
	}
	else
	{
		mouse.strength = (candidate - mouse.strength) * 0.3f + mouse.strength;
	}
	mouse.x = (static_cast<float>(frame.mouse.x) - halfW) / halfW;
	mouse.y = (static_cast<float>(frame.mouse.y) - halfH) / halfW;
	if (frame.wideScreen)
	{
		mouse.strength = 0.0f;
	}
	_zones[5] = {-4.0f, 0.0f, 0.0f, 0.0f, 0.6f}; // 0x5BDD0C..0x5BDD28
}

void HelpDude::UpdateMatrix(float dt)
{
	// Update1 0x5BE2F5..0x5BE6E4: the HUD pose
	const Queries& q = _control.GetQueries();
	const glm::mat3 camera = q.cameraAxes ? q.cameraAxes() : glm::mat3(1.0f);
	const float scale = _data.scale * _modelScale; // 0x5BE323..0x5BE329
	glm::mat3 rows = camera;
	for (int i = 0; i < 3; ++i)
	{
		rows[i] *= scale; // 0x5BE32F..0x5BE372
	}
	// each turn below stores its cosine as a float and keeps the sine on the FPU stack (fcos; fstp [esp+0x78]; fsin)
	const auto turn = [](float angle) {
		return std::pair<double, double>(static_cast<float>(std::cos(static_cast<double>(angle))),
		                                 std::sin(static_cast<double>(angle)));
	};
	float c = 0.0f;
	if (_state == dude_state::Cling)
	{
		c = 1.0f;
	}
	else if (_state == dude_state::ClingArrive)
	{
		c = _stateTime;
	}
	else if (_state == dude_state::ClingLeave)
	{
		c = 1.0f - _stateTime;
	}
	const float smooth = Smooth(c);
	const float k = 1.0f - smooth;
	{
		// the pitch (0x5BE3CB..0x5BE456): r1' = c r1 - s r2, r2' = c r2 + s r1
		const float a = (_depth.speed * 0.3f + _data.pitchOffset) * k;
		const auto [ca, sa] = turn(a);
		lh_matrix::RotateX(rows, ca, sa);
	}
	{
		// the yaw (0x5BE459..0x5BE4DE, [0x8AA3A8]): r0' = c r0 + s r2, r2' = c r2 - s r0 (RotateY)
		const float b = _hoverX.value * -0.8f * k;
		const auto [cb, sb] = turn(b);
		lh_matrix::RotateY(rows, cb, sb);
	}
	{
		// the roll toward the cling edge, the short way at 3 rad/s (0x5BE4E1..0x5BE5FA)
		float target = static_cast<float>(((static_cast<int32_t>(_edge) - 2) & 3) - 2) * k_HalfPi * smooth;
		if (_roll < 0.0f)
		{
			_roll += k_TwoPi;
		}
		if (_roll > k_TwoPi)
		{
			_roll -= k_TwoPi;
		}
		if (_state != dude_state::Cling && _state != dude_state::ClingArrive)
		{
			target = 0.0f;
		}
		float diff = target - _roll;
		if (diff > k_Pi)
		{
			diff -= k_TwoPi;
		}
		if (diff < -k_Pi)
		{
			diff += k_TwoPi;
		}
		const float goal = diff + _roll;
		const float rate = dt * 3.0f;
		if (goal < _roll)
		{
			_roll -= rate;
			if (goal > _roll)
			{
				_roll = goal;
			}
		}
		if (goal > _roll)
		{
			_roll += rate;
			if (goal < _roll)
			{
				_roll = goal;
			}
		}
		// 0x5BE5FC..0x5BE69D: r0' = c r0 - s r1, r1' = c r1 + s r0 (fn_0086AFA0's turn, RotateZ)
		const auto [cr, sr] = turn(_roll);
		lh_matrix::RotateZ(rows, cr, sr);
	}
	_rows = rows;
	_position = HoverTo3D(_hoverX.value, _hoverY.value, -_depth.value, false); // 0x5BE6D2

	if (_inWorld == 0.0f)
	{
		return;
	}
	// 0x5BE702..0x5BE9B6: into the world, next to the target
	const float sb = Smooth(_inWorld);
	_control.worldBob += dt * 0.3f; // [0xD15AA4]
	const glm::vec3 axisR0 = camera[0];
	glm::vec3 t = _worldTarget.GetCurrentValue();
	t.y += _worldHeight + std::sin(_control.worldBob * 2.3f); // [0x900C8C]
	t -= _worldSide * axisR0;
	// GetAltitude at (ftol(x 65536 0.1), ftol(z 65536 0.1)) + 3 ([0x8C2C50], 0x5BE7FA..0x5BE83D)
	const float ground = (q.altitude ? q.altitude(t.x, t.z) : 0.0f) + 3.0f;
	if (t.y < ground)
	{
		t.y = ground;
	}
	for (int i = 0; i < 3; ++i)
	{
		_rows[i] = (camera[i] - _rows[i]) * sb + _rows[i];
	}
	_position = (t - _position) * sb + _position;
	const float grow = (sb * 3.0f + 1.0f) * _data.scale * _modelScale; // 0x5BE943..0x5BE96A
	lh_matrix::NormaliseRows(_rows); // fn_007FB5C0 0x5BE93E
	for (int i = 0; i < 3; ++i)
	{
		_rows[i] *= grow; // 0x5BE970..0x5BE9B3
	}
}

void HelpDude::UpdateBasePose(bool stable, float rate, bool sfx)
{
	// fn_005BC7D0
	if (stable)
	{
		if (_stableBlend < 1.0f)
		{
			_stableBlend += rate;
			if (_stableBlend > 1.0f)
			{
				_stableBlend = 1.0f;
			}
		}
	}
	else if (_stableBlend > 0.0f)
	{
		_stableBlend -= rate;
		if (_stableBlend < 0.0f)
		{
			_stableBlend = 0.0f;
		}
	}
	const CAnim* hover = Clip(anim::Hover);
	const CAnim* hoverStable = Clip(anim::HoverStable);
	const auto clock = static_cast<int32_t>(_hoverClock * 1000.0f);
	if (_stableBlend > 0.0f && hoverStable != nullptr)
	{
		if (!(_stableBlend < 1.0f))
		{
			const int32_t ms = clock % hoverStable->durationMs;
			_layers.push_back({AnimLayer::Kind::Set, anim::HoverStable, ms});
			Sound(anim::HoverStable, static_cast<float>(ms) / static_cast<float>(hoverStable->durationMs), sfx);
		}
		else if (hover != nullptr)
		{
			const int32_t msHover = clock % hover->durationMs;
			const int32_t msStable = clock % hoverStable->durationMs;
			AnimLayer layer {AnimLayer::Kind::SetBlend, anim::Hover, msHover};
			layer.clipB = anim::HoverStable;
			layer.millisecondsB = msStable;
			layer.blend = _stableBlend;
			_layers.push_back(layer);
			// sic: the stable clip's time over the hover clip's duration (0x5BCA99)
			Sound(anim::Hover, static_cast<float>(msStable) / static_cast<float>(hover->durationMs), sfx);
		}
	}
	else if (hover != nullptr)
	{
		const int32_t ms = clock % hover->durationMs;
		_layers.push_back({AnimLayer::Kind::Set, anim::Hover, ms});
		Sound(anim::Hover, static_cast<float>(ms) / static_cast<float>(hover->durationMs), sfx);
	}
	if (const CAnim* wing = Clip(anim::Wingflap); wing != nullptr) // empty in both files
	{
		const int32_t ms = clock % wing->durationMs;
		_layers.push_back({AnimLayer::Kind::Add, anim::Wingflap, ms, 0});
		Sound(anim::Wingflap, static_cast<float>(ms) / static_cast<float>(wing->durationMs), sfx);
	}
	const float step = (_index == k_EvilDude ? 1.01f : 0.999811f) * rate; // [0x8DE2D0] / [0x900C68]
	_hoverClock += step;
	_hoverClock2 += step;
}

void HelpDude::UpdateFace(float dt)
{
	// fn_005BD0B0
	float w = _emotionWeight;
	if (!(w > 0.0f))
	{
		w = 0.0f;
	}
	else if (w > 1.0f)
	{
		w = 1.0f;
	}
	const auto& neutral = _data.faces[0];
	// (openblack) guard: the original indexes +0x2C38 by +0x2C28 << 6 without a mask (0x5BD0F6..0x5BD104); the tags
	// and the file only give 0..7
	const auto& current = _data.faces[_emotion & 7];
	for (size_t i = 0; i < _face.size(); ++i)
	{
		_face[i] = (current[i] - neutral[i]) * w + neutral[i]; // fn_005BAA40
	}
	// the blink fn_005BAC10: rec[0] / rec[1] the wait, rec[2] its length
	_blinkTimer += dt;
	if (_blinkTimer > _face[2])
	{
		_blinkTimer = -(_control.LocalFloatRand(_face[1] - _face[0]) + _face[0]);
		if (_control.LocalRand(5) == 0)
		{
			_blinkTimer = -0.01f; // [0x8CEFC4]: a double blink
		}
	}
	float b = 0.0f;
	if (_blinkTimer > 0.0f)
	{
		b = (_blinkTimer + _blinkTimer) / _face[2];
		if (b > 1.0f)
		{
			b = 2.0f - b;
		}
		b *= 1.2f; // [0x8C6C98]
		if (b > 1.0f)
		{
			b = 1.0f;
		}
	}
	const float lidL = (1.0f - _face[9]) * b + _face[9];
	const float lidR = (1.0f - _face[10]) * b + _face[10];
	if ((_activeFlags & 8) == 0)
	{
		// fn_005C0310: the eye bones scaled by rec[3] / rec[4] (renderer), then the two lids
		// fn_005C0310: ms clamped to [0, dur - 1] (0x5C056B..0x5C057D, 0x5C05BB..0x5C05D1), reference key 0
		AddAt(anim::LeftEyeShut, lidL, 0);
		AddAt(anim::RightEyeShut, lidR, 0);
	}
	// fn_005BF9D0, the pupils, is the renderer's (rec[5..8])
	// clips[+0x2C28 + 10], no mask (0x5BD19C..0x5BD1A5; Clip's bound is the 80 slots)
	if (Clip(anim::FirstEmotion + _emotion) != nullptr && w > 0.0f)
	{
		AddAt(anim::FirstEmotion + _emotion, w, 0); // 0x5BD19C..0x5BD1FE
	}
}

void HelpDude::UpdateAnimStack(float dt, bool sfx)
{
	// ApplyLipSync 0x5BCD00
	_gimme = false;
	const Queries& q = _control.GetQueries();
	// IsTalking 0x5BCD11 and [0xD15A9C] 0x5BCD1E, the times and CalcKey are audio::advisor's (LipSyncFrame); so is the
	// StopSentence of a position < 0 past 0.5 s (0x5BCDE2..0x5BCDF7)
	if (const std::optional<LipSyncFrame> lip = q.lipSync ? q.lipSync(_index) : std::nullopt; lip)
	{
		if (lip->playing)
		{
			// fn_005BF810(+0x2F60) 0x5BCDD2: the vowels 7..9 at ms = clamp(ftol(dur w), 0, dur - 1), key 0, for each
			// weight > 0 (0x5BF833..0x5BF892)
			for (uint32_t i = 0; i < 3; ++i)
			{
				if (lip->weights.at(i) > 0.0f)
				{
					AddAt(anim::VowelE + i, lip->weights.at(i), 0);
				}
			}
		}
		// fn_005BCBC0(+0x2F70, 1) 0x5BCE07, in both cases: every tag whose time the sentence has passed (fld t; fcomp
		// time; it fires while t > the tag's time and stops on <=, 0x5BCBE1..0x5BCBF3)
		while (_nextTag < _tags.size() && lip->time > _tags[_nextTag].time)
		{
			FireTag(_tags[_nextTag], true, true);
			++_nextTag;
		}
	}

	uint32_t flags = 0;
	for (uint32_t i = 0; i < k_AnimSlots; ++i)
	{
		int32_t& mode = _slotMode[i];
		if (mode == 0)
		{
			continue;
		}
		const CAnim* clip = Clip(i);
		float step = dt;
		float start = 0.0f;
		float end = 0.0f;
		float window = 0.0f;
		if (clip != nullptr)
		{
			step = dt / (static_cast<float>(clip->durationMs) * 0.001f);
			start = _data.loopStart[i];
			end = _data.loopEnd[i];
			window = end - start;
		}
		const float old = _slotPhase[i];
		float phase = old;
		if (mode == 1 || mode == 4) // table 0x5BD098: play once
		{
			phase = old + step;
			if (phase > 1.0f)
			{
				mode = 0;
				_slotLast[i] = -1.0f;
				continue;
			}
		}
		else if (mode == 2 || mode == 3) // loop the window
		{
			if (!(window > 0.0f))
			{
				mode = 1;
				phase = old + step;
				if (phase > 1.0f)
				{
					phase = 1.0f;
				}
			}
			else if (old < end)
			{
				phase = old + step;
				if (!(phase < end))
				{
					const float x = (phase - start) / window;
					phase = (x - static_cast<float>(static_cast<int32_t>(x))) * window + start;
				}
			}
			else
			{
				mode = 4;
				phase = old + step;
				if (phase > 1.0f)
				{
					phase = 1.0f;
				}
			}
		}
		else
		{
			continue;
		}
		if (i == anim::GimmeFive)
		{
			// FlyToGimme 0x5BCC90
			if (_index == k_EvilDude)
			{
				SetHoverX(0.15f, 0.4f, true);
				SetHoverY(0.0f, 0.4f, true);
			}
			else
			{
				SetHoverX(-0.15f, 0.4f, true);
				SetHoverY(0.1f, 0.4f, true);
			}
			_gimme = true;
		}
		const bool skip = (_data.flags[i] & 0x20) != 0 && (_state & 0x100) != 0;
		if (!skip)
		{
			ApplyAnim(i, phase, 0.0f, false);
			Sound(i, old, sfx);
		}
		_slotPhase[i] = phase;
		flags |= static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(_data.flags[i]))); // movsx
	}
	_activeFlags = flags;
}

void HelpDude::FireTag(const AudioTag& tag, bool resolve, bool apply)
{
	// fn_0042ACC0
	if (resolve)
	{
		HelpDude* target = nullptr;
		HelpDude* second = nullptr;
		switch (tag.who)
		{
		case 0:
			target = this;
			break;
		case 1:
			target = partner;
			break;
		case 2:
			target = _index == k_EvilDude ? partner : this;
			break;
		case 3:
			target = _index == k_EvilDude ? this : partner;
			break;
		case 4:
			target = this;
			second = partner;
			break;
		default:
			return;
		}
		if (target != nullptr)
		{
			target->FireTag(tag, false, true);
		}
		if (second != nullptr)
		{
			second->FireTag(tag, false, true);
		}
		return;
	}
	const auto index = static_cast<uint32_t>(tag.index % static_cast<int32_t>(k_AnimSlots));
	switch (tag.action)
	{
	case 1:
		if (apply)
		{
			_slotPhase[index] = 0.0f;
			_slotMode[index] = 1;
		}
		break;
	case 2:
		if (apply)
		{
			_slotPhase[index] = 0.0f;
			_slotMode[index] = 2;
		}
		break;
	case 3:
		if (_slotMode[index] == 2 || _slotMode[index] == 3)
		{
			_slotMode[index] = 4;
		}
		break;
	case 4:
		SetEmotion(index, static_cast<float>(tag.value) * 0.01f); // [0x8C4B10]
		break;
	case 5:
	case 6:
		_savedLookMode = _tagLookMode;
		_tagLookMode = static_cast<int32_t>(index);
		break;
	case 7:
		_tagLookMode = _savedLookMode;
		_savedLookMode = 0;
		break;
	default:
		break;
	}
}

void HelpDude::Update1(float dt, bool focus, float zMin, bool sfx)
{
	// HelpDude::Update1 0x5BDDA0
	_totalTime += dt;
	_stateTime += dt;

	// 2. the in-world blend toward (state & 8 ? +0x3670 : 0) at 1/s
	const float target = (_state & 8) != 0 ? _inWorldTarget : 0.0f;
	if (target > _inWorld)
	{
		_inWorld += dt;
		if (_inWorld > target)
		{
			_inWorld = target;
		}
	}
	else if (target < _inWorld)
	{
		_inWorld -= dt;
		if (_inWorld < target)
		{
			_inWorld = target;
		}
	}

	// 3. how close the spirit comes (+0x34D0)
	const Queries& q = _control.GetQueries();
	const bool talked = q.talkedRecently ? q.talkedRecently(_index) : (q.isTalking && q.isTalking(_index));
	// 0x5BDE4F..0x5BDEA3: four rules in order, the last that applies wins
	if (talked)
	{
		_closenessTarget = 1.0f;
		_closenessRate = 2.0f;
	}
	else
	{
		_closenessTarget = 0.0f;
		_closenessRate = 1.5f;
	}
	if ((_activeFlags & 0x40) != 0)
	{
		_closenessTarget = 0.0f;
		_closenessRate = 2.5f;
	}
	if (_gimme)
	{
		_closenessTarget = 1.0f;
		_closenessRate = 0.5f;
	}
	if (_closeness < _closenessTarget)
	{
		_closeness += dt * _closenessRate;
		if (_closeness > _closenessTarget)
		{
			_closeness = _closenessTarget;
		}
	}
	if (_closeness > _closenessTarget)
	{
		_closeness -= dt * _closenessRate;
		if (_closeness < _closenessTarget)
		{
			_closeness = _closenessTarget;
		}
	}

	// 4. the hover search and the depth spacing
	UpdateHoverPosition(_state == dude_state::Hover ? 1.0f : k_Third);
	if (focus)
	{
		UpdateDepthSpacing(zMin);
	}

	// 5. the hover channels (inline Zoomer::Update 0x5BDF6A..0x5BE1A9) and the world target
	_hoverX.Update(dt);
	_hoverY.Update(dt);
	_depth.Update(dt);
	_worldTarget.Update(dt);

	// 6. the zones; fn_005BDD40's four records +0x3404 (pending: nothing reads them here)
	UpdateZones(dt, focus);
	_emotionTime += dt;

	// 7. the emotion cross-fade (0x5BE1F5..0x5BE2F5)
	const float fade = dt * 3.0f; // 0x5BE207
	if (_emotion != _emotionTarget)
	{
		_emotionWeight -= fade;
		if (_emotionWeight < 0.0f)
		{
			_emotionWeight = -_emotionWeight;
			if (_emotionWeight > _emotionPeak)
			{
				_emotionWeight = _emotionPeak;
			}
			_emotion = _emotionTarget;
		}
	}
	else
	{
		_emotionWeight += fade;
		if (_emotionWeight > _emotionPeak)
		{
			_emotionWeight = _emotionPeak;
		}
	}
	if (!(_emotionWeight > 0.0f))
	{
		_emotionWeight = 0.0f;
		_emotion = 0;
	}
	if (_emotionTarget != 0 && _emotionTime > 1.5f) // 0x5BE2BF
	{
		_emotionPeak -= dt * 0.3f; // 0x5BE2D0
		if (_emotionPeak < 0.0f)
		{
			_emotionPeak = 0.0f;
		}
	}

	// 8. / 9. the model matrix and the in-world pose
	UpdateMatrix(dt);

	// 10. the base pose
	if (_data.clips[anim::Stand] != nullptr)
	{
		_layers.push_back({AnimLayer::Kind::Set, anim::Stand, 0}); // fn_005BB8B0(0)
	}
	if (_state != dude_state::Cling)
	{
		const bool stable = !(_state == dude_state::Hover && (_activeFlags & 2) == 0);
		UpdateBasePose(stable, dt * _face[11], sfx);
	}
	// 11. the face
	if ((_activeFlags & 4) == 0)
	{
		UpdateFace(dt);
	}
	// 12. the mouth, the tags and the 80 slots
	UpdateAnimStack(dt, sfx);

	// 13. the state switch (0x5BEB0A..0x5BEEB1)
	const float shown = 1.0f - _pointBlend;
	uint32_t next = _state;
	const bool noArm = (_activeFlags & 0x10) != 0;
	// [esp+0x78]: 1.0 (0x5BEB27), `shown` in a point hold with the arm (0x5BECAB / 0x5BECD6); the point arm's angle is
	// scaled by it (0x5BF373)
	float pointWeight = 1.0f;
	switch (_state)
	{
	case dude_state::PointIntroL:
	case dude_state::PointIntroR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::PointIntroL ? anim::PointLIn : anim::PointRIn, shown * _stateTime * 4.0f,
			          0.0f, false); // 0x5BEB6D
		}
		if (_stateTime > 0.25f) // 0x5BEB8F
		{
			next = _state == dude_state::PointIntroL ? dude_state::PointHoldL : dude_state::PointHoldR;
		}
		break;
	case dude_state::PointHoldL:
	case dude_state::PointHoldR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::PointHoldL ? anim::PointLIn : anim::PointRIn, shown, 0.0f, false);
			pointWeight = shown;
		}
		break;
	case dude_state::PointOutroL:
	case dude_state::PointOutroR:
		if (!noArm)
		{
			ApplyAnim(_state == dude_state::PointOutroL ? anim::PointLIn : anim::PointRIn,
			          (1.0f - _stateTime * 4.0f) * shown, 0.0f, false); // 4 t (as 0x5BEB6D)
		}
		if (_stateTime > 0.25f) // 0.25 (as 0x5BEB8F)
		{
			next = 0;
		}
		break;
	case dude_state::Avoid:
	{
		const uint32_t clip = anim::AvoidL + _avoidDir;
		ApplyAnim(clip, _stateTime * 1.5f, 0.0f, false);
		Sound(clip, _stateTime * 1.5f, sfx);
		if (_stateTime > 0.666666687f) // [0x900C88]
		{
			next = 0;
			_slotLast[clip] = -1.0f;
		}
		break;
	}
	case dude_state::FlyToAnim:
		if (_hoverX.time == _hoverX.duration && _hoverY.time == _hoverY.duration)
		{
			next = _afterFly;
		}
		break;
	case dude_state::Cling:
		SnapClingEdge();
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::ClingArrive:
		SnapClingEdge();
		if (!(_stateTime < 1.0f))
		{
			next = dude_state::Cling;
		}
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::ClingLeave:
		if (!(_stateTime < 1.0f))
		{
			next = 0;
		}
		SetHoverX(_clingX, 1.0f, false);
		SetHoverY(_clingY, 1.0f, false);
		break;
	case dude_state::ScriptedAnim:
		if (const CAnim* clip = Clip(_scriptAnim); clip != nullptr)
		{
			const float p = _animSpeed * _stateTime * 1000.0f / static_cast<float>(clip->durationMs);
			ApplyAnim(_scriptAnim, p, 0.0f, false);
			Sound(_scriptAnim, p, sfx);
			if (p > 1.0f)
			{
				next = 0;
				_slotLast[_scriptAnim] = -1.0f;
			}
		}
		break;
	default:
		break;
	}

	// 14. the cling clip (0x5BEEB1..0x5BF124)
	if ((_state & 0x100) != 0)
	{
		static constexpr std::array<uint32_t, 4> k_ClingClips = {anim::ClingD, anim::ClingL, anim::ClingU,
		                                                         anim::ClingR};
		const uint32_t c = k_ClingClips[static_cast<size_t>(_edge) & 3];
		if (const CAnim* clip = Clip(c); clip != nullptr)
		{
			float w = 1.0f;
			if (_state == dude_state::ClingArrive)
			{
				w = _stateTime;
			}
			if (_state == dude_state::ClingLeave)
			{
				w = 1.0f - _stateTime;
			}
			const float loopEnd = _data.loopEnd[c];
			if (!clip->frames.empty() && !clip->frames[0].positions.empty())
			{
				// keep the gripping hand still: the first position channel at the loop's end minus at key 0
				auto key = static_cast<size_t>(static_cast<int32_t>(static_cast<float>(clip->frameCount) * loopEnd));
				key = std::min(key, clip->frames.size() - 1); // (openblack) the original reads past the last key
				const glm::vec3 d = (clip->frames[key].positions[0] - clip->frames[0].positions[0]) * w;
				_position -= ByRows(d, _rows);
			}
			const auto duration = static_cast<float>(clip->durationMs);
			float p = _clingClock * 1000.0f / duration;
			if (_state == dude_state::Cling)
			{
				if (p > loopEnd)
				{
					p = loopEnd;
					_clingClock = loopEnd * duration * 0.001f;
				}
				else
				{
					_clingClock += dt;
				}
			}
			if (_state == dude_state::ClingLeave)
			{
				p = (1.0f - loopEnd) * _stateTime + loopEnd;
			}
			if (p < 0.0f)
			{
				p = 0.0f;
			}
			else if (p > 1.0f)
			{
				p = 1.0f;
			}
			ApplyAnim(c, p, 0.0f, false);
			Sound(c, p, sfx);
		}
	}

	// 15. the bank lean (0x5BF124..0x5BF1CA)
	{
		// 0.025 (0x5BF136), 0.8 (0x5BF142), the dead zone 0.01 (0x5BF159)
		float w = _hoverX.speed / (_data.scale * _modelScale) * 0.025f * (1.0f - _stableBlend * 0.8f);
		if (std::abs(w) > 0.01f)
		{
			uint32_t clip = anim::HoverRight;
			if (w < 0.0f)
			{
				clip = anim::HoverLeft;
				w = -w;
			}
			if (w != 0.0f && _inWorld == 0.0f)
			{
				ApplyAnim(clip, w, 0.0f, false);
			}
		}
	}

	// 16. the point arm (0x5BF1CF..0x5BF562)
	if ((_state & 8) != 0)
	{
		const glm::vec3 tip = q.fingertip ? q.fingertip(_index, _rows, _position) : _position;
		glm::vec2 tipHover;
		if (const auto h = WorldToHover(tip, true))
		{
			tipHover = *h;
		}
		else
		{
			tipHover = {_hoverX.value, _hoverY.value};
		}
		float dx = tipHover.x - _pointX;
		const float dy = tipHover.y - _pointY;
		uint32_t arm = anim::PointR;
		if ((_state & 1) != 0)
		{
			arm = anim::PointL;
			dx = -dx;
		}
		// 0x5BF36B..0x5BF37D: fpatan (not rounded by the precision), fmul qword 0.57295777918682045 [0x900C80] rounded
		// to a float (FPU at 24 bits), fmul [esp+0x78] (pointWeight), fadd 0.5f, each rounded to a float
		const auto degrees = static_cast<float>(static_cast<double>(std::atan2(dy, dx)) * 0.57295777918682045);
		float p = degrees * pointWeight + 0.5f;
		if (_inWorld != 0.0f)
		{
			p = 0.2f;
			arm = anim::PointL;
		}
		else if (p < 0.0f)
		{
			p = 0.0f;
			SetHoverY(_pointY, 1.0f, true);
		}
		else if (p > 1.0f)
		{
			p = 1.0f;
			SetHoverY(_pointY, 1.0f, true);
		}
		const float side = _hoverX.value - _pointX;
		if (std::abs(side) < 0.16f || std::abs(side) > 0.3f)
		{
			SetHoverX(side < 0.0f ? _pointX - 0.2f : _pointX + 0.2f, 1.0f, true);
		}
		if (_pointOffScreen)
		{
			SetHoverX(_pointX, 1.0f, true);
			SetHoverY(_pointY, 1.0f, true);
			ApplyAnim(arm, (1.0f - _pointBlend) * p + _pointBlend * 0.5f, 0.5f, false);
			ApplyAnim(anim::PointAtCamera, _pointBlend * 0.5f, 0.0f, false);
			_pointBlend += dt * 0.5f;
			if (_pointBlend > 1.0f)
			{
				_pointBlend = 1.0f;
			}
		}
		else
		{
			ApplyAnim(arm, p, 0.5f, false);
			_pointBlend -= dt * 0.5f;
			if (_pointBlend < 0.0f)
			{
				_pointBlend = 0.0f;
			}
		}
	}

	// 17.
	SetState(next, true);
}

void HelpDude::UpdateLookTarget(float dt, bool engaged, bool partnerOut, bool talkOrPoint,
                                const glm::vec3* lookPosition)
{
	// fn_005BC0A0
	const FrameInput& frame = _control.Frame();
	const Queries& q = _control.GetQueries();
	const int32_t halfW = frame.screen.HalfWidth();
	const int32_t halfH = frame.screen.HalfHeight();
	// the mouse in hover space by integer division (idiv, 0x5BC0CB / 0x5BC0FD): -1, 0 or 1 per axis (sic)
	const float mx = static_cast<float>((frame.mouse.x - halfW) / halfW) - _hoverX.value;
	const float my = static_cast<float>((frame.mouse.y - halfH) / halfH) - _hoverY.value;
	const float distance = std::sqrt(my * my + mx * mx);

	int32_t mode = _tagLookMode;
	int32_t alternate = 2;
	switch (_tagLookMode)
	{
	case 0:
		if (engaged)
		{
			mode = 2;
			alternate = 1;
		}
		else
		{
			mode = 1;
			alternate = 2;
		}
		if ((_state & 8) != 0)
		{
			alternate = mode;
			mode = 3;
		}
		break;
	case 2:
		alternate = 1;
		break;
	case 4:
		mode = 4;
		alternate = 4;
		break;
	default:
		break;
	}
	if (lookPosition != nullptr)
	{
		mode = 5;
		alternate = partnerOut ? 1 : 2;
	}
	// the interest in a fast mouse (+0x3344): speed 4, distance 0.3, Random < 0.3, 0.2 and 2, the decay 0.5 / s, the clamp
	// [0, 3] (0x5BC1A0..0x5BC248)
	if (_mouseSpeed > 4.0f && distance < 0.3f && _mouseInterest < 1.0f && _control.Random(0.0f, 1.0f) < 0.3f)
	{
		_mouseInterest = (_mouseSpeed - 4.0f) * 0.2f + 2.0f;
	}
	else
	{
		_mouseInterest -= dt * 0.5f;
	}
	if (_mouseInterest < 0.0f)
	{
		_mouseInterest = 0.0f;
	}
	if (_mouseInterest > 3.0f)
	{
		_mouseInterest = 3.0f;
	}
	const bool talked = q.talkedRecently ? q.talkedRecently(_index) : (q.isTalking && q.isTalking(_index));
	if (!engaged && _mouseInterest > 1.0f && !talked)
	{
		alternate = mode;
		mode = 4;
	}
	if (talked || _state == dude_state::ScriptedAnim)
	{
		mode = 2;
		alternate = 2;
	}
	// the alternation (+0x3348 / +0x334C): Random(2, 4) / Random(3, 6) (0x5BC2A1..0x5BC2E9)
	_lookTimer -= dt;
	if (_lookTimer < 0.0f)
	{
		_lookToggle = !_lookToggle;
		_lookTimer += _lookToggle ? _control.Random(2.0f, 4.0f) : _control.Random(3.0f, 6.0f);
	}
	if (_lookToggle)
	{
		mode = alternate;
	}
	if ((mode == 1 && !partnerOut) || (mode == 3 && (_state & 8) == 0))
	{
		mode = 2;
	}
	_lookMode = mode;

	switch (mode)
	{
	case 1: // 0x5BC720: the partner's model point
		if (partner != nullptr)
		{
			_lookTarget = partner->_position;
		}
		break;
	case 3:
		_lookTarget = HoverTo3D(_pointX, _pointY, -_depth.value - 0.4f, false); // [0x8C7A44]
		break;
	case 4:
	{
		const glm::vec2 pixel(static_cast<float>(frame.mouse.x), static_cast<float>(frame.mouse.y));
		const float depth = _data.nearDepth * 0.8f; // [0x8C4A04]
		_lookTarget = q.pointFromScreen ? q.pointFromScreen(pixel, depth) : glm::vec3(pixel, depth);
		break;
	}
	case 5:
		if (lookPosition != nullptr)
		{
			// the look position is converted (0x5BC350) and then not used (sic): the point target instead
			_lookTarget = HoverTo3D(_pointX, _pointY, -_depth.value - 0.5f, false);
		}
		break;
	default: // 2 and anything else (0x5BC45D): the camera while talking or pointing, else nothing
		if (talkOrPoint)
		{
			_lookTarget = q.cameraPosition ? q.cameraPosition() : glm::vec3(0.0f);
		}
		else
		{
			_lookTarget.reset();
		}
		break;
	}
}

void HelpDude::UpdateHead(float dt)
{
	// fn_005BF620
	const Queries& q = _control.GetQueries();
	if (_lookTarget && (_activeFlags & 1) == 0)
	{
		// CalcHeadPos 0x5BFE00 (0x5BF639..0x5BF655): +0x34BC / +0x34C0 only, nothing when the target is behind
		const std::optional<glm::vec2> angles =
		    q.headAngles ? q.headAngles(_index, _rows, _position, *_lookTarget) : std::optional<glm::vec2>(glm::vec2(0.0f));
		if (angles)
		{
			_headTarget.x = angles->x;
			_headTarget.y = angles->y;
		}
	}
	else
	{
		_headTarget = glm::vec3(0.0f); // 0x5BF657..: the three zeroed
	}
	const float rate = (_index == k_EvilDude ? 4.5f : 1.35f) * dt; // 0x40900000 / 0x3FACCCCD
	for (int i = 0; i < 3; ++i)
	{
		float d = (_headTarget[i] - _head[i]) * 0.95f; // [0x8CF000]
		if (d > rate)
		{
			d = rate;
		}
		else if (d < -rate)
		{
			d = -rate;
		}
		_head[i] += d;
	}
	if (_inWorld != 0.0f)
	{
		return;
	}
	// fn_005C0140: the look layers; the noise amplitudes rec[13] / rec[14] are 0 in both files
	auto noise = [](float x) {
		// fn_005C00C0 0x5C00C0..0x5C0138: the constants are doubles (fmul / fadd qword 0x900CE0, 0x900CD8, 0x8AC3F8,
		// 0x900CD0, 0x8C7C68, 0x900CC8, 0x8AB680, 0x900CC0, 0x900CB8, 0x900CB0, 0x900CA8, 0x8C9D40), each step rounded to
		// 24 bits by the FPU's precision (fn_007DEE00); fsin / fcos are not rounded, the product after them is
		const auto r = [](double v) { return static_cast<float>(v); };
		const auto xd = static_cast<double>(x);
		float sum = r(std::cos(static_cast<double>(r(r(xd * 0.95325) + 53.0))) * 0.75);
		sum = r(static_cast<double>(sum) - r(std::sin(static_cast<double>(r(r(xd * 2.2335) - 53.0))) * 0.2));
		sum = r(static_cast<double>(sum) + std::sin(static_cast<double>(r(r(xd * 0.7647) + 1.0))));
		sum = r(static_cast<double>(sum) - r(std::cos(static_cast<double>(r(22.0 - r(xd * 0.13)))) * 0.53));
		return r(static_cast<double>(sum) - r(std::sin(static_cast<double>(r(r(xd * 6.2335) - 53.0))) * 0.1));
	};
	float x = _face[13] * noise(3.0f * (_totalTime * 1.24f + _hoverClock2 * 0.95f)) + _head.x + 0.5f;
	float y = _face[14] * noise(3.0f * (_totalTime * 0.935f + _hoverClock2 + 245.0f)) + _head.y + 0.5f;
	x = std::clamp(x, 0.0f, 1.0f);
	y = std::clamp(y, 0.0f, 1.0f);
	const bool pointing = (_state & 8) != 0;
	const uint32_t clipY = pointing ? anim::LookUDStable : anim::LookUD;
	const uint32_t clipX = pointing ? anim::LookLRStable : anim::LookLR;
	if (const CAnim* clip = Clip(clipY); clip != nullptr)
	{
		AddAt(clipY, y, clip->frameCount / 2);
	}
	if (const CAnim* clip = Clip(clipX); clip != nullptr)
	{
		AddAt(clipX, x, clip->frameCount / 2);
	}
}

void HelpDude::UpdateDraw(int32_t frameMs, uint32_t tickMs)
{
	// fn_005C0700 0x5C080F..0x5C0931: the alpha byte, then the fade
	int32_t a = static_cast<int32_t>(_alpha * 255.0f);
	if (_flicker)
	{
		if (static_cast<int32_t>(tickMs) > static_cast<int32_t>(_control._flickerNext))
		{
			_control._flickerNext = tickMs + 10 + _control.LocalRand(50);
			_control._flickerMultiplier = _control.LocalRand(255) >= 64 ? 256 : 0;
		}
		a = (_control._flickerMultiplier * a) / 256;
		_flicker = false;
	}
	_alphaByte = a;
	const float seconds = static_cast<float>(frameMs) * 0.001f;
	if (_alphaTarget > _alpha)
	{
		_alpha += seconds * 3.0f;
		if (_alpha > _alphaTarget)
		{
			_alpha = _alphaTarget;
		}
	}
	else if (_alphaTarget < _alpha)
	{
		_alpha -= seconds + seconds;
		if (_alpha < _alphaTarget)
		{
			_alpha = _alphaTarget;
		}
	}
	// the puff (0x5C0AE5..0x5C0E95): time unit ms x 0.0032
	if (_puffRunning)
	{
		const float u = static_cast<float>(frameMs) * 0.0032f; // [0x900CFC], fst [esp+0x14] 0x5C0B0E
		_puffTime += u;
		if (!_puffParticles)
		{
			// 0x5C0B24..0x5C0C06: the first draw of a puff makes the 16 particles (LH3DSprite::Create(16) +0x2C0C, new
			// 0x200 +0x2C10), each with the six Random draws in fn_005C1D20's order
			_puffParticles.emplace();
			for (auto& particle : *_puffParticles)
			{
				RandomisePuffParticle(particle);
			}
		}
		// 0x5C0C2A..0x5C0C64: the fade in, 0 for <= 0, 1 for >= 1
		float fadeIn = _puffTime * 4.0f;
		if (!(fadeIn > 0.0f))
		{
			fadeIn = 0.0f;
		}
		else if (!(fadeIn < 1.0f))
		{
			fadeIn = 1.0f;
		}
		_puffFade = fadeIn;
		bool drawn = false; // [esp+0x20], 0x5C0C14
		for (auto& particle : *_puffParticles)
		{
			const auto base = static_cast<int32_t>(255.0f - particle.age * 32.0f); // 0x5C0C7A..0x5C0C8E
			particle.age += u;                                                      // 0x5C0C93..0x5C0C9D
			float alpha = static_cast<float>(base) * fadeIn;
			if (!(alpha > 0.0f))
			{
				alpha = 0.0f;
			}
			else if (!(alpha < 255.0f))
			{
				alpha = 255.0f;
			}
			particle.drawAlpha = static_cast<int32_t>(alpha); // 0x5C0CD3
			particle.drawVelocityY = particle.velocity.y;
			if (particle.drawAlpha <= 0) // 0x5C0CDA
			{
				continue;
			}
			drawn = true; // 0x5C0D21
			// 0x5C0E38..0x5C0E52, after the sprite: vy -= (spin + 1) u 0.3 ([0x900CF4])
			particle.velocity.y -= (particle.spin + 1.0f) * u * 0.3f;
		}
		_puffRunning = drawn; // 0x5C0E8C..0x5C0E95
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// HelpDudeControl

HelpDudeControl::HelpDudeControl(const DudeData& good, const DudeData& evil, Queries queries, Screen screen)
    : _queries(std::move(queries))
{
	_frame.screen = screen;
	_dudes[k_GoodDude] = std::make_unique<HelpDude>(k_GoodDude, good, *this);
	_dudes[k_EvilDude] = std::make_unique<HelpDude>(k_EvilDude, evil, *this);
	_spirits[k_GoodDude].type = 1;
	_spirits[k_EvilDude].type = 2;
	// 0x5C2CEA..0x5C2D16: both at their home point, state 0
	for (int i = 0; i < k_Dudes; ++i)
	{
		_state[i] = ControlState::Home;
		_timer[i] = 0.0f;
		_dudes[i]->SetPosition(HomePoint(i));
	}
	_focus = 0;
}

uint32_t HelpDudeControl::LocalRand(int32_t n) const
{
	return _queries.localRand ? _queries.localRand(n) : game_random::LocalRand(n);
}

float HelpDudeControl::LocalFloatRand(float x) const
{
	return _queries.localFloatRand ? _queries.localFloatRand(x) : game_random::LocalFloatRand(x);
}

float HelpDudeControl::Random(float a, float b) const
{
	return _queries.random ? _queries.random(a, b) : game_random::crt::Random(a, b);
}

glm::ivec2 HelpDudeControl::Anchor(int edge) const
{
	// fn_005BD440
	const int32_t halfW = _frame.screen.HalfWidth();
	const int32_t halfH = _frame.screen.HalfHeight();
	switch (edge)
	{
	case 0:
		return {halfW, halfH * 3};
	case 1:
		return {-halfW, halfH};
	case 2:
		return {halfW, -halfH};
	default:
		return {halfW * 3, halfH};
	}
}

glm::ivec2 HelpDudeControl::HomePoint(int dude) const
{
	// fn_005C2E90
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	const HelpDude& d = *_dudes[dude];
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	const auto halfH = static_cast<float>(_frame.screen.HalfHeight());
	const auto px = static_cast<int32_t>((d.HoverX().value + 1.0f) * halfW); // fn_005C3120
	const auto py = static_cast<int32_t>(d.HoverY().value * halfW + halfH);  // fn_005C3160
	int32_t best = 10000;
	glm::ivec2 nearest(0);
	for (int i = 0; i < 4; ++i)
	{
		const glm::ivec2 anchor = Anchor(i);
		const int32_t dx = anchor.x - px;
		const int32_t dy = anchor.y - py;
		const int32_t d2 = dx * dx + dy * dy;
		if (i == 0 || d2 < best)
		{
			best = d2;
			nearest = anchor;
		}
	}
	if (best < 0x40)
	{
		// already there: the top corner of its side
		if (dude == 0)
		{
			return {(w * 3) / 2, -(h / 2)};
		}
		return {-(w / 2), -(h / 2)};
	}
	return nearest;
}

bool HelpDudeControl::IsHome(int dude) const
{
	return _state[dude] == ControlState::Home || _state[dude] == ControlState::GoingHome;
}

void HelpDudeControl::Eject(int dude)
{
	// fn_005C32C0
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	HelpDude& d = *_dudes[dude];
	d.SetAlphaTarget(1.0f);
	if (IsHome(dude))
	{
		_state[dude] = ControlState::Out;
		_timer[dude] = 0.0f;
		d.SetPosition(HomePoint(dude));
		const int32_t x = static_cast<int32_t>(LocalRand(w / 2)) + w / 4;
		const int32_t y = static_cast<int32_t>(LocalRand(h / 2)) + h / 4;
		d.FlyTo({x, y}, 1.0f, true);
		d.SetEmotion(0, 1.0f);
	}
	else if (_state[dude] == ControlState::Clinging)
	{
		const int32_t x = static_cast<int32_t>(LocalRand(w / 3)) + w / 3;
		const int32_t y = static_cast<int32_t>(LocalRand(h / 3)) + h / 3;
		d.FlyTo({x, y}, 1.0f, true);
		d.SetEmotion(0, 1.0f);
	}
	d.ResetTrail();
}

void HelpDudeControl::Appear(int dude)
{
	// fn_005C3400
	const int32_t w = _frame.screen.width;
	const int32_t h = _frame.screen.height;
	const glm::ivec2 spot = dude == 0 ? glm::ivec2(w / 4, h / 2) : glm::ivec2((w * 3) / 4, h / 2);
	HelpDude& d = *_dudes[dude];
	if (IsHome(dude))
	{
		_state[dude] = ControlState::Out;
		_timer[dude] = 0.0f;
		d.SetPosition(spot);
		d.SetAlpha(0.0f);
		d.StartPuff();
		d.SetAlphaTarget(1.0f);
		d.SetEmotion(0, 1.0f);
		d.SetInWorld(0.0f);
		d.SetInWorldTarget(0.0f);
	}
	else if (_state[dude] == ControlState::Clinging)
	{
		d.FlyTo(spot, 1.0f, true);
		d.SetEmotion(0, 1.0f);
		d.SetInWorld(0.0f);
		d.SetInWorldTarget(0.0f);
	}
	d.ResetTrail();
}

void HelpDudeControl::Home(int dude)
{
	// fn_005C3590
	if (IsHome(dude))
	{
		return;
	}
	StopPointing(dude);
	StopLooking(dude);
	_dudes[dude]->FlyTo(HomePoint(dude), 1.0f, false);
	_state[dude] = ControlState::GoingHome;
	_timer[dude] = 0.0f;
}

void HelpDudeControl::Vanish(int dude)
{
	// fn_005C3540
	if (IsHome(dude))
	{
		return;
	}
	StopPointing(dude);
	StopLooking(dude);
	_dudes[dude]->StartPuff();
	_dudes[dude]->SetAlphaTarget(0.0f);
	_state[dude] = ControlState::GoingHome;
	_timer[dude] = 0.0f;
}

void HelpDudeControl::Cling(int dude, float px, float py)
{
	// fn_005C2FB0
	HelpDude& d = *_dudes[dude];
	d.SetAlphaTarget(1.0f);
	bool fromHome = false;
	if (_state[dude] != ControlState::Clinging)
	{
		fromHome = _state[dude] == ControlState::Home;
		_state[dude] = ControlState::Clinging;
		_timer[dude] = 0.0f;
		d.SetEmotion(0, 1.0f);
	}
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	float hx;
	float hy;
	if (px == 0.0f && py == 0.0f)
	{
		hy = 0.0f;
		hx = dude == 0 ? 1.0f : -1.0f; // the default edge: good right, evil left
	}
	else
	{
		hx = px / halfW - 1.0f;
		hy = (py - static_cast<float>(_frame.screen.HalfHeight())) / halfW;
	}
	d.Cling(hx, hy, fromHome);
}

void HelpDudeControl::Fly(int dude, float px, float py)
{
	// fn_005C3250
	if (_state[dude] == ControlState::Home)
	{
		return;
	}
	_dudes[dude]->FlyTo({static_cast<int32_t>(px), static_cast<int32_t>(py)}, 1.0f, true);
}

void HelpDudeControl::PlayAnim(int dude, float px, float py, uint32_t anim, float speed)
{
	// fn_005C31B0
	if (_state[dude] == ControlState::Home)
	{
		return;
	}
	HelpDude& d = *_dudes[dude];
	if (anim == 0)
	{
		if (d.IsPlayingAnim())
		{
			d.SetState(dude_state::Hover, false);
		}
		return;
	}
	const auto halfW = static_cast<float>(_frame.screen.HalfWidth());
	const auto halfH = static_cast<float>(_frame.screen.HalfHeight());
	d.PlayAnim((px - halfW) / halfW, (py - halfH) / halfW, anim, speed);
}

void HelpDudeControl::PointAtPosition(int dude, const glm::vec3& position, bool inWorld, float side, float height)
{
	// fn_005C3960
	_pointMode[dude] = 1;
	_pointPosition[dude] = position;
	_pointInWorld[dude] = inWorld;
	_pointSide[dude] = side;
	_pointHeight[dude] = height;
}

void HelpDudeControl::PointAtPixel(int dude, glm::ivec2 pixel)
{
	// fn_005C39B0
	_pointMode[dude] = 2;
	_pointPosition[dude].x = static_cast<float>(pixel.x);
	_pointPosition[dude].y = static_cast<float>(pixel.y);
	_pointInWorld[dude] = false;
}

void HelpDudeControl::LookAt(int dude, const glm::vec3& position)
{
	// fn_005C39F0
	_lookOn[dude] = true;
	_lookPosition[dude] = position;
}

uint32_t HelpDudeControl::SayDelayMs(int dude) const
{
	// HelpDudeControl::Say 0x5C36D0
	// 0x5C36E3..0x5C3708 with the FPU at 24 bits: fabs, fsub qword 0.94999998807907104 [0x915438] (0.95f as a double:
	// the float subtraction), fcom 0, fadd 1.0f, fmul 250.0f, each rounded to a float (as audio::advisor::Say)
	const float v = std::abs(_dudes[dude]->HoverX().value) - 0.95f;
	if (v < 0.0f)
	{
		return 0;
	}
	const float ms = (v + 1.0f) * 250.0f;
	return static_cast<uint32_t>(ms > 500.0f ? 500.0f : ms);
}

void HelpDudeControl::SpiritEject(int32_t type, bool isHelp)
{
	// fn_005C4BD0
	const int dude = DudeOf(type);
	_spirits[dude].lookObject = 0;
	_spirits[dude].pointObject = 0;
	if (isHelp)
	{
		Appear(dude);
	}
	else
	{
		Eject(dude);
	}
}

void HelpDudeControl::SpiritHome(int32_t type, bool isHelp)
{
	// fn_005C5200
	const int dude = DudeOf(type);
	_spirits[dude].pointObject = 0;
	_spirits[dude].lookObject = 0;
	if (isHelp)
	{
		Vanish(dude);
	}
	else
	{
		Home(dude);
	}
}

void HelpDudeControl::SpiritPointPosition(int32_t type, const glm::vec3& position, bool inWorld)
{
	// fn_005C4EF0
	const int dude = DudeOf(type);
	Eject(dude);
	PointAtPosition(dude, position, inWorld, 8.0f, 5.0f); // 0x5C4F26 / 0x5C4F2B
	_spirits[dude].pointInWorld = inWorld;
	_spirits[dude].pointObject = 0;
}

void HelpDudeControl::SpiritPointObject(int32_t type, uint32_t object, bool inWorld)
{
	// fn_005C4FA0
	if (object == 0 || !_queries.object)
	{
		return;
	}
	const auto info = _queries.object(object);
	if (!info)
	{
		return;
	}
	const int dude = DudeOf(type);
	Eject(dude);
	glm::vec3 position = info->position;
	position.y += info->height; // vt+0x42C GetHeight (0x5C5018)
	PointAtPosition(dude, position, inWorld, 8.0f, 5.0f);
	_spirits[dude].pointInWorld = inWorld;
	_spirits[dude].pointObject = object;
}

void HelpDudeControl::SpiritScreenPoint(int32_t type, glm::ivec2 pixel)
{
	// fn_005C4F50
	const int dude = DudeOf(type);
	Eject(dude);
	PointAtPixel(dude, pixel);
	_spirits[dude].pointInWorld = false;
	_spirits[dude].pointObject = 0;
}

void HelpDudeControl::SpiritStopPointing(int32_t type)
{
	// fn_005C5060
	const int dude = DudeOf(type);
	_spirits[dude].pointObject = 0;
	StopPointing(dude);
}

void HelpDudeControl::SpiritLookAtPosition(int32_t type, const glm::vec3& position)
{
	// fn_005C4E00
	const int dude = DudeOf(type);
	Eject(dude);
	LookAt(dude, position);
	_spirits[dude].lookObject = 0;
}

void HelpDudeControl::SpiritLookObject(int32_t type, uint32_t object)
{
	// fn_005C4E50
	if (object == 0 || !_queries.object)
	{
		return;
	}
	const auto info = _queries.object(object);
	if (!info)
	{
		return;
	}
	const int dude = DudeOf(type);
	Eject(dude);
	LookAt(dude, info->position); // no GetHeight here
	_spirits[dude].lookObject = object;
}

void HelpDudeControl::SpiritStopLooking(int32_t type)
{
	// fn_005C5090
	const int dude = DudeOf(type);
	_spirits[dude].lookObject = 0;
	StopLooking(dude);
}

void HelpDudeControl::SpiritPlayAnim(int32_t type, float x, float y, uint32_t anim, float speed)
{
	// fn_005C4C80
	const int dude = DudeOf(type);
	Eject(dude);
	PlayAnim(dude, x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height), anim,
	         speed);
}

bool HelpDudeControl::SpiritPlayingAnim(int32_t type) const
{
	// fn_005C4D10
	return _dudes[DudeOf(type)]->IsPlayingAnim();
}

void HelpDudeControl::SpiritCling(int32_t type, float x, float y)
{
	// fn_005C4D40 (no eject)
	Cling(DudeOf(type), x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height));
}

void HelpDudeControl::SpiritFly(int32_t type, float x, float y)
{
	// fn_005C4DA0
	Fly(DudeOf(type), x * static_cast<float>(_frame.screen.width), y * static_cast<float>(_frame.screen.height));
}

void HelpDudeControl::ProcessTurn()
{
	// HelpSpirit::Process 0x5C5270, one spirit after the other (HelpSystem's order: (pending) which goes first)
	for (int dude = 0; dude < k_Dudes; ++dude)
	{
		Spirit& spirit = _spirits[dude];
		if (spirit.pointObject != 0) // 0x5C50C0
		{
			const auto info = _queries.object ? _queries.object(spirit.pointObject) : std::nullopt;
			if (!info)
			{
				SpiritStopPointing(spirit.type);
			}
			else
			{
				glm::vec3 position = info->position;
				position.y += info->height;
				PointAtPosition(dude, position, spirit.pointInWorld, 8.0f, 5.0f);
			}
		}
		if (spirit.lookObject != 0) // 0x5C5170: re-sent through the point function (original bug, kept)
		{
			const auto info = _queries.object ? _queries.object(spirit.lookObject) : std::nullopt;
			if (!info)
			{
				SpiritStopLooking(spirit.type);
			}
			else
			{
				PointAtPosition(dude, info->position, false, 8.0f, 5.0f);
			}
		}
	}
}

void HelpDudeControl::SetSentenceTags(int dude, std::vector<AudioTag> tags)
{
	_dudes[dude]->_tags = std::move(tags);
	_dudes[dude]->_nextTag = 0;
}

void HelpDudeControl::Process(float dt, float focusBias)
{
	// HelpDudeControl::Process 0x5C3A30
	auto talked = [this](int dude) {
		if (_queries.talkedRecently)
		{
			return _queries.talkedRecently(dude);
		}
		return _queries.isTalking && _queries.isTalking(dude);
	};
	int focus = _pointMode[0] != 0 ? 1 : 0;
	if (_pointMode[1] != 0)
	{
		focus |= 2;
	}
	if (talked(0))
	{
		focus |= 1;
	}
	if (talked(1))
	{
		focus |= 2;
	}
	if (focus == 0 || focus == 3)
	{
		focus = focusBias < 0.5f ? 0 : 1;
	}
	else
	{
		focus -= 1;
	}
	_focus = focus;
	// the model scales 0.8 e^(-+0.5 (bias - 0.5)): 0.841 / 0.761 for the constant 0.4 (0x5C3A9E..0x5C3AFF). FPU at 24
	// bits: bias - 0.5f, x -0.5f [0x8CEFCC] / x 0.5f, the inline exp (Exp24), fmul qword 0.80000001192092896 [0x8CBF28]
	// (0x5C3ACE / 0x5C3AF9), each rounded to a float
	const float bias = focusBias - 0.5f;
	_dudes[0]->SetModelScale(static_cast<float>(static_cast<double>(Exp24(bias * -0.5f)) * 0.80000001192092896));
	_dudes[1]->SetModelScale(static_cast<float>(static_cast<double>(Exp24(bias * 0.5f)) * 0.80000001192092896));

	for (int i = 0; i < k_Dudes; ++i)
	{
		HelpDude& d = *_dudes[i];
		d._layers.clear();
		d._sounds.clear();
		_timer[i] += dt;
		if (_state[i] == ControlState::GoingHome)
		{
			if (_timer[i] > 1.0f && !d.PuffRunning())
			{
				_state[i] = ControlState::Home;
				d.SetPosition(HomePoint(i));
				d.ClearAnims();
			}
			else if (!d.PuffRunning())
			{
				d.FlyTo(HomePoint(i), 1.0f, false);
			}
		}
		if (_state[i] != ControlState::Home)
		{
			if (_pointMode[i] == 1)
			{
				d.PointAt(_pointPosition[i], _pointInWorld[i], _pointSide[i], _pointHeight[i]);
			}
			else if (_pointMode[i] == 2)
			{
				d.ScreenPoint({static_cast<int32_t>(_pointPosition[i].x), static_cast<int32_t>(_pointPosition[i].y)});
			}
			else if ((d.State() & 8) != 0 && (d.QueuedState() & 0x100) == 0)
			{
				d.SetState(dude_state::Hover, false);
			}
			// the rest zone: good right, evil left (0x5C3C13..0x5C3C41)
			d._zones[0] = {2.0f, i == 0 ? 0.66f : -0.66f, 0.0f, 0.4f, 0.6f};
			d.partner = _dudes[i == 0 ? 1 : 0].get();
			d.Update1(dt, focus == i, 0.0f, true);
		}
		else if (d.State() != dude_state::Hover)
		{
			// fn_005BF5D0: Update1 and fn_005BF620 (spec_spirits_motion section 1)
			d.Update1(dt, focus == i, 0.0f, true);
			d.UpdateHead(dt);
		}
		// else only UpdateSaySentence (audio::advisor)
	}
	for (int i = 0; i < k_Dudes; ++i)
	{
		if (_state[i] == ControlState::Home)
		{
			continue;
		}
		const bool talking = (_queries.sayActive && _queries.sayActive(i)) && (_queries.isTalking && _queries.isTalking(i));
		const glm::vec3* lookPosition = _lookOn[i] ? &_lookPosition[i] : nullptr;
		const bool talkOrPoint = talking || _pointMode[i] != 0;
		const bool engaged = talking || (focus == i && _pointMode[i] != 0);
		const bool partnerOut = _state[i == 0 ? 1 : 0] != ControlState::Home;
		_dudes[i]->UpdateLookTarget(dt, engaged, partnerOut, talkOrPoint, lookPosition);
		_dudes[i]->UpdateHead(dt);
	}
}

void HelpDudeControl::Update(const FrameInput& input)
{
	_frame = input;
	Process(input.dt, 0.4f); // push 0x3ECCCCCD at 0x5C5ADC
	// the draws: the overlay callback fn_005C3920 (blend < 0.5) and Draw3D 0x5C5B26 (>= 0.5) for dudes out of home
	for (int i = 0; i < k_Dudes; ++i)
	{
		if (_state[i] != ControlState::Home)
		{
			_dudes[i]->UpdateDraw(input.frameMs, input.tickMs);
		}
	}
}

} // namespace openblack::help::spirits
