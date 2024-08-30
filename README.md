<img align="center" padding="2" src="data/images/arxivision/arxivisionlogo_black.png"/>
ArXiVision is a Vulkan-based engine designed as a personal playground for exploring real-time optimization solutions and rendering techniques. It currently focuses on rendering rasterized voxels by importing .vox files. There are future plans for terrain generation using cellular automata.

- <img align="left" width="32" src="https://i.pinimg.com/736x/99/65/5e/99655e9fe24eb0a7ea38de683cedb735.jpg"/>For more frequent updates you can follow me on <a href="https://twitter.com/Kiiiri7">X</a>.
  
- <img align="left" width="32" src="https://www.freeiconspng.com/uploads/youtube-icon-app-logo-png-9.png"/>Demo videos on <a href="https://www.youtube.com/@kiriakosgavras108">YouTube</a>.

### Features
#### Rendering
- [x] Frustum and Hierarchical-Z Occlusion Culling on GPU
- [x] Multi Draw Instanced Indirect Draw with GPU generated commands
- [x] Screen Space Ambient Occlusion (SSAO)
- [x] Clustered Deferred Area Lights
- [ ] Shadows

#### General
- [x] Importing .vox format scenes
- [x] Editor (WIP)

### Render Graph
<img align="center" padding="2" src="data/images/arxivision/Flowchart.jpg"/>

# Media
| Video: Area Lights | Video: Photogrammetry Scene |
|:-:|:-:|
|[![Image1](data/images/arxivision/Clustered%20Deferred%20Area%20Lights.png)](https://www.youtube.com/watch?v=KPrkTDQyz8M) | [![Image2](data/images/arxivision/HintzeHall.png)](https://twitter.com/Kiiiri7/status/1822658585152676118/video/1)

#### Screenshots
<img align="center" padding="2" src="data/images/arxivision/monuments.png"/>


## Build Instructions
### Prerequisites

Before you start, install the following:
- **CMake**: Install it via Homebrew with `brew install cmake`
- **glslang**: Install it via Homebrew with `brew install glslang`.

### Build 
```bash
git clone https://github.com/Qirias/Arxivision
cd Arxivision
chmod +x shaders/compile.sh
```
### Xcode
```bash
chmod +x xcode.sh
./xcode.sh
open xcode_build/ArXiVision.xcodeproj
```

### Standalone (only macOS for now)
```bash
chmod +x general.sh
./general.sh
```

### License
Please adhere to the <a href="https://en.wikipedia.org/wiki/MIT_License">MIT license</a>