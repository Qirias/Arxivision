#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <atomic>

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
        
        Chunk(ArxDevice &device, const glm::vec3& position, ArxGameObject::Map& voxel, std::vector<ArxModel::Vertex>& vertices);
        ~Chunk();
        
        void Update();
        void Render();
        glm::vec3 getPosition() const { return position; }
        
        int Voxelize(const std::vector<arx::ArxModel::Vertex>& vertices);
        inline bool intersect_aabb_triangle(const glm::vec3& minBox, const glm::vec3& maxBox, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
        bool CheckVoxelIntersection(const std::vector<arx::ArxModel::Vertex>& vertices, const glm::vec3& voxelPosition);
        inline bool point_inside_aabb(const glm::vec3& point, const glm::vec3& minBox, const glm::vec3& maxBox);
        inline bool aabb_edge_intersects_triangle_face(const glm::vec3& minBox, const glm::vec3& maxBox, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);
        bool intersect_aabb_triangle_cgal(const CGAL::Bbox_3& aabb, const Point& p0, const Point& p1, const Point& p2);
    private:
        Block                                       ***blocks;
        glm::vec3                                   position;
        std::vector<std::vector<InstanceData>>      instanceData;
    };
}
