//
//  app.h
//  ArXivision
//
//  Created by kiri on 26/3/23.
//
#pragma once

#include "arx_window.h"
#include "arx_pipeline.h"
#include "arx_device.h"

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 800;
        static constexpr int HEIGHT = 600;
        
        void run();
    private:
        ArxWindow arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice arxDevice{arxWindow};
        ArxPipeline arxPipeline{arxDevice,
                                "shaders/vert.spv", "shaders/frag.spv",
                                ArxPipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
    };
}
