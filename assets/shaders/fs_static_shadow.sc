$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_shadowParams; // x: alpha cut-out threshold (0: solid), y: 1 when the primitive has a texture

// Coverage of the static shadow; chroma materials (tree leaves) are alpha tested like the original's textured
// shadow path (DrawTextureShadow)
void main()
{
	if (u_shadowParams.x > 0.0f && u_shadowParams.y > 0.0f)
	{
		float alpha = texture2D(s_diffuse, v_texcoord0.xy).a;
		if (alpha * 255.0f < u_shadowParams.x * 255.0f - 5.0f)
		{
			discard;
		}
	}
	gl_FragColor = vec4_splat(1.0f);
}
