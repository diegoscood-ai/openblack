$input v_texcoord0, v_color0, v_snowLight

#include <bgfx_shader.sh>

// A particle surface in one pass, as the fixed function draws it with the specular on: the texture times the colour,
// the specular colour (v_snowLight) added after and the sum clamped, the alpha file times the colour's alpha. The blend
// then weighs all of it, the specular included, by that alpha. The specular's own alpha is not read
SAMPLER2D(s_diffuse, 0); // X.raw
SAMPLER2D(s_alpha, 1);   // Xa.raw

void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb * v_color0.rgb + v_snowLight;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r * v_color0.a;
	gl_FragColor = vec4(min(colour, vec3_splat(1.0)), alpha);
}
