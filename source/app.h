#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_renderer.h"
#include "arx_game_object.h"
#include "arx_descriptors.h"
#include "arx_texture_manager.hpp"
#include "arx_render_pass_manager.hpp"

#include "chunkManager.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 1920;
        static constexpr int HEIGHT = 1080;
        
        App();
        ~App();
        
        App(const ArxWindow &) = delete;
        App &operator=(const ArxWindow &) = delete;
        
        ArxWindow& getWindow() { return arxWindow; }
        
        void run();
        
    private:
        void initializeImgui();
        void createQueryPool();
        void drawCoordinateVectors(const ArxCamera& camera);
        void printMat4(const glm::mat4& mat);
        
        ArxWindow                           arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice                           arxDevice{arxWindow};
        TextureManager                      textureManager{arxDevice};
        RenderPassManager                   rpManager{arxDevice};
        ArxRenderer                         arxRenderer{arxWindow, arxDevice, rpManager, textureManager};
        
        // Managers
        ChunkManager                        chunkManager{arxDevice};

        // note: order of declarations matters
        std::unique_ptr<ArxDescriptorPool>  globalPool{};
        VkDescriptorPool                    imguiPool;
        VkQueryPool                         queryPool;
        
        ArxGameObject::Map                  gameObjects;
    };
}
