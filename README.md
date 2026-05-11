# Ruang — Interior Designer Simulator

**Ruang** adalah game desain interior 3D berbasis OpenGL 1.x fixed-function pipeline dan GLUT (freeglut). Pemain berperan sebagai desainer interior yang harus memenuhi brief klien dengan menata furnitur di dalam ruangan sesuai budget dan aturan yang diberikan.

Dibangun sebagai **Tugas Besar Praktikum Grafika dan Teknik Interaktif** — Program Studi Informatika 2025/2026.

## Tech Stack

- **C++17** — Standar bahasa
- **OpenGL 1.x** — Fixed-function pipeline, glBegin/glEnd
- **GLUT (freeglut 3.8+)** — Window management, input, font
- **stb_image.h** — Texture loading (single-header)
- **nlohmann/json** — glTF JSON parsing (single-header)
- **glTF 2.0** — Model format untuk 9 furniture 3D

## Build dan Run

```bash
# Linux
g++ src/*.cpp -o build/main -lGL -lGLU -lglut -std=c++17 -Isrc
./build/main

# Windows (MSYS2 UCRT64)
g++ src/*.cpp -o build/main.exe -lfreeglut -lopengl32 -lglu32 -std=c++17 -Isrc
./build/main.exe
```

## Gameplay

### Alur Permainan
1. **Menu** — Lihat brief klien, requirement, dan budget
2. **EDIT mode** — Pilih furnitur (key 1-8), tempatkan (Space), atur posisi (WASD/drag), rotasi (Q/E), hapus (X)
3. **VIEW mode** — Inspeksi 3D, orbit kamera, toggle fly-through (P)
4. **Submit** (Enter) — Sistem evaluasi: requirement, bonus rules, aesthetics
5. **WIN** — Lihat skor breakdown, lanjut ke level berikutnya
6. **LOSE** — Lihat penyebab gagal, retry atau kembali ke menu

### 3 Level

| Level | Klien | Item Wajib | Budget | Bonus |
|-------|-------|-------------|--------|-------|
| 1 | Ruang Tamu | 2 sofa, 1 meja, 1 karpet | 50 | Meja di atas karpet + Sofa dekat meja |
| 2 | Ruang Makan | 1 meja bundar, 4 kursi | 60 | Kursi dekat meja + Kursi tidak menumpuk |
| 3 | Kamar Tidur | 1 lemari, 1 lampu, 1 karpet | 45 | Lampu dekat lemari + Pintu tidak terhalang |

### Scoring (0-100)

| Komponen | Maks | Detail |
|----------|------|--------|
| Requirement | 50 | Semua item wajib terpasang |
| Bonus Rules | 30 | +10 per rule, max 3 |
| Aesthetics | 20 | Balance + Utilization |
| Penalti | -50 | -5 per placement gagal |

## Kontrol Lengkap

### Global
| Tombol | Fungsi |
|--------|--------|
| Tab | Mode EDIT ↔ VIEW |
| Enter | Submit / Mulai / Lanjut / Ulang |
| Esc | Menu / Keluar |

### Mode EDIT
| Tombol | Fungsi |
|--------|--------|
| 1 — 8 | Pilih jenis furnitur (Meja, Sofa, Kursi, Meja Bundar, Lemari, Rak, Karpet, Lampu) |
| Space | Tempatkan furnitur terpilih |
| W A S D | Geser objek terpilih |
| Q / E | Rotasi objek (-15° / +15°) |
| [ / ] | Pilih objek sebelumnya / berikutnya |
| X / Delete | Hapus objek terpilih |
| Mouse Drag | Geser objek terpilih |

### Mode VIEW
| Tombol | Fungsi |
|--------|--------|
| 1 / 2 / 3 | Preset kamera 1-point / 2-point / 3-point |
| W A S D | Gerak target kamera |
| Q / E | Naik / turun target kamera |
| Mouse Drag | Orbit kamera (yaw / pitch) |
| L | Toggle directional light |
| F | Toggle smooth / flat shading |
| Z | Toggle depth test |
| P | Toggle fly-through kamera (auto orbit) |

## Struktur Project

```
gti-bts/
├── src/                          # Source code
│   ├── main.cpp                  # Entry point, GLUT callbacks, key handling
│   ├── renderer.cpp / .h         # Rendering pipeline
│   ├── scene.cpp / .h            # Game state, objects, collision, scoring
│   ├── texture.cpp / .h          # Texture loading + procedural generation
│   ├── ui.cpp / .h               # Text rendering, window title
│   ├── gltf_loader.cpp / .h      # glTF model parser + renderer
│   ├── json.hpp                  # nlohmann/json (single-header)
│   └── stb_image.h               # stb_image (single-header)
├── assets/
│   ├── texture/                  # Texture files (PNG)
│   │   ├── carpet.png, fabric.png, metal.png, wall.png
│   │   ├── wood-floor.png, wood-grain.png
│   └── object/                   # glTF furniture models
│       ├── sofa/, table/, cabinet/, shelf/, chair/
│       ├── rounded-table/, lamp/, carpet/, fan/
│           └── scene.gltf + scene.bin
├── build/                        # Output binary (gitignored)
├── docs/
│   ├── PLAN.md                   # Rencana dan status project
│   ├── AGENTS.md                 # Identitas dan masalah proyek
│   └── README.md                 # (file ini)
└── .gitignore
```

## Penerapan Konsep Grafika

### 1. Proyeksi
- **Orthographic** (`glOrtho`): Mode EDIT — top-down view untuk penempatan objek presisi
- **Perspective** (`gluPerspective`): Mode VIEW — 60° FOV, 3 preset kamera (1/2/3-point) + free orbit

### 2. Transformasi
- Translasi (`glTranslatef`) dan rotasi (`glRotatef`) per objek via matrix stack
- Transformasi hirarkis untuk model glTF (node parenting dengan TRS + matrix)

### 3. Rendering glTF
- 9 model 3D (sofa, meja, kursi, dll) di-parse dari glTF 2.0
- `glBegin/glEnd` dengan vertex color + normal + texcoord per primitive
- FLOOR mesh filtering via material alphaMode + name check

### 4. Tekstur
- `stb_image.h` untuk loading, `gluBuild2DMipmaps()` untuk mipmap
- `GL_MODULATE` untuk menggabungkan tekstur × material color
- Procedural checker texture sebagai fallback
- Texture atlas untuk lantai (repeat) dan dinding (mapped)

### 5. Lighting & Material
- **Directional light** (LIGHT0): matahari, toggle L
- **Point light** (LIGHT1): lampu lokal dengan attenuation
- **Material system**: ROUGH (specular rendah) vs GLOSSY (specular tinggi)
- **Emission**: highlight selection, lamp glow pulsing

### 6. Shadow
- **Projective shadow + stencil buffer**: dua pass rendering
- Pass 1: render lantai ke stencil buffer dengan depth test ON
- Pass 2: render shadow hitam semi-transparan di area stencil
- Shadow hanya di VIEW mode (skip di EDIT agar tidak mengganggu)

### 7. Animasi
- **Kipas**: rotasi Y (120°/detik) via `gAnimTime` increment di Idle
- **Lampu**: emission glow pulsing via `sin(gAnimTime * 2.5)`
- **Fly-through kamera**: 8 waypoints orbit 360°, smoothstep lerp, looping (toggle P)
- **Selection highlight**: wireframe bounding box dengan pulse effect

### 8. Depth Test & Shading
- Z-buffer via `GL_DEPTH_TEST`, toggle Z
- `GL_SMOOTH` / `GL_FLAT` shading toggle via F key

### 9. Collision Detection
- AABB pairwise check saat placement + drag
- Rotation-aware via `GetRotatedBounds()` (4 corner rotasi)
- Boundary room clamp ke ±kRoomSize

## Urutan Render (`Display()`)

1. `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)`
2. Set projection matrix (ortho / perspective)
3. Set view matrix (`gluLookAt`)
4. Setup lights (LIGHT0 + LIGHT1)
5. Draw room (floor + walls with textures)
6. RenderShadows (projective + stencil, VIEW mode only)
7. DrawSceneObjects (loop scene objects with glTF models)
8. Draw selection highlight (EDIT mode)
9. RenderOverlay / RenderHUD (menu, score, budget, checklist)
10. `glutSwapBuffers()`
