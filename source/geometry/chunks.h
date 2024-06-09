#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/noise.hpp>

#include <vector>
#include <atomic>
#include <random>

#include "arx_pipeline.h"
#include "arx_frame_info.h"
#include "arx_game_object.h"
#include "block.h"
#include "arx_model.h"
#include "threadpool.h"

// cgal
#include <CGAL/Simple_cartesian.h>
#include <CGAL/AABB_triangle_primitive.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>

typedef CGAL::Simple_cartesian<double> K;
typedef K::FT FT;
typedef K::Point_3 Point;
typedef K::Triangle_3 Triangle;
typedef std::vector<Triangle>::iterator Iterator;
typedef CGAL::AABB_triangle_primitive<K, Iterator> Primitive;
typedef CGAL::AABB_traits<K, Primitive> AABB_triangle_traits;
typedef CGAL::AABB_tree<AABB_triangle_traits> Tree;


namespace arx {
    
    class ArxModel;
    
    class Chunk {
    public:

        Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, std::vector<ArxModel::Vertex>& vertices);
        Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, glm::ivec3 terrainSize);
        Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, const std::vector<InstanceData>& instanceDataVec);
        ~Chunk();

        void Update();
        void Render();
        glm::vec3 getPosition() const { return position; }
        unsigned int getID() const { return id; }
        uint32_t getInstanceCount() const { return instances; }

        int Voxelize(const std::vector<arx::ArxModel::Vertex>& vertices);
        bool CheckVoxelIntersection(const std::vector<arx::ArxModel::Vertex>& vertices, const glm::vec3& voxelPosition);
        bool intersect_aabb_triangle_cgal(const CGAL::Bbox_3& aabb, const Point& p0, const Point& p1, const Point& p2);
        void deactivateHiddenVoxels();
        
        void applyCARule(glm::ivec3 terrainSize);
        glm::vec3 determineColorBasedOnPosition(glm::vec3 voxelGlobalPos, glm::ivec3 terrainSize);
        int calculateRecursionDepth(glm::ivec3 terrainSize);
        bool isVoxelinSponge(int x, int y, int z, int depth);
        
    private:
        std::unique_ptr<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>      blocks;
        std::unique_ptr<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>      culledBlocks;
        std::unique_ptr<std::unique_ptr<std::unique_ptr<glm::vec3[]>[]>[]>  colors;
        
        glm::vec3                                                           position;
        std::map<unsigned int, std::vector<InstanceData>>                   instanceData;
        uint32_t                                                            instances = 0;
        unsigned int                                                        id = -1;

        void initializeBlocks();
    };
}
