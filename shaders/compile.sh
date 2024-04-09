cd /Users/kiri/Documents/ArXivision/ArXivision/shaders
echo "Compiling shaders..."
glslangValidator -V simple_shader.vert -o vert.spv
glslangValidator -V simple_shader.frag -o frag.spv

glslangValidator -V depth_pyramid.comp -o depth_pyramid.spv

echo "Done."
