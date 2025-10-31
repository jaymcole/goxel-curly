/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

// CUSTOM MODEL SUBSTITUTION: Include tinyobjloader for OBJ loading
// Note: Implementation is in wavefront.c, we just need the declarations
#include "../ext_src/tinyobjloader/tinyobj_loader_c.h"

enum {
    A_POS_LOC = 0,
    A_COLOR_LOC = 1,
    A_NORMAL_LOC = 2,
    A_UV_LOC = 3,
};

static const char *ATTR_NAMES[] = {
    [A_POS_LOC]         = "a_pos",
    [A_COLOR_LOC]       = "a_color",
    [A_NORMAL_LOC]      = "a_normal",
    [A_UV_LOC]          = "a_uv",
    NULL
};

static gl_shader_t *g_shader = NULL;
static texture_t *g_white_tex = NULL;

static texture_t *create_white_tex(void)
{
    texture_t *tex;
    uint8_t *buffer;
    const int s = 16;
    tex = texture_new_surface(s, s, TF_RGB);
    buffer = malloc(s * s * 3);
    memset(buffer, 255, s * s * 3);
    glBindTexture(GL_TEXTURE_2D, tex->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, s, s, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, buffer);
    free(buffer);
    return tex;
}

static void model3d_init(void)
{
    const char *shader;
    if (!g_shader) {
        shader = assets_get("asset://data/shaders/model3d.glsl", NULL);
        g_shader = gl_shader_create(shader, shader, NULL, ATTR_NAMES);
        GL(glUseProgram(g_shader->prog));
        gl_update_uniform(g_shader, "u_tex", 0);
        g_white_tex = create_white_tex();
    }
}

void model3d_release_graphics(void)
{
    gl_shader_delete(g_shader);
    g_shader = NULL;
    texture_delete(g_white_tex);
    g_white_tex = NULL;
}

void model3d_delete(model3d_t *model)
{
    GL(glDeleteBuffers(1, &model->vertex_buffer));
    free(model->vertices);
    free(model);
}

model3d_t *model3d_cube(void)
{
    int f, i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->solid = true;
    model->cull = true;
    const int *p;
    model->nb_vertices = 6 * 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 6; i++) {
            v = (int[]){0, 1, 2, 2, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            vec3_set(model->vertices[f * 6 + i].pos,
                (p[0] - 0.5) * 2, (p[1] - 0.5) * 2, (p[2] - 0.5) * 2);
            vec3_set(model->vertices[f * 6 + i].normal, FACES_NORMALS[f][0],
                                                        FACES_NORMALS[f][1],
                                                        FACES_NORMALS[f][2]);
            vec4_set(model->vertices[f * 6 + i].color, 255, 255, 255, 255);
        }
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_wire_cube(void)
{
    int f, i, v;
    const int *p;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6 * 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    for (f = 0; f < 6; f++) {
        for (i = 0; i < 8; i++) {
            v = (int[]){0, 1, 1, 2, 2, 3, 3, 0}[i];
            p = VERTICES_POSITIONS[FACES_VERTICES[f][v]];
            vec3_set(model->vertices[f * 8 + i].pos,
                (p[0] - 0.5) * 2, (p[1] - 0.5) * 2, (p[2] - 0.5) * 2);
            vec4_set(model->vertices[f * 8 + i].color, 255, 255, 255, 255);
            vec2_set(model->vertices[f * 8 + i].uv, 0.5, 0.5);
        }
    }
    model->cull = true;
    model->dirty = true;
    return model;
}

model3d_t *model3d_sphere(int slices, int stacks)
{
    int slice, stack, ind, i;
    float z0, z1, r0, r1, a0, a1;
    model3d_t *model = calloc(1, sizeof(*model));
    model_vertex_t *v;
    model->nb_vertices = slices * stacks * 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    v = model->vertices;;
    for (stack = 0; stack < stacks; stack++) {
        z0 = -1 + stack * 2.0f / stacks;
        z1 = -1 + (stack + 1) * 2.0f / stacks;
        r0 = sqrt(1 - z0 * z0);
        r1 = sqrt(1 - z1 * z1);
        for (slice = 0; slice < slices; slice++) {
            a0 = slice * M_PI * 2 / slices;
            a1 = (slice + 1) * M_PI * 2 / slices;
            ind = (stack * slices + slice) * 6;
            vec3_set(v[ind + 0].pos, r0 * cos(a0), r0 * sin(a0), z0);
            vec3_set(v[ind + 1].pos, r0 * cos(a1), r0 * sin(a1), z0);
            vec3_set(v[ind + 2].pos, r1 * cos(a0), r1 * sin(a0), z1);
            vec3_set(v[ind + 3].pos, r1 * cos(a1), r1 * sin(a1), z1);
            vec3_set(v[ind + 4].pos, r1 * cos(a0), r1 * sin(a0), z1);
            vec3_set(v[ind + 5].pos, r0 * cos(a1), r0 * sin(a1), z0);
            for (i = 0; i < 6; i++)
                vec3_copy(v[ind + i].pos, v[ind + i].normal);

        }
    }
    model->cull = true;
    model->dirty = true;
    return model;
}

model3d_t *model3d_grid(int nx, int ny)
{
    model3d_t *model = calloc(1, sizeof(*model));
    model_vertex_t *v;
    int i;
    float x, y;
    bool side;
    const uint8_t c[4]  = {255, 255, 255, 160};
    const uint8_t cb[4] = {255, 255, 255, 255};

    model->nb_vertices = (nx + ny + 2) * 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    v = model->vertices;
    for (i = 0; i < nx + 1; i++) {
        side = i == 0 || i == nx;
        x = (float)i / nx - 0.5;
        vec3_set(v[i * 2 + 0].pos, x, -0.5, 0);
        vec3_set(v[i * 2 + 1].pos, x, +0.5, 0);
        vec4_copy(side ? cb : c, v[i * 2 + 0].color);
        vec4_copy(side ? cb : c, v[i * 2 + 1].color);
    }
    v = model->vertices + 2 * nx + 2;
    for (i = 0; i < ny + 1; i++) {
        side = i == 0 || i == ny;
        y = (float)i / ny - 0.5;
        vec3_set(v[i * 2 + 0].pos, -0.5, y, 0);
        vec3_set(v[i * 2 + 1].pos, +0.5, y, 0);
        vec4_copy(side ? cb : c, v[i * 2 + 0].color);
        vec4_copy(side ? cb : c, v[i * 2 + 1].color);
    }
    model->dirty = true;
    return model;
}

model3d_t *model3d_line(void)
{
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 2;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    vec3_set(model->vertices[0].pos, -0.5, 0, 0);
    vec3_set(model->vertices[1].pos, +0.5, 0, 0);
    vec4_set(model->vertices[0].color, 255, 255, 255, 255);
    vec4_set(model->vertices[1].color, 255, 255, 255, 255);
    model->dirty = true;
    return model;
}

model3d_t *model3d_rect(void)
{
    int i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 6;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    const float POS_UV[4][2][2] = {
        {{-0.5, -0.5}, {0, 1}},
        {{+0.5, -0.5}, {1, 1}},
        {{+0.5, +0.5}, {1, 0}},
        {{-0.5, +0.5}, {0, 0}},
    };

    for (i = 0; i < 6; i++) {
        v = (int[]){0, 1, 2, 2, 3, 0}[i];
        vec2_copy(POS_UV[v][0], model->vertices[i].pos);
        vec2_copy(POS_UV[v][1], model->vertices[i].uv);
        vec4_set(model->vertices[i].color, 255, 255, 255, 255);
        vec3_set(model->vertices[i].normal, 0, 0, 1);
    }
    model->solid = true;
    model->dirty = true;
    return model;
}

model3d_t *model3d_wire_rect(void)
{
    int i, v;
    model3d_t *model = calloc(1, sizeof(*model));
    model->nb_vertices = 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    const float POS_UV[4][2][2] = {
        {{-0.5, -0.5}, {0, 1}},
        {{+0.5, -0.5}, {1, 1}},
        {{+0.5, +0.5}, {1, 0}},
        {{-0.5, +0.5}, {0, 0}},
    };

    for (i = 0; i < 8; i++) {
        v = ((i + 1) / 2) % 4;
        vec2_copy(POS_UV[v][0], model->vertices[i].pos);
        vec2_copy(POS_UV[v][1], model->vertices[i].uv);
        vec4_set(model->vertices[i].color, 255, 255, 255, 255);
    }
    model->dirty = true;
    return model;
}

/*
 * Cone with top at the point Z = 1:
 *
 *          ^ z
 *          |
 *          +
 *         /|\
 *       /  |  \
 *      /   |   \
 *     /    |    \
 *   /      |      \
 *  /       |       \
 * +--------+--------+---->x
 *
 */
model3d_t *model3d_cone(void)
{
    int i, j, idx;
    float da;
    model3d_t *model = calloc(1, sizeof(*model));
    model_vertex_t *v;

    model->nb_vertices = 2 * 3 * 8;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    da = 2 * M_PI / 8;
    v = model->vertices;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 8; j++) {
            idx = i * 3 * 8 + j * 3;
            vec4_set(v[idx + 0].color, 255, 255, 255, 255);
            vec4_set(v[idx + 1].color, 255, 255, 255, 255);
            vec4_set(v[idx + 2].color, 255, 255, 255, 255);
            vec3_set(v[idx].pos, 0, 0, i);
            vec3_set(v[idx + 1].pos, cos(da * (j + 0)), sin(da * (j + 0)), 0);
            vec3_set(v[idx + 2].pos, cos(da * (j + 1)), sin(da * (j + 1)), 0);
            // Todo: normals.
        }
    }
    model->dirty = true;
    model->solid = true;
    return model;
}

static void copy_color(const uint8_t in[4], uint8_t out[4])
{
    if (in == NULL) {
        out[0] = 255;
        out[1] = 255;
        out[2] = 255;
        out[3] = 255;
    } else {
        memcpy(out, in, 4);
    }
}

void model3d_render(model3d_t *model3d,
                    const float model[4][4],
                    const float view[4][4],
                    const float proj[4][4],
                    const uint8_t color[4],
                    const texture_t *tex,
                    const float light[3],
                    const float clip_box[4][4],
                    int   effects)
{
    uint8_t c[4];
    float cf[4];
    float light_dir[3];
    float clip[4][4] = {};
    float grid_alpha;

    model3d_init();
    copy_color(color, c);
    GL(glUseProgram(g_shader->prog));
    gl_update_uniform(g_shader, "u_model", model);
    gl_update_uniform(g_shader, "u_view", view);
    gl_update_uniform(g_shader, "u_proj", proj);
    GL(glEnableVertexAttribArray(A_POS_LOC));
    GL(glEnable(GL_BLEND));
    if (effects & EFFECT_NO_DEPTH_TEST) {
        GL(glDisable(GL_DEPTH_TEST));
    } else {
        GL(glEnable(GL_DEPTH_TEST));
    }
    GL(glDepthFunc(GL_LEQUAL));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glCullFace(GL_BACK));
    model3d->cull ? GL(glEnable(GL_CULL_FACE)) :
                    GL(glDisable(GL_CULL_FACE));
    GL(glCullFace(effects & EFFECT_SEE_BACK ? GL_FRONT : GL_BACK));

    vec4_set(cf, c[0] / 255.0, c[1] / 255.0, c[2] / 255.0, c[3] / 255.0);
    gl_update_uniform(g_shader, "u_color", cf);
    gl_update_uniform(g_shader, "u_strip", effects & EFFECT_STRIP ? 1.0 : 0.0);
    gl_update_uniform(g_shader, "u_time", 0.0); // No moving strip effects.

    tex = tex ?: g_white_tex;
    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindTexture(GL_TEXTURE_2D, tex->tex));
    gl_update_uniform(g_shader, "u_uv_scale",
            VEC((float)tex->w / tex->tex_w, (float)tex->h / tex->tex_h));

    if (model3d->dirty) {
        if (!model3d->vertex_buffer)
            GL(glGenBuffers(1, &model3d->vertex_buffer));
        GL(glBindBuffer(GL_ARRAY_BUFFER, model3d->vertex_buffer));
        GL(glBufferData(GL_ARRAY_BUFFER,
                        model3d->nb_vertices * sizeof(*model3d->vertices),
                        model3d->vertices, GL_STATIC_DRAW));
        model3d->dirty = false;
    }
    GL(glBindBuffer(GL_ARRAY_BUFFER, model3d->vertex_buffer));
    GL(glEnableVertexAttribArray(A_POS_LOC));
    GL(glVertexAttribPointer(A_POS_LOC, 3, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, pos)));
    GL(glEnableVertexAttribArray(A_COLOR_LOC));
    GL(glVertexAttribPointer(A_COLOR_LOC, 4, GL_UNSIGNED_BYTE, true,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, color)));
    GL(glEnableVertexAttribArray(A_UV_LOC));
    GL(glVertexAttribPointer(A_UV_LOC, 2, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, uv)));
    GL(glEnableVertexAttribArray(A_NORMAL_LOC));
    GL(glVertexAttribPointer(A_NORMAL_LOC, 3, GL_FLOAT, false,
            sizeof(*model3d->vertices), (void*)offsetof(model_vertex_t, normal)));

    gl_update_uniform(g_shader, "u_l_emit", 1.0);
    gl_update_uniform(g_shader, "u_l_diff", 0.0);

    if (clip_box && !box_is_null(clip_box)) mat4_invert(clip_box, clip);
    gl_update_uniform(g_shader, "u_clip", clip);

    grid_alpha = (effects & EFFECT_GRID) ? 0.05 : 0.0;
    gl_update_uniform(g_shader, "u_grid_alpha", grid_alpha);

    if (model3d->solid) {
        if (light && (!(effects & EFFECT_NO_SHADING))) {
            vec3_copy(light, light_dir);
            if (effects & EFFECT_SEE_BACK) vec3_imul(light_dir, -1);
            gl_update_uniform(g_shader, "u_l_dir", light_dir);
            gl_update_uniform(g_shader, "u_l_emit", 0.2);
            gl_update_uniform(g_shader, "u_l_diff", 0.8);

        }
        GL(glDrawArrays(GL_TRIANGLES, 0, model3d->nb_vertices));
    } else {
        GL(glDrawArrays(GL_LINES, 0, model3d->nb_vertices));
    }
    GL(glDisableVertexAttribArray(A_POS_LOC));
    GL(glDisableVertexAttribArray(A_COLOR_LOC));
    GL(glCullFace(GL_BACK));
    GL(glEnable(GL_DEPTH_TEST));
}

// CUSTOM MODEL SUBSTITUTION START
/*
 * Helper function for tinyobj to read file data
 */
static void get_file_data_for_obj(void *ctx, const char *filename,
                                    const int is_mtl,
                                    const char *obj_filename,
                                    char **data, size_t *len)
{
    int size;
    (void)ctx;
    (void)is_mtl;
    (void)obj_filename;

    if (!filename) {
        *data = NULL;
        *len = 0;
        return;
    }
    *data = read_file(filename, &size);
    *len = size;
}

/*
 * Calculate face normal from three vertices using cross product
 */
static void calculate_normal(const float v0[3], const float v1[3],
                             const float v2[3], float normal[3])
{
    float edge1[3], edge2[3];

    // edge1 = v1 - v0
    edge1[0] = v1[0] - v0[0];
    edge1[1] = v1[1] - v0[1];
    edge1[2] = v1[2] - v0[2];

    // edge2 = v2 - v0
    edge2[0] = v2[0] - v0[0];
    edge2[1] = v2[1] - v0[1];
    edge2[2] = v2[2] - v0[2];

    // normal = cross(edge1, edge2)
    normal[0] = edge1[1] * edge2[2] - edge1[2] * edge2[1];
    normal[1] = edge1[2] * edge2[0] - edge1[0] * edge2[2];
    normal[2] = edge1[0] * edge2[1] - edge1[1] * edge2[0];

    // Normalize
    float len = sqrt(normal[0] * normal[0] +
                     normal[1] * normal[1] +
                     normal[2] * normal[2]);
    if (len > 0.0001f) {
        normal[0] /= len;
        normal[1] /= len;
        normal[2] /= len;
    }
}

/*
 * Function: model3d_from_obj
 * Load a 3D model from an OBJ file.
 *
 * Parameters:
 *   path - Path to the .obj file
 *
 * Returns:
 *   A newly created model3d_t, or NULL if loading failed.
 *
 * Note: This converts indexed triangle meshes to vertex arrays (no indexing).
 *       Normals are calculated if not present in the file.
 */
model3d_t *model3d_from_obj(const char *path)
{
    int err;
    tinyobj_attrib_t attrib = {0};
    tinyobj_shape_t *shapes = NULL;
    size_t num_shapes = 0;
    tinyobj_material_t *materials = NULL;
    size_t num_materials = 0;
    unsigned int flags;
    model3d_t *model = NULL;
    int i, j, idx;
    float v0[3], v1[3], v2[3], normal[3];
    int vertex_index = 0;

    // Parse OBJ file with triangulation
    flags = TINYOBJ_FLAG_TRIANGULATE;
    err = tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials,
                            &num_materials, path, get_file_data_for_obj,
                            NULL, flags);

    if (err != TINYOBJ_SUCCESS) {
        LOG_E("Failed to load OBJ file: %s", path);
        goto cleanup;
    }

    if (attrib.num_faces == 0) {
        LOG_W("OBJ file has no faces: %s", path);
        goto cleanup;
    }

    // Create model (3 vertices per triangle)
    model = calloc(1, sizeof(*model));
    model->nb_vertices = attrib.num_faces * 3;
    model->vertices = calloc(model->nb_vertices, sizeof(*model->vertices));
    model->solid = true;
    model->cull = true;

    // Convert indexed triangles to vertex array
    for (i = 0; i < attrib.num_faces; i++) {
        // Get the 3 vertices of this triangle
        for (j = 0; j < 3; j++) {
            idx = attrib.faces[i * 3 + j].v_idx;

            // Position
            model->vertices[vertex_index].pos[0] = attrib.vertices[idx * 3 + 0];
            model->vertices[vertex_index].pos[1] = attrib.vertices[idx * 3 + 1];
            model->vertices[vertex_index].pos[2] = attrib.vertices[idx * 3 + 2];

            // Check if normals are provided
            if (attrib.faces[i * 3 + j].vn_idx >= 0) {
                int nidx = attrib.faces[i * 3 + j].vn_idx;
                model->vertices[vertex_index].normal[0] = attrib.normals[nidx * 3 + 0];
                model->vertices[vertex_index].normal[1] = attrib.normals[nidx * 3 + 1];
                model->vertices[vertex_index].normal[2] = attrib.normals[nidx * 3 + 2];
            }

            // UV coordinates if available
            if (attrib.faces[i * 3 + j].vt_idx >= 0) {
                int tidx = attrib.faces[i * 3 + j].vt_idx;
                model->vertices[vertex_index].uv[0] = attrib.texcoords[tidx * 2 + 0];
                model->vertices[vertex_index].uv[1] = attrib.texcoords[tidx * 2 + 1];
            } else {
                model->vertices[vertex_index].uv[0] = 0.5f;
                model->vertices[vertex_index].uv[1] = 0.5f;
            }

            // Default white color
            model->vertices[vertex_index].color[0] = 255;
            model->vertices[vertex_index].color[1] = 255;
            model->vertices[vertex_index].color[2] = 255;
            model->vertices[vertex_index].color[3] = 255;

            vertex_index++;
        }

        // Calculate normals if not provided
        if (attrib.num_normals == 0 || attrib.faces[i * 3].vn_idx < 0) {
            // Get the three vertices of this triangle
            memcpy(v0, model->vertices[(i * 3) + 0].pos, sizeof(v0));
            memcpy(v1, model->vertices[(i * 3) + 1].pos, sizeof(v1));
            memcpy(v2, model->vertices[(i * 3) + 2].pos, sizeof(v2));

            calculate_normal(v0, v1, v2, normal);

            // Set the same normal for all 3 vertices
            memcpy(model->vertices[(i * 3) + 0].normal, normal, sizeof(normal));
            memcpy(model->vertices[(i * 3) + 1].normal, normal, sizeof(normal));
            memcpy(model->vertices[(i * 3) + 2].normal, normal, sizeof(normal));
        }
    }

    model->dirty = true;
    LOG_I("Loaded OBJ model: %s (%d triangles)", path, attrib.num_faces);

cleanup:
    // Free tinyobj data
    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, num_shapes);
    tinyobj_materials_free(materials, num_materials);

    return model;
}
// CUSTOM MODEL SUBSTITUTION END
