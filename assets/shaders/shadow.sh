#ifndef SHADOW_SH
#define SHADOW_SH

// The projected shadows' texture coordinates (graphics::shadow_list, wiki: rendering.md, "Sombras proyectadas"). The
// texture is the CPU's 32 x 32 alpha n / 15 with the fade baked in and an empty outer ring, sampled with CLAMP
// (material +5 = 0, 0x87864D), so outside the box it gives 0.

// fn_00878350 0x8784A7..0x8784EC, the land: u = (Lx + (x - Lx) t' - x0) / (x1 - x0), v the same in z.
// light: xyz si+0x444, w t' (one per shadow); box: x0, z0, 1 / (x1 - x0), 1 / (z1 - z0)
vec2 LandShadowUv(vec2 xz, vec4 light, vec4 box)
{
	return (light.xz + (xz - light.xz) * light.w - box.xy) * box.zw;
}

// fn_00880770 (fn_0084E200), the objects: straight down, u = (x - x0) / (x1 - x0), v the same in z
vec2 ObjectShadowUv(vec2 xz, vec4 box)
{
	return (xz - box.xy) * box.zw;
}

// The code 0x400 of a vertex (0x878502 on the land, fn_0084E200 on the objects): k = x d.x + z d.z below si+0x440
// (d = si+0x450 / 0x458, the caster minus the light). 1 kept, 0 coded. cull: x, y d.x, d.z, z the least k
float ShadowKept(vec2 xz, vec4 cull)
{
	return xz.x * cull.x + xz.y * cull.y < cull.z ? 0.0f : 1.0f;
}

#endif // SHADOW_SH
