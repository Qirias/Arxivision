#include "chunks.h"

#include <iostream>

namespace arx {

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, std::vector<ArxModel::Vertex>& vertices) : position{pos} {
        blocks = new Block **[CHUNK_SIZE];
        for (int i = 0; i < CHUNK_SIZE; i++) {
            blocks[i] = new Block *[CHUNK_SIZE];
            for (int j = 0; j < CHUNK_SIZE; j++) {
                blocks[i][j] = new Block[CHUNK_SIZE];
            }
        }
        
        std::vector<InstanceData> tmpInstance;
        tmpInstance.resize(Voxelize(vertices));
        
        uint32_t instances = 0;
        float scale = 1.0f;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    //int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
                    if (!blocks[x][y][z].isActive()) continue;
                    tmpInstance[instances].color = glm::vec3(x / static_cast<float>(CHUNK_SIZE),
                                                             y / static_cast<float>(CHUNK_SIZE),
                                                             z / static_cast<float>(CHUNK_SIZE));
                    
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

        // Perform intersection check for each triangle in the object's mesh
        for (size_t i = 0; i < vertices.size(); i += 3) {
            // Convert to CGAL Point
            Point v0(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);
            Point v1(vertices[i + 1].position.x, vertices[i + 1].position.y, vertices[i + 1].position.z);
            Point v2(vertices[i + 2].position.x, vertices[i + 2].position.y, vertices[i + 2].position.z);
            
            // Check for intersection between voxel and triangle using CGAL
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

        std::atomic<int> instances(0); // Atomic counter for thread-safe increments

        int totalVoxels = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
        int voxelsPerThread = totalVoxels / numThreads;

        for (unsigned int t = 0; t < numThreads; ++t) {
            int startIdx = t * voxelsPerThread;
            int endIdx = (t + 1) * voxelsPerThread;
            if (t == numThreads - 1) {
                endIdx = totalVoxels;
            }

            // Lambda captures instances by reference
            threadPool.threads[t]->addJob([this, startIdx, endIdx, &vertices, &instances]() {
                for (int idx = startIdx; idx < endIdx; ++idx) {
                    int x = idx / (CHUNK_SIZE * CHUNK_SIZE);
                    int y = (idx / CHUNK_SIZE) % CHUNK_SIZE;
                    int z = idx % CHUNK_SIZE;
                    
                    glm::vec3 voxelPosition = position + glm::vec3(x, y, z);
                    if (CheckVoxelIntersection(vertices, voxelPosition)) {
                        blocks[x][y][z].setActive(true);
                        instances.fetch_add(1, std::memory_order_relaxed); // Safely increment instances
                    } else {
                        blocks[x][y][z].setActive(false);
                    }
                }
            });
        }

        threadPool.wait(); // Wait for all threads to complete their jobs

        // Convert atomic<int> to int for the return value
        return instances.load(std::memory_order_relaxed);
    }



    bool Chunk::intersect_aabb_triangle_cgal(const CGAL::Bbox_3& aabb, const Point& p0, const Point& p1, const Point& p2) {
        std::vector<Triangle> triangles;
        // Insert the triangle to check
        triangles.push_back(Triangle(p0, p1, p2));

        // Construct the AABB tree with a single triangle
        Tree tree(triangles.begin(), triangles.end());

        // Query the tree for intersection with AABB
        return tree.do_intersect(aabb);
    }


    Chunk::~Chunk() {
        for (int i = 0; i < CHUNK_SIZE; ++i) {
              for (int j = 0; j < CHUNK_SIZE; ++j) {
                  delete[] blocks[i][j];
              }
              delete[] blocks[i];
          }
          delete[] blocks;
    }
}
