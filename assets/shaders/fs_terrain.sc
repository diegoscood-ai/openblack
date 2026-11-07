$input v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_shoreFade, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ

#include <bgfx_shader.sh>

#define M_PI 3.1415926535897932384626433832795


SAMPLER2DARRAY(s0_materials, 0);
SAMPLER2D(s1_bump, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s10_blockTexture, 10); // the original's block textures (BlockTexture.h), RGBA8 of ARGB4444, rows along +z
SAMPLER2DARRAY(s9_lowResPages, 9); // the land's low-resolution pages, a layer each (ARGB4444 as RGBA8)
uniform vec4 u_blockTile;      // x: the block's texture level: 0 its texture, 1 its tile only, 2 both; y: its tile's page
uniform vec4 u_blockTileDebug; // x: 1 = tint each block by its level (debug)
uniform vec4 u_blockTexture;      // x: 1 = colour from it (else the per-vertex materials),
                                  // y: 1 = its alpha is the coast alpha (else no block texture: 1)
uniform vec4 u_islandExtent;   // xy: minimum x/z, zw: maximum x/z

uniform vec4 u_skyAndBump; // x unused, y: bump strength, z: small bump strength (w: vs_terrain)
uniform vec4 u_terrainPass; // x: light scale (0.5 for the mirrored land in the reflection, as the original),
                            // y, w: unused (0), z: static shadow strength (0.5, or 0.25 with low textures)

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

// The light over a block's colour, its texture's or its low-resolution tile's (the footprints, the static shadows,
// the river beds and the snow are baked into both)
vec3 ShadeLand(vec3 colour, vec3 landLight)
{
	return colour * landLight * u_terrainPass.x;
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
	// countries blended, with the footprints, static shadows and river beds baked in at its prints step; the light goes
	// on top.
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

	col.rgb = ShadeLand(col.rgb, v_landLight);
	// the lit land alone, before the small bump and the specular are blended in below
	vec3 landLit = col.rgb;

	// Small bump (render mode 0xE: TEXTURE * DIFFUSE in colour and alpha): a second pass over the lit
	// land, blended SRCALPHA / INVSRCALPHA, 12 repeats per block. Its vertex diffuse (vs_terrain) is white or black
	// with alpha = fade, 0 at the vertices of altitude 1 or less, so it fades out towards the water; the triangles
	// with all three at 0 are skipped by the original, which changes nothing. It does not use the coast alpha.
	vec4 smallBump = texture2D(s2_smallBump, v_texcoord0.xy * 12.0f);
	smallBump.rgb *= v_smallBumpFade.x;
	float bumpAlpha = smallBump.a * v_smallBumpFade.y * smallBumpMapStrength;

	// The land's alpha (render mode 14, SRCALPHA / INVSRCALPHA over the sea already drawn): the coast alpha of the
	// block texture, lowered in the river channels at its prints step. Z is written even where it is 0.
	float landAlpha = u_blockTexture.y > 0.5f ? block.a : 1.0f;

	// D3D specular (SPECULARENABLE): cell colour + haze, added in both the land and the small bump pass. The two
	// passes as one premultiplied colour (blend ONE / INV_SRC_ALPHA): land * a * (1 - b) + bump * b over dst * (1 - a)(1 - b)
	col.rgb = (col.rgb + v_landSpecular) * landAlpha * (1.0f - bumpAlpha) + (smallBump.rgb + v_landSpecular) * bumpAlpha;
	float transmitted = (1.0f - landAlpha) * (1.0f - bumpAlpha);

	// The texture detail line (u_blockTile.x): beyond it the block's low-resolution tile alone, render mode 14 over the
	// sea by the tile's own alpha, with no small bump; across its band the tile over the block's texture (render mode
	// 0xE: SRCALPHA / INVSRCALPHA, the tile's alpha times the vertices' alpha), between the land and the small bump
	// passes. The tile is lit, hazed and has the specular as the block; its alpha holds the river channels as the block's
	if (u_blockTile.x > 0.5f)
	{
		vec4 tile = texture2DArray(s9_lowResPages, vec3(v_texcoord0.zw, u_blockTile.y));
		vec3 tileLit = ShadeLand(tile.rgb, v_landLight) + v_landSpecular;
		if (u_blockTile.x < 1.5f)
		{
			col.rgb = tileLit * tile.a;
			transmitted = 1.0f - tile.a;
		}
		else
		{
			float tileAlpha = tile.a * v_texcoord1.z;
			vec3 land = (landLit + v_landSpecular) * landAlpha;
			land = tileLit * tileAlpha + land * (1.0f - tileAlpha);
			col.rgb = land * (1.0f - bumpAlpha) + (smallBump.rgb + v_landSpecular) * bumpAlpha;
			transmitted = (1.0f - landAlpha) * (1.0f - tileAlpha) * (1.0f - bumpAlpha);
		}
	}
	// (debug) each block tinted by its level: red its tile only, blue both
	if (u_blockTileDebug.x > 0.5f && u_blockTile.x > 0.5f)
	{
		vec3 tint = u_blockTile.x < 1.5f ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f, 0.0f, 1.0f);
		col.rgb = mix(col.rgb, tint, 0.4f * (1.0f - transmitted));
	}

	// The projected shadows are drawn over the block afterwards, one draw per shadow (
	// vs_land_shadow / fs_land_shadow): black, SRCALPHA / INVSRCALPHA over the block, so over the sea seen through it too
	gl_FragColor = vec4(col.rgb, 1.0f - transmitted);
}
