$input v_texcoord0, v_color0, v_texcoord1

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // X.raw
SAMPLER2D(s_alpha, 1);   // Xa.raw
uniform vec4 u_alphaTest; // as fs_world_textured's

// fs_world_textured with the vertex specular (v_texcoord1) added to the colour and saturated, alpha untouched: the
// particle sprites lit by the land and hazed (UseLandscapeColor)
void main()
{
	vec3 colour = texture2D(s_diffuse, v_texcoord0.xy).rgb;
	float alpha = texture2D(s_alpha, v_texcoord0.xy).r;
	float tested = alpha * (u_alphaTest.y > 1.5f ? v_color0.a : 1.0f);
	if (u_alphaTest.x >= 0.0f && floor(tested * 255.0f + 0.5f) < floor(u_alphaTest.x * 255.0f + 0.5f))
	{
		discard;
	}
	gl_FragColor = vec4(min(colour * v_color0.rgb + v_texcoord1.rgb, vec3_splat(1.0f)), alpha * v_color0.a);
}
