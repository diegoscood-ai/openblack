/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LightMap.h"

#include <cmath>

#include <algorithm>
#include <memory>
#include <vector>

#include "3D/FrameAnim.h"
#include "3D/LandLight.h"
#include "Common/GameRandom.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::unique_ptr<Creator> MakeLightMapCreator(const Object& object)
{
	auto creator = std::make_unique<LightMapCreator>();
	ReadCreatorProperties(object, *creator);
	// ctor 0x6A9CE0 defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter
	creator->pitch = std::max(1, object.Int("Pitch", 1));
	creator->numFramesInFile = std::max(1, object.Int("NumFramesInFile", 1));
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, creator->numFramesInFile);
	creator->randJitter = object.Float("RandJitter", 0.0f);
	creator->useRandJitter = object.Bool("UseRandJitter", false);
	creator->shiftX = object.Float("ShiftX", 0.0f);
	creator->shiftZ = object.Float("ShiftZ", 0.0f);
	// not drawn by the sprite pass: stamped into the land (light_map_atoms::SubmitFrame)
	creator->kind = Creator::Kind::Other;
	creator->texture = object.String("TextureFileName");
	// GetBitmap 0x6A9D40: LoadBitmapFromFile(TextureFileName, Pitch, 3, NumFramesInFile, NumFramesInUse)
	creator->bitmap = land_light::LoadBitmapFile(creator->texture, creator->pitch, 3, creator->numFramesInFile, creator->numFramesInUse);
	creator->numFrames = creator->numFramesInUse;
	creator->fileOffset = 0;
	creator->initFrame = 0;
	creator->frameRate = object.Float("FrameRate", 1.0f);
	creator->playAnim = object.Bool("PlayAnim", false);
	creator->loopAnim = object.Bool("LoopAnim", false);
	return creator;
}
} // namespace

void LightMapCreator::InitAtom(Effect& /*effect*/, Atom& atom) const
{
	// CreateParticleLightMap 0x6A9DEF..0x6A9E16: the atom's frame rate (+0x110 = FrameRate +0x78), its frames (+0x114 =
	// NumFramesInUse +0x6C) and PlayAnim (+0x118 = +0x7C); the frame starts at 0. The light fades through the bitmap's
	// frames (PSys.cpp steps them, FramesPerAtom = numFrames)
	atom.frame = 0.0f;
	atom.frameRate = frameRate;
	atom.playAnim = playAnim;
}

void light_map_atoms::SubmitFrame()
{
	// (openblack guard) DrawAt 0x67B220 appends each atom's record once a frame to the list 0xD4EDB8 (0x67B35A), which
	// fn_006CA660 empties after PSysLightMaps::AddDrawing 0x6CA6E0: a second call before this frame's stamps are taken
	// out (land_light::ClearStamps) would stamp every atom twice, so it is skipped
	static uint32_t s_Submitted = ~0u;
	if (s_Submitted == land_light::StampFrame())
	{
		return;
	}
	s_Submitted = land_light::StampFrame();
	Stamp(manager::Collect(Creator::Kind::Other));
}

int light_map_atoms::Stamp(const std::vector<manager::Drawable>& drawables)
{
	int stamped = 0;
	for (const auto& drawable : drawables)
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const LightMapCreator*>(atom.creator);
			// 0x67B2A6..0x67B2B6 and fn_006CA280 0x6CA284..0x6CA2A2: a bitmap with data, 3 or 1 bytes per texel
			if (creator == nullptr || !creator->bitmap || (creator->bitmap->channels != 3 && creator->bitmap->channels != 1))
			{
				continue;
			}
			// DrawAt 0x67B228..0x67B261: the frame (DrawData +0x10), the position (the atom's +0x24) and alpha = DrawData
			// alpha / 255 ([0x9357AC])
			const int frame = graphics::frame_anim::PSysFrameIndex(atom.frame, creator->numFrames, creator->loopAnim) & 0xFF;
			glm::vec3 position = atom.position;
			const float alpha = static_cast<float>(static_cast<int>(std::clamp(atom.alpha, 0.0f, 255.0f))) * (1.0f / 255.0f);
			// 0x67B264..0x67B2A3: with UseRandJitter, LocalFloatRand(RandJitter) three times at every draw; the first
			// goes to z (0x67B29C..0x67B2A3), the second to y (0x67B292..0x67B299) and the third to x (0x67B289..0x67B28F)
			if (creator->useRandJitter)
			{
				const float first = game_random::LocalFloatRand(creator->randJitter);
				const float second = game_random::LocalFloatRand(creator->randJitter);
				const float third = game_random::LocalFloatRand(creator->randJitter);
				position += glm::vec3(third, second, first);
			}
			// fn_006CA280: + (10, 0, 10) ([0x8AB414], 0x6CA2AE / 0x6CA2CA), the frame frame % frames, centred, mode 1 for
			// 3 bytes per texel and 2 for 1 (0x6CA317..0x6CA320), keepBrighter 0
			const auto* texels = graphics::frame_anim::FrameTexels(*creator->bitmap, frame);
			const int mode = creator->bitmap->channels == 3 ? 1 : 2;
			if (land_light::AddStamp(position + glm::vec3(10.0f, 0.0f, 10.0f), texels, creator->bitmap->pitch, true, alpha,
			                         mode))
			{
				++stamped;
			}
		}
	}
	return stamped;
}

void openblack::psys::RegisterLightMapCreator()
{
	RegisterCreator("ParticleLightMapCreator", MakeLightMapCreator);
}
