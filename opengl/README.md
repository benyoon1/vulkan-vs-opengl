# 3D Rover Explorer with OpenGL

A simple OpenGL-based 3D rover explorer featuring hierarchical robot arm animation, dynamic skybox, shadow mapping, spotlight and more.

## Preview

![3D Rover Explorer Demo](/assets/gif/valley1.gif)
![3D Rover Explorer Demo](/assets/gif/valley2.gif)
![3D Rover Explorer Demo](/assets/gif/valley3.gif)

## Features

- Robot arm animation
- Dynamic skybox (day/night cycle)
- Flashlight from robot arm
- Shadow mapping
- OBJ model loading via Assimp
- Phong lighting
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

- Install Homebrew
- Install dependencies:
    ```sh
    brew install glfw assimp git-lfs
    git lfs install   # for obj assets
    ```

#### Linux (Ubuntu)

- Install dependencies:
    ```sh
    sudo apt update
    sudo apt install -y \
        libglfw3-dev \
        libassimp-dev \
        libgl1-mesa-dev \
        libx11-dev \
        libpthread-stubs0-dev \
        libxrandr-dev \
        libxi-dev \
        git-lfs \
        build-essential \
        ninja-build
    git lfs install
    ```

### General

- (Recommended) Install Visual Studio Code for development/building
- Clone this repository:

    ```sh
    # make sure you have git lfs first
    git clone https://github.com/benyoon1/OpenGL.git
    ```

## Build & Run

1. In VS Code, press `Cmd+Shift+P` (Mac) or `Ctrl+Shift+P` (Windows/Linux) to open the command palette.
2. Type `Tasks: Run Task` and select it.
3. Select `CMake: configure && build && run (Debug)` to generate build files via CMake, build, and run the executable in one step.
