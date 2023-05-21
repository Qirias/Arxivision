#pragma once

namespace arx {

    enum BlockType {
        BlockType_Default = 0
    };

    class Block {
    public:
        Block() { active = true; }
        Block(BlockType type) : blockType(type), active(true) {}
        ~Block() {}
        
        bool isActive() const { return active; }
        void setActive(bool active) { this->active = active; }
        
    private:
        BlockType blockType;
        bool active;
    };

}
