# Vulkan vs OpenGL : Performance Comparison

A simple renderer to compare the performance (fps) of two graphics APIs: Vulkan and OpenGL. The scene features a star with an asteroid belt. Every object (asteroid) is a single draw call and it is intentional; compute shaders or instancing would be far more performant but the purpose of this test is to quantify the driver overhead of each graphics API, on different platforms.

## Features

- Shadow mapping (off for now)
- OBJ model loading via Assimp
- Phong shading
- Simple camera controls (orbit, pan)
- Cross-platform (macOS/Linux) build via CMake

## Controls

| Key        | Description                          |
| ---------- | ------------------------------------ |
| WASD       | Move camera                          |
| Mouse drag | Pan camera                           |
| J / K      | Increase / Decrease num of asteroids |
| Left Shift | Speed boost while moving             |

## Prerequisites:

First, install the dependencies based on your operating system.

#### macOS

- Install Vulkan SDK from [LunarG](https://vulkan.lunarg.com/sdk/home)
- Install CMake (version 3.10 or higher)
- Install Homebrew
- Install dependencies:
    ```sh
    brew install assimp git-lfs
    git lfs install   # for obj assets
    ```
- Set `VULKAN_SDK` environment variable to point to the Vulkan SDK installation path.
    ```sh
    export VULKAN_SDK=/path/to/VulkanSDK/version
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
3. Select `Vulkan: configure && build && run (RelWithDebInfo)` (or `OpenGL: configure && build && run (RelWithDebInfo)`) to generate build files via CMake, build, and run the executable in one step.

## FYI

If you build and run in Debug, the flag will activate the validation layer in Vulkan, so the performance will drop significantly. For proper performance test, build/run in RelWithDebInfo/Release.
