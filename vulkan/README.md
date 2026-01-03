# 3D Rover Explorer in Vulkan

A simple Vulkan-based 3D rover explorer featuring hierarchical robot arm animation, dynamic skybox, shadow mapping, spotlight and more.

## Preview

![3D Rover Explorer Demo](/assets/gif/shot1.jpg)

## Features

- Robot arm animation
- Dynamic skybox (day/night cycle)
- Flashlight from robot arm
- Shadow mapping
- OBJ model loading via Assimp
- Phong shading
- Simple camera controls (orbit, pan, zoom)
- Cross-platform build with CMake

## Controls

| Key              | Description                          |
| ---------------- | ------------------------------------ |
| WASD             | Move camera                          |
| Mouse drag       | Pan camera                           |
| Mouse left click | Boost flashlight intensity           |
| I / K            | Raise / lower the upper arm          |
| U / J            | Raise / lower the lower arm          |
| O / L            | Raise / lower the wrist (flashlight) |
| Left Shift       | Run / speed boost while moving       |
| Space            | Speed up Sun rotation                |

## Prerequisites:

First, install the dependencies based on your operating system.

#### MacOS

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
    vulkan-tools \
    libvulkan-dev \
    vulkan-validationlayers \
    spirv-tools \
    glslang-tools \
    libassimp-dev \
    vulkan-utility-libraries-dev
  git lfs install
  ```

### General

- (Recommended) Install Visual Studio Code for development/building
- Clone this repository:
  ```sh
  # make sure you have git lfs first
  git clone https://github.com/benyoon1/vulkan.git
  ```

## Build & Run

1. In VS Code, press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows/Linux) to open the command palette.
2. Type `Tasks: Run Task` and select it.
3. Select `CMake: configure && build && run (Debug)` to generate build files via CMake, build, and run the executable in one step.
