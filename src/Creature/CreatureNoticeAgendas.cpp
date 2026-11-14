/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureNoticeAgendas.h"

#include <array>

using namespace openblack::creature_mind;

namespace
{
using openblack::creature_face::Cue;

Step LookAtTheCamera(Cue face)
{
	Step look {.kind = Step::Kind::LookAtCamera, .seconds = k_NoticeLookSeconds, .face = face, .gaze = Gaze {.camera = true}};
	look.main = true;
	return look;
}

Step Clip(size_t animation)
{
	return {.kind = Step::Kind::Action, .animation = animation};
}
} // namespace

std::vector<Step> openblack::creature_mind::NoticeDeed(NoticeKind kind, uint32_t thing, bool handFull,
                                                       std::optional<glm::vec2> camera, const Random& random)
{
	std::vector<Step> agenda;
	if (!handFull)
	{
		const float seconds = kind == NoticeKind::Aggressive ? k_NoticeAggressivePointSeconds : k_NoticePointSeconds;
		agenda.push_back({.kind = Step::Kind::Object,
		                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = thing, .seconds = seconds},
		                  .face = Cue::Amazed,
		                  .gaze = Gaze {.object = thing, .itself = true}});
	}
	if (camera.has_value())
	{
		Step turn {.kind = Step::Kind::Move, .gaze = Gaze {.camera = true}};
		turn.movement = {.kind = Movement::Kind::TurnToFace, .point = *camera};
		agenda.push_back(turn);
	}
	switch (kind)
	{
	case NoticeKind::Helpful:
		agenda.push_back(LookAtTheCamera(Cue::Curiosity));
		agenda.push_back(LookAtTheCamera(Cue::Compassion));
		agenda.push_back(Clip(random(k_NoticeHelpfulLots) == 0 ? k_NoticeHappyClip : k_NoticeAmazedClip));
		break;
	case NoticeKind::Aggressive:
		agenda.push_back(LookAtTheCamera(Cue::Anger));
		if (random(k_NoticeAggressiveLots) == 0)
		{
			agenda.push_back(Clip(k_NoticeDazedClip));
		}
		agenda.push_back(Clip(k_NoticeAngryClip));
		break;
	case NoticeKind::Neutral:
	case NoticeKind::Stealing:
	case NoticeKind::Playful:
	{
		agenda.push_back(LookAtTheCamera(Cue::Curiosity));
		agenda.push_back(LookAtTheCamera(Cue::Curiosity));
		constexpr std::array k_Clips {k_NoticeAmazedClip, k_NoticeStealingClip, k_NoticeHappyClip};
		agenda.push_back(Clip(k_Clips.at(static_cast<size_t>(kind) - static_cast<size_t>(NoticeKind::Neutral))));
		break;
	}
	}
	return agenda;
}
