$input a_position, a_texcoord0, a_color0, a_color1
$output v_texcoord0, v_color0, v_texcoord1

#include <bgfx_shader.sh>

// World-space quads with a vertex colour and a specular: the particle sprites lit by the land (UseLandscapeColor)
void main()
{
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	v_color0 = a_color0;
	v_texcoord1 = a_color1;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
