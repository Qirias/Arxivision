#pragma once

#include "../source/arx_window.h"
#include "../source/arx_device.h"
#include "../source/arx_renderer.h"
#include "../source/arx_game_object.h"
#include "../source/arx_descriptors.h"
#include "../source/managers/arx_texture_manager.hpp"
#include "../source/managers/arx_render_pass_manager.hpp"

#include "../source/geometry/chunkManager.h"

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 2560;
        static constexpr int HEIGHT = 1600;
        
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
        
        // note: order of declarations matters
        ArxWindow                           arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice                           arxDevice{arxWindow};
        
        std::unique_ptr<TextureManager>     textureManager;
        std::unique_ptr<RenderPassManager>  rpManager;
        std::unique_ptr<ArxRenderer>        arxRenderer;
        std::unique_ptr<ChunkManager>       chunkManager;
        
        std::unique_ptr<ArxDescriptorPool>  globalPool{};
        VkDescriptorPool                    imguiPool;
        VkQueryPool                         queryPool;
        
        ArxGameObject::Map                  gameObjects;
    };
}
