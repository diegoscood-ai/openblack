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
SAMPLER2D(s_heightmap, 1); // per cell: r = altitude (height units), g = split bit (LandIsland::CreateHeightMap)
uniform vec4 u_islandExtent;

vec2 HeightCell(vec2 cell, vec2 size)
{
	return texture2DLod(s_heightmap, (cell + 0.5f) / size, 0.0f).rg;
}

// LH3DIsland::GetAltitude (0x803090, LandIsland::HeightAt): the landscape triangle under the point. Each cell is split
// along the diagonal its split bit chooses and the fourth corner is extrapolated from the other three, so the
// bilinear blend is planar on that triangle; next to the sea (base corner <= 4) heights of 3 or less count as 0.
float LandAltitude(vec2 xz)
{
	vec2 size = (u_islandExtent.zw - u_islandExtent.xy) * 0.1f + 1.0f;
	vec2 position = (xz - u_islandExtent.xy) * 0.1f;
	vec2 cell = floor(position);
	vec2 f = position - cell;
	vec2 base = HeightCell(cell, size);
	float v00 = base.r;
	float v01 = HeightCell(cell + vec2(0.0f, 1.0f), size).r;
	float v10 = HeightCell(cell + vec2(1.0f, 0.0f), size).r;
	float v11 = HeightCell(cell + vec2(1.0f, 1.0f), size).r;
	if (v00 <= 4.0f)
	{
		v00 = v00 > 3.0f ? v00 : 0.0f;
		v01 = v01 > 3.0f ? v01 : 0.0f;
		v10 = v10 > 3.0f ? v10 : 0.0f;
		v11 = v11 > 3.0f ? v11 : 0.0f;
	}
	float c00 = v00;
	float c01 = v01;
	float c10 = v10;
	float c11 = v11;
	if (base.g > 0.5f)
	{
		if (f.y > 1.0f - f.x)
		{
			c00 = v10 + v01 - v11;
		}
		else
		{
			c11 = v10 + v01 - v00;
		}
	}
	else if (f.x > f.y)
	{
		c01 = v00 + v11 - v10;
	}
	else
	{
		c10 = v00 + v11 - v01;
	}
	return mix(mix(c00, c01, f.y), mix(c10, c11, f.y), f.x) * 0.67f;
}
#endif // USE_HEIGHT_MAP

#ifdef USE_INSTANCING
// Model lighting of the original (fn_00801C90 + fn_0084BA90): the object takes the landscape light of the ground it
// stands on, table[cell luminosity] interpolated bilinearly over the 4 cells around its origin, and the cells' r, g, b
// as specular; each vertex gets ambient 90/256 + 166/256 * N.L with the light at (-500000, 500000, -500000).
SAMPLER2D(s_cellMap, 2);   // per cell: rgb = the cell colour read as a D3DCOLOR (R and B swapped), a = luminosity
SAMPLER2D(s_landLight, 3); // landscape light table, 256x1
SAMPLER2D(s_cloudShadow, 4); // cloud shadow luminosity cap per cell
uniform vec4 u_cellMap;     // xy: world position of the map's first cell, zw: map size in cells
uniform vec4 u_objectLight; // x > 0: light like the original, y: colour boost (the hand: x1.5, CHand::AddDrawing),
                            // w > 0: no distance haze (the hand)
uniform vec4 u_haze;        // x: near, y: far, z: k, w: on ("Fog" detail key)
uniform vec4 u_hazeColour;  // rgb: fog colour 0..255
uniform vec4 u_window;      // x > 0: a window submesh (L3D isWindow), lit at night by the instance (Abode::Draw)
                            // w: 1 = the primitive takes the object's texture offset
uniform vec4 u_objectClip;  // y > 0: mirrored in y = 0 (the parts under the water drawn into the reflection target)

vec4 CellTexel(vec2 cell)
{
	// off the map: the full light and no specular, like the cells of missing blocks (fn_00801C90, 0x8020F8)
	if (any(lessThan(cell, vec2_splat(0.0f))) || any(greaterThanEqual(cell, u_cellMap.zw)))
	{
		return vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	vec4 texel = texture2DLod(s_cellMap, (cell + 0.5f) / u_cellMap.zw, 0.0f);
	texel.a = min(texel.a, texture2DLod(s_cloudShadow, (cell + 0.5f) / u_cellMap.zw, 0.0f).r);
	return texel;
}

vec3 LandLight(float luminosity)
{
	return texture2DLod(s_landLight, vec2((floor(luminosity * 255.0f + 0.5f) + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;
}
#endif // USE_INSTANCING

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
	// The w of the fourth column: 2 + the grey of the house's windows at night (1 when they are lit normally), or
	// negative: -1 - the object colour r 65536 + g 256 + b (Field::Draw, fn_0080BF10)
	// or 3e6 + the specular colour Living::SetSpecularColor adds (components::SpecularColour: r 16384 + g 128 + b, 7
	// bits each, the heal chakra's glow)
	float windowGrey = i_data3.w > 1.5f && i_data3.w < 2500000.0f ? i_data3.w - 2.0f : -1.0f;
	vec3 objectSpecular = vec3_splat(0.0f);
	if (i_data3.w > 2500000.0f)
	{
		float packedSpecular = i_data3.w - 3000000.0f;
		float red = floor(packedSpecular / 16384.0f);
		float green = floor((packedSpecular - red * 16384.0f) / 128.0f);
		objectSpecular = vec3(red, green, packedSpecular - red * 16384.0f - green * 128.0f) * 2.0f / 255.0f;
	}
	vec3 drawColour = vec3_splat(-1.0f);
	if (i_data3.w < -0.5f)
	{
		float packedColour = -i_data3.w - 1.0f;
		float red = floor(packedColour / 65536.0f);
		float green = floor((packedColour - red * 65536.0f) / 256.0f);
		drawColour = vec3(red, green, packedColour - red * 65536.0f - green * 256.0f);
	}

	v_position = instMul(model, v_position);
	normal = instMul(model, vec4(normal, 0.0f)).xyz;
#endif // USE_INSTANCING

#ifdef USE_HEIGHT_MAP
	// Morphing with the land (LH3DObject::UpdateMelting 0x8168F0): every vertex is raised by the land's height under
	// it minus the height under the object's origin, so the object's own altitude offset (a pile sinking, a field's
	// food) is kept
#ifdef USE_INSTANCING
	vec2 origin = i_data3.xz;
#else
	vec2 origin = u_model[modelIndex][3].xz;
#endif // USE_INSTANCING
	v_position.y += LandAltitude(v_position.xz) - LandAltitude(origin);
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
		// DrawCutByPlane (fn_00811C70 / fn_0080C050 -> fn_00858BA0 per vertex): I = 255 (L.n) with the light 0xF03140
		// (the one of fn_0084BA90); I < 0 -> amb, else amb + (255 - amb) I >> 8, amb = [0xC39264] = 90; rgb = colour.rgb x
		// I >> 8 (the colour of SetColorSpecular, u_objectLight.z), no land light, no haze, the object's specular (0 for
		// every caller). I is stored with fistp (0x858CDF): rounded to the nearest, halves to even (the FPU default)
		const vec3 cutLight = vec3(-0.57735027f, 0.57735027f, -0.57735027f);
		float packedCut = u_objectLight.z;
		float cutRed = floor(packedCut / 65536.0f);
		float cutGreen = floor((packedCut - cutRed * 65536.0f) / 256.0f);
		vec3 cutColour = vec3(cutRed, cutGreen, packedCut - cutRed * 65536.0f - cutGreen * 256.0f);
		float lit = 255.0f * dot(normalize(normal), cutLight);
		float intensity = floor(lit + 0.5f);
		if (intensity - lit == 0.5f && mod(intensity, 2.0f) != 0.0f)
		{
			intensity -= 1.0f;
		}
		intensity = intensity < 0.0f ? 90.0f : 90.0f + floor(165.0f * intensity / 256.0f);
		objectColour = floor(cutColour * intensity / 256.0f) / 255.0f;
	}
	else if (u_objectLight.x > 1.5f && u_objectLight.x < 2.5f)
	{
		// a grey 0..1 (the hand's 0xA0A0A0), or above 1 a packed r 65536 + g 256 + b (the boat's 0x303070)
		if (u_objectLight.z > 1.5f)
		{
			float packedRed = floor(u_objectLight.z / 65536.0f);
			float packedGreen = floor((u_objectLight.z - packedRed * 65536.0f) / 256.0f);
			objectColour = vec3(packedRed, packedGreen, u_objectLight.z - packedRed * 65536.0f - packedGreen * 256.0f) / 255.0f;
		}
		else
		{
			objectColour = vec3_splat(u_objectLight.z);
		}
	}
	else if (u_objectLight.x > 0.0f)
	{
		vec2 cellPosition = (i_data3.xz - u_cellMap.xy) * 0.1f;
		vec2 cell = floor(cellPosition);
		vec2 w = cellPosition - cell;
		vec4 c00 = CellTexel(cell);
		vec4 c10 = CellTexel(cell + vec2(1.0f, 0.0f));
		vec4 c01 = CellTexel(cell + vec2(0.0f, 1.0f));
		vec4 c11 = CellTexel(cell + vec2(1.0f, 1.0f));
		objectColour = mix(mix(LandLight(c00.a), LandLight(c01.a), w.y), mix(LandLight(c10.a), LandLight(c11.a), w.y), w.x);
		specular = mix(mix(c00.rgb, c01.rgb, w.y), mix(c10.rgb, c11.rgb, w.y), w.x);
		// + the object's own specular, per channel with saturation (fn_0080BF10 from fn_0080BEC0: Villager / Animal Draw)
		specular = min(specular + objectSpecular, vec3_splat(1.0f));
		objectColour = min(objectColour * u_objectLight.y, vec3_splat(1.0f));
		// the object colour multiplies the land light byte by byte, (c x tint) >> 8 (fn_0080BF10)
		if (drawColour.r >= 0.0f)
		{
			objectColour = floor(floor(objectColour * 255.0f + 0.5f) * drawColour / 256.0f) / 255.0f;
		}
		// x = 3: only that colour and specular, as fn_00801C90 leaves them in the object (obj+0x4C / +0x50) for
		// DrawUnderWater (reflections: no haze, no vertex lighting)
		if (u_objectLight.x < 2.5f)
		{
		// Distance haze once per object at its origin (fn_007FEB30); none closer than near
		float originDepth = mul(u_view, vec4(i_data3.xyz, 1.0f)).z;
		float hazeT = originDepth < u_haze.x || u_objectLight.w > 0.0f
		                  ? 0.0f
		                  : u_haze.w * saturate((originDepth - u_haze.x) / (u_haze.y - u_haze.x));
		objectColour *= (256.0f - floor((256.0f - u_haze.z) * hazeT)) / 256.0f;
		specular = min(specular + floor(u_hazeColour.rgb * hazeT + 0.5f) / 255.0f, vec3_splat(1.0f));
		const vec3 lightDirection = vec3(-0.57735027f, 0.57735027f, -0.57735027f);
		// mod graphics.hd-tweaks (u_window.y > 0): fs_object does this per pixel on the villager
		if (u_window.y <= 0.0f)
		{
			objectColour *= 90.0f / 256.0f + 166.0f / 256.0f * max(0.0f, dot(normalize(normal), lightDirection));
		}
		}
		// Windows at night (fn_00856D40): unlit, the flat grey instead of the land light, the specular kept
		if (u_window.x > 0.0f && windowGrey >= 0.0f)
		{
			objectColour = vec3_splat(windowGrey);
		}
	}
	// A PSys mesh atom (PSys/Creators/Mesh.h): -1 - (r 65536 + g 256 + b) in the w of the third column is its DrawData
	// colour, which Particle3DObj::DrawAt 0x679FD0 gives the object with SetColour (vt 0x2C: obj +0x4C) instead of the
	// land light of fn_00801C90 and without fn_007FEB30's haze; the model light (ambient 90 + 166 N.L) stays
	if (i_data2.w < -0.5f && u_objectLight.x > 0.0f && (u_objectLight.x < 1.5f || u_objectLight.x > 2.5f))
	{
		float packedParticle = -i_data2.w - 1.0f;
		float particleRed = floor(packedParticle / 65536.0f);
		float particleGreen = floor((packedParticle - particleRed * 65536.0f) / 256.0f);
		vec3 particleColour = vec3(particleRed, particleGreen, packedParticle - particleRed * 65536.0f - particleGreen * 256.0f) / 255.0f;
		const vec3 particleLight = vec3(-0.57735027f, 0.57735027f, -0.57735027f);
		objectColour = particleColour;
		if (u_window.y <= 0.0f)
		{
			objectColour *= 90.0f / 256.0f + 166.0f / 256.0f * max(0.0f, dot(normalize(normal), particleLight));
		}
		specular = vec3_splat(0.0f);
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
#ifdef USE_INSTANCING
	// fs_object clips on the real position; only the drawn one is mirrored
	gl_Position = mul(u_viewProj, u_objectClip.y > 0.0f ? vec4(v_position.x, -v_position.y, v_position.zw) : v_position);
#else
	gl_Position = mul(u_viewProj, v_position);
#endif // USE_INSTANCING
#ifdef USE_INSTANCING
	// Window submeshes exist only while the house's windows are lit (by day they fail the LOD test)
	if (u_window.x > 0.0f && windowGrey < 0.0f)
	{
		gl_Position = vec4(2.0f, 2.0f, 2.0f, 1.0f);
	}
#endif // USE_INSTANCING
	v_texcoord0.zw = specular.rg;
	v_position.w = specular.b;
}
