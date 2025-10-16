/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
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
#include "file_format.h"

static int export_as_png_slices(const file_format_t *format,
                                const image_t *image, const char *path)
{
    float box[4][4];
    const volume_t *volume;
    const layer_t *layer;
    int x, y, z, w, h, d, pos[3], start_pos[3];
    uint8_t c[4];
    uint8_t *img;
    volume_iterator_t iter = {0};
    float material_alpha;

    // Get the bounding box from the merged volume
    volume = goxel_get_layers_volume(image);
    mat4_copy(image->box, box);
    if (box_is_null(box)) volume_get_box(volume, true, box);
    w = box[0][0] * 2;
    h = box[1][1] * 2;
    d = box[2][2] * 2;
    start_pos[0] = box[3][0] - box[0][0];
    start_pos[1] = box[3][1] - box[1][1];
    start_pos[2] = box[3][2] - box[2][2];

    img = calloc(w * h * d, 4);

    // Iterate through layers to preserve material alpha information
    DL_FOREACH(image->layers, layer) {
        if (!layer->visible) continue;
        if (!layer->volume) continue;

        // Get material alpha (default to 1.0 if no material)
        material_alpha = 1.0f;
        if (layer->material) {
            material_alpha = layer->material->base_color[3];
        }

        iter = (volume_iterator_t){0};
        for (z = 0; z < d; z++)
        for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            pos[0] = x + start_pos[0];
            pos[1] = y + start_pos[1];
            pos[2] = z + start_pos[2];

            volume_get_at(layer->volume, &iter, pos, c);

            // Skip empty voxels
            if (c[3] == 0) continue;

            int idx = (y * w * d + z * w + x) * 4;

            // Apply material alpha to voxel alpha
            float final_alpha = (c[3] / 255.0f) * material_alpha;

            // Alpha blend with existing pixel
            float src_a = final_alpha;
            float dst_a = img[idx + 3] / 255.0f;
            float out_a = src_a + dst_a * (1.0f - src_a);

            if (out_a > 0) {
                img[idx + 0] = (uint8_t)((c[0] * src_a + img[idx + 0] * dst_a * (1.0f - src_a)) / out_a);
                img[idx + 1] = (uint8_t)((c[1] * src_a + img[idx + 1] * dst_a * (1.0f - src_a)) / out_a);
                img[idx + 2] = (uint8_t)((c[2] * src_a + img[idx + 2] * dst_a * (1.0f - src_a)) / out_a);
                img[idx + 3] = (uint8_t)(out_a * 255.0f);
            }
        }
    }

    img_write(img, w * d, h, 4, path);
    free(img);
    return 0;
}

FILE_FORMAT_REGISTER(png_slices,
    .name = "png slices",
    .exts = {"*.png"},
    .exts_desc = "png",
    .export_func = export_as_png_slices,
)
