#ifndef SEA_PLANE_SH
#define SEA_PLANE_SH
// The pass under the sea, GPU side of src/Graphics/SeaPass.h (graphics::sea_pass; the two must stay the same).
// The user plane of LH3D (fn_00822560: world [0xF03128], view [0xF03118]; default (0, 1, 0, 0) from the CRT
// fn_0084A380 0x84A390 / fn_0084A3C0 0x84A3D0), the only plane any sea draw uses, read on the REAL world y.
// x: sea_pass::SeaPlane (1 KeepAbove, -1 KeepBelow, 0 None); y: 1 = unmirror (a vt+0x11C draw, which never mirrors
// (fn_00858BA0 0x858C5D..0x858CAE), inside openblack's mirrored Reflection pass); zw unused (sea_pass::PackClip)
uniform vec4 u_objectClip;

// Code 0x800: DrawUnderWater's "d > 0 out" on the mirrored point (fn_00850FC0 0x85113C..0x851149) and DrawCutByPlane's
// "d < 0 out" (fn_00858BA0 0x858D49..0x858D80) both become "keep the side the plane says" on the real y
// (sea_pass::Kept). (aproximado) strict per pixel: y = 0 is kept by both; the original clips triangles (fn_0081D2C0)
bool SeaPlaneDiscard(float worldY)
{
	return (u_objectClip.x > 0.5f && worldY < 0.0f) || (u_objectClip.x < -0.5f && worldY > 0.0f);
}

// Back to the real position of a vertex drawn through the mirrored reflection camera (ReflectionXZCamera): the mirror
// of fn_00850FC0's fsubp (0x851094) undone (sea_pass::Unmirror)
vec4 SeaUnmirror(vec4 worldPos)
{
	return u_objectClip.y > 0.5f ? vec4(worldPos.x, -worldPos.y, worldPos.zw) : worldPos;
}
#endif // SEA_PLANE_SH
