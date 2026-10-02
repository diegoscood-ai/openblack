$input v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ, v_worldY

#include <bgfx_shader.sh>

#define M_PI 3.1415926535897932384626433832795

SAMPLER2DARRAY(s0_materials, 0);
SAMPLER2D(s1_bump, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s3_footprints, 3);
SAMPLER2D(s5_staticShadow, 5);
SAMPLER2D(s8_landAlpha, 8); // 1, or lower in the river channels (the sea drawn before the land shows through)
SAMPLER2D(s10_blockTexture, 10); // the original's block textures (BlockTexture.h), RGBA8 of ARGB4444, rows along +z
uniform vec4 u_blockTexture;      // x: 1 = colour from it (else the per-vertex materials of the terrain mods),
                                  // y: 1 = its alpha is the coast alpha (else no block texture: 1)
uniform vec4 u_islandExtent;   // xy: minimum x/z, zw: maximum x/z

uniform vec4 u_skyAndBump; // x unused, y: bump strength, z: small bump strength (w: vs_terrain)
uniform vec4 u_terrainPass; // x: light scale (0.5 for the mirrored land in the reflection, like fn_007FF4F0),
                            // y: material repeats per block (1 in the original; terrain-x2 mod; pictures stay at 1),
                            // z: static shadow strength (0.5, or 0.25 with low textures),
                            // w: 1 = cliffs projected from the side too (triplanar; terrain-x2 mod)

// The three corner materials of the triangle (terrain mods only), each a blend of two: the first one weighs the
// coefficient and the second the rest (fn_008732C0 0x873438); uv spans one block, repeat0/1 per material
vec4 SampleMaterials(vec2 uv, vec3 id0, vec3 id1, vec3 repeat0, vec3 repeat1, vec3 blend, vec3 weight)
{
	return mix(texture2DArray(s0_materials, vec3(uv * repeat1.r, id1.r)),
	           texture2DArray(s0_materials, vec3(uv * repeat0.r, id0.r)), blend.r) * weight.r +
	       mix(texture2DArray(s0_materials, vec3(uv * repeat1.g, id1.g)),
	           texture2DArray(s0_materials, vec3(uv * repeat0.g, id0.g)), blend.g) * weight.g +
	       mix(texture2DArray(s0_materials, vec3(uv * repeat1.b, id1.b)),
	           texture2DArray(s0_materials, vec3(uv * repeat0.b, id0.b)), blend.b) * weight.b;
}

// Repeats per block of the three materials: the mod's count, or 1 for pictures (bits 0-2 of the id's w)
vec3 MaterialRepeats(float bits)
{
	vec3 single = mod(floor(vec3_splat(bits) / vec3(1.0f, 2.0f, 4.0f)), 2.0f);
	return mix(vec3_splat(u_terrainPass.y), vec3_splat(1.0f), single);
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
	// countries blended (fn_008732C0 / fn_00871850); the footprints, static shadows and light go on top as before.
	vec4 col = vec4(block.rgb, 1.0f);
	if (u_blockTexture.x < 0.5f)
	{
		// the terrain mods: each vert with both materials
		vec3 id0 = vec3(v_materialID0.xyz);
		vec3 id1 = vec3(v_materialID1.xyz);
		vec3 repeat0 = MaterialRepeats(float(v_materialID0.w));
		vec3 repeat1 = MaterialRepeats(float(v_materialID1.w));
		col = SampleMaterials(v_texcoord0.xy, id0, id1, repeat0, repeat1, v_materialBlend, v_weight);

		// Mod: on steep faces the top-down projection stretches the texture into streaks; blend in the materials
		// projected along x and z (world units, one repeat per block like the top projection)
		if (u_terrainPass.w > 0.5f)
		{
			vec3 axis = abs(normalize(v_normal));
			vec3 projection = axis * axis;
			projection = projection * projection;
			projection /= projection.x + projection.y + projection.z;
			vec2 uvX = vec2(v_worldXZ.y, -v_worldY) / 160.0f;
			vec2 uvZ = vec2(v_worldXZ.x, -v_worldY) / 160.0f;
			vec4 alongX = SampleMaterials(uvX, id0, id1, repeat0, repeat1, v_materialBlend, v_weight);
			vec4 alongZ = SampleMaterials(uvZ, id0, id1, repeat0, repeat1, v_materialBlend, v_weight);
			col = col * projection.y + alongX * projection.x + alongZ * projection.z;
		}

		// apply bump map (2x because it's half bright?)
		float bump = mix(1.0f, texture2D(s1_bump, v_texcoord0.xy).r * 2.0f, bumpMapStrength);
		col = col * bump;
	}

	vec4 footprints = texture2D(s3_footprints, v_texcoord1.xy);
	col.rgb = mix(col.rgb, footprints.rgb, footprints.a);

	// Static object shadows, baked into the block texture in the original (after the footprints)
	col.rgb *= 1.0f - u_terrainPass.z * texture2D(s5_staticShadow, v_texcoord1.xy).r;

	// apply light map
	col.rgb = col.rgb * v_landLight * u_terrainPass.x;

	// Small bump (render mode 0xE, fn_0082DD90: TEXTURE * DIFFUSE in colour and alpha): a second pass over the lit
	// land, blended SRCALPHA / INVSRCALPHA, 12 repeats per block. Its vertex diffuse (vs_terrain) is white or black
	// with alpha = fade, 0 at the vertices of altitude 1 or less, so it fades out towards the water; the triangles
	// with all three at 0 are skipped (SSE 0x7A31A0), which changes nothing. It does not use the coast alpha.
	vec4 smallBump = texture2D(s2_smallBump, v_texcoord0.xy * 12.0f);
	smallBump.rgb *= v_smallBumpFade.x;
	float bumpAlpha = smallBump.a * v_smallBumpFade.y * smallBumpMapStrength;

	// The land's alpha (render mode 14, SRCALPHA / INVSRCALPHA over the sea already drawn): the coast alpha of the
	// block texture (fn_008732C0), lowered by the rivers with min (fn_00872AB0). Z is written even where it is 0.
	float coastAlpha = u_blockTexture.y > 0.5f ? block.a : 1.0f;
	float landAlpha = min(coastAlpha, texture2D(s8_landAlpha, v_texcoord1.xy).r);

	// D3D specular (SPECULARENABLE): cell colour + haze, added in both the land and the small bump pass. The two
	// passes as one premultiplied colour (blend ONE / INV_SRC_ALPHA): land * a * (1 - b) + bump * b over dst * (1 - a)(1 - b)
	col.rgb = (col.rgb + v_landSpecular) * landAlpha * (1.0f - bumpAlpha) + (smallBump.rgb + v_landSpecular) * bumpAlpha;
	float transmitted = (1.0f - landAlpha) * (1.0f - bumpAlpha);

	// The projected shadows are drawn over the block afterwards, one draw per shadow (fn_007FF610 -> fn_00878350,
	// vs_land_shadow / fs_land_shadow): black, SRCALPHA / INVSRCALPHA over the block, so over the sea seen through it too
	gl_FragColor = vec4(col.rgb, 1.0f - transmitted);
}
