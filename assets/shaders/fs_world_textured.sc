$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // X.raw
SAMPLER2D(s_alpha, 1);   // Xa.raw
uniform vec4 u_alphaTest; // x: ALPHAREF / 255 of an alpha tested material, -1 without the test,
                          // y: stage 0 alpha (render_modes::PrimitiveAlpha): 2 texture x diffuse, else the texture's

// Textured world quads in render mode 6 (LH3DSprite, e.g. the fish farm shoals): colour = texture x diffuse,
// alpha = texture alpha x diffuse alpha. In an alpha tested mode a pixel whose alpha byte is below ALPHAREF is dropped
// (ALPHAFUNC GREATEREQUAL on the alpha stage 0 outputs: the texture's in mode 9, the texture's times the diffuse's in
// mode 15; compared as a byte, rounded, as in fs_object)
void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r;
	float tested = alpha * (u_alphaTest.y > 1.5f ? v_color0.a : 1.0f);
	if (u_alphaTest.x >= 0.0f && floor(tested * 255.0f + 0.5f) < floor(u_alphaTest.x * 255.0f + 0.5f))
	{
		discard;
	}
	gl_FragColor = vec4(colour * v_color0.rgb, alpha * v_color0.a);
}
