#include "model_utils.h"
#include "app.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

bool loadCubeModel(App *app, const char *modelPath)
{
    size_t glbSize = 0;
    void *glbData = SDL_LoadFile(modelPath, &glbSize);
    if (!glbData)
    {
        SDL_Log("Failed to load Cube GLB file: %s", SDL_GetError());
        return false;
    }

    cgltf_options options = {};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse(&options, glbData, glbSize, &data);

    if (result != cgltf_result_success)
    {
        SDL_Log("Failed to parse Cube GLB model: %s", modelPath);
        SDL_free(glbData);
        return false;
    }

    cgltf_load_buffers(&options, data, modelPath);

    cgltf_mesh &mesh = data->meshes[0];
    cgltf_primitive &prim = mesh.primitives[0];

    cgltf_accessor *posAccessor = nullptr;
    cgltf_accessor *uvAccessor = nullptr;
    cgltf_accessor *indexAccessor = prim.indices;

    for (cgltf_size i = 0; i < prim.attributes_count; ++i)
    {
        if (prim.attributes[i].type == cgltf_attribute_type_position)
        {
            posAccessor = prim.attributes[i].data;
        }
        else if (prim.attributes[i].type == cgltf_attribute_type_texcoord)
        {
            uvAccessor = prim.attributes[i].data;
        }
    }

    glGenVertexArrays(1, &app->cubeVao);
    glBindVertexArray(app->cubeVao);

    if (posAccessor)
    {
        float *positions = (float *)((uint8_t *)posAccessor->buffer_view->buffer->data +
                                     posAccessor->buffer_view->offset +
                                     posAccessor->offset);
        size_t posSize = posAccessor->count * 3 * sizeof(float);
        float *positionsCopy = (float *)SDL_malloc(posSize);
        memcpy(positionsCopy, positions, posSize);

        float maxVal = 0.0f;
        for (size_t i = 0; i < posAccessor->count * 3; ++i)
        {
            float val = std::abs(positions[i]);
            if (val > maxVal)
                maxVal = val;
        }
        if (maxVal > 0.01f)
        {
            app->cubeHalfExtent = maxVal;
        }

        glGenBuffers(1, &app->cubeVbo);
        glBindBuffer(GL_ARRAY_BUFFER, app->cubeVbo);
        glBufferData(GL_ARRAY_BUFFER, posSize, positionsCopy, GL_STATIC_DRAW);
        SDL_free(positionsCopy);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    }

    if (uvAccessor)
    {
        float *uvs = (float *)((uint8_t *)uvAccessor->buffer_view->buffer->data +
                               uvAccessor->buffer_view->offset +
                               uvAccessor->offset);
        size_t uvSize = uvAccessor->count * 2 * sizeof(float);
        float *uvsCopy = (float *)SDL_malloc(uvSize);
        memcpy(uvsCopy, uvs, uvSize);

        glGenBuffers(1, &app->cubeUvVbo);
        glBindBuffer(GL_ARRAY_BUFFER, app->cubeUvVbo);
        glBufferData(GL_ARRAY_BUFFER, uvSize, uvsCopy, GL_STATIC_DRAW);
        SDL_free(uvsCopy);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    }

    if (indexAccessor)
    {
        void *indices = (void *)((uint8_t *)indexAccessor->buffer_view->buffer->data +
                                 indexAccessor->buffer_view->offset +
                                 indexAccessor->offset);
        size_t indexSize = indexAccessor->count * indexAccessor->stride;
        void *indicesCopy = SDL_malloc(indexSize);
        memcpy(indicesCopy, indices, indexSize);

        glGenBuffers(1, &app->cubeEbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->cubeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexSize, indicesCopy, GL_STATIC_DRAW);
        SDL_free(indicesCopy);

        if (indexAccessor->component_type == cgltf_component_type_r_32u)
        {
            app->cubeIndexType = GL_UNSIGNED_INT;
        }
        else if (indexAccessor->component_type == cgltf_component_type_r_16u)
        {
            app->cubeIndexType = GL_UNSIGNED_SHORT;
        }
        else if (indexAccessor->component_type == cgltf_component_type_r_8u)
        {
            app->cubeIndexType = GL_UNSIGNED_BYTE;
        }

        app->cubeIndexCount = (GLsizei)indexAccessor->count;
    }

    glBindVertexArray(0);

    cgltf_free(data);
    SDL_free(glbData);
    return true;
}

void initGroundPlane(App *app, float halfSize, float uvRepeat)
{
    // 2 triangles forming a quad at y = 0.0f
    float groundVertices[] = {
        // Positions (X, Y, Z)        TexCoords (U, V)
        -halfSize, 0.0f, -halfSize, 0.0f, 0.0f,
        halfSize, 0.0f, -halfSize, uvRepeat, 0.0f,
        halfSize, 0.0f, halfSize, uvRepeat, uvRepeat,

        -halfSize, 0.0f, -halfSize, 0.0f, 0.0f,
        halfSize, 0.0f, halfSize, uvRepeat, uvRepeat,
        -halfSize, 0.0f, halfSize, 0.0f, uvRepeat
    };

    glGenVertexArrays(1, &app->groundVao);
    glGenBuffers(1, &app->groundVbo);

    glBindVertexArray(app->groundVao);
    glBindBuffer(GL_ARRAY_BUFFER, app->groundVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);

    // Position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);

    // TexCoord (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

    glBindVertexArray(0);
}
