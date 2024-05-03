#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_renderer.h"
#include "arx_game_object.h"
#include "arx_descriptors.h"
#include "chunkManager.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 1080;
        static constexpr int HEIGHT = 1080;
        
        App();
        ~App();
        
        App(const ArxWindow &) = delete;
        App &operator=(const ArxWindow &) = delete;
        
        ArxWindow& getWindow() { return arxWindow; }
        
        void run();
        
    private:
        void initializeImgui();
        
        ArxWindow                           arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice                           arxDevice{arxWindow};
        ArxRenderer                         arxRenderer{arxWindow, arxDevice};

        // note: order of declarations matters
        std::unique_ptr<ArxDescriptorPool>  globalPool{};
        VkDescriptorPool                    imguiPool;
        
        ArxGameObject::Map                  gameObjects;
        ChunkManager                        chunkManager;
    };
}
