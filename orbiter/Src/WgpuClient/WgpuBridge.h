#pragma once
#include <stddef.h>
#include <stdint.h>
#include "NativeWindow.h"

#ifdef __cplusplus
extern "C" {
#endif
// Opaque Rust renderer. Calls are serialized on the simulation/render thread.
typedef struct OrbGpu OrbGpu;
typedef struct OrbFill {
    int32_t x, y;
    uint32_t width, height;
    uint8_t rgba[4];
} OrbFill;
// source=0 denotes a solid fill; flags bit 0 enables source RGB colour key.
// Sequence numbers preserve order across batches targeting different surfaces.
typedef struct OrbDraw {
    uint64_t sequence, source;
    int32_t x, y;
    uint32_t width, height;
    int32_t src_x, src_y;
    uint32_t src_width, src_height;
    uint32_t color, key, flags, reserved; // color/key are 0x00RRGGBB
} OrbDraw;
#ifdef __cplusplus
static_assert(sizeof(OrbDraw) == 64 && offsetof(OrbDraw, flags) == 56, "Rust draw ABI layout");
#endif
int orb_gpu_queue_batch(OrbGpu *gpu, uint64_t target, const OrbDraw *commands, size_t count);
int orb_gpu_flush(OrbGpu *gpu);
typedef struct OrbMeshVertex { float position[3], normal[3], uv[2], color[4], light[4], emission[4], specular[4]; } OrbMeshVertex;
#ifdef __cplusplus
static_assert(sizeof(OrbMeshVertex)==96 && offsetof(OrbMeshVertex,specular)==80,"Rust mesh vertex ABI");
#endif
typedef struct OrbStar {float direction[3], color[3], radius;} OrbStar;
#ifdef __cplusplus
static_assert(sizeof(OrbStar)==28,"Rust star ABI");
#endif
typedef struct OrbParticle {float center[4],extent[2],color[4];uint32_t sprite,padding;uint64_t texture;} OrbParticle;
#ifdef __cplusplus
static_assert(sizeof(OrbParticle)==56,"Rust particle ABI");
#endif
int orb_gpu_particles_upload(OrbGpu *gpu,const OrbParticle *particles,size_t count);
int orb_gpu_stars_upload(OrbGpu *gpu,const OrbStar *stars,size_t count);
int orb_gpu_stars_camera(OrbGpu *gpu,const float *rotation_projection);
// Galactic ray right/up/forward vec4s plus intensity vec4. Texture 0 disables.
// values[13]: 0/3 = whole sphere; 5..8 = legacy level in 32-column, 256px tile atlas.
int orb_gpu_background(OrbGpu *gpu,uint64_t texture,const float *values);
// Same camera layout; additive star-image layer with independent lifetime.
int orb_gpu_star_image(OrbGpu *gpu,uint64_t texture,const float *values);
// Five vec4s: ray right/up/forward, planet-relative eye/radius (km), Sun direction/outer radius.
// right.w=1 disables sky haze; up.w=1 disables haze over opaque surfaces.
// Requires infinite reversed-Z mesh projection with a 0.01 m near plane.
// Null disables sky and depth-limited atmospheric haze.
int orb_gpu_atmosphere(OrbGpu *gpu,const float *values);
uint64_t orb_gpu_mesh_create(OrbGpu *gpu, const OrbMeshVertex *vertices, size_t nvertices, const uint32_t *indices, size_t nindices);
// clamp_texture is 1 for independent terrain images, 0 for repeating mesh UVs.
// alpha_blend is 1 for transparent layers; 0 retains opaque alpha-cutout meshes.
// shadow_only keeps exterior hulls out of cockpit colour/depth while casting.
// sort_center is a local-space centre for transparent ordering, defaulting to
// the mesh origin. It permits separate flames in one batch to straddle smoke.
typedef struct OrbMeshGroup { uint32_t first, count; uint64_t texture, emission, clamp_texture, alpha_blend, shadow_only; float sort_center[3],reserved; } OrbMeshGroup;
#ifdef __cplusplus
static_assert(sizeof(OrbMeshGroup)==64,"Rust mesh group ABI");
#endif
int orb_gpu_mesh_groups(OrbGpu *gpu, uint64_t mesh, const OrbMeshGroup *groups, size_t count);
int orb_gpu_mesh_update(OrbGpu *gpu, uint64_t mesh, const OrbMeshVertex *vertices, size_t count);
typedef struct OrbMeshRange { uint32_t first, count; } OrbMeshRange;
#ifdef __cplusplus
static_assert(sizeof(OrbMeshRange)==8,"Rust mesh range ABI");
#endif
int orb_gpu_mesh_update_ranges(OrbGpu *gpu, uint64_t mesh, const OrbMeshVertex *vertices, size_t count, const OrbMeshRange *ranges, size_t range_count);
int orb_gpu_mesh_release(OrbGpu *gpu, uint64_t mesh);
// Column-major MVP; translation must be made camera-relative before float conversion.
// Vessel-local position/range; direction/outer cosine; attenuation/inner cosine.
// Diffuse.w is 1 for spotlights, 0 for point lights. Zero range disables a slot.
#define ORB_MAX_LOCAL_LIGHTS 8
typedef struct OrbLocalLight {float position[4],direction[4],attenuation[4],diffuse[4],ambient[4];} OrbLocalLight;
// shadow_mvp maps local vertices into the focused sunlight shadow volume.
// shadow[0] enables casting/receiving; shadow[1] is receiver depth bias;
// shadow[2] > 1 enables an outer volume enlarged by that factor.
// eye.xyz is vessel-local camera position; eye.w enables material specularity.
// Vertex specular.rgb is reflection colour, specular.w is Phong power.
typedef struct OrbMeshDraw {uint64_t mesh;float mvp[16];float sunlight[4];OrbLocalLight lights[ORB_MAX_LOCAL_LIGHTS];float shadow_mvp[16],shadow[4],eye[4];} OrbMeshDraw;
#ifdef __cplusplus
static_assert(sizeof(OrbLocalLight)==80 && sizeof(OrbMeshDraw)==824 && offsetof(OrbMeshDraw,sunlight)==72,"Rust scene draw ABI");
#endif
int orb_gpu_mesh_render_scene(OrbGpu *gpu,uint64_t target,const OrbMeshDraw *draws,size_t count);
int orb_gpu_mesh_render(OrbGpu *gpu, uint64_t target, uint64_t mesh, const float *mvp);
// ImGui vertices use NDC positions and RGBA bytes; scissors are framebuffer pixels.
typedef struct OrbUiVertex {float position[2],uv[2];uint8_t color[4];} OrbUiVertex;
typedef struct OrbUiCommand {uint64_t texture;uint32_t first,count;int32_t base_vertex;uint32_t clip[4],reserved;} OrbUiCommand;
#ifdef __cplusplus
static_assert(sizeof(OrbUiVertex)==20 && sizeof(OrbUiCommand)==40 && offsetof(OrbUiCommand,clip)==20,"Rust ImGui ABI");
#endif
int orb_gpu_ui_render(OrbGpu *gpu,uint64_t target,const OrbUiVertex *vertices,size_t vertex_count,
    const uint32_t *indices,size_t index_count,const OrbUiCommand *commands,size_t command_count);
OrbGpu *orb_gpu_create(void);
// Host must keep native handles alive until detach, on the main thread.
int orb_gpu_window_attach(OrbGpu *gpu, const OrbNativeWindow *window);
void orb_gpu_window_detach(OrbGpu *gpu);
int orb_gpu_window_resize(OrbGpu *gpu, uint32_t width, uint32_t height);
int orb_gpu_present(OrbGpu *gpu, uint64_t surface);
void orb_gpu_destroy(OrbGpu *gpu);
// Error text belongs to the calling thread and remains valid until its next call.
const char *orb_gpu_error(void);
uint64_t orb_gpu_dds_create(OrbGpu *gpu, const uint8_t *data, size_t size);
// Complete missing power-of-two BC1/RGBA mip levels; preserve supplied levels.
uint64_t orb_gpu_dds_create_mipped(OrbGpu *gpu, const uint8_t *data, size_t size);
uint64_t orb_gpu_surface_create(OrbGpu *gpu, uint32_t width, uint32_t height);
int orb_gpu_surface_release(OrbGpu *gpu, uint64_t surface);
int orb_gpu_surface_make_writable(OrbGpu *gpu, uint64_t surface);
int orb_gpu_surface_upload(OrbGpu *gpu, uint64_t surface, const uint8_t *rgba, size_t size, uint32_t pitch);
// One call for all recorded fills targeting this surface; order is preserved.
int orb_gpu_surface_fill_batch(OrbGpu *gpu, uint64_t surface, const OrbFill *fills, size_t count);
int orb_gpu_surface_read(OrbGpu *gpu, uint64_t surface, uint8_t *rgba, size_t size, uint32_t pitch);
#ifdef __cplusplus
}
#endif
