$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1); // the texture's "<name>a.raw" alpha, when u_spriteAlpha.x is 1
uniform vec4 u_tint;
// x: 1 when s_diffuse is a texture of colours with its alpha in s_alpha, 0 when it is a single channel of alpha that is
// also the sprite's brightness; y: 1 for an added sprite (its colour is added times its alpha), 0 for one blended
// premultiplied; z: when above 0, a solid sprite drawn only where its alpha reaches z, in its colours times the tint
uniform vec4 u_spriteAlpha;

void main()
{
	vec4 texel = texture2D(s_diffuse, v_texcoord0.xy);
	if (u_spriteAlpha.z > 0.0)
	{
		float solidAlpha = texture2D(s_alpha, v_texcoord0.xy).r * u_tint.a;
		if (solidAlpha < u_spriteAlpha.z)
		{
			discard;
		}
		gl_FragColor = vec4(texel.rgb * u_tint.rgb, solidAlpha);
		return;
	}
	if (u_spriteAlpha.x > 0.5)
	{
		float alpha = texture2D(s_alpha, v_texcoord0.xy).r;
		texel = u_spriteAlpha.y > 0.5 ? vec4(texel.rgb, alpha) : vec4(texel.rgb * alpha, alpha);
	}
	else
	{
		texel = texel.rrrr;
	}
	gl_FragColor = texel * u_tint;
}
