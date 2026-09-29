$input a_position, a_texcoord0, a_indices, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0

#if BGFX_SHADER_LANGUAGE_HLSL == 3
#define BGFX_CONFIG_MAX_BONES 48
#else
#define BGFX_CONFIG_MAX_BONES 128
#endif

#include <bgfx_shader.sh>

uniform vec4 u_shadowLight; // xyz: light position, w: y0 (the plane the silhouette is projected onto)
uniform vec4 u_shadowBox;   // xy: projected box minimum x/z, zw: 1 / box width, 1 / box depth
uniform vec4 u_shadowSlot;  // xy: the slot's corner in the target, z: its size (0, 0, 1: the whole target)

// Dynamic shadow silhouette (fn_00806F60 / fn_00850900): each vertex projected from the light point onto the plane
// y = y0, s = (y0 - Ly) / (y - Ly), then fitted into the shadow texture over the projected box
void main()
{
#if BGFX_SHADER_LANGUAGE_HLSL > 300 || BGFX_SHADER_LANGUAGE_PSSL || BGFX_SHADER_LANGUAGE_SPIRV
	uint modelIndex = uint(max(0, asint(a_indices.x)));
#else
	uint modelIndex = uint(max(0, a_indices.x));
#endif
	mat4 model;
	model[0] = vec4(i_data0.xyz, 0.0f);
	model[1] = vec4(i_data1.xyz, 0.0f);
	model[2] = vec4(i_data2.xyz, 0.0f);
	model[3] = vec4(i_data3.xyz, 1.0f); // w: the window light of houses (vs_object)
	vec3 world = instMul(model, mul(u_model[modelIndex], vec4(a_position.xyz, 1.0f))).xyz;

	vec3 light = u_shadowLight.xyz;
	float s = (u_shadowLight.w - light.y) / min(world.y - light.y, -0.001f);
	vec2 projected = light.xz + (world.xz - light.xz) * s;
	vec2 uv = u_shadowSlot.xy + (projected - u_shadowBox.xy) * u_shadowBox.zw * u_shadowSlot.z;

	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
