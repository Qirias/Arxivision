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
                    tmpInstance[index].color = glm::vec3(0.5f);
                    tmpInstance[index].translation = translation;
                    index++;
                }
            }
        }
        
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            id = cube.getId();
            cube.model = cubeModel;
            voxel.emplace(id, std::move(cube));
            instanceData[id] = tmpInstance;
        }
    }

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, const std::vector<std::vector<float>>& heightMap, const glm::ivec3& globalOffset) : position(pos) {
        
        initializeBlocks();
        
        std::vector<InstanceData> tmpInstance;
        activateVoxelsFromHeightmap(heightMap, globalOffset);
        smoothTerrain();
        colorVoxels();
//        deactivateHiddenVoxels();
        tmpInstance.resize(instances);
        
        uint32_t index = 0;
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    if (!blocks[x][y][z].isActive()) continue;
                    glm::vec3 translation = glm::vec3(x*VOXEL_SIZE, y*VOXEL_SIZE, z*VOXEL_SIZE) + position;
                    tmpInstance[index].color = colors[x][y][z];
                    tmpInstance[index].translation = translation;
                    index++;
                }
            }
        }
        
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            cube.model = cubeModel;
            id = cube.getId();
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
                    tmpInstance[instances].color = colors[x][y][z];
                    tmpInstance[instances].translation = translation;
                    instances++;
                }
            }
        }

//        std::cout << "Instances drawn: " << instances << std::endl;
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            cube.model = cubeModel;
            id = cube.getId();
            voxel.emplace(id, std::move(cube));
            instanceData[id] = tmpInstance;
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
                          voxelPosition.x + VOXEL_SIZE, voxelPosition.y + VOXEL_SIZE, voxelPosition.z + VOXEL_SIZE);

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

        // Adjust the total voxels based on the VOXEL_SIZE
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
  
    void Chunk::activateVoxelsFromHeightmap(const std::vector<std::vector<float>>& heightMap, const glm::ivec3& globalOffset) {
        // Iterate over each voxel in the chunk
        
        int maxHeight = static_cast<int>(heightMap[0].size());
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                // Convert local chunk coordinates to global heightmap coordinates
                int globalX = x + globalOffset.x;
                int globalZ = z + globalOffset.z;

                // Ensure global coordinates are within the bounds of the heightmap
                if (globalX >= 0 && globalX < heightMap.size() && globalZ >= 0 && globalZ < heightMap[globalX].size()) {
                    // Get the height from the heightmap and adjust for the actual height range
                    float heightValue = heightMap[globalX][globalZ]; // Assuming this is normalized to [0, maxHeight]

                    // Flip the height by adjusting how we activate voxels based on the Y position
                    for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                        int globalY = y + globalOffset.y;
                        // Instead of activating voxels below the heightmap value, activate those above it
                        // Adjust this condition based on how your heights are represented in the heightMap
                        if (maxHeight - globalY <= heightValue) {
                            blocks[x][y][z].setActive(true);
                            instances++;
                        }
                    }
                } else {
                    // Deactivate all voxels if outside the heightmap bounds
                    for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                        blocks[x][y][z].setActive(false);
                    }
                }
            }
        }
    }

    int Chunk::countActiveNeighbors(int x, int y, int z) {
        int count = 0;
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                for (int k = -1; k <= 1; k++) {
                    if (i == 0 && j == 0 && k == 0) continue; // Skip the center voxel itself
                    
                    int nx = x + i;
                    int ny = y + j;
                    int nz = z + k;
                    
                    // Check bounds
                    if (nx >= 0 && nx < ADJUSTED_CHUNK && ny >= 0 && ny < ADJUSTED_CHUNK && nz >= 0 && nz < ADJUSTED_CHUNK) {
                        if (blocks[nx][ny][nz].isActive()) {
                            count++;
                        }
                    }
                }
            }
        }
        return count;
    }

    void Chunk::smoothTerrain() {
        auto newBlocks = std::make_unique<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>(ADJUSTED_CHUNK);
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            newBlocks[x] = std::make_unique<std::unique_ptr<Block[]>[]>(ADJUSTED_CHUNK);
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                newBlocks[x][y] = std::make_unique<Block[]>(ADJUSTED_CHUNK);
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    newBlocks[x][y][z].setActive(blocks[x][y][z].isActive());
                }
            }
        }
        
        const int thresholdDeactivate = 4;
        const int thresholdActivate = 5;
        
        // Apply smoothing logic
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    int activeNeighbors = countActiveNeighbors(x, y, z);
                    
                    
                    if (blocks[x][y][z].isActive() && activeNeighbors < thresholdDeactivate) {
                        newBlocks[x][y][z].setActive(false);
                        instances--;
                    } else if (!blocks[x][y][z].isActive() && activeNeighbors > thresholdActivate) {
                        newBlocks[x][y][z].setActive(true);
                        instances++;
                    }
                }
            }
        }
        
        blocks.swap(newBlocks);
    }

    void Chunk::colorVoxels() {
        glm::vec3 green(0.0f, 1.0f, 0.0f); // Green for the top voxels
        glm::vec3 brown(0.55f, 0.27f, 0.07f); // Brown for the rest

        // Iterate over each column in the chunk
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                // Find the highest active voxel in the current column
                int highestActiveY = -1;
                for (int y = ADJUSTED_CHUNK - 1; y >= 0; y--) {
                    if (blocks[x][y][z].isActive()) {
                        highestActiveY = y;
                        break; // Stop at the first active voxel found from top to bottom
                    }
                }

                // Color only the highest active voxel green, and the rest below it brown
                for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                    if (blocks[x][y][z].isActive()) {
                        colors[x][y][z] = (y == highestActiveY) ? green : brown;
                    }
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
        int minSize = (terrainSize.x*ADJUSTED_CHUNK);
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

    Chunk::~Chunk() {
        
    }
}
