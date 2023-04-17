cd /Users/kiri/Documents/ArXivision/ArXivision/shaders
echo "Compiling shaders..."
glslangValidator -V simple_shader.vert -o vert.spv
glslangValidator -V simple_shader.frag -o frag.spv

glslangValidator -V point_light.vert -o point_light.vert.spv
glslangValidator -V point_light.frag -o point_light.frag.spv
echo "Done."
