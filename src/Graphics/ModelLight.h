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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The one model light of the original, CPU side; the GPU twin is assets/shaders/model_light.sh and the two must stay
/// the same. Wiki: rendering-objects.md, "Luz de los modelos".
///
/// LH3DTech keeps a single point light, [0xEA9E90]. fn_0084BA90 (0x84BB90..0x84BC1D) lights every model vertex with it
/// in integers: I = fistp(255 (n . l)) with the light brought into the mesh's own space (fn_00855340), then
/// f = I < 0 ? amb : amb + ((255 - amb) I >> 8) with amb = [0xC39264] = 90, and each colour channel times f >> 8. That
/// integer rule colours every normal model (buildings, villagers, trees, rocks): the float formula of the D3D T&L path
/// (fn_0082C680) is unreachable in this build, because start_system writes [0xC386E4] = 1 (0x642EA7) and so OpenD3D
/// never sets the hardware T&L flag [0xECA60C] (0x82D0F5) that DrawTnL waits for (0x80DC7A).
///
/// The light itself moves with the hour: fn_005E5830, called once a frame by GLandscape::Draw (0x5E488E) before the
/// models, leaves it at the default sun by day and takes it next to the player's hand in full night.
namespace openblack::model_light
{

/// The default sun [0xEA1C88], written by the __xc_a initialiser fn_00818920 (0x818930..0x81894E) and never changed
/// again; fn_00818950 (0x818960) copies it into the light [0xEA9E90] at start-up. The static shadows read it directly
/// (fn_0080ECB0 0x80EDA8, fn_008721A0 0x8721E1), so they keep the day sun all night.
constexpr glm::vec3 k_DefaultSun {-500000.0f, 500000.0f, -500000.0f};
/// [0xC39264] = 90 in .data
constexpr int k_DefaultAmbient = 90;
/// The ambient LH3DMist::Draw puts on while it draws the clouds and the shrinking mists (fn_007FA300, 0x7FA56D; put
/// back to 90 at 0x7FA586)
constexpr int k_MistAmbient = 210;

/// The light LH3DTech keeps, [0xEA9E90], in world coordinates
[[nodiscard]] glm::vec3 Light();
/// fn_0081E1F0 (0x81E1F0): writes the new position into [0xEA9E90] (0x81E1F6..0x81E20D) and returns it (the hidden
/// pointer of its first argument, 0x81E2E6). Saving and restoring is the caller's job (0x8254A3 / 0x82551F): ScopedLight.
void SetLight(const glm::vec3& position);

/// A caller that moves the light for a few draws and puts the old one back, like fn_008254A0 (0x8254A3 / 0x82551F)
class ScopedLight
{
public:
	explicit ScopedLight(const glm::vec3& position)
	    : _previous(Light())
	{
		SetLight(position);
	}
	~ScopedLight() { SetLight(_previous); }
	ScopedLight(const ScopedLight&) = delete;
	ScopedLight(ScopedLight&&) = delete;
	ScopedLight& operator=(const ScopedLight&) = delete;
	ScopedLight& operator=(ScopedLight&&) = delete;

private:
	glm::vec3 _previous;
};

/// The ambient [0xC39264], 0..255
[[nodiscard]] int Ambient();
void SetAmbient(int ambient);

/// LH3DMist::Draw's 210 and back (fn_007FA300, 0x7FA56D / 0x7FA586)
class ScopedAmbient
{
public:
	explicit ScopedAmbient(int ambient)
	    : _previous(Ambient())
	{
		SetAmbient(ambient);
	}
	~ScopedAmbient() { SetAmbient(_previous); }
	ScopedAmbient(const ScopedAmbient&) = delete;
	ScopedAmbient(ScopedAmbient&&) = delete;
	ScopedAmbient& operator=(const ScopedAmbient&) = delete;
	ScopedAmbient& operator=(ScopedAmbient&&) = delete;

private:
	int _previous;
};

/// fn_005E5830 (0x5E5853..0x5E5B7A), once a frame from GLandscape::Draw (0x5E488E), before the models: the focus is
/// the player hand's position (CHand::position, MyInterface()->hand +0x78, 0x5E5848), lifted to at least 10 over the
/// land under it ([0x8AB414], 0x5E58AC..0x5E58C6). With a sky type of 1.5 or less (the double of [0x8C5838];
/// LH3DSky::Time2SkyType 0x86A1B0 of the visual time, 2 = night, 0 = day) the light is the default sun (0x5E5B70), so
/// only the darker half of the night moves it: there it goes 3 units ([0x8C2C50]) from the focus towards the camera
/// (g_camera [0xEA1DB8], 0x5E5A7D..0x5E5B64).
void UpdateFrameLight(glm::vec3 focus, const glm::vec3& cameraPosition, float skyType);

/// fn_00855340 (0x855340): the light's position brought into the mesh's own space by the inverse of the drawn matrix
/// (LHMatrix::SetInverse 0x7FB290, the general inverse, so a non-uniform scale is kept) and normalised. The boned path
/// does the same per bone (0x84BD82..0x84BDFE).
[[nodiscard]] glm::vec3 LightInMeshSpace(const glm::mat4& model);

/// The uniform both sides share, u_modelLight of assets/shaders/model_light.sh: xyz the light [0xEA9E90], w the
/// ambient [0xC39264]
[[nodiscard]] glm::vec4 Uniform();

/// I = fistp(255 (n . l)) (0x84BBAF..0x84BBBE): to the nearest, halves to even. `truncate` is the __ftol variant of
/// the same rule (0x859649).
[[nodiscard]] int Intensity(float dot, bool truncate = false);
/// f = I < 0 ? amb : amb + ((255 - amb) I >> 8) (0x84BBC3..0x84BBE5)
[[nodiscard]] int Factor(int intensity, int ambient);
/// The whole rule on one colour: each channel of c times f >> 8, the alpha untouched (0x84BBEA..0x84BC1D)
[[nodiscard]] uint32_t Apply(uint32_t colour, int intensity, int ambient);

} // namespace openblack::model_light
