$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // Data\Textures\smoke.raw
SAMPLER2D(s_alpha, 1);   // smokea.raw

// The mist material [0xEA1ABC] (fn_0080BBD0): render mode 6, colour = texture x diffuse, alpha = texture alpha x
// diffuse alpha, SRCALPHA / INVSRCALPHA, two-sided
void main()
{
	vec4 texel = vec4(texture2D(s_diffuse, v_texcoord0.xy).rgb, texture2D(s_alpha, v_texcoord0.xy).r);
	gl_FragColor = texel * v_color0;
}
