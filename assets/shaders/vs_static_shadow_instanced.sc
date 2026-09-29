$input a_position, a_texcoord0, a_indices, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0

#if BGFX_SHADER_LANGUAGE_HLSL == 3
#define BGFX_CONFIG_MAX_BONES 48
#else
#define BGFX_CONFIG_MAX_BONES 128
#endif

#include <bgfx_shader.sh>

// Static shadow bake (fn_008721A0 / fn_0080ECB0): the object seen from above, each vertex slid along the sun by its
// height over the object's base, x' = x + h and z' = z + h (the sun at (-500000, 500000, -500000): -Lx/Ly = -Lz/Ly = 1),
// drawn into the island-wide shadow texture with the landscape's orthographic view.
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
	model[3] = i_data3;
	vec4 world = instMul(model, mul(u_model[modelIndex], vec4(a_position.xyz, 1.0f)));

	float height = max(0.0f, world.y - i_data3.y);
	vec4 shadow = vec4(world.x + height, 0.0f, world.z + height, 1.0f);

	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	gl_Position = mul(u_viewProj, shadow);
	gl_Position.z = 0.0f;
}
