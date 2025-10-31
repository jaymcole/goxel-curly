/* Goxel 3D voxels editor
 *
 * Model Manager - Manages custom 3D models for voxel substitution
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

#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Include model3d for model3d_t type
#include "model3d.h"

// Maximum number of custom models that can be loaded
#define MODEL_MANAGER_MAX_MODELS 256

/*
 * Function: model_manager_init
 * Initialize the model manager subsystem.
 *
 * This should be called once at application startup after OpenGL is initialized.
 */
void model_manager_init(void);

/*
 * Function: model_manager_load
 * Load all predefined custom models from disk.
 *
 * This loads models from data/models/ directory and assigns them model IDs:
 * - model_id 0: reserved for normal cube rendering
 * - model_id 1+: custom models (lightbulb, door, etc.)
 */
void model_manager_load(void);

/*
 * Function: model_manager_get
 * Get a custom model by its model_id.
 *
 * Parameters:
 *   model_id - The ID of the model to retrieve (1-255)
 *
 * Returns:
 *   Pointer to the model3d_t, or NULL if model_id is invalid or not loaded.
 */
model3d_t *model_manager_get(uint8_t model_id);

/*
 * Function: model_manager_register
 * Register a custom model with a specific model_id.
 *
 * Parameters:
 *   model_id - The ID to assign to this model (1-255, 0 is reserved)
 *   model - The model3d_t to register
 *
 * Returns:
 *   true if successfully registered, false if model_id is invalid or already in use.
 */
bool model_manager_register(uint8_t model_id, model3d_t *model);

/*
 * Function: model_manager_get_count
 * Get the number of registered custom models.
 *
 * Returns:
 *   The count of registered models (excluding model_id 0).
 */
int model_manager_get_count(void);

/*
 * Function: model_manager_free
 * Free all resources used by the model manager.
 *
 * This should be called once at application shutdown.
 */
void model_manager_free(void);

#endif // MODEL_MANAGER_H
