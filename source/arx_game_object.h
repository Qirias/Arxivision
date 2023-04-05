#pragma once

#include "arx_model.h"

// std
#include <memory>

namespace arx {
    
    struct Transform2dComponent {
        glm::vec2 translation{};
        glm::vec2 scale{1.f, 1.f};
        float rotation;
        
        glm::mat2 mat2() {
            const float s = glm::sin(rotation);
            const float c = glm::cos(rotation);
            glm::mat2 rotMatrix{{c,s}, {-s, c}};
            
            glm::mat2 scaleMat{{scale.x, .0f}, {.0f, scale.y}};
            return rotMatrix * scaleMat;
        }
    };

    class ArxGameObject {
    public:
        using id_t = unsigned int;
        
        static ArxGameObject createGameObject() {
            static id_t currentId = 0;
            return ArxGameObject{currentId++};
        }
        
        ArxGameObject(const ArxGameObject &) = delete;
        ArxGameObject &operator=(const ArxGameObject &) = delete;
        ArxGameObject(ArxGameObject &&) = default;
        ArxGameObject &operator=(ArxGameObject &&) = default;
        
        id_t getId() const { return id; }
        
        std::shared_ptr<ArxModel> model{};
        glm::vec3 color{};
        Transform2dComponent transform2d{};
    private:
        ArxGameObject(id_t objId) : id{objId} {}
        
        id_t id;
    };
}
