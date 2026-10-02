$input v_texcoord0, v_shoreFade

#include <bgfx_shader.sh>

// the shadow's texture (si+0x45C): alpha n / 15, CLAMP; register 11, past the terrain's 0..10 (RendererShadows.cpp)
SAMPLER2D(s_shadow, 11);

// fn_00878350, render mode 6 (the material si+0x460, CreateMaterial(6) 0x87FE12): texture (black) x diffuse, blended
// SRCALPHA / INVSRCALPHA over the block. The original drops the triangles whose three vertices have the code 0x400
// (fn_00875C60); here the fragments of the triangles with that code at the three vertices (z = 0 all over them)
// (aproximado: a hairline of a kept triangle's edge between two coded vertices goes too)
void main()
{
	if (v_texcoord0.z <= 0.0f)
	{
		discard;
	}
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, texture2D(s_shadow, v_texcoord0.xy).r * v_shoreFade);
}
