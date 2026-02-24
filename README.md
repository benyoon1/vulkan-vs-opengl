# MoltenVK vs OpenGL 4.1 API Overhead Benchmark

A simple benchmarking application that compares the CPU/GPU overhead and performance of Vulkan and OpenGL.

It features two scenes:

- Synthetic (asteroid belt) to stress test raw draw call overhead
- Amazon Lumberyard Bistro to test in a more practical setting

For now, the benchmark focuses on MoltenVK vs OpenGL 4.1 on macOS, but it can run on Linux and Windows. However, it won't be a fair comparison since modern AZDO techniques such as Multi-Draw Indirect or Bindless Textures in OpenGL 4.6 are not implemented in this project.

![synthetic-vk](assets/screenshots/synthetic-vk.png)
![bistro-gl](assets/screenshots/bistro-gl.png)

## Features

- Instancing
- Phong shading
- Shadow mapping (PCF)
- OBJ loading via Assimp
- Metrics
  - frame time
  - CPU draw time (command buffer recording)
  - GPU draw time
  - num of triangles
  - num of draw calls
  - FPS
  - 1% low FPS
  - 0.1% low FPS
  - frame time graph

Vulkan specific features:

- Bindless textures (descriptor indexing)
- Dynamic rendering
- Metrics
  - fence wait time
  - flush time
  - submit time
  - present time

## Controls

| Key        | Description              |
| ---------- | ------------------------ |
| WASD       | Move camera              |
| Mouse drag | Pan camera               |
| Left Shift | Speed boost while moving |

## Prerequisites:

First, install the dependencies based on your operating system.

#### macOS

- Install the latest Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home)
- Install CMake (version 3.10 or higher)
- Install Homebrew
- Install dependencies:
  ```sh
  brew install assimp git-lfs
  git lfs install   # for obj assets
  ```
- Add the following environment variables in `~/.zshrc`:
  ```sh
  export VULKAN_SDK=/path/to/VulkanSDK/version
  export PATH="$VULKAN_SDK/bin:$PATH"
  export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH"
  ```

#### Linux (Ubuntu)

- Install dependencies:
  ```sh
  sudo apt update
  sudo apt install -y \
      git-lfs \
      build-essential \
      ninja-build \
      libassimp-dev \
      # vulkan
      vulkan-tools \
      libvulkan-dev \
      vulkan-validationlayers \
      spirv-tools \
      glslang-tools \
      libassimp-dev \
      vulkan-utility-libraries-dev
      # opengl
      libglfw3-dev \
      libgl1-mesa-dev \
      libx11-dev \
      libpthread-stubs0-dev \
      libxrandr-dev \
      libxi-dev \
  git lfs install
  ```

### General

- (Recommended) Install Visual Studio Code for development/building
- Clone this repository:
  ```sh
  # make sure you have git lfs first
  git clone https://github.com/benyoon1/vulkan-vs-opengl.git
  ```

## Build & Run

1. In VS Code, press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows/Linux) to open the command palette.
2. Type `Tasks: Run Task` and select it.
3. Select `Vulkan: generate build files & build & run (Release)` (or `OpenGL: generate build files & build & run (Release)`) to generate build files via CMake, build, and run the executable in one step.
4. If you do not want to build with `Tasks: Run Task` in VS Code, refer to `tasks.json` to generate build files, build, and run in the terminal.

## Acknowledgements

- [LearnOpenGL](https://learnopengl.com/) for OpenGL tutorials and code references
- [Vulkan Guide](https://vkguide.dev/) for Vulkan tutorials and code references
