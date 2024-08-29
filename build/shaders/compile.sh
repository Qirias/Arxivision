echo "Compiling shaders..."

glslangValidator -V depth_pyramid.comp -o depth_pyramid.spv
glslangValidator -V occlusion_culling.comp -o occlusion_culling.spv
glslangValidator -V occlusionEarly_culling.comp -o occlusionEarly_culling.spv

glslangValidator -V fullscreen.vert -o fullscreen.spv
glslangValidator -V composition.frag -o composition.spv
glslangValidator -V ssao.frag -o ssao.spv
glslangValidator -V ssaoBlur.frag -o ssaoBlur.spv
glslangValidator -V deferred.frag -o deferred.spv

glslangValidator -V frustum_clusters.comp -o frustum_clusters.spv
glslangValidator -V cluster_cullLight.comp -o cluster_cullLight.spv

glslangValidator -V gbuffer.vert -o gbuffer_vert.spv
glslangValidator -V gbuffer.frag -o gbuffer_frag.spv

echo "Done."