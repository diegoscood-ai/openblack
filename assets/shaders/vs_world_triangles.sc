$input a_position, a_texcoord0, a_color0, a_color1
$output v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

// LH3DTech::Draw3DWorldTriangle 0x81C090 and its indexed sibling fn_0081C780 (src/Graphics/WorldTriangles.h): the
// vertices are already in the world (the world matrix is the identity, RenderParticleGJMesh::DrawAt 0x67C8ED..0x67C979;
// FragMesh's fn_007F7ED0 transforms them itself, 0x7F80AE..0x7F8116) and already lit on the CPU (the land light and the
// model light, 0x67C175..0x67C6B2, 0x7F7F5F..0x7F8363), so the vertex only goes through the camera. fs_object does the
// rest, as for the models: v_position.w and v_texcoord0.zw are the specular, a_color1 (0 from Draw3DWorldTriangle,
// 0x81C2BF; fn_0081C780 copies each vertex's, 0x81C9BE..0x81C9C4); v_normal 0 keeps fs_object out of the MeshTint
// branch (y > 500) and u_window.y = 0 out of the per pixel light of the hd-tweaks mod.
void main()
{
	v_position = vec4(a_position.xyz, a_color1.b);
	v_texcoord0 = vec4(a_texcoord0, a_color1.rg);
	v_normal = vec3_splat(0.0f);
	v_color0 = a_color0;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
