cd /Users/kiri/Documents/ArXivision/ArXivision/shaders
echo "Compiling shaders..."
glslangValidator -V simple_shader.vert -o vert.spv
glslangValidator -V simple_shader.frag -o frag.spv

glslangValidator -V depth_pyramid.comp -o depth_pyramid.spv
glslangValidator -V occlusion_culling.comp -o occlusion_culling.spv
glslangValidator -V occlusionEarly_culling.comp -o occlusionEarly_culling.spv

glslangValidator -V gbuffer.vert -o gbuffer_vert.spv
glslangValidator -V gbuffer.frag -o gbuffer_frag.spv

echo "Done."
