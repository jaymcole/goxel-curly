/* Goxel 3D voxels editor
 *
 * Model Selection Panel - UI for selecting custom 3D models
 *
 * copyright (c) 2025
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"
#include "../model_manager.h"

void gui_model_panel(void)
{
    int i, current = -1;
    int nb_models;
    gui_icon_info_t *grid;
    const char *name;

    // Count total models: 1 (Cube) + loaded custom models
    nb_models = 1 + model_manager_get_count();

    // Allocate grid for icons
    grid = calloc(nb_models, sizeof(*grid));

    // Add Cube (model_id 0) as first entry
    grid[0] = (gui_icon_info_t) {
        .label = "Cube",
        .icon = 0,
        .color = {255, 255, 255, 255},  // White background
    };
    if (goxel.painter.model_id == 0) {
        current = 0;
    }

    // Add custom models (model_id 1+)
    int grid_idx = 1;
    for (i = 1; i < MODEL_MANAGER_MAX_MODELS && grid_idx < nb_models; i++) {
        name = model_manager_get_name(i);
        if (name != NULL) {
            grid[grid_idx] = (gui_icon_info_t) {
                .label = name,
                .icon = 0,
                .color = {200, 200, 255, 255},  // Light blue background for custom models
            };
            if (goxel.painter.model_id == i) {
                current = grid_idx;
            }
            grid_idx++;
        }
    }

    // Display grid and handle selection
    if (gui_icons_grid(nb_models, grid, &current)) {
        // Find the actual model_id corresponding to the grid selection
        if (current == 0) {
            goxel.painter.model_id = 0;
        } else {
            // Map grid index back to model_id
            int grid_count = 1;  // Start after Cube
            for (i = 1; i < MODEL_MANAGER_MAX_MODELS; i++) {
                if (model_manager_get_name(i) != NULL) {
                    if (grid_count == current) {
                        goxel.painter.model_id = i;
                        break;
                    }
                    grid_count++;
                }
            }
        }
    }

    free(grid);
}
