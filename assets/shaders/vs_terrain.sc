$input a_position, a_texcoord1, a_color1, a_color2, a_texcoord2, a_color0, a_color3, a_normal
$output v_normal, v_texcoord0, v_texcoord1, v_weight, v_materialID0, v_materialID1, v_materialBlend, v_lightLevel, v_shoreFade, v_distToCamera, v_smallBumpFade, v_landLight, v_landSpecular, v_worldXZ, v_worldY

#include <bgfx_shader.sh>

#if BGFX_SHADER_LANGUAGE_HLSL > 300 || BGFX_SHADER_LANGUAGE_SPIRV
#   define materialIdFix(x) (floatBitsToInt(x))
#else
#   define materialIdFix(x) (ivec4(x))
#endif

SAMPLER2D(s4_landLight, 4);
SAMPLER2D(s6_cloudShadow, 6); // cloud shadow luminosity cap per cell (Clouds::BuildShadowCap), same layout as the cells

uniform vec4 u_blockPositionAndSize;
uniform vec4 u_islandExtent;
uniform vec4 u_skyAndBump;    // w: distance of the small bump fade line ahead of the camera
uniform vec4 u_smallBumpLine; // xy: camera x/z, zw: normalised horizontal camera forward
uniform vec4 u_haze;          // x: near, y: far, z: k, w: on ("Fog" detail key)
uniform vec4 u_hazeColour;    // rgb: fog colour 0..255

void main()
{
	// Unpack
	vec2 blockPosition = u_blockPositionAndSize.xy;
	vec2 blockSize = u_blockPositionAndSize.zw;
	vec2 extentMin = u_islandExtent.xy;
	vec2 extentMax = u_islandExtent.zw;

	v_texcoord0 = vec4(a_position.zx / blockSize.yx, 0.0f, 0.0f);
	vec2 blockStartUv = (blockPosition + a_position.xz - extentMin) / (extentMax - extentMin);
	#if !BGFX_SHADER_LANGUAGE_GLSL
		blockStartUv.y = 1.0f - blockStartUv.y;
	#endif
	v_texcoord1 = vec4(blockStartUv, 0.0f, 0.0f);
	v_weight = a_texcoord1;
	v_materialID0 = materialIdFix(a_color1);
	v_materialID1 = materialIdFix(a_color2);
	v_materialBlend = a_texcoord2;
	v_lightLevel = a_color0.x;
	// Vertex diffuse = landscape light table[luminosity] (fn_00874AA0), interpolated across the triangle like D3D
	vec2 cellIndex = floor((blockPosition + a_position.xz - u_islandExtent.xy) * 0.1f + 0.5f);
	vec2 cellMapSize = (u_islandExtent.zw - u_islandExtent.xy) * 0.1f + 1.0f;
	float cloudCap = texture2DLod(s6_cloudShadow, (cellIndex + 0.5f) / cellMapSize, 0.0f).r;
	float luminosity = min(a_color0.x, cloudCap);
	v_landLight = texture2DLod(s4_landLight, vec2((floor(luminosity * 255.0f + 0.5f) + 0.5f) / 256.0f, 0.5f), 0.0f).rgb;
	v_shoreFade = a_color3; // 0 at altitude 1 or less: no small bump there, dynamic shadows fade out

	vec3 transformedPosition = vec3(a_position.x + blockPosition.x, a_position.y, a_position.z + blockPosition.y);

	// Small bump fade, per vertex like the original's Gouraud alpha (fn_007A1800)
	float forwardDistance = dot(transformedPosition.xz - u_smallBumpLine.xy, u_smallBumpLine.zw);
	v_smallBumpFade = clamp((u_skyAndBump.w - forwardDistance + 20.0f) / 40.0f, 0.0f, 1.0f);

	v_normal = a_normal;
	v_worldXZ = transformedPosition.xz;
	v_worldY = transformedPosition.y;
	vec4 cs_position = mul(u_view, vec4(transformedPosition, 1.0f));
	v_distToCamera = cs_position.z;

	// Distance haze per vertex (fn_00874AA0 0x874C5B / fn_007A1800), with the view depth along the camera axis
	float hazeT = u_haze.w * saturate((cs_position.z - u_haze.x) / (u_haze.y - u_haze.x));
	v_landLight *= (256.0f - floor((256.0f - u_haze.z) * hazeT)) / 256.0f;
	v_landSpecular = min(a_color0.yzw * 255.0f + floor(u_hazeColour.rgb * hazeT + 0.5f), vec3_splat(255.0f)) / 255.0f;
	gl_Position = mul(u_proj, cs_position);
}
