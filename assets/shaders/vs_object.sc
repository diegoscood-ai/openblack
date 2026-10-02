#ifdef USE_INSTANCING
$input a_position, a_texcoord0, a_normal, a_indices, i_data0, i_data1, i_data2, i_data3, i_data4
#else
$input a_position, a_texcoord0, a_normal, a_indices
#endif // USE_INSTANCING
$output v_position, v_texcoord0, v_normal, v_color0

// The *_static variants define 1: every draw copies the whole u_model array into the backend's per-frame uniform
// scratch buffer (8 MB with Vulkan), so 128 bones for every static mesh overflowed it on the bigger maps
#ifndef BGFX_CONFIG_MAX_BONES
#if BGFX_SHADER_LANGUAGE_HLSL == 3
#define BGFX_CONFIG_MAX_BONES 48
#else
#define BGFX_CONFIG_MAX_BONES 128
#endif
#endif

#include <bgfx_shader.sh>

#ifdef USE_HEIGHT_MAP
// LandAltitude and LandMelting: the GPU side of src/3D/LandMorph.h
#include "land_altitude.sh"
#endif // USE_HEIGHT_MAP

#ifdef USE_INSTANCING
// ModelLightI / ModelLightFactor / ModelLightDiffuse / ModelLightLocal and u_modelLight: the GPU side of
// src/Graphics/ModelLight.h (fn_0084BA90, the light [0xEA9E90] and the ambient [0xC39264] = 90)
#include "model_light.sh"
// Lh3dMulShr8 / Lh3dAddSat / Lh3dUnpackRgb24: the GPU side of src/Graphics/Lh3dColour.h (LH3DColor's byte arithmetic
// and the instance's colour column)
#include "lh3d_colour.sh"

// Model lighting of the original (fn_00801C90 + fn_0084BA90): the object takes the landscape light of the ground it
// stands on, table[cell luminosity] interpolated bilinearly over the 4 cells around its origin, and the cells' r, g, b
// as specular; each vertex is then lit by the one point light of LH3DTech with the integer rule of model_light.sh.
SAMPLER2D(s_landLightTable, 3); // landscape light table [0xEDD90C], 256x1
SAMPLER2D(s_landCells, 4);      // this frame's cells (land_light::Texels): rgb = the colour as a D3DCOLOR, a = luminosity
#include "land_light.sh"
#include "haze.sh"
uniform vec4 u_objectLight; // x > 0: light like the original, y: colour boost (the hand: x1.5, CHand::AddDrawing),
                            // w: 1 = no distance haze (the hand) + 2 x the mesh's land_light::ObjectMode
uniform vec4 u_window;      // x > 0: a window submesh (L3D isWindow), lit at night by the instance (Abode::Draw)
                            // w: 1 = the primitive takes the object's texture offset

#endif // USE_INSTANCING
// u_objectClip and SeaUnmirror (y: drawn back unmirrored in the reflection target): the GPU side of
// src/Graphics/SeaPass.h, in both branches (the sky writes 0)
#include "sea_plane.sh"

void main()
{
	// Unpack
#if BGFX_SHADER_LANGUAGE_HLSL > 300 || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_SPIRV
	uint modelIndex = uint(max(0, asint(a_indices.x)));
#else
	uint modelIndex = uint(max(0, a_indices.x));
#endif
	modelIndex = min(modelIndex, uint(BGFX_CONFIG_MAX_BONES - 1));

	v_position = mul(u_model[modelIndex], vec4(a_position.xyz, 1.0f));
	// Normals follow the bone / model rotation and then the instance rotation (uniform scales only, renormalised)
	vec3 normal = mul(u_model[modelIndex], vec4(a_normal.xyz, 0.0f)).xyz;

#ifdef USE_INSTANCING
	// The w of the first column carries 1 - opacity for fading meshes (0 for the others).
	float fade = i_data0.w;
	mat4 model;
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f);
	// The fifth column, the object's LH3DColor fields (lh3d_colour::PackInstance*, src/Graphics/Lh3dColour.h), each
	// an rgb 0xRRGGBB below 2^24:
	// x, obj+0x4C: 0 the land light alone; < 0: -1 - a tint t that multiplies the land light (fn_0080BF10's: Field::Draw,
	// the white tint 0xFFFFFFFF, the poison, the charring grey; or Tree::Draw's own, see w); > 0: 1 + the colour set
	// with SetColorSpecular 0x7F9770 instead of the land light (the power-up bands, the PSys mesh atoms)
	// y, obj+0x50: the specular, 8 bits a channel (Living +0xD0, the poison's, the fire's glow, the bands' 0x141414)
	// z, obj+0x54: 0, or 1 + the house's window colour at night (Abode::Draw vt 0x30)
	// w: 1 = the tint after the haze (Tree::Draw: haze 0x74AB60, then the brightness 0x74B077 or fn_0074B3A0 0x74B48F)
	vec3 drawColour = i_data4.x < -0.5f ? Lh3dUnpackRgb24(-i_data4.x - 1.0f) : vec3_splat(-1.0f);
	bool setColour = i_data4.x > 0.5f;
	vec3 setColour255 = setColour ? Lh3dUnpackRgb24(i_data4.x - 1.0f) : vec3_splat(0.0f);
	vec3 objectSpecular255 = Lh3dUnpackRgb24(i_data4.y);
	bool tintAfterHaze = i_data4.w > 0.5f;
	bool windowLit = i_data4.z > 0.5f;
	vec3 windowColour = windowLit ? Lh3dUnpackRgb24(i_data4.z - 1.0f) / 255.0f : vec3_splat(0.0f);

	v_position = instMul(model, v_position);
	normal = instMul(model, vec4(normal, 0.0f)).xyz;

	// The one light of the original in the space the vertex lives in, for the fn_0084BA90 branches below (fn_00855340
	// for the rigid meshes; the boned path 0x84BD82..0x84BDFE does SetInverse of each bone matrix (0x84BD9E) over the
	// light already in camera space (0x84BDA3), which is the light per bone only if those matrices go from the bone to
	// the camera, so that the camera cancels out (inferido)). It is taken from the ORIGIN of the bone (or of the object)
	// and meets the raw local normal, so a non-uniform scale (a tree's sway, a field's shear) gives the light of the
	// original and not that of a rotated normal. The axes come out of mul / instMul instead of u_model[i][k]: with HLSL
	// that indexes a row of the maths matrix, not the axis, because bgfx packs its matrices column major (the compiler
	// folds the unit vectors away). Only the vertex-lit cases need it: mode 1 and the PSys mesh atoms of modes 1 and 3,
	// and not with the hd-tweaks per-pixel light (u_window.y > 0); the cut (mode 4) takes its own light below.
	vec3 lightLocal = vec3_splat(0.0f);
	if (u_window.y <= 0.0f && u_objectLight.x > 0.0f &&
	    (u_objectLight.x < 1.5f || (u_objectLight.x > 2.5f && u_objectLight.x < 3.5f && setColour)))
	{
		vec3 lightAxisX = instMul(model, vec4(mul(u_model[modelIndex], vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightAxisY = instMul(model, vec4(mul(u_model[modelIndex], vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightAxisZ = instMul(model, vec4(mul(u_model[modelIndex], vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz, 0.0f)).xyz;
		vec3 lightOrigin = instMul(model, mul(u_model[modelIndex], vec4(0.0f, 0.0f, 0.0f, 1.0f))).xyz;
		lightLocal = ModelLightLocal(lightAxisX, lightAxisY, lightAxisZ, lightOrigin, u_modelLight.xyz);
	}
	float lightAmbient = u_modelLight.w;
#endif // USE_INSTANCING

#ifdef USE_HEIGHT_MAP
	// Morphing with the land (LH3DObject::UpdateMelting 0x8168F0, land_altitude.sh): every vertex is raised along the
	// object's local Y by the land's height under it minus the height under the object's origin
#ifdef USE_INSTANCING
	// (the scale: column 0 is the rotation x the uniform scale; the field and tree sway only touch column 1)
	v_position.xyz = LandMelting(v_position.xyz, i_data3.xz, i_data1.xyz, length(i_data0.xyz));
#else
	v_position.xyz = LandMelting(v_position.xyz, u_model[modelIndex][3].xz, u_model[modelIndex][1].xyz,
	                             length(u_model[modelIndex][0].xyz));
#endif // USE_INSTANCING
#endif // USE_HEIGHT_MAP

	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
#ifdef USE_INSTANCING
	// The w of the second column carries the object's texture offset (components::UvScroll, SetUVOffset): V (food piles,
	// the Land 3 waterfall) + 4 x U in 1/256 steps (the one-shot orbs' 4x4 animation, the PSys AnimTextured meshes),
	// added (U and V) only to the primitives whose material lacks bit 0x10 of byte +5 (u_window.w;
	// LH3DRender::DrawTriangle 0x82F8BE). V may be -2..2: the waterfall's is -1..0 (frac of a decreasing V), the piles'
	// and orbs' 0..1
	float uSteps = floor((i_data1.w + 2.0f) / 4.0f);
	v_texcoord0.x += uSteps / 256.0f * u_window.w;
	v_texcoord0.y += (i_data1.w - uSteps * 4.0f) * u_window.w;
#endif // USE_INSTANCING
	vec3 specular = vec3_splat(0.0f);
#ifdef USE_INSTANCING
	vec3 objectColour = vec3_splat(1.0f);
	if (u_objectLight.x > 3.5f)
	{
		// DrawCutByPlane (fn_00811C70 / fn_0080C050 -> fn_00858BA0 per vertex): the same rule as fn_0084BA90 with the
		// light [0xF03140] in the OBJECT's space: both callers set it once with fn_00855340 over the object matrix
		// obj+0x14 (static 0x80C0EE, animated 0x811D2F), and fn_00858BA0 reads [0xF03140] in both its branches (rigid
		// 0x858CB1, boned 0x859049), using the bone matrices [0xE9FE48] (0x858F77) only for the positions. So a boned
		// mesh is lit with the object's light, not per bone: the instance matrix alone, without u_model. rgb =
		// colour.rgb (the colour of SetColorSpecular, u_objectLight.z) x f >> 8, no land light, no haze, + the
		// object's specular obj+0x50 (u_objectLight.w, sea_pass::SeaDraw::specular). z < 0: each instance's own
		// obj+0x4C / +0x50 from the fifth column (sea_pass::CutAtoms: the PSys mesh atoms' DrawData +8 / +0xC, set by
		// SetColorSpecular vt+0x2C 0x67A02F before vt+0x11C 0x679F4A)
		bool cutOwnColour = u_objectLight.z < -0.5f;
		vec3 cutLight = ModelLightLocal(i_data0.xyz, i_data1.xyz, i_data2.xyz, i_data3.xyz, u_modelLight.xyz);
		vec3 cutColour = cutOwnColour ? setColour255 : Lh3dUnpackRgb24(u_objectLight.z);
		float cutFactor = ModelLightFactor(ModelLightI(a_normal.xyz, cutLight, false), lightAmbient);
		objectColour = ModelLightDiffuse(cutColour, cutFactor) / 255.0f;
		specular = (cutOwnColour ? objectSpecular255 : Lh3dUnpackRgb24(u_objectLight.w)) / 255.0f;
	}
	else if (u_objectLight.x > 1.5f && u_objectLight.x < 2.5f)
	{
		// DrawUnderWater in a constant colour (sea_pass::SeaLight::Constant, fn_00811010 -> fn_00850FC0): obj+0x4C packed
		// r 65536 + g 256 + b (the hand's 0xA0A0A0, the boat's 0x303070), no vertex light, + the specular obj+0x50 (w)
		objectColour = Lh3dUnpackRgb24(u_objectLight.z) / 255.0f;
		specular = Lh3dUnpackRgb24(u_objectLight.w) / 255.0f;
	}
	else if (u_objectLight.x > 0.0f)
	{
		// The land light at the origin by the mesh's mode (land_light.sh, land_light::ObjectMode): fn_00801C90 bilinear,
		// fn_00802120 cell >> 8, 0x803340 the cell alone, [0xEDDD08] alone (Dove::Draw 0x41F75B writes +0x4C only;
		// (inferido) +0x50 left at 0, nothing in Dove::Draw sets it)
		float landMode = floor(u_objectLight.w / 2.0f);
		bool hazeOff = u_objectLight.w - landMode * 2.0f > 0.5f;
		vec3 landSpecular = vec3_splat(0.0f);
		vec3 landDiffuse = LandLightFull();
		if (landMode < 0.5f)
		{
			landDiffuse = LandLightBilinear(i_data3.xz, landSpecular);
		}
		else if (landMode < 1.5f)
		{
			landDiffuse = LandLightCellShift(i_data3.xz, landSpecular);
		}
		else if (landMode < 2.5f)
		{
			landDiffuse = LandLightCell(i_data3.xz, landSpecular);
		}
		objectColour = landDiffuse / 255.0f;
		// + the object's own specular, per channel with saturation (fn_0080BF10 0x80BF1B..0x80BFB9, before the haze)
		specular = Lh3dAddSat(landSpecular, objectSpecular255) / 255.0f;
		objectColour = min(objectColour * u_objectLight.y, vec3_splat(1.0f));
		// the tint multiplies the land light byte by byte, (c t) >> 8 (fn_0080BF10 0x80BFA3..0x80C00B): the white
		// 0xFFFFFFFF takes 1 off each channel
		if (drawColour.r >= 0.0f && !tintAfterHaze)
		{
			objectColour = Lh3dMulShr8(floor(objectColour * 255.0f + 0.5f), drawColour) / 255.0f;
		}
		// x = 3: only that colour and specular, as fn_00801C90 leaves them in the object (obj+0x4C / +0x50) for
		// DrawUnderWater (reflections: no haze, no vertex lighting)
		if (u_objectLight.x < 2.5f)
		{
		// Distance haze once per object at its origin (fn_007FEB30, haze.sh): none with the key off or closer than near
		// (0x7FEB36, 0x7FEB7D); the colour (c f) >> 8 per byte, the specular + the fistp colour, saturated
		float originDepth = mul(u_view, vec4(i_data3.xyz, 1.0f)).z;
		if (u_haze.w > 0.0f && !hazeOff && !(originDepth < u_haze.x))
		{
			float hazeT = HazeT(originDepth);
			objectColour = ApplyHazeDiffuse(floor(objectColour * 255.0f + 0.5f), HazeFactor(hazeT)) / 255.0f;
			specular = HazeAddSaturated(floor(specular * 255.0f + 0.5f), HazeColour(hazeT)) / 255.0f;
		}
		}
		// Tree::Draw's tint, the same (c t) >> 8 over the hazed +0x4C (0x74B077..0x74B0C4, 0x74B48F..0x74B4D3); the
		// specular is left as the haze made it
		if (drawColour.r >= 0.0f && tintAfterHaze)
		{
			objectColour = Lh3dMulShr8(floor(objectColour * 255.0f + 0.5f), drawColour) / 255.0f;
		}
		if (u_objectLight.x < 2.5f)
		{
		// The vertex light of fn_0084BA90 (model_light.sh) over the object's byte colour, so it is at most 254/256
		// mod graphics.hd-tweaks (u_window.y > 0): fs_object does this per pixel on the villager
		if (u_window.y <= 0.0f)
		{
			float factor = ModelLightFactor(ModelLightI(a_normal.xyz, lightLocal, false), lightAmbient);
			objectColour = ModelLightDiffuse(floor(objectColour * 255.0f + 0.5f), factor) / 255.0f;
		}
		}
		// Windows at night (fn_00856D40): unlit, the flat colour obj+0x54 instead of the land light, the specular kept
		if (u_window.x > 0.0f && windowLit)
		{
			objectColour = windowColour;
		}
	}
	// The colour of SetColorSpecular (vt 0x2C, 0x7F9770: obj+0x4C and +0x50) instead of the land light of fn_00801C90
	// and without fn_007FEB30's haze: a PSys mesh atom's DrawData colour (Particle3DObj::DrawAt 0x679FD0,
	// PSys/Creators/Mesh.h) and the power-up bands (components::ObjectColour). The model light stays: the object is an
	// LH3DObject that draws like every other model, fn_00855340 -> fn_0084BA90 (inferido: the draw that follows
	// Particle3DObj::DrawAt is not disassembled). Not in the cut (mode 4), which lights that colour itself
	if (setColour && u_objectLight.x > 0.0f && (u_objectLight.x < 1.5f || (u_objectLight.x > 2.5f && u_objectLight.x < 3.5f)))
	{
		objectColour = setColour255 / 255.0f;
		if (u_window.y <= 0.0f)
		{
			float factor = ModelLightFactor(ModelLightI(a_normal.xyz, lightLocal, false), lightAmbient);
			objectColour = ModelLightDiffuse(setColour255, factor) / 255.0f;
		}
		specular = objectSpecular255 / 255.0f;
	}
	float opacity = 1.0f - fade;
	if (u_objectLight.x > 3.5f)
	{
		opacity *= u_objectLight.y; // A = colour A (fn_00858BA0)
	}
	// components::MeshTint: 1e6 (2e6 dissolving instead of blending) + 5 bits each of the ground colour and of `own`
	float tintMarker = 0.0f;
	vec3 tintGround = vec3_splat(0.0f);
	float tintOwn = 0.0f;
	if (i_data2.w > 500000.0f)
	{
		bool dissolve = i_data2.w > 1500000.0f;
		float packedTint = i_data2.w - (dissolve ? 2000000.0f : 1000000.0f);
		tintOwn = floor(packedTint / 32768.0f);
		packedTint -= tintOwn * 32768.0f;
		float red = floor(packedTint / 1024.0f);
		float green = floor((packedTint - red * 1024.0f) / 32.0f);
		float blue = packedTint - red * 1024.0f - green * 32.0f;
		tintGround = min(vec3(red, green, blue) / 31.0f, vec3_splat(0.99f));
		tintMarker = 1000.0f;
		// fs_object reads a negative alpha -1 - opacity as a dissolve
		opacity = dissolve ? -1.0f - opacity : opacity;
	}
	v_color0 = vec4(objectColour, opacity);
#else
	v_color0 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#endif // USE_INSTANCING
	v_normal = normal; // not normalised here: the sky mesh has zero normals
#ifdef USE_INSTANCING
	// fs_object doesn't light with the normal: a tinted mesh (MeshTint) carries its tint there instead, 1000 + 2 own
	// in x plus the ground colour in the fractions (the same at every vertex, so it survives interpolation)
	if (tintMarker > 0.0f)
	{
		v_normal = vec3(tintMarker + 2.0f * tintOwn, tintMarker, tintMarker) + tintGround;
	}
#endif // USE_INSTANCING
	// The specular colour rides in the unused texcoord z/w and position w: vs_object is shared with the sky (fs_sky)
	// and a new varying broke its interface
	// fs_object clips on the real position; only the drawn one is mirrored back
	gl_Position = mul(u_viewProj, SeaUnmirror(v_position));
#ifdef USE_INSTANCING
	// Window submeshes exist only while the house's windows are lit (by day they fail the LOD test)
	if (u_window.x > 0.0f && !windowLit)
	{
		gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);
	}
#endif // USE_INSTANCING
	v_texcoord0.zw = specular.rg;
	v_position.w = specular.b;
}
