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
SAMPLER2D(s_heightmap, 1);
uniform vec4 u_islandExtent;
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
#ifdef USE_HEIGHT_MAP
	vec2 extentMin = u_islandExtent.xy;
	vec2 extentMax = u_islandExtent.zw;
#endif // USE_HEIGHT_MAP

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
	model[3] = i_data3;

	v_position = instMul(model, v_position);
	normal = instMul(model, vec4(normal, 0.0f)).xyz;
#endif // USE_INSTANCING

#ifdef USE_HEIGHT_MAP
#ifdef USE_INSTANCING
	float original_height = i_data3.y;
#else
    float original_height = u_model[modelIndex][3].y;
#endif // USE_INSTANCING
	vec2 blockUv = (v_position.xz - extentMin) / (extentMax - extentMin);
	float terrain_height = texture2DLod(s_heightmap, blockUv, 0.0f).r * 170.85f;
	v_position.y += terrain_height - original_height;
#ifdef USE_INSTANCING
	// The w of the third column carries a vertical offset kept while morphing (piles rising / sinking).
	v_position.y += i_data2.w;
#endif // USE_INSTANCING
#endif // USE_HEIGHT_MAP

	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
#ifdef USE_INSTANCING
	// The w of the second column carries a texture V offset (scrolling food piles).
	v_texcoord0.y += i_data1.w;
#endif // USE_INSTANCING
	vec3 specular = vec3_splat(0.0f);
#ifdef USE_INSTANCING
	vec3 objectColour = vec3_splat(1.0f);
	if (u_objectLight.x > 1.5f && u_objectLight.x < 2.5f)
	{
		objectColour = vec3_splat(u_objectLight.z);
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
		objectColour = min(objectColour * u_objectLight.y, vec3_splat(1.0f));
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
		objectColour *= 90.0f / 256.0f + 166.0f / 256.0f * max(0.0f, dot(normalize(normal), lightDirection));
		}
	}
	v_color0 = vec4(objectColour, 1.0f - fade);
#else
	v_color0 = vec4(1.0f, 1.0f, 1.0f, 1.0f);
#endif // USE_INSTANCING
	v_normal = normal; // not normalised here: the sky mesh has zero normals
	// The specular colour rides in the unused texcoord z/w and position w: vs_object is shared with the sky (fs_sky)
	// and a new varying broke its interface
	gl_Position = mul(u_viewProj, v_position);
	v_texcoord0.zw = specular.rg;
	v_position.w = specular.b;
}
