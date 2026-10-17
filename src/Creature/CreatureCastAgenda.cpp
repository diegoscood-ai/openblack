/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastAgenda.h"

#include "Creature/CreatureLayers.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// How each kind of casting goes: what it shows first and the faces it pulls, and how near it goes and how far it
/// backs off, for its height
struct Style
{
	size_t emote;
	creature_face::Cue feeling;
	creature_face::Cue facing;
	float goNear;
	float backOff;
};

Style StyleOf(CastStyle style, float height)
{
	switch (style)
	{
	case CastStyle::Lightning:
		return {.emote = animations::k_Angry,
		        .feeling = creature_face::Cue::Anger,
		        .facing = creature_face::Cue::Anger,
		        .goNear = k_LightningCastDistance,
		        .backOff = height + k_LightningBackOff};
	case CastStyle::Helpful:
		return {.emote = animations::k_FeelingNice,
		        .feeling = creature_face::Cue::Compassion,
		        .facing = creature_face::Cue::Compassion,
		        .goNear = 2.0f * height,
		        .backOff = 2.0f * height};
	case CastStyle::Playful:
		break;
	}
	// Playful going near, but kind as it turns to face the creature it casts on
	return {.emote = animations::k_FeelPlayful,
	        .feeling = creature_face::Cue::Playfulness,
	        .facing = creature_face::Cue::Compassion,
	        .goNear = 5.0f * height,
	        .backOff = 2.0f * height};
}
} // namespace

std::vector<Step> creature_mind::CastAt(CastStyle style, uint32_t magicType, uint32_t gesture, uint32_t object, float height,
                                        const Random& random)
{
	const auto how = StyleOf(style, height);
	std::vector<Step> agenda;
	if (random(k_CastEmoteLots) == 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = how.emote, .face = how.feeling});
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = how.goNear},
	                  .face = how.feeling});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GetAwayFromObject, .object = object, .maxDistance = how.backOff}});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .face = how.facing});
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	// It grimaces as it casts
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_CastPose[1],
	                  .sequence = k_CastPose,
	                  .cast = {.magicType = magicType, .object = object},
	                  .face = creature_face::Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::CastFoodAt(uint32_t gesture, uint32_t object, float goNear)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = goNear},
	                  .face = creature_face::Cue::Compassion});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .face = creature_face::Cue::Compassion});
	if (gesture != 0)
	{
		agenda.push_back({.kind = Step::Kind::Gesture, .animation = gesture});
	}
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Food), .object = object},
	                  .face = creature_face::Cue::Grimace});
	return agenda;
}

std::vector<Step> creature_mind::CastWaterAt(uint32_t object, float height)
{
	std::vector<Step> agenda;
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::GoNearObject, .object = object, .maxDistance = 2.0f * height}});
	agenda.push_back({.kind = Step::Kind::Move,
	                  .seconds = k_CastSettleSeconds,
	                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = object},
	                  .face = creature_face::Cue::Smile});
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Water), .object = object},
	                  .face = creature_face::Cue::Smile});
	return agenda;
}

std::vector<Step> creature_mind::CastWaterOnItself(glm::vec2 spot, uint32_t self, float height)
{
	std::vector<Step> agenda;
	// It goes to within twice its height of the spot, pulling no face
	agenda.push_back(
	    {.kind = Step::Kind::Move, .movement = {.kind = Movement::Kind::ToPoint, .point = spot, .maxDistance = 2.0f * height}});
	agenda.push_back({.kind = Step::Kind::Cast,
	                  .seconds = k_CastHoldSeconds,
	                  .animation = k_ScatterPose[1],
	                  .sequence = k_ScatterPose,
	                  .cast = {.magicType = static_cast<uint32_t>(MagicType::Water), .object = self},
	                  .face = creature_face::Cue::Smile});
	return agenda;
}
