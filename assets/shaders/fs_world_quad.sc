$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // X.raw
SAMPLER2D(s_alpha, 1);   // Xa.raw

// Textured world quads in render mode 6 (LH3DSprite, e.g. the fish farm shoals): colour = texture x diffuse,
// alpha = texture alpha x diffuse alpha
void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r;
	gl_FragColor = vec4(colour * v_color0.rgb, alpha * v_color0.a);
}
