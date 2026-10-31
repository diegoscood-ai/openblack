// vs_object_instanced.sc for the skinned sub-meshes of the posed villagers and animals of a mesh drawn in one instanced
// draw: each instance's bones come from the bone palette texture (graphics::bone_palette), so only one model matrix is
// uploaded with the draw
#define BGFX_CONFIG_MAX_BONES 1
#define USE_BONE_PALETTE 1
#include "vs_object_instanced.sc"
