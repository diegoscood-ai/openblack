$input v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_waterAlpha, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ, v_worldY

#include <bgfx_shader.sh>

#define M_PI 3.1415926535897932384626433832795

SAMPLER2DARRAY(s0_materials, 0);
SAMPLER2D(s1_bump, 1);
SAMPLER2D(s2_smallBump, 2);
SAMPLER2D(s3_footprints, 3);
SAMPLER2D(s5_staticShadow, 5);
SAMPLER2D(s7_dynamicShadow, 7);
uniform vec4 u_dynamicShadowBox; // xy: box minimum x/z, zw: 1 / size
uniform vec4 u_dynamicShadow;    // x: opacity (8/15 x fade), y: the silhouette's plane height

uniform vec4 u_skyAndBump;
uniform vec4 u_terrainPass; // x: light scale (0.5 for the mirrored land in the reflection, like fn_007FF4F0),
                            // y: material repeats per block (1 in the original; terrain-x2 mod; pictures stay at 1),
                            // z: static shadow strength (0.5, or 0.25 with low textures),
                            // w: 1 = cliffs projected from the side too (triplanar; terrain-x2 mod)

// The three corner materials of the triangle, each a blend of two; uv spans one block, repeat0/1 per material
vec4 SampleMaterials(vec2 uv, vec3 id0, vec3 id1, vec3 repeat0, vec3 repeat1, vec3 blend, vec3 weight)
{
	return mix(texture2DArray(s0_materials, vec3(uv * repeat0.r, id0.r)),
	           texture2DArray(s0_materials, vec3(uv * repeat1.r, id1.r)), blend.r) * weight.r +
	       mix(texture2DArray(s0_materials, vec3(uv * repeat0.g, id0.g)),
	           texture2DArray(s0_materials, vec3(uv * repeat1.g, id1.g)), blend.g) * weight.g +
	       mix(texture2DArray(s0_materials, vec3(uv * repeat0.b, id0.b)),
	           texture2DArray(s0_materials, vec3(uv * repeat1.b, id1.b)), blend.b) * weight.b;
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
	float skyType = u_skyAndBump.x;
	float bumpMapStrength = u_skyAndBump.y;
	float smallBumpMapStrength = u_skyAndBump.z;

	// do each vert with both materials
	vec3 id0 = vec3(v_materialID0.xyz);
	vec3 id1 = vec3(v_materialID1.xyz);
	vec3 repeat0 = MaterialRepeats(float(v_materialID0.w));
	vec3 repeat1 = MaterialRepeats(float(v_materialID1.w));
	vec4 col = SampleMaterials(v_texcoord0.xy, id0, id1, repeat0, repeat1, v_materialBlend, v_weight);

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

	vec4 footprints = texture2D(s3_footprints, v_texcoord1.xy);
	col.rgb = mix(col.rgb, footprints.rgb, footprints.a);

	// Static object shadows, baked into the block texture in the original (after the footprints)
	col.rgb *= 1.0f - u_terrainPass.z * texture2D(s5_staticShadow, v_texcoord1.xy).r;

	// apply light map
	float skyBightness = skyType / 2.0f;
	col.rgb = col.rgb * v_landLight * u_terrainPass.x;

	// Small bump (render mode 0xE, fn_0082DD90): a second pass over the lit land, blended SRCALPHA / INVSRCALPHA
	// with the unlit texture colour (vertex diffuse is white) and alpha = smallbumpa * fade; 12 repeats per block.
	vec4 smallBump = texture2D(s2_smallBump, v_texcoord0.xy * 12.0f);
	col.rgb = mix(col.rgb, smallBump.rgb, smallBump.a * v_smallBumpFade * smallBumpMapStrength);

	// D3D specular (SPECULARENABLE): cell colour + haze, added after both the land and the small bump pass
	col.rgb += v_landSpecular;

	// Dynamic shadow (fn_00878350, render mode 6: black, SRCALPHA / INVSRCALPHA over the drawn block): draped
	// vertically inside its box, none on sea-level cells
	if (u_dynamicShadow.x > 0.0f)
	{
		vec2 shadowUv = (v_worldXZ - u_dynamicShadowBox.xy) * u_dynamicShadowBox.zw;
		#if !BGFX_SHADER_LANGUAGE_GLSL
			shadowUv.y = 1.0f - shadowUv.y; // render target rows start at the top outside OpenGL
		#endif
		if (all(greaterThanEqual(shadowUv, vec2_splat(0.0f))) && all(lessThanEqual(shadowUv, vec2_splat(1.0f))) &&
		    v_worldY > 0.67f)
		{
			col.rgb *= 1.0f - u_dynamicShadow.x * texture2D(s7_dynamicShadow, shadowUv).r;
		}
	}

	gl_FragColor = vec4(col.rgb, v_waterAlpha);

	//gl_FragColor.r = v_distToCamera / 200.0f;

	if (v_waterAlpha == 0.0f) {
		discard;
	}
}
