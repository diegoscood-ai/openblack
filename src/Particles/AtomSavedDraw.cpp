/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtomSavedDraw.h"

#include "3D/AffineMatrix.h"

using namespace openblack::particles;

saved_draw::SavedDraw saved_draw::Local(const glm::mat3& rotation, float baseScale, float ruleScale, float stretch,
                                        const glm::vec3& position)
{
	// each product a float, in the original's order: the scale times the base scale, that times the stretch, then
	// every cell of a row by its factor
	const float scale = ruleScale * baseScale;
	const float rowScale = scale * stretch;
	SavedDraw saved;
	for (glm::length_t row = 0; row < 3; ++row)
	{
		const float factor = row == 1 ? rowScale : scale;
		for (glm::length_t column = 0; column < 3; ++column)
		{
			saved.matrix.at(static_cast<size_t>(row * 3 + column)) = factor * rotation[row][column];
		}
	}
	saved.matrix[9] = position.x;
	saved.matrix[10] = position.y;
	saved.matrix[11] = position.z;
	saved.scale = scale;
	return saved;
}

float saved_draw::BlendFraction(float t)
{
	if (!(t > 0.0f))
	{
		return 0.0f;
	}
	return t < k_MaxBlend ? t : k_MaxBlend;
}

saved_draw::SavedDraw saved_draw::Blend(const SavedDraw& last, const SavedDraw& current, float t)
{
	SavedDraw blended;
	for (size_t i = 0; i < blended.matrix.size(); ++i)
	{
		blended.matrix.at(i) = (current.matrix.at(i) - last.matrix.at(i)) * t + last.matrix.at(i);
	}
	blended.scale = (current.scale - last.scale) * t + last.scale;
	if (last.alpha == current.alpha)
	{
		blended.alpha = last.alpha;
	}
	else
	{
		const float alpha =
		    (static_cast<float>(current.alpha) - static_cast<float>(last.alpha)) * t + static_cast<float>(last.alpha);
		blended.alpha = !(alpha > 0.0f) ? 0 : (alpha >= 255.0f ? 255 : static_cast<uint8_t>(alpha));
	}
	return blended;
}

uint8_t saved_draw::WithEffectAlpha(uint8_t alpha, uint8_t effectAlpha)
{
	constexpr uint8_t k_Opaque = 0xFF;
	return effectAlpha == k_Opaque ? alpha : static_cast<uint8_t>((static_cast<uint32_t>(effectAlpha) * alpha) >> 8U);
}

saved_draw::SavedDraw saved_draw::Step(const SavedDraw& local, const SavedDraw* frame, uint8_t colourAlpha,
                                       uint8_t collectionAlpha)
{
	constexpr uint8_t k_Opaque = 0xFF;
	SavedDraw saved = local;
	if (frame != nullptr)
	{
		// the local matrix first, then the frame
		saved.matrix =
		    affine::MultiplyReversed(affine::AffineMatrix {.m = frame->matrix}, affine::AffineMatrix {.m = local.matrix}).m;
		saved.scale = local.scale * frame->scale;
	}
	saved.alpha = collectionAlpha == k_Opaque
	                  ? colourAlpha
	                  : static_cast<uint8_t>((static_cast<uint32_t>(colourAlpha) * collectionAlpha) / k_Opaque);
	return saved;
}
