# engine-public — OpenGL Rendering & Game Engine (WIP)

A personal C++ game engine / renderer focused on real-time 3D graphics.  
This repository contains the **engine + samples**

> **Tech:** C++ • OpenGL • GLSL • Premake5 → Visual Studio 2022 • Git LFS (large assets)

---

## Highlights

### Rendering
- **PBR pipeline**
- **Deferred rendering**
- **HDR pipeline** (tonemapping/exposure workflow)
- **CSM (Cascaded Shadow Maps)**
- **Soft shadows**
- **Area lights**
- **Volumetric clouds**
- **CPU + GPU particle systems**

### World / Simulation
- **Procedural terrain generator**
- **Custom 3D physics engine**
- **Custom animation system**

### Other
- **Custom tcp/winsock2 library**
- **Hot-reload scripting**

…and more (work in progress).

---

## Requirements (Windows)

- Windows 10/11
- **Visual Studio 2022** (Desktop development with C++)
- **Premake5** (to generate `.sln`)
- **Git LFS** (required to fetch large assets like `.hdr`)

---

## Build & Run (Visual Studio 2022)

### 1) Clone (with LFS)
```bash
git lfs install
git clone https://github.com/mgoongaBabangida/engine-public.git
cd engine-public
git lfs pull

### 2) Generate Visual Studio solution (Premake) premake5.lua
### 3) See BuildInstructions.txt file
### 4) SandBoxGame.exe as startup project