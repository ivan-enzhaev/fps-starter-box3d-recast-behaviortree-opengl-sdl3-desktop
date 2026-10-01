# SDL3 Box3D FPS + Recast Navigation + BehaviorTree.CPP Starter

A cross-platform 3D First-Person Controller starter in modern C++ with **SDL3**, **Box3D** physics, **Recast/Detour** crowd pathfinding, **BehaviorTree.CPP** enemy AI, **cgltf** model loading, and **OpenGL 3.3** debug visualization.

---

## 📖 Step-by-Step Build Tutorial

Follow the full guide to build and run the project on Windows and Ubuntu:

👉 **[First-Person Controller with Box3D Physics, Recast Navigation, and BehaviorTree.CPP Guide](http://ivan-enzhaev.github.io/tutorials/sdl3-opengl/desktop/setup-first-person-controller-box3d-recast-behaviortree-desktop)**

---

## 🕹️ Controls

| Key | Action |
| :--- | :--- |
| <kbd>W</kbd> <kbd>A</kbd> <kbd>S</kbd> <kbd>D</kbd> | Move |
| **Mouse** | Look around (first-person) |
| <kbd>Space</kbd> | Jump |
| <kbd>P</kbd> / <kbd>F1</kbd> | Toggle Debug Overlays (Box3D wireframes, NavMesh & AI states) |
| <kbd>G</kbd> | Toggle Ground Texture (hides floor for clear debug lines) |
| <kbd>Esc</kbd> | Release mouse cursor |

---

## 🚀 Quick Run

### Windows (MinGW)
```cmd
config-exe.bat
build-exe.bat
run-exe.bat
```

### Ubuntu (Linux)
```bash
chmod +x config-bin.sh build-bin.sh run-bin.sh
./config-bin.sh
./build-bin.sh
./run-bin.sh
```