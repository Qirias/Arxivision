#pragma once

//#include "arx_game_object.h"
#include "arx_pipeline.h"
#include "arx_frame_info.h"
#include "block.h"

namespace arx {

    class Chunk {
    public:
        
        Chunk(ArxDevice &device);
        ~Chunk();
        
        void CreateMesh(ArxGameObject::Map &gameObjects);
        void Update();
        void Render();
        
    private:
        Block           ***blocks;
        ArxDevice       &arxDevice;        
    };
}
