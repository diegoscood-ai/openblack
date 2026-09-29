$input v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_dynamicShadow, 5);
uniform vec4 u_dynamicShadowBox; // xy: box minimum x/z, zw: 1 / size
uniform vec4 u_dynamicShadow;    // x: opacity (8/15 x fade)

// The hand's dynamic shadow on objects (fn_0080B050 / fn_0084E200): the object drawn again with ZFUNC EQUAL, the
// shadow texture projected straight down (u, v from the world x and z inside the shadow box), render mode 6 (black,
// SRCALPHA / INVSRCALPHA), vertex colour white
void main()
{
	vec2 shadowUv = (v_position.xz - u_dynamicShadowBox.xy) * u_dynamicShadowBox.zw;
	if (any(lessThan(shadowUv, vec2_splat(0.0f))) || any(greaterThan(shadowUv, vec2_splat(1.0f))))
	{
		discard;
	}
#if !BGFX_SHADER_LANGUAGE_GLSL
	shadowUv.y = 1.0f - shadowUv.y; // render target rows start at the top outside OpenGL
#endif
	float alpha = u_dynamicShadow.x * texture2D(s_dynamicShadow, shadowUv).r;
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, alpha);
}
