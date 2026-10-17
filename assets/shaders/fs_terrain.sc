$input v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ

#include <bgfx_shader.sh>

#define M_PI 3.1415926535897932384626433832795

// SnowDepthCell, SnowDepthAt and u_snow: the snow lying on the island, src/3D/SnowCover.h
#include "snow.sh"

SAMPLER2DARRAY(s0_materials, 0);
SAMPLER2D(s1_bump, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s3_footprints, 3);
SAMPLER2D(s5_staticShadow, 5);
SAMPLER2D(s8_landAlpha, 8); // 1, or lower in the river channels (the sea drawn before the land shows through)
SAMPLER2D(s10_blockTexture, 10); // the original's block textures (BlockTexture.h), RGBA8 of ARGB4444, rows along +z
uniform vec4 u_blockTexture;      // x: 1 = colour from it (else the per-vertex materials),
                                  // y: 1 = its alpha is the coast alpha (else no block texture: 1)
uniform vec4 u_islandExtent;   // xy: minimum x/z, zw: maximum x/z

uniform vec4 u_skyAndBump; // x unused, y: bump strength, z: small bump strength (w: vs_terrain)
uniform vec4 u_terrainPass; // x: light scale (0.5 for the mirrored land in the reflection, as the original),
                            // y, w: unused (0), z: static shadow strength (0.5, or 0.25 with low textures)

// The noise that makes the snow's edges ragged, once across each land block of 160 units (data/WeatherSystem/snowmap.raw)
SAMPLER2D(s12_snowNoise, 12);

// The land under the snow lying on it (snow_cover::LandLevel, LandWhite): whiter the deeper the snow over each texel of
// the noise, from its own colour to a grey that is blue-white where it lies thickest. Where no corner of its cell has
// more than a little snow, none shows.
vec3 LandUnderSnow(vec3 colour, vec2 xz)
{
	vec2 cell = floor(xz * 0.025f);
	float most = max(max(SnowDepthCell(cell), SnowDepthCell(cell + vec2(1.0f, 0.0f))),
	                 max(SnowDepthCell(cell + vec2(0.0f, 1.0f)), SnowDepthCell(cell + 1.0f)));
	if (most <= 5.0f)
	{
		return colour;
	}
	vec2 texel = floor(fract(xz / 160.0f) * 256.0f);
	float noise = floor(texture2DLod(s12_snowNoise, (texel + 0.5f) / 256.0f, 0.0f).r * 255.0f + 0.5f);
	float level = clamp(floor((floor(SnowDepthAt(xz)) - noise) / 8.0f), 0.0f, 16.0f);
	if (level <= 0.0f)
	{
		return colour;
	}
	float white = 14.0f + mod(floor(noise / 4.0f) + floor(noise / 2.0f), 2.0f);
	if (level >= 16.0f)
	{
		return vec3(white, white, 15.0f) / 15.0f;
	}
	return colour * (16.0f - level) / 16.0f + vec3_splat(floor(white * level / 16.0f) / 15.0f);
}

// The three corner materials of the triangle (an island without a block texture), each a blend of two: the first one
// weighs the coefficient and the second the rest; uv spans one block
vec4 SampleMaterials(vec2 uv, vec3 id0, vec3 id1, vec3 blend, vec3 weight)
{
	return mix(texture2DArray(s0_materials, vec3(uv, id1.r)), texture2DArray(s0_materials, vec3(uv, id0.r)), blend.r) *
	           weight.r +
	       mix(texture2DArray(s0_materials, vec3(uv, id1.g)), texture2DArray(s0_materials, vec3(uv, id0.g)), blend.g) *
	           weight.g +
	       mix(texture2DArray(s0_materials, vec3(uv, id1.b)), texture2DArray(s0_materials, vec3(uv, id0.b)), blend.b) *
	           weight.b;
}

void main()
{
	// unpack uniforms
	float bumpMapStrength = u_skyAndBump.y;
	float smallBumpMapStrength = u_skyAndBump.z;

	vec2 coastUv = (v_worldXZ - u_islandExtent.xy) / (u_islandExtent.zw - u_islandExtent.xy);
	vec4 block = texture2D(s10_blockTexture, coastUv);

	// The original: the block texture, one texel per 1/16 cell, filtered bilinearly by D3D. Per texel the material of
	// min((h >> 8) + noise, 255) with the cone-weighted altitude h, x bump >> 8, 4 bits per channel, the corner
	// countries blended; the footprints, static shadows and light go on top as before.
	vec4 col = vec4(block.rgb, 1.0f);
	if (u_blockTexture.x < 0.5f)
	{
		// each vert with both materials
		vec3 id0 = vec3(v_materialID0.xyz);
		vec3 id1 = vec3(v_materialID1.xyz);
		col = SampleMaterials(v_texcoord0.xy, id0, id1, v_materialBlend, v_weight);

		// apply bump map (2x because it's half bright?)
		float bump = mix(1.0f, texture2D(s1_bump, v_texcoord0.xy).r * 2.0f, bumpMapStrength);
		col = col * bump;
	}

	vec4 footprints = texture2D(s3_footprints, v_texcoord1.xy);
	col.rgb = mix(col.rgb, footprints.rgb, footprints.a);

	// Static object shadows, baked into the block texture in the original (after the footprints)
	col.rgb *= 1.0f - u_terrainPass.z * texture2D(s5_staticShadow, v_texcoord1.xy).r;

	// The snow lies over all of it, under the light; without snow u_snow.x is 0 and the colour is left as it is
	if (u_snow.x > 0.5f)
	{
		col.rgb = LandUnderSnow(col.rgb, v_worldXZ);
	}

	// apply light map
	col.rgb = col.rgb * v_landLight * u_terrainPass.x;

	// Small bump (render mode 0xE: TEXTURE * DIFFUSE in colour and alpha): a second pass over the lit
	// land, blended SRCALPHA / INVSRCALPHA, 12 repeats per block. Its vertex diffuse (vs_terrain) is white or black
	// with alpha = fade, 0 at the vertices of altitude 1 or less, so it fades out towards the water; the triangles
	// with all three at 0 are skipped by the original, which changes nothing. It does not use the coast alpha.
	vec4 smallBump = texture2D(s2_smallBump, v_texcoord0.xy * 12.0f);
	smallBump.rgb *= v_smallBumpFade.x;
	float bumpAlpha = smallBump.a * v_smallBumpFade.y * smallBumpMapStrength;

	// The land's alpha (render mode 14, SRCALPHA / INVSRCALPHA over the sea already drawn): the coast alpha of the
	// block texture, lowered by the rivers with min. Z is written even where it is 0.
	float coastAlpha = u_blockTexture.y > 0.5f ? block.a : 1.0f;
	float landAlpha = min(coastAlpha, texture2D(s8_landAlpha, v_texcoord1.xy).r);

	// D3D specular (SPECULARENABLE): cell colour + haze, added in both the land and the small bump pass. The two
	// passes as one premultiplied colour (blend ONE / INV_SRC_ALPHA): land * a * (1 - b) + bump * b over dst * (1 - a)(1 - b)
	col.rgb = (col.rgb + v_landSpecular) * landAlpha * (1.0f - bumpAlpha) + (smallBump.rgb + v_landSpecular) * bumpAlpha;
	float transmitted = (1.0f - landAlpha) * (1.0f - bumpAlpha);

	// The projected shadows are drawn over the block afterwards, one draw per shadow (
	// vs_land_shadow / fs_land_shadow): black, SRCALPHA / INVSRCALPHA over the block, so over the sea seen through it too
	gl_FragColor = vec4(col.rgb, 1.0f - transmitted);
}
