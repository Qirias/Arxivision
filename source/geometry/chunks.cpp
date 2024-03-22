#include "chunks.h"

#include <iostream>

namespace arx {

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, std::vector<ArxModel::Vertex>& vertices, glm::ivec3 terrainSize) : position{pos} {
        initializeBlocks();
        
        std::vector<InstanceData> tmpInstance;
        tmpInstance.resize(applyCARule(terrainSize));
        
        uint32_t instances = 0;
        float scale = 1.0f;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    //int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
                    if (!blocks[x][y][z].isActive()) continue;
                    tmpInstance[instances].color = glm::vec3(colors[x][y][z]);
                    
                    tmpInstance[instances].translation = glm::vec3(static_cast<float>(x) + position.x,
                                                                   static_cast<float>(y) + position.y,
                                                                   static_cast<float>(z) + position.z)*glm::vec3(scale);
                    instances++;
                }
            }
        }
        
        std::cout << "Instances drawn: " << instances << std::endl;
        if (instances > 0)
        {
            instanceData.push_back(tmpInstance);
            
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            cube.model = cubeModel;
            voxel.emplace(cube.getId(), std::move(cube));
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

        std::atomic<int> instances(0);

        int totalVoxels = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
        int voxelsPerThread = totalVoxels / numThreads;

        for (unsigned int t = 0; t < numThreads; ++t) {
            int startIdx = t * voxelsPerThread;
            int endIdx = (t + 1) * voxelsPerThread;
            if (t == numThreads - 1) {
                endIdx = totalVoxels;
            }

            threadPool.threads[t]->addJob([this, startIdx, endIdx, &vertices, &instances]() {
                for (int idx = startIdx; idx < endIdx; ++idx) {
                    int x = idx / (CHUNK_SIZE * CHUNK_SIZE);
                    int y = (idx / CHUNK_SIZE) % CHUNK_SIZE;
                    int z = idx % CHUNK_SIZE;
                    
                    glm::vec3 voxelPosition = position + glm::vec3(x, y, z);
                    if (CheckVoxelIntersection(vertices, voxelPosition)) {
                        blocks[x][y][z].setActive(true);
                        instances.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        blocks[x][y][z].setActive(false);
                    }
                }
            });
        }

        threadPool.wait();

        // Convert atomic<int> to int for the return value
        return instances.load(std::memory_order_relaxed);
    }



    bool Chunk::intersect_aabb_triangle_cgal(const CGAL::Bbox_3& aabb, const Point& p0, const Point& p1, const Point& p2) {
        std::vector<Triangle> triangles;
        triangles.push_back(Triangle(p0, p1, p2));

        Tree tree(triangles.begin(), triangles.end());

        return tree.do_intersect(aabb);
    }

    void Chunk::initializeBlocks() {
        blocks = std::make_unique<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>(CHUNK_SIZE);
        colors = std::make_unique<std::unique_ptr<std::unique_ptr<glm::vec3[]>[]>[]>(CHUNK_SIZE);
        for (int x = 0; x < CHUNK_SIZE; x++) {
            blocks[x] = std::make_unique<std::unique_ptr<Block[]>[]>(CHUNK_SIZE);
            colors[x] = std::make_unique<std::unique_ptr<glm::vec3[]>[]>(CHUNK_SIZE);
            for (int y = 0; y < CHUNK_SIZE; y++) {
                blocks[x][y] = std::make_unique<Block[]>(CHUNK_SIZE);
                colors[x][y] = std::make_unique<glm::vec3[]>(CHUNK_SIZE);
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    blocks[x][y][z].setActive(false);
                    colors[x][y][z] = glm::vec3(0);
                }
            }
        }
    }

int Chunk::applyCARule(glm::ivec3 terrainSize) {
    int instances = 0;
    int recursionDepth = calculateRecursionDepth(terrainSize);

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                glm::vec3 voxelGlobalPos = position + glm::vec3(x, y, z);

                if (isVoxelinSponge(voxelGlobalPos.x, voxelGlobalPos.y, voxelGlobalPos.z, recursionDepth)) {
                    blocks[x][y][z].setActive(true);
                    instances++;

                    colors[x][y][z] = determineColorBasedOnPosition(voxelGlobalPos, terrainSize);
                } else {
                    blocks[x][y][z].setActive(false);
                }
            }
        }
    }

    return instances;
}


    glm::vec3 Chunk::determineColorBasedOnPosition(glm::vec3 voxelGlobalPos, glm::ivec3 terrainSize) {
        glm::vec3 yellowBase(0.95f, 0.95f, 0.2f);
       
        float redModulation = (std::sin(voxelGlobalPos.x * 0.1f) + 1.0f) * 0.5f;
        float greenModulation = (std::cos(voxelGlobalPos.y * 0.1f) + 1.0f) * 0.5f;

        glm::vec3 modulatedColor = glm::vec3(
            yellowBase.r * redModulation,
            yellowBase.g * greenModulation,
            yellowBase.b
        );

        modulatedColor = glm::clamp(modulatedColor, 0.0f, 1.0f);

        return modulatedColor;
    }

    int Chunk::calculateRecursionDepth(glm::ivec3 terrainSize) {
        int minSize = std::min({terrainSize.x, terrainSize.y, terrainSize.z});
        int depth = 0;

        while (minSize >= pow(3, depth)) {
            depth++;
        }

        return depth - 1;
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


//    int Chunk::applyCARule(glm::ivec3 terrainSize) {
//        int instances = 0;
//        for (int x = 0; x < CHUNK_SIZE; x++) {
//            for (int y = 0; y < CHUNK_SIZE; y++) {
//                for (int z = 0; z < CHUNK_SIZE; z++) {
//                    // Simple rule for an initial fractal-like structure
//                    if ((x ^ y ^ z) % 21 == 0) { // Example condition, not an actual fractal rule
//                        blocks[x][y][z].setActive(true);
//                        colors[x][y][z] = glm::vec3(x / static_cast<float>(CHUNK_SIZE),
//                                                    y / static_cast<float>(CHUNK_SIZE),
//                                                    z / static_cast<float>(CHUNK_SIZE));
//                        instances++;
//                    } else {
//                        blocks[x][y][z].setActive(false);
//                    }
//                }
//            }
//        }
//        
//        return instances;
//    }




    Chunk::~Chunk() {
        
    }
}
