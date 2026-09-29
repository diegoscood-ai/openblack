$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // human_shadow.raw (8-bit; the original keeps byte & 0xF0 as the alpha of a black texel)

// Render mode 6: colour = texture x diffuse (black), alpha = texture alpha x diffuse alpha, SRCALPHA / INVSRCALPHA
void main()
{
	float alpha = floor(texture2D(s_diffuse, v_texcoord0.xy).r * 15.0f + 0.001f) / 15.0f;
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, alpha * v_color0.a);
}
