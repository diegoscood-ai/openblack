$input a_position, a_texcoord0, a_color0
$output v_position, v_texcoord0, v_normal, v_color0

#include <bgfx_shader.sh>

// LH3DTech::Draw3DWorldTriangle 0x81C090 (src/Graphics/WorldTriangles.h): the vertices are already in the world (the
// world matrix is the identity, RenderParticleGJMesh::DrawAt 0x67C8ED..0x67C979) and already lit on the CPU (the land
// light and the model light, 0x67C175..0x67C6B2), so the vertex only goes through the camera. fs_object does the rest,
// as for the models: v_position.w and v_texcoord0.zw are the specular, 0 here (0x81C2BF); v_normal 0 keeps fs_object
// out of the MeshTint branch (y > 500) and u_window.y = 0 out of the per pixel light of the hd-tweaks mod.
void main()
{
	v_position = vec4(a_position.xyz, 0.0f);
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	v_normal = vec3_splat(0.0f);
	v_color0 = a_color0;
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
