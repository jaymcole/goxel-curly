# Goxel Rendering and Tile System - Technical Notes

## Overview
This document captures critical learnings about Goxel's rendering pipeline, tile system, and custom model implementation. These notes are essential for understanding how voxel rendering works and where model_id affects rendering.

---

## 1. Tile System Architecture

### Hash-Based Sparse Storage
- **Location**: `src/volume.c`
- Goxel uses a sparse tile-based storage system
- Tiles are 16³ voxel blocks stored in a hash table
- **Critical**: Only tiles that exist in the hash table are rendered
- Empty regions don't create tiles, saving memory

### Tile Materialization
- Tiles are "materialized" (added to hash) when:
  1. Voxels are placed in that region via `volume_set_at()`
  2. Iterator with `VOLUME_ITER_INCLUDES_NEIGHBORS` is used
  3. Volume operations access that region

### Why This Matters for Rendering
- `render_custom_models()` iterates over tiles in the hash
- If a tile doesn't exist in the hash, it won't be rendered
- This caused the "models only render near cursor" bug initially

---

## 2. Volume Iterators

### Iterator Flags (src/volume.h)
```c
VOLUME_ITER_VOXELS             // Iterate individual voxels
VOLUME_ITER_TILES              // Iterate tile by tile
VOLUME_ITER_SKIP_EMPTY         // Skip empty voxels (alpha < 127)
VOLUME_ITER_INCLUDES_NEIGHBORS // Force neighbor tiles to exist
```

### Critical Discovery: VOLUME_ITER_INCLUDES_NEIGHBORS
- **Location**: `src/render.c:601-602`
- **Purpose**: Calls `volume_add_neighbors_tiles()` to materialize adjacent tiles
- **Why needed**: Without this, only tiles already in hash are visited
- **Result**: Models render everywhere, not just near cursor

```c
// CORRECT usage in render_custom_models()
volume_iterator_t iter = volume_get_iterator(volume,
    VOLUME_ITER_VOXELS | VOLUME_ITER_INCLUDES_NEIGHBORS);
```

---

## 3. Tool Preview System

### How Tool Preview Works
**Location**: `src/tools/brush.c:194-196`

When hovering with a tool:
```c
if (!goxel.tool_volume) goxel.tool_volume = volume_new();
volume_set(goxel.tool_volume, volume);        // Copies ENTIRE layer
volume_op(goxel.tool_volume, painter, box);   // Applies preview at cursor
```

### Preview Rendering
**Location**: `src/goxel.c:1302-1306`

During rendering:
```c
if (with_tool_preview && goxel.tool_volume &&
    l->volume == goxel.image->active_layer->volume)
{
    volume_set(layer->volume, goxel.tool_volume);  // Uses preview for rendering
}
```

### Critical Issue We Fixed
- **Problem**: Preview system copied entire layer, then `volume_op()` set model_id on ALL voxels in box
- **Result**: Existing voxels near cursor showed current tool's model_id instead of stored value
- **Solution**: Only set model_id on newly created voxels (transparent→opaque transition)

---

## 4. Model ID Storage and Retrieval

### Storage Locations

#### Per-Voxel Model IDs
- **Location**: `src/volume.c` - stored in separate hash table `model_ids`
- Each voxel can have a model_id (0 = cube, 1+ = custom models)
- Uses same tile/position addressing as main voxel data

#### API Functions
```c
// Set model_id for a voxel
void volume_set_model_id_at(volume_t *volume, volume_accessor_t *accessor,
                            const int pos[3], uint8_t model_id)

// Get model_id for a voxel
uint8_t volume_get_model_id_at(const volume_t *volume,
                               volume_iterator_t *iter, const int pos[3])
```

### Where Model IDs Are Set

#### 1. During Painting (volume_op)
**Location**: `src/volume_utils.c:378-394`

```c
bool was_opaque = (value[3] >= 127);
combine(value, c, mode, new_value);
if (!vec4_equal(value, new_value)) {
    volume_set_at(volume, &accessor, vp, new_value);

    // Only set model_id on newly created voxels
    bool is_now_opaque = (new_value[3] >= 127);
    if (!was_opaque && is_now_opaque) {
        volume_set_model_id_at(volume, &accessor, vp, painter->model_id);
    }
}
```

**Critical**: Only sets model_id when voxel transitions from transparent to opaque
- Prevents preview system from contaminating existing voxels
- Preserves stored model_ids during hover

#### 2. During Tile Merging
**Location**: `src/volume_utils.c:478-489`

```c
// Preserve model_ids during merge
uint8_t model_id_src = volume_get_model_id_at(other, &a2, p);
uint8_t model_id_dst = volume_get_model_id_at(volume, &a1, p);
uint8_t final_model_id = (v2[3] >= 127) ? model_id_src : model_id_dst;

// Always set, even if 0, to clear previous model assignments
if (v1[3] >= 127) {  // Only for opaque voxels in result
    volume_set_model_id_at(tile, &a3, (int[]){x, y, z}, final_model_id);
}
```

**Critical**: Always sets model_id (including 0) to ensure cubes can overwrite lightbulbs

### Where Model IDs Are Retrieved for Rendering

#### render_custom_models()
**Location**: `src/render.c:604-630`

```c
while (volume_iter(&iter, pos)) {
    uint8_t model_id = volume_get_model_id_at(volume, &iter, pos);
    if (model_id == 0) continue;  // Skip cubes - rendered separately

    model3d_t *model = model_manager_get(model_id);
    if (!model) continue;

    // Render custom model at this position
    // ...
}
```

---

## 5. Rendering Pipeline

### Main Rendering Flow
**Location**: `src/goxel.c:1157-1159`

```c
void goxel_render_view(const float viewport[4])
{
    for (layer = goxel_get_render_layers(true); layer; layer = layer->next) {
        // Renders each layer
    }
}
```

### Two-Stage Custom Model Rendering

#### Stage 1: Skip Custom Model Voxels in Cube Generation
**Location**: `src/volume_to_vertices.c:267-272`

```c
// Skip voxels with custom models - don't generate cube geometry
int world_pos[3] = {block_pos[0] + x, block_pos[1] + y, block_pos[2] + z};
volume_iterator_t iter = volume_get_accessor(volume);
uint8_t model_id = volume_get_model_id_at(volume, &iter, world_pos);
if (model_id > 0) continue;  // Skip - will be rendered as custom model
```

**Purpose**: Prevent both cube AND custom model from rendering at same position

#### Stage 2: Render Custom Models
**Location**: `src/render.c:596-632`

```c
static void render_custom_models(renderer_t *rend, const volume_t *volume,
                                 const material_t *material, int effects)
{
    volume_iterator_t iter = volume_get_iterator(volume,
        VOLUME_ITER_VOXELS | VOLUME_ITER_INCLUDES_NEIGHBORS);

    while (volume_iter(&iter, pos)) {
        uint8_t model_id = volume_get_model_id_at(volume, &iter, pos);
        if (model_id > 0) {
            // Render model at this position
        }
    }
}
```

---

## 6. Face Culling System

### Overview
Goxel uses occlusion culling to skip rendering cube faces that are adjacent to other opaque voxels. This significantly reduces vertex count and improves performance.

### How Face Culling Works

#### Step 1: Neighbor Detection
**Location**: `src/volume_to_vertices.c:194-210` (`get_neighboors()`)

```c
static uint32_t get_neighboors(const uint8_t *data,
                               const int pos[3],
                               uint8_t neighboors[27])
{
    // Creates a bitmask of 27 surrounding positions (3x3x3 cube)
    // Bit is set if neighbor's alpha >= 127 (opaque)
    for (zz = -1; zz <= 1; zz++)
    for (yy = -1; yy <= 1; yy++)
    for (xx = -1; xx <= 1; xx++) {
        data_get_at(data, pos[0] + xx, pos[1] + yy, pos[2] + zz, v);
        if (v[3] >= 127) ret |= 1 << i;  // Mark as occupied
    }
    return ret;
}
```

Returns a 32-bit bitmask where each bit represents whether a neighbor is opaque.

#### Step 2: Face Visibility Check
**Location**: `src/volume_to_vertices.c:46-55` (`block_is_face_visible()`)

```c
static bool block_is_face_visible(uint32_t neighboors_mask, int f)
{
    static const uint32_t MASKS[6] = {
        M(0, -1, 0), M(0, +1, 0), M(0, 0, -1),
        M(0, 0, +1), M(+1, 0, 0), M(-1, 0, 0),
    };
    return !(MASKS[f] & neighboors_mask);
}
```

Checks if the neighbor in the direction of face `f` is opaque. If opaque → cull face.

#### Step 3: Face Direction Mapping
**Location**: `src/block_def.h:95-102` (`FACES_NORMALS`)

```c
static const int FACES_NORMALS[6][3] = {
    { 0, -1,  0},  // Face 0: Bottom (Y-)
    { 0, +1,  0},  // Face 1: Top (Y+)
    { 0,  0, -1},  // Face 2: Back (Z-)
    { 0,  0, +1},  // Face 3: Front (Z+)
    { 1,  0,  0},  // Face 4: Right (X+)
    {-1,  0,  0},  // Face 5: Left (X-)
};
```

### Custom Model Exception

#### The Problem
Custom models (like lightbulbs) don't fill the entire voxel space. When a cube is adjacent to a custom model voxel, the cube's face should still be rendered (not culled), otherwise you see through the world.

#### The Fix
**Location**: `src/volume_to_vertices.c:276-293`

```c
bool face_visible = block_is_face_visible(neighboors_mask, f);

// CUSTOM MODEL SUBSTITUTION: Don't cull faces adjacent to custom models
if (!face_visible) {
    int neighbor_pos[3] = {
        world_pos[0] + FACES_NORMALS[f][0],
        world_pos[1] + FACES_NORMALS[f][1],
        world_pos[2] + FACES_NORMALS[f][2]
    };
    uint8_t neighbor_model_id = volume_get_model_id_at(volume, &iter, neighbor_pos);
    if (neighbor_model_id > 0) {
        face_visible = true;  // Override culling - render face anyway
    }
}

if (!face_visible) continue;
```

**Logic**:
1. First check normal occlusion culling
2. If face would be culled, calculate neighbor's world position
3. Check if neighbor has `model_id > 0` (custom model)
4. If yes, override culling decision and render the face

**Result**: Cube faces adjacent to custom models are rendered, preventing visual gaps.

### Performance Considerations

- Model ID check only happens when a face would be culled
- Uses existing `iter` accessor (cached tile access)
- Adds ~6 model_id lookups per cube voxel adjacent to custom models
- Negligible performance impact since it only affects boundary voxels

---

## 7. Mouse Picking for Custom Models

### The Problem
Mouse picking/raycasting determines which voxel the cursor is pointing at. The picking system relies on detecting solid geometry. Custom models render as separate 3D models, not as cube voxels, so the original picking system wouldn't detect them - clicks would pass right through.

### The Solution: Low-Opacity Picking Cubes
Instead of modifying the complex picking system, we render a nearly-invisible cube at each custom model voxel position. This cube is:
- Nearly transparent (alpha = 5)
- Detected by mouse picking
- Invisible to users (custom model renders on top)

### Implementation

**Location**: `src/volume_to_vertices.c`

#### Step 1: Detect Custom Model Voxels (lines 267-277)
```c
// Check if this voxel has a custom model
int world_pos[3] = {block_pos[0] + x, block_pos[1] + y, block_pos[2] + z};
volume_iterator_t iter = volume_get_accessor(volume);
uint8_t model_id = volume_get_model_id_at(volume, &iter, world_pos);
bool is_custom_model = (model_id > 0);

if (is_custom_model) {
    // Render nearly-invisible cube for mouse picking
    v[3] = 5;  // Low opacity - pickable but nearly invisible
}
```

Instead of skipping cube generation (as we did initially), we set the voxel's alpha to 5 and proceed with cube generation.

#### Step 2: Preserve Low Alpha (lines 313-318)
```c
memcpy(out[nb * 4 + i].color, v, sizeof(v));

// CUSTOM MODEL SUBSTITUTION: Preserve low alpha for picking cubes
if (is_custom_model) {
    out[nb * 4 + i].color[3] = 5;  // Keep low opacity for picking
} else {
    out[nb * 4 + i].color[3] = out[nb * 4 + i].color[3] ? 255 : 0;
}
```

The normal cube rendering quantizes alpha to either 255 or 0. For custom models, we preserve the low alpha value (5) instead.

### Depth Testing Fix with Polygon Offset

**Problem Encountered**: Picking cubes (alpha=5) rendered first and wrote to depth buffer, causing custom models to fail depth testing. Only visible when camera was inside the cube.

**Attempted Solution 1 - Rendering Order Change**: Rendering custom models before cubes caused GL_INVALID_OPERATION errors due to OpenGL state conflicts (calling render_custom_models() in the middle of cube shader setup).

**Final Solution**: Use polygon offset in `src/render.c:746-754`

```c
// Render cubes first (including picking cubes)
// ... cube rendering ...

// Render custom models with polygon offset
GL(glEnable(GL_POLYGON_OFFSET_FILL));
GL(glPolygonOffset(-1.0f, -1.0f));  // Slight offset toward camera
render_custom_models(rend, volume, material);
GL(glDisable(GL_POLYGON_OFFSET_FILL));
```

**Why this works**:
1. Cubes (including picking cubes) render normally with depth writes
2. Custom models render after with polygon offset enabled
3. Polygon offset shifts depth values slightly toward camera
4. Custom models pass depth test even when picking cubes wrote depth at same position
5. Both geometries co-exist without depth conflicts
6. No OpenGL state management issues

### Result
- **Two layers render at same position**:
  1. Low-opacity cube (renders first) - writes depth, nearly invisible but pickable
  2. Custom model (renders second with offset) - passes depth test due to polygon offset, fully visible
- **Mouse picking works**: Detects the low-opacity cube geometry
- **Visual appearance correct**: Custom model fully visible, picking cube nearly invisible
- **No picking system changes needed**: Leverages existing geometry-based picking
- **Proper depth occlusion**: Both layers participate in depth testing with other voxels
- **Clean OpenGL state**: No shader/attribute conflicts

### Trade-offs
- **Pro**: Simple, elegant solution using existing systems
- **Pro**: No complex picking system modifications
- **Pro**: Works with all existing mouse/tool interactions
- **Con**: Adds minimal extra geometry (one cube per custom model voxel)
- **Con**: Very slight transparency where cube shows through (negligible at alpha=5)

---

## 8. Cache System

### Render Items Cache
**Location**: `src/render.c`

- Caches vertex buffers for rendered tiles
- Key: tile data ID
- **Critical**: Must invalidate when model_ids change

### Cache Invalidation Points

#### 1. When Model ID Changes
**Location**: `src/volume.c:962-964`

```c
void volume_set_model_id_at(...)
{
    // ... set model_id ...
    render_clear_items_cache();  // Force cache invalidation
}
```

#### 2. Manual Clear Function
**Location**: `src/render.c:1180-1185`

```c
void render_clear_items_cache(void)
{
    if (g_items_cache) {
        cache_clear(g_items_cache);
    }
}
```

---

## 9. All Places Where Rendering/Tiles Are Affected

### Critical Code Locations Summary

| File | Lines | Purpose | Affects Rendering? |
|------|-------|---------|-------------------|
| **src/render.c** | 596-632 | render_custom_models() - Main custom model rendering | ✅ YES - Core rendering |
| **src/render.c** | 746-754 | Polygon offset for custom models - depth testing fix | ✅ YES - Depth handling |
| **src/render.c** | 1180-1185 | render_clear_items_cache() - Cache invalidation | ✅ YES - Forces re-render |
| **src/volume_to_vertices.c** | 267-277 | Render low-opacity cube for custom model picking | ✅ YES - Mouse picking |
| **src/volume_to_vertices.c** | 313-318 | Preserve low alpha for custom model cubes | ✅ YES - Mouse picking |
| **src/volume_to_vertices.c** | 281-298 | Face culling override for custom model neighbors | ✅ YES - Renders adjacent faces |
| **src/volume.c** | 962-964 | Cache clear on model_id change | ✅ YES - Triggers re-render |
| **src/volume_utils.c** | 378-394 | volume_op() - Set model_id on new voxels | ✅ YES - Affects what renders |
| **src/volume_utils.c** | 478-489 | tile_merge() - Preserve model_ids | ✅ YES - Affects stored data |
| **src/tools/brush.c** | 194-196 | Tool preview creation | ✅ YES - Preview rendering |
| **src/goxel.c** | 1302-1306 | Replace layer with tool_volume for preview | ✅ YES - Preview rendering |
| **src/goxel.c** | 1157-1159 | Main render loop with layers | ✅ YES - Core rendering |
| **src/volume.c** | 25-27 | External declaration for cache clear | ⚠️ Indirect |
| **src/model_manager.c** | 78-86 | Model loading and registration | ⚠️ Indirect - provides models |
| **src/model_manager.c** | 80 | Disable face culling for custom models | ✅ YES - Visibility fix |

---

## 10. Key Bugs We Fixed

### Bug 1: Models Only Render Near Cursor
- **Cause**: Iterator didn't have `VOLUME_ITER_INCLUDES_NEIGHBORS` flag
- **Location**: `src/render.c:601-602`
- **Fix**: Added flag to materialize all tiles in hash

### Bug 2: Wrong Model IDs After Tool Switch
- **Cause**: `volume_op()` set model_id on ALL voxels in box, not just new ones
- **Location**: `src/volume_utils.c:378-394`
- **Fix**: Only set model_id when voxel transitions transparent→opaque

### Bug 3: Model IDs Not Preserved During Merge
- **Cause**: `tile_merge()` didn't copy model_ids
- **Location**: `src/volume_utils.c:478-489`
- **Fix**: Added model_id preservation logic

### Bug 4: Cubes Couldn't Overwrite Lightbulbs
- **Cause**: `tile_merge()` only set model_id if > 0
- **Location**: `src/volume_utils.c:485-489`
- **Fix**: Always set model_id (including 0) for opaque voxels

### Bug 5: Cube Faces Culled Adjacent to Custom Models
- **Cause**: Face culling system only checked if neighbor is opaque, not if it's a custom model
- **Location**: `src/volume_to_vertices.c:276-293`
- **Symptom**: Visual gaps - could see through world when cube next to lightbulb
- **Fix**: Override culling decision when neighbor has model_id > 0, render face anyway

### Bug 6: Mouse Clicks Pass Through Custom Models
- **Cause**: Custom models render as separate geometry, not detected by picking system
- **Location**: `src/volume_to_vertices.c:267-277, 313-318`
- **Symptom**: Clicks go through lightbulbs as if they weren't there
- **Fix**: Render low-opacity cube (alpha=5) at custom model positions for picking

### Bug 7: Custom Models Hidden by Picking Cubes (Depth Test Failure)
- **Cause**: Picking cubes rendered first and wrote to depth buffer, blocking custom models behind them
- **Location**: `src/render.c:746-754`
- **Symptom**: Only see transparent cube, custom model appears only when camera inside cube
- **Fix**: Use glPolygonOffset() to render custom models with slight depth bias toward camera
- **Note**: Initial attempt to change rendering order caused GL_INVALID_OPERATION due to state conflicts

### Bug 8: Custom Models Only Visible from Inside (Face Culling)
- **Cause**: OBJ models have face culling enabled by default (model->cull = true)
- **Location**: `src/model_manager.c:80`
- **Symptom**: Lightbulb invisible from outside, only visible when camera inside the voxel
- **Reason**: Lightbulb OBJ file has reversed face winding order, back faces culled when viewing from outside
- **Fix**: Set model->cull = false after loading to disable face culling for custom models

---

## 11. Best Practices Learned

### When Adding New Features That Affect Rendering

1. **Check Iterator Flags**: Use `VOLUME_ITER_INCLUDES_NEIGHBORS` when you need all tiles
2. **Invalidate Cache**: Call `render_clear_items_cache()` when model data changes
3. **Preserve Existing Data**: Don't overwrite stored voxel properties during preview
4. **Handle model_id=0**: Remember that 0 is a valid model_id (cubes), not "unset"
5. **Tile Materialization**: Operations only materialize tiles they touch - plan accordingly
6. **Rendering Order Matters**: Consider depth testing when rendering multiple geometries at same position

### Testing Checklist for Rendering Changes

- [ ] Models render at all positions (not just near cursor)
- [ ] Tool preview doesn't contaminate stored voxels
- [ ] Switching tools doesn't change existing voxels
- [ ] Painting cubes over custom models clears the model_id
- [ ] Volume merge operations preserve model_ids
- [ ] Cache invalidates when model data changes
- [ ] No double-rendering (both cube and custom model)
- [ ] Cube faces adjacent to custom models are rendered (no visual gaps)
- [ ] Mouse clicks detect custom model voxels correctly
- [ ] Custom models visible from all camera angles (depth testing works correctly)
- [ ] Custom models visible from outside voxels (face culling disabled)

---

## 12. Architecture Insights

### Why Goxel Uses This Design

1. **Sparse Storage**: Hash-based tiles save massive amounts of memory for mostly-empty worlds
2. **Copy-on-Write**: Operations work on copies, can be cached efficiently
3. **Tool Preview**: Entire layer copy allows preview without affecting stored data
4. **Iterator Abstraction**: Different iteration modes for different use cases

### Trade-offs We Encountered

- **Pro**: Memory efficient, fast for localized edits
- **Con**: Must materialize tiles explicitly for rendering
- **Pro**: Preview system doesn't modify original data
- **Con**: Preview can show incorrect data if not careful with state

---

## 13. Future Considerations

### If Adding More Model Types

- Register in `src/model_manager.c`
- Update model selector UI in `src/gui/model_panel.c`
- Ensure model_id fits in uint8_t (max 255 models)

### If Modifying Volume Operations

- Always consider how it affects model_id storage
- Test with tool preview enabled
- Verify cache invalidation happens correctly
- Check both volume_op() and tile_merge() paths

### If Changing Rendering Pipeline

- Consider tile materialization requirements
- Check iterator flags are appropriate
- Verify no double-rendering with cube geometry
- Test with cached and uncached scenarios
- Verify face culling logic handles custom models correctly

---

## 14. Quick Reference: "Where Does This Happen?"

**Q: Where are voxels stored?**
A: `src/volume.c` - hash table of 16³ tiles

**Q: Where are model_ids stored?**
A: `src/volume.c` - separate hash table, same tile addressing

**Q: Where are model_ids set during painting?**
A: `src/volume_utils.c:378-394` (volume_op, only on new voxels)

**Q: Where are model_ids retrieved for rendering?**
A: `src/render.c:604-630` (render_custom_models)

**Q: Where are low-opacity picking cubes rendered for custom models?**
A: `src/volume_to_vertices.c:267-277` (sets alpha=5) and `313-318` (preserves low alpha)

**Q: Where is face culling overridden for custom models?**
A: `src/volume_to_vertices.c:281-298` (checks neighbor model_id)

**Q: Where does tool preview replace the layer?**
A: `src/goxel.c:1302-1306` (goxel_get_render_layers)

**Q: Where is the render cache cleared?**
A: `src/render.c:1180-1185` and `src/volume.c:962-964`

**Q: Why do models only render near cursor?**
A: Missing `VOLUME_ITER_INCLUDES_NEIGHBORS` flag - tiles not materialized

**Q: Why do voxels show wrong model during preview?**
A: `volume_op()` setting model_id on existing voxels instead of only new ones

**Q: Why are cube faces adjacent to custom models rendered?**
A: Custom models don't fill entire voxel space - without rendering adjacent faces, you'd see through the world

**Q: How do mouse clicks detect custom model voxels?**
A: Low-opacity cubes (alpha=5) render at custom model positions - nearly invisible but pickable by mouse system

**Q: What's the rendering order for custom models vs cubes?**
A: Cubes render first, then custom models with polygon offset (`src/render.c:746-754`). Offset prevents depth conflicts.

**Q: Why were custom models only visible when camera was inside?**
A: Two issues: (1) Picking cubes wrote depth buffer - fixed with glPolygonOffset(). (2) Face culling removed back faces - fixed by setting model->cull = false.

**Q: Should I enable face culling for custom models?**
A: No. OBJ files may have inconsistent face winding. Disable culling (model->cull = false) to ensure visibility from all angles.

---

## Document Version
- Created: 2025-11-01
- Last Updated: 2025-11-01
- Based on: Goxel custom model implementation debugging session
