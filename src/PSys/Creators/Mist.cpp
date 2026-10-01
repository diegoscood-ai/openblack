/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Mist.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <memory>

#include "3D/LH3DRandom.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Graphics/Mists.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
std::unique_ptr<Creator> MakeMistCreator(const Object& object)
{
	auto creator = std::make_unique<MistCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Other; // drawn by mist_atoms::SubmitFrame, not by the sprite or mesh passes
	// ctor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame in the
	// file and in use, InitialScaleMin 1.0, Ratio 0, TextureFileName ""
	creator->takeRatioFromMatrix = object.Bool("TakeRatioFromMatrix", false);
	creator->isShadowMap = object.Bool("IsShadowMap", true);
	creator->loadLightMap = object.Bool("LoadLightMap", true);
	creator->lightMap = object.String("TextureFileName");
	if (creator->lightMap == "NULL_STRING")
	{
		creator->lightMap.clear();
	}
	creator->pitch = std::clamp(object.Int("Pitch", 12), 1, 12);                    // range [1, 12]
	creator->numFramesInFile = std::clamp(object.Int("NumFramesInFile", 1), 1, 32); // range [1, 32]
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, 32);
	creator->initialScaleMin = object.Float("InitialScaleMin", 1.0f);
	creator->ratio = object.Float("Ratio", 0.0f);
	// GetBitmap 0x6AA540 (0x6AA54B..0x6AA586): only with LoadLightMap (+0x7A); bpp 1 with IsShadowMap (+0x79), else 3.
	// (inferido) the file's Pitch as written, not the [1, 12] property range: SF_LightningStormPush's S_SMClouds16 is
	// Pitch 16 (256 bytes, 16 x 16 x 1)
	if (creator->loadLightMap && !creator->lightMap.empty())
	{
		creator->landBitmap = land_light::LoadBitmapFile(creator->lightMap, std::max(1, object.Int("Pitch", 12)),
		                                                 creator->isShadowMap ? 1 : 3, creator->numFramesInFile,
		                                                 creator->numFramesInUse);
	}
	return creator;
}
} // namespace

void MistCreator::InitAtom(Effect& effect, Atom& atom) const
{
	// CreateParticleMist 0x6AA610: RandomiseScale ? PSysFloatRand(InitialScaleMin (+0x70), InitialScale (+0x30)) :
	// InitialScale -> atom +0x74 (it replaces fn_006A85E0's)
	atom.baseScale = randomiseScale ? initialScaleMin + effect.Random(initialScale - initialScaleMin) : initialScale;
	// 0x6AA683..0x6AA6AF: atom +0x110 = 1.0, +0x114 = 1, +0x118 PlayAnim = 0, +0x119 LoopAnim = the creator's +0x0C:
	// one frame that never steps
	atom.frame = 0.0f;
	atom.frameRate = 1.0f;
	atom.playAnim = false;
	// CreateLH3DMist 0x6AA5A0 makes the atom's own LH3DMist: its ctor 0x7F9560 starts +0x84 at ftol(Random(0, 16)) & 15
	// (0x7F95DC..0x7F95FB), LH3D's generator (graphics::lh3d::Random), not the PSys one
	atom.mist = {graphics::frame_anim::MistStartCounter(graphics::lh3d::Random(0.0f, 16.0f)), 0.0f};
}

uint32_t mist_atoms::MistColour(uint32_t atomArgb, uint32_t baseArgb)
{
	// 0x67A6C1..0x67A741: the base's alpha byte is set to 0xFF ([esp+0x17] = 0xFF), then each channel is (c x g) >> 8
	const auto channel = [&](int shift, uint32_t g) { return ((((atomArgb >> shift) & 0xFFu) * g) >> 8) << shift; };
	return channel(24, 0xFFu) | channel(16, (baseArgb >> 16) & 0xFFu) | channel(8, (baseArgb >> 8) & 0xFFu) |
	       channel(0, baseArgb & 0xFFu);
}

void mist_atoms::SubmitFrame(float milliseconds)
{
	for (const auto& drawable : manager::Collect(Creator::Kind::Other))
	{
		for (const auto& atom : drawable.atoms)
		{
			const auto* creator = dynamic_cast<const MistCreator*>(atom.creator);
			if (creator == nullptr)
			{
				continue;
			}
			mists::MistDesc mist {};
			mist.position = atom.position; // vt 0x24 SetPos(PSR +0x24)
			mist.size = atom.scale;        // mist +0x88 = PSR +0x30
			// CreateLH3DMist 0x6AA5A0: +0x80 |= 2 (the effect branch) and k = Ratio, or 2.5 + LocalFloatRand(2.5) when 0.
			// (aproximado) with Ratio 0 the per-mist random k has no per-atom slot here: its mean, 3.75
			mist.edgeShrink = true;
			mist.k = creator->ratio != 0.0f ? creator->ratio : 3.75f;
			// DrawAt: TakeRatioFromMatrix -> k = M[1][1] / M[0][0] when |M[0][0]| > 0.0001 (0x8BF518)
			// (the PSR matrix's Y axis carries the stretch, fn_00673DB0: the storm clouds' cloud ratio, UR_CloudGather)
			const glm::mat3 matrix = atom.rotation * glm::mat3(atom.scale, 0.0f, 0.0f, 0.0f, atom.scale * atom.stretch, 0.0f,
			                                                   0.0f, 0.0f, atom.scale);
			if (creator->takeRatioFromMatrix && std::abs(matrix[0][0]) > 0.0001f)
			{
				mist.k = matrix[1][1] / matrix[0][0];
			}
			// DrawData colour (the atom's, its alpha with the collection's) x [0xFA26A4], the land light table's base
			// colour (the renderer's last Build, LandLightTable::Current().GetRawBase(): (aproximado) the previous frame's)
			const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, 255.0f));
			const uint32_t argb = (alpha << 24) | (static_cast<uint32_t>(atom.colour[0]) << 16) |
			                      (static_cast<uint32_t>(atom.colour[1]) << 8) | static_cast<uint32_t>(atom.colour[2]);
			mist.colour = MistColour(argb, LandLightTable::Current().GetRawBase());
			// fn_007FA300: every LH3DMist its own counter += ftol(g_game_time_inc x 0.255), modulo 900 once past it, run
			// only for a mist on screen (AddDrawing 0x7FA7F0, mists::InView); the fraction kept as the map mists do
			// (frame_anim::MistAdvance)
			if (atom.atom != nullptr)
			{
				if (mists::InView(mist.position, mist.size))
				{
					graphics::frame_anim::MistAdvance(atom.atom->mist, milliseconds);
				}
				mist.counter = atom.atom->mist.counter;
			}
			// DrawData +0xC, the atom's +0x90 (0x679BF4), to SetColour 0x7F9770 as the specular (0x67A6C4/0x67A6D6)
			mist.specular = atom.specular;
			// vt 0x100 (Z-sorted, [0xC0215D] set) / vt 0x104: the sorting is mists::Submit's
			mists::Submit(mist);
			// 0x67A792..0x67A8C1: with the creator's bitmap a record in the list 0xD4EDB8 at (x, 0, z), frame 0, alpha =
			// DrawData alpha / 255 ([0x9357AC]) clamped to [0, 1]; PSysLightMaps fn_006CA280: + (10, 0, 10), centred,
			// mode 1 for bpp 3 / 2 for bpp 1 (the storm's shadow: fn_00878C70, land_light::AddStamp)
			if (creator->landBitmap)
			{
				const int mode = creator->landBitmap->channels == 3 ? 1 : 2;
				land_light::AddStamp(glm::vec3(mist.position.x + 10.0f, 0.0f, mist.position.z + 10.0f),
				                     graphics::frame_anim::FrameTexels(*creator->landBitmap, 0), creator->landBitmap->pitch, true,
				                     static_cast<float>(alpha) * 0.00392157f, mode);
			}
		}
	}
}

void openblack::psys::RegisterMistCreator()
{
	RegisterCreator("ParticleMistCreator", MakeMistCreator);
}
