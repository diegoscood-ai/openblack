/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "3D/CameraPath.h"
#include "3D/L3DAnim.h"
#include "3D/L3DSubMesh.h"
#include "3D/Light.h"
#include "3D/PickMask.h"
#include "Audio/Device/Sound.h"
#include "Common/HelpText.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "Graphics/Texture2D.h"
#include "ResourcesInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::resources
{
class Resources final: public ResourcesInterface
{
public:
	// Out of line, so that the caches of every resource type are made and destroyed in one place
	Resources();
	~Resources() override;

	[[nodiscard]] MeshManager& GetMeshes() override { return _meshes; }
	[[nodiscard]] L3DFileManager& GetL3DFiles() override { return _l3dFiles; }
	[[nodiscard]] TextureManager& GetTextures() override { return _textures; }
	[[nodiscard]] PickMaskManager& GetPickMasks() override { return _pickMasks; }
	[[nodiscard]] AnimationManager& GetAnimations() override { return _animations; }
	[[nodiscard]] LevelManager& GetLevels() override { return _levels; }
	[[nodiscard]] CreatureMindManager& GetCreatureMinds() override { return _creatureMinds; }
	[[nodiscard]] CreatureRigManager& GetCreatureRigs() override { return _creatureRigs; }
	[[nodiscard]] CreatureSkinArtManager& GetCreatureSkinArt() override { return _creatureSkinArt; }
	[[nodiscard]] SoundManager& GetSounds() override { return _sounds; }
	[[nodiscard]] GlowManager& GetGlows() override { return _glows; }
	[[nodiscard]] BlobManager& GetBlobs() override { return _blobs; }
	[[nodiscard]] FontManager& GetFonts() override { return _fonts; }
	[[nodiscard]] const FontHashPermutation& GetFontHashPermutation() const override { return _fontHashPermutation; }
	void SetFontHashPermutation(const FontHashPermutation& table) override { _fontHashPermutation = table; }
	[[nodiscard]] HelpTextManager& GetHelpTexts() override { return _helpTexts; }
	[[nodiscard]] CameraEditManager& GetCameraEdits() override { return _cameraEdits; }
	[[nodiscard]] SourceMeshManager& GetSourceMeshes() override { return _sourceMeshes; }
	[[nodiscard]] HdModelManager& GetHdModels() override { return _hdModels; }
	[[nodiscard]] ParticleFileManager& GetParticleFiles() override { return _particleFiles; }
	[[nodiscard]] LightBitmapManager& GetLightBitmaps() override { return _lightBitmaps; }
	[[nodiscard]] EnumHeaderManager& GetEnumHeaders() override { return _enumHeaders; }
	[[nodiscard]] TextureStemsManager& GetTextureStems() override { return _textureStems; }
	[[nodiscard]] GestureTemplatesManager& GetGestureTemplates() override { return _gestureTemplates; }
	[[nodiscard]] GestureShapeManager& GetGestureShapes() override { return _gestureShapes; }
	[[nodiscard]] CameraPathManager& GetCameraPaths() override { return _cameraPaths; }
	[[nodiscard]] LandLightPaletteManager& GetLandLightPalettes() override { return _landLightPalettes; }
	[[nodiscard]] DanceFileManager& GetDanceFiles() override { return _danceFiles; }
	[[nodiscard]] DanceShapeManager& GetDanceShapes() override { return _danceShapes; }
	[[nodiscard]] PhysicsMaterialsManager& GetPhysicsMaterials() override { return _physicsMaterials; }
	[[nodiscard]] HandDemoManager& GetHandDemos() override { return _handDemos; }
	[[nodiscard]] ClipSoundsManager& GetClipSounds() override { return _clipSounds; }
	[[nodiscard]] CameraZoneManager& GetCameraZones() override { return _cameraZones; }
	[[nodiscard]] entt::id_type NextGeneratedMeshId(std::string_view family, std::string_view name) override
	{
		auto [it, added] = _generatedMeshNumbers.try_emplace(std::string(family), 0);
		const auto id = entt::hashed_string((std::string(name) + "/" + std::to_string(it->second++)).c_str()).value();
		_generatedMeshes.push_back(id);
		return id;
	}
	void ForgetGeneratedMeshes() override
	{
		// Some went already, with the entity that drew them
		for (const auto id : _generatedMeshes)
		{
			if (_meshes.Contains(id))
			{
				_meshes.Erase(id);
			}
		}
		_generatedMeshes.clear();
		_generatedMeshNumbers.clear();
	}
	[[nodiscard]] size_t CountGeneratedMeshes() const override
	{
		return static_cast<size_t>(
		    std::ranges::count_if(_generatedMeshes, [this](entt::id_type id) { return _meshes.Contains(id); }));
	}

private:
	MeshManager _meshes;
	L3DFileManager _l3dFiles;
	TextureManager _textures;
	AnimationManager _animations;
	LevelManager _levels;
	CreatureMindManager _creatureMinds;
	CreatureRigManager _creatureRigs;
	CreatureSkinArtManager _creatureSkinArt;
	SoundManager _sounds;
	GlowManager _glows;
	BlobManager _blobs;
	FontManager _fonts;
	FontHashPermutation _fontHashPermutation {};
	HelpTextManager _helpTexts;
	CameraEditManager _cameraEdits;
	SourceMeshManager _sourceMeshes;
	HdModelManager _hdModels;
	ParticleFileManager _particleFiles;
	LightBitmapManager _lightBitmaps;
	EnumHeaderManager _enumHeaders;
	TextureStemsManager _textureStems;
	GestureTemplatesManager _gestureTemplates;
	GestureShapeManager _gestureShapes;
	CameraPathManager _cameraPaths;
	LandLightPaletteManager _landLightPalettes;
	DanceFileManager _danceFiles;
	DanceShapeManager _danceShapes;
	PhysicsMaterialsManager _physicsMaterials;
	HandDemoManager _handDemos;
	ClipSoundsManager _clipSounds;
	CameraZoneManager _cameraZones;
	PickMaskManager _pickMasks;
	std::map<std::string, uint32_t, std::less<>> _generatedMeshNumbers;
	/// Every generated mesh's id since the last ForgetGeneratedMeshes
	std::vector<entt::id_type> _generatedMeshes;
};
} // namespace openblack::resources
