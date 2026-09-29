$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0); // the silhouettes, 4 x 2 subsamples per texel
uniform vec4 u_resolve;  // x: shadow texels per side of the atlas, y: texels per shadow (32)

// fn_00880FC0: each texel's alpha is the number of its 8 covered subsamples / 15 (table 0xFA95C4, ARGB4444); the
// outer ring of each 32 x 32 texture is never written. Coordinates follow the clip space the silhouettes were drawn in.
void main()
{
	vec2 texel = floor(v_texcoord0.xy * u_resolve.x);
	vec2 local = mod(texel, u_resolve.y);
	float covered = 0.0f;
	if (all(greaterThanEqual(local, vec2_splat(1.0f))) && all(lessThan(local, vec2_splat(u_resolve.y - 1.0f))))
	{
		vec2 size = vec2(4.0f, 2.0f) * u_resolve.x;
		for (int j = 0; j < 2; ++j)
		{
			for (int i = 0; i < 4; ++i)
			{
				vec2 uv = (texel * vec2(4.0f, 2.0f) + vec2(float(i), float(j)) + 0.5f) / size;
				#if !BGFX_SHADER_LANGUAGE_GLSL
					uv.y = 1.0f - uv.y; // render target rows start at the top outside OpenGL
				#endif
				covered += step(0.5f, texture2DLod(s_diffuse, uv, 0.0f).r);
			}
		}
	}
	gl_FragColor = vec4_splat(covered / 15.0f);
}
