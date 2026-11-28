$input v_texcoord0, v_color0, v_landSpecular

#include <bgfx_shader.sh>

// The sheet's colour and its alpha beside it, tinted by the particle's colour and alpha, then the colour added after
// the texture (none unless the vertex program adds one); blended as the particle's creator asks, added to what is
// behind or over it
SAMPLER2D(s_diffuse, 0);
SAMPLER2D(s_alpha, 1);

void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r;
	gl_FragColor = vec4(colour * v_color0.rgb + v_landSpecular, alpha * v_color0.a);
}
