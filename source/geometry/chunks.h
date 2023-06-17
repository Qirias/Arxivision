#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include "arx_pipeline.h"
#include "arx_frame_info.h"
#include "arx_game_object.h"
#include "block.h"

namespace arx {

    class Chunk {
    public:
        
        Chunk(ArxDevice &device, const glm::vec3& position, ArxGameObject::Map& gameObjects);
        ~Chunk();
        
        void CreateMesh();
        void Update();
        void Render();
        glm::vec3 getPosition() const { return position; }
        
    private:
        Block           ***blocks;
        glm::vec3       position;
    };
}
