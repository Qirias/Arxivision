#include "chunks.h"

#include <iostream>

namespace arx {

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, std::vector<ArxModel::Vertex>& vertices) : position{pos} {
        initializeBlocks();
        
        std::vector<InstanceData> tmpInstance;
        instances = Voxelize(vertices);
        tmpInstance.resize(instances);
        
        uint32_t index = 0;
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    if (!blocks[x][y][z].isActive()) continue;
                    glm::vec3 translation = glm::vec3(x*VOXEL_SIZE, y*VOXEL_SIZE, z*VOXEL_SIZE) + position;
                    tmpInstance[index].color = glm::vec4(x / static_cast<float>(ADJUSTED_CHUNK),
                                                         y / static_cast<float>(ADJUSTED_CHUNK),
                                                         z / static_cast<float>(ADJUSTED_CHUNK), 1.0f);
                    tmpInstance[index].translation = glm::vec4(translation, 1.0f);
                    index++;
                }
            }
        }

        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "data/models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            id = cube.getId();
            cube.model = cubeModel;
            voxel.emplace(id, std::move(cube));
            instanceData[id] = tmpInstance;
        }
    }

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, glm::ivec3 terrainSize) : position{pos} {
        initializeBlocks();

        std::vector<InstanceData> tmpInstance;
        applyCARule(terrainSize);
        tmpInstance.resize(instances);

        uint32_t instances = 0;
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    if (!blocks[x][y][z].isActive()) continue;
                    glm::vec3 translation = glm::vec3(x*VOXEL_SIZE, y*VOXEL_SIZE, z*VOXEL_SIZE) + position;
                    tmpInstance[instances].color = glm::vec4(colors[x][y][z], 1.0f);
                    tmpInstance[instances].translation = glm::vec4(translation, 1.0f);
                    instances++;
                }
            }
        }

//        std::cout << "Instances drawn: " << instances << std::endl;
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "data/models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            cube.model = cubeModel;
            id = cube.getId();
            voxel.emplace(id, std::move(cube));
            instanceData[id] = tmpInstance;
        }
    }

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, const std::vector<InstanceData>& instanceDataVec) : position{pos} {
        instances = static_cast<uint32_t>(instanceDataVec.size());
        
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "data/models/cube.obj", instances, instanceDataVec);
            auto cube = ArxGameObject::createGameObject();
            id = cube.getId();
            cube.model = cubeModel;
            voxel.emplace(id, std::move(cube));
            instanceData[id] = instanceDataVec;
        }
    }

    void Chunk::deactivateHiddenVoxels() {
        // First, mark all voxels as active based on the original 'blocks' array.
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    culledBlocks[x][y][z].setActive(blocks[x][y][z].isActive());
                }
            }
        }

        // Then, iterate through each voxel to check if it's completely occluded by its neighbors.
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    if (!blocks[x][y][z].isActive()) continue; // Skip inactive blocks
                    
                    bool isHidden = true;
                    // Check negative X neighbor or boundary
                    isHidden &= (x > 0) ? blocks[x-1][y][z].isActive() : true;
                    // Check positive X neighbor or boundary
                    isHidden &= (x < ADJUSTED_CHUNK - 1) ? blocks[x+1][y][z].isActive() : true;
                    // Check negative Y neighbor or boundary
                    isHidden &= (y > 0) ? blocks[x][y-1][z].isActive() : true;
                    // Check positive Y neighbor or boundary
                    isHidden &= (y < ADJUSTED_CHUNK - 1) ? blocks[x][y+1][z].isActive() : true;
                    // Check negative Z neighbor or boundary
                    isHidden &= (z > 0) ? blocks[x][y][z-1].isActive() : true;
                    // Check positive Z neighbor or boundary
                    isHidden &= (z < ADJUSTED_CHUNK - 1) ? blocks[x][y][z+1].isActive() : true;
                    
                    // If all neighbors are active, mark the voxel as inactive in the 'culledBlocks' array
                    if (isHidden) {
                        culledBlocks[x][y][z].setActive(false);
                        instances--;
                    }
                }
            }
        }
    }


    bool Chunk::CheckVoxelIntersection(const std::vector<arx::ArxModel::Vertex>& vertices, const glm::vec3& voxelPosition) {

        CGAL::Bbox_3 aabb(voxelPosition.x, voxelPosition.y, voxelPosition.z,
                          voxelPosition.x + 1, voxelPosition.y + 1, voxelPosition.z + 1);

        for (size_t i = 0; i < vertices.size(); i += 3) {
            // Convert to CGAL Point
            Point v0(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);
            Point v1(vertices[i + 1].position.x, vertices[i + 1].position.y, vertices[i + 1].position.z);
            Point v2(vertices[i + 2].position.x, vertices[i + 2].position.y, vertices[i + 2].position.z);
            
            if (intersect_aabb_triangle_cgal(aabb, v0, v1, v2)) {
                return true;
            }
        }
        return false;
    }


    int Chunk::Voxelize(const std::vector<arx::ArxModel::Vertex>& vertices) {
        ThreadPool threadPool;
        unsigned int numThreads = std::thread::hardware_concurrency();
        threadPool.setThreadCount(numThreads);

        std::atomic<int> inst(0);

        int adjustedChunk = CHUNK_SIZE / VOXEL_SIZE;
        int totalVoxels = adjustedChunk * adjustedChunk * adjustedChunk;
        int voxelsPerThread = totalVoxels / numThreads;

        for (unsigned int t = 0; t < numThreads; ++t) {
            int startIdx = t * voxelsPerThread;
            int endIdx = (t + 1) * voxelsPerThread;
            if (t == numThreads - 1) {
                endIdx = totalVoxels;
            }
            
            threadPool.threads[t]->addJob([this, startIdx, endIdx, &vertices, &inst, adjustedChunk]() {
                for (int idx = startIdx; idx < endIdx; ++idx) {
                    int x = idx / (adjustedChunk * adjustedChunk);
                    int y = (idx / adjustedChunk) % adjustedChunk;
                    int z = idx % adjustedChunk;

                    // Calculate the voxel position considering the VOXEL_SIZE
                    glm::vec3 voxelPosition = position + glm::vec3(x * VOXEL_SIZE, y * VOXEL_SIZE, z * VOXEL_SIZE);
                    if (CheckVoxelIntersection(vertices, voxelPosition)) {
                        // Properly index into the blocks array based on VOXEL_SIZE
                        blocks[x][y][z].setActive(true);
                        inst.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        threadPool.wait();

        // Convert atomic<int> to int for the return value
        return inst.load(std::memory_order_relaxed);
    }


    bool Chunk::intersect_aabb_triangle_cgal(const CGAL::Bbox_3& aabb, const Point& p0, const Point& p1, const Point& p2) {
        std::vector<Triangle> triangles;
        triangles.push_back(Triangle(p0, p1, p2));

        Tree tree(triangles.begin(), triangles.end());

        return tree.do_intersect(aabb);
    }

    void Chunk::initializeBlocks() {
        blocks = std::make_unique<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>(ADJUSTED_CHUNK);
        culledBlocks = std::make_unique<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>(ADJUSTED_CHUNK);
        colors = std::make_unique<std::unique_ptr<std::unique_ptr<glm::vec3[]>[]>[]>(ADJUSTED_CHUNK);
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            blocks[x] = std::make_unique<std::unique_ptr<Block[]>[]>(ADJUSTED_CHUNK);
            culledBlocks[x] = std::make_unique<std::unique_ptr<Block[]>[]>(ADJUSTED_CHUNK);
            colors[x] = std::make_unique<std::unique_ptr<glm::vec3[]>[]>(ADJUSTED_CHUNK);
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                blocks[x][y] = std::make_unique<Block[]>(ADJUSTED_CHUNK);
                culledBlocks[x][y] = std::make_unique<Block[]>(ADJUSTED_CHUNK);
                colors[x][y] = std::make_unique<glm::vec3[]>(ADJUSTED_CHUNK);
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    blocks[x][y][z].setActive(false);
                    culledBlocks[x][y][z].setActive(false);
                    colors[x][y][z] = glm::vec3(0);
                }
            }
        }
    }


    void Chunk::applyCARule(glm::ivec3 terrainSize) {
        int recursionDepth = calculateRecursionDepth(terrainSize);

        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    glm::vec3 voxelGlobalPos = position + glm::vec3(x, y, z);

                    if (isVoxelinSponge(voxelGlobalPos.x, voxelGlobalPos.y, voxelGlobalPos.z, recursionDepth)) {
                        blocks[x][y][z].setActive(true);
                        instances++;

                        colors[x][y][z] = determineColorBasedOnPosition(voxelGlobalPos, terrainSize);
                    }
                }
            }
        }
    }


    glm::vec3 Chunk::determineColorBasedOnPosition(glm::vec3 voxelGlobalPos, glm::ivec3 terrainSize) {
        glm::vec3 color1(0.8f, 0.1f, 0.3f);
        glm::vec3 color2(0.1f, 0.8f, 0.3f);
        glm::vec3 color3(0.1f, 0.3f, 0.8f);

        glm::vec3 normalizedPos = voxelGlobalPos / glm::vec3(terrainSize);

        glm::vec3 modulatedColor = glm::mix(glm::mix(color1, color2, normalizedPos.x),
                                            color3,
                                            normalizedPos.y);

        modulatedColor = glm::clamp(modulatedColor, 0.0f, 1.0f);

        return modulatedColor;
    }

    int Chunk::calculateRecursionDepth(glm::ivec3 terrainSize) {
        int minSize = (terrainSize.x*ADJUSTED_CHUNK);
        int depth = -1;

        while (minSize >= pow(3, depth)) {
            depth++;
        }

        return depth;
    }

    bool Chunk::isVoxelinSponge(int x, int y, int z, int depth) {
        if (depth == 0 ||
            (x % 3 == 1 && y % 3 == 1) ||
            (x % 3 == 1 && z % 3 == 1) ||
            (y % 3 == 1 && z % 3 == 1) ||
            (x % 3 == 1 && y % 3 == 1 && z % 3 == 1)) {
            return false;
        }

        // If we're not at the base level, and this isn't a center piece, recurse towards the base level
        if (x > 0 || y > 0 || z > 0) {
            return isVoxelinSponge(x / 3, y / 3, z / 3, depth - 1);
        }

        return true;
    }

    Chunk::~Chunk() {}
}
