/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiracleVisuals.h"

#include <cmath>

#include <array>

#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

using namespace openblack::magic;
using namespace openblack::magic::visuals;

namespace
{
/// A glint cell is a quarter of its sheet each way
constexpr float k_GlintCellSize = 0.25f;
constexpr int k_GlintCellsPerRow = 4;
/// The rings' fixed turns
constexpr float k_SecondRingTurn = 0.5f;
constexpr float k_RingFirstTip = 0.3f;
constexpr float k_RingTurnBack = 1.0f;
constexpr float k_RingSecondTip = 0.2f;

/// A frame stepped on and kept within its cells, either way round
float Wrap(float frame, float cells)
{
	frame = std::fmod(frame, cells);
	return frame < 0.0f ? frame + cells : frame;
}

/// The rows of a turn, as the game turns its matrices: each row turned about the vertical (mixing x and z) or tipped
/// (mixing x and y)
using Rows = std::array<glm::vec3, 3>;

void TurnAboutVertical(Rows& rows, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	for (auto& row : rows)
	{
		const float x = row.x;
		row.x = c * x - s * row.z;
		row.z = c * row.z + s * x;
	}
}

void Tip(Rows& rows, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	for (auto& row : rows)
	{
		const float x = row.x;
		row.x = s * row.y + c * x;
		row.y = c * row.y - s * x;
	}
}
} // namespace

float visuals::StepGlint(float frame, float seconds)
{
	return Wrap(frame + seconds * k_GlintFrameRate, static_cast<float>(k_GlintCells));
}

glm::vec2 visuals::GlintUvOffset(float frame)
{
	const int cell = static_cast<int>(frame) % k_GlintCells;
	return {static_cast<float>(cell % k_GlintCellsPerRow) * k_GlintCellSize,
	        static_cast<float>(cell / k_GlintCellsPerRow) * k_GlintCellSize};
}

int visuals::RingCount(int powerUp)
{
	return powerUp < 0 ? 0 : powerUp + 1;
}

uint8_t visuals::RingAlpha(uint8_t drawnAlpha)
{
	return static_cast<uint8_t>((static_cast<uint32_t>(k_RingAlpha) * drawnAlpha) >> 8u);
}

glm::mat3 visuals::RingTurn(int ring, float spin)
{
	// Laid flat: its height becomes its depth
	Rows rows {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
	const bool first = ring == 0;
	TurnAboutVertical(rows, (first ? 0.0f : k_SecondRingTurn) + spin);
	Tip(rows, k_RingFirstTip);
	TurnAboutVertical(rows, first ? -k_RingTurnBack : k_RingTurnBack);
	Tip(rows, k_RingSecondTip);
	// The rows are where each of the ring's axes goes: they are the columns of the turn
	return glm::transpose(glm::mat3(rows[0], rows[1], rows[2]));
}
