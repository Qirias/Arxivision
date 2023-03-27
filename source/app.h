//
//  app.h
//  ArXivision
//
//  Created by kiri on 26/3/23.
//
#pragma once

#include "arx_window.h"
#include "arx_pipeline.h"

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 800;
        static constexpr int HEIGHT = 600;
        
        void run();
    private:
        ArxWindow arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxPipeline arxPipeline{"shaders/vert.spv", "shaders/frag.spv"};
    };
}
