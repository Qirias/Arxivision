#pragma once

#include "arx_model.h"

// libs
#include <glm/gtc/matrix_transform.hpp>

// std
#include <memory>
#include <unordered_map>

namespace arx {
    
    struct TransformComponent {
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{};
        
        glm::mat4 mat4();
        glm::mat3 normalMatrix();
    };

    class ArxGameObject {
    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, ArxGameObject>;
        
        static ArxGameObject createGameObject() {
            static id_t currentId = 0;
            return ArxGameObject{currentId++};
        }
        
        static ArxGameObject makePointLight(float intensity = 10.f, float radius = .1f, glm::vec3 color = glm::vec3(1.0f));
        
        ArxGameObject() = default;
        ArxGameObject(const ArxGameObject &) = delete;
        ArxGameObject &operator=(const ArxGameObject &) = delete;
        ArxGameObject(ArxGameObject &&) = default;
        ArxGameObject &operator=(ArxGameObject &&) = default;
        
        id_t getId() const { return id; }
        
        glm::vec3 color{};
        TransformComponent transform{};
        
        // Optional pointer components
        std::shared_ptr<ArxModel> model{};
    private:
        ArxGameObject(id_t objId) : id{objId} {}
        
        id_t id;
    };
}
