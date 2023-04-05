#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_renderer.h"
#include "arx_game_object.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 800;
        static constexpr int HEIGHT = 600;
        
        App();
        ~App();
        
        App(const ArxWindow &) = delete;
        App &operator=(const ArxWindow &) = delete;
        
        void run();
        
    private:
        void loadGameObjects();
        
        ArxWindow                       arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice                       arxDevice{arxWindow};
        ArxRenderer                     arxRenderer{arxWindow, arxDevice};
        
        std::vector<ArxGameObject>      gameObjects;
    };
}
