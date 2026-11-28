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

#include <entt/entity/entity.hpp>

#include "ECS/Components/MistFade.h"

// The scripts' mist fades (SET_MIST_FADE): a bank of mist's size and transparency moved one step a game turn
// (docs/wiki/world/haze-and-mist.md#map-mist-create_mist).

namespace openblack::ecs::mist_fade
{
/// A fade over `seconds` (ten steps a second) from the start size and transparency (0..255) to the end ones. Each
/// step is the difference over the steps, worked out once
[[nodiscard]] components::MistFade Start(float startSize, float endSize, float startAlpha, float endAlpha, float seconds);

/// The alpha byte of a transparency: its whole part, as the drawn mist keeps it
[[nodiscard]] uint32_t AlphaOf(float alpha);

/// One turn of a fade: false, with nothing changed, once every step is taken (the fade is over); else one more step,
/// the size moved by its step and the transparency by its, kept to 0..255
bool Step(components::MistFade& fade, float& size);

/// SET_MIST_FADE on a bank of mist: its size and its alpha are the start ones at once, then the fade moves them each
/// turn. A fade started again replaces the one going on. Nothing for what is not a mist
void Set(entt::entity mist, float startSize, float endSize, float startAlpha, float endAlpha, float seconds);

/// The game turn's step of every fading mist, in the turn's global lists after the walks along paths. A fade that is
/// over stays on the mist with its steps all taken, the mist as it ended
void ProcessTurn();

/// GET_OBJECT_FADE of a mist: the steps its fade has taken over the steps it takes. A mist never faded holds nought
/// steps of nought, and answers the processor's not-a-number of nought over nought (bits 0xFFC00000)
[[nodiscard]] float Progress(const components::MistFade* fade);
} // namespace openblack::ecs::mist_fade
