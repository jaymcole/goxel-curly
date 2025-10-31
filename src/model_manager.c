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

#include "model_manager.h"
#include "model3d.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

// Internal storage for custom models
static struct {
    bool initialized;
    model3d_t *models[MODEL_MANAGER_MAX_MODELS];
    int count;
} g_model_manager = {0};

void model_manager_init(void)
{
    if (g_model_manager.initialized) {
        LOG_W("Model manager already initialized");
        return;
    }

    memset(&g_model_manager, 0, sizeof(g_model_manager));
    g_model_manager.initialized = true;

    LOG_I("Model manager initialized");
}

void model_manager_load(void)
{
    model3d_t *model;

    if (!g_model_manager.initialized) {
        LOG_E("Model manager not initialized");
        return;
    }

    // Load models from data/models/ directory
    // Each model is assigned a specific model_id (1-255)
    // model_id 0 is reserved for normal cube rendering

    // Model ID 1: Lightbulb
    model = model3d_from_obj("data/models/lightbulb.obj");
    if (model) {
        model_manager_register(1, model);
        LOG_D("Registered lightbulb.obj as model_id 1");
    }

    // Model ID 2: Door
    model = model3d_from_obj("data/models/door.obj");
    if (model) {
        model_manager_register(2, model);
        LOG_D("Registered door.obj as model_id 2");
    }

    // Model ID 3: Torch
    model = model3d_from_obj("data/models/torch.obj");
    if (model) {
        model_manager_register(3, model);
        LOG_D("Registered torch.obj as model_id 3");
    }

    // Add more models here as needed...
    // model = model3d_from_obj("data/models/yourmodel.obj");
    // if (model) {
    //     model_manager_register(4, model);
    // }

    LOG_I("Model manager loaded %d custom models", g_model_manager.count);
}

model3d_t *model_manager_get(uint8_t model_id)
{
    if (!g_model_manager.initialized) {
        LOG_E("Model manager not initialized");
        return NULL;
    }

    if (model_id == 0 || model_id >= MODEL_MANAGER_MAX_MODELS) {
        return NULL;
    }

    return g_model_manager.models[model_id];
}

bool model_manager_register(uint8_t model_id, model3d_t *model)
{
    if (!g_model_manager.initialized) {
        LOG_E("Model manager not initialized");
        return false;
    }

    if (model_id == 0) {
        LOG_E("Model ID 0 is reserved for normal cube rendering");
        return false;
    }

    if (model_id >= MODEL_MANAGER_MAX_MODELS) {
        LOG_E("Model ID %d exceeds maximum %d", model_id, MODEL_MANAGER_MAX_MODELS - 1);
        return false;
    }

    if (g_model_manager.models[model_id] != NULL) {
        LOG_W("Model ID %d already registered, replacing", model_id);
        // Note: In production, you might want to free the old model here
    }

    g_model_manager.models[model_id] = model;
    if (model != NULL) {
        g_model_manager.count++;
    }

    LOG_D("Registered model ID %d", model_id);
    return true;
}

int model_manager_get_count(void)
{
    if (!g_model_manager.initialized) {
        return 0;
    }
    return g_model_manager.count;
}

void model_manager_free(void)
{
    if (!g_model_manager.initialized) {
        return;
    }

    // Free all loaded models
    for (int i = 1; i < MODEL_MANAGER_MAX_MODELS; i++) {
        if (g_model_manager.models[i] != NULL) {
            // Note: Depending on the model3d API, you might need to call
            // a specific cleanup function here, e.g., model3d_delete()
            // For now, we just NULL the pointer
            g_model_manager.models[i] = NULL;
        }
    }

    g_model_manager.initialized = false;
    g_model_manager.count = 0;

    LOG_I("Model manager freed");
}
