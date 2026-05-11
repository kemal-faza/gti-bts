# PLAN - GTI BTS: Interior Designer Simulator

## 1. Status Implementasi vs Spesifikasi Proyek

| Spesifikasi | Status | Detail Implementasi |
|-------------|--------|---------------------|
| a. Primitif Drawing | ✅ | `glBegin/glEnd` (glTF models), `DrawTexturedCube`, `gluCylinder`+`gluDisk`, `glutSolidSphere`, `GL_LINES` |
| b. Translasi & Rotasi | ✅ | `glTranslatef`/`glRotatef`, WASD move, drag-to-move, Q/E rotate 15°, orbit camera |
| c. Proyeksi | ✅ | Ortho (top-down edit) + Perspektif (60° FOV), Tab toggle |
| c. Animasi | ✅ | Kipas berputar (`gAnimTime * 120°`), lamp glow pulse, **fly-through kamera** (8 waypoint orbit via tombol P) |
| d. Kamera | ✅ | 3 preset (1/2/3-point) + free orbit + fly-through + ortho top-down |
| d. Depth | ✅ | `GL_DEPTH_TEST`, toggle Z key |
| d. Lighting | ✅ | Directional LIGHT0 + Point LIGHT1 + material ROUGH/GLOSSY, toggle L |
| e. Tekstur | ✅ | `stb_image.h` + `gluBuild2DMipmaps` + `GL_MODULATE`. Floor, wall, glTF textures, procedural checker fallback. |
| e. Bayangan | ✅ | Projective shadow + stencil buffer, `GL_LEQUAL` depth test, skip in edit mode |
| f. Interaksi Antar Objek | ✅ | AABB collision (`CanPlaceAt`), rotation-aware (`GetRotatedBounds`), mouse pick, boundary clamp, drag-to-move |
| **Game System** | ✅ | State machine `MENU → PLAYING → WIN/LOSE`, 3 level, scoring (0-100), HUD, overlay |

### Fitur yang Sudah Dibangun

| Fitur | Detail |
|-------|--------|
| **glTF Model Loader** | 9 model 3D (sofa, meja, lemari, rak, kursi, meja bundar, lampu, karpet, kipas) |
| **ObjectSubType enum** | Membedakan varian furniture dalam ObjectType yang sama |
| **Selection highlight** | Wireframe bounding box + pulse + corner dots + rotation-aware |
| **Collision detection** | AABB pairwise check, rotation via `GetRotatedBounds`, boundary room |
| **Delete object** | X key (atau Delete) untuk hapus objek terpilih |
| **Auto-spawn kipas** | Kipas langit-langit muncul otomatis di setiap level |
| **Scoring system** | Requirement (50) + Bonus (30) + Aesthetics (20) - Penalti |
| **Bonus rules** | Proximity, AABB overlap, exclusion zone per level |
| **Aesthetics heuristic** | Balance kiri-kanan + utilisasi ruangan |
| **Penalty system** | -5 per placement gagal (cap 50) |
| **Fly-through kamera** | Tombol P, 8 waypoint, smooth lerp, loop |
| **glTF Texture** | Base color texture dari material glTF |
| **White shadow fix** | `shadowMode` skip color emission |
| **Model centering** | Auto-center X/Z setelah normalisasi |

## 2. Konsep Game: Interior Designer Simulator

Pemain berperan sebagai desainer interior. Setiap level = proyek dari klien berbeda.

### Alur Gameplay
1. **Menu**: Layar start berisi brief klien + item wajib + budget
2. **Gameplay**: Pemain menempatkan furniture (model 3D glTF) di dalam ruangan, rotasi, atur posisi
3. **Submit**: Enter → evaluasi requirement + bonus + penalti
4. **Win/Lose**: Tampilkan skor breakdown, lanjut/retry

### Scoring System (0-100)

| Komponen | Maks | Detail |
|----------|------|--------|
| Requirement Fulfillment | 50 | Semua item wajib harus terpasang |
| Bonus Rules | 30 | +10 per rule, max 3 rules per level |
| Aesthetics | 20 | Balance (0-10) + Utilization (0-10) |
| Penalti | -50 | -5 per placement gagal, cap -50 |

## 3. Yang Telah Ditambahkan / Diubah

### 3.1 Tekstur ✅ (Selesai)
- load gambar 2D dengan `stb_image.h`
- mipmaps dengan `gluBuild2DMipmaps()`
- `GL_MODULATE` untuk gabung tekstur × material
- procedural checker texture sebagai fallback
- wall texture + floor texture dengan repeat
- glTF base color texture per primitive

### 3.2 Bayangan ✅ (Selesai)
- **Projective shadow + stencil buffer**: dua pass — render lantai ke stencil, render shadow di area stencil
- `GL_LEQUAL` depth test agar shadow tidak bocor ke dinding
- Shadow hanya di VIEW mode (skip di EDIT agar tidak ganggu)
- Warna shadow hitam semi-transparan (0,0,0,0.10)
- Skip `glColor4fv` di shadow pass agar shadow tetap hitam

### 3.3 Animasi ✅ (Selesai)
- **Fly-through kamera**: tombol P toggle, 8 waypoints orbit 360°, smoothstep lerp, looping
- **Kipas langit-langit**: berputar via `glRotatef(angle)`, angle increment tiap frame
- **Lampu glow**: emission pulsing via `sin(gAnimTime)`

### 3.4 Interaksi Antar Objek ✅ (Selesai)
- **AABB collision**: pairwise check saat placement, tolak jika overlap
- **Rotation-aware**: `GetRotatedBounds()` compute rotated AABB dari 4 corner
- **Boundary room**: clamp posisi ke ±kRoomSize
- **Mouse pick**: unproject ray → y=0 plane → local-space inverse rotation AABB test

### 3.5 Bonus Rules ✅ (Selesai)

| Level | Rules | Max Bonus |
|-------|-------|-----------|
| 1 (Ruang Tamu) | Karpet di bawah meja (AABB overlap), Sofa dekat meja (< 4 unit) | 20 |
| 2 (Ruang Makan) | Kursi dekat meja (< 2.5 unit), Kursi tidak menumpuk (> 1 unit) | 20 |
| 3 (Kamar Tidur) | Lampu dekat lemari (< 3 unit), Pintu tidak terhalang (exclusion zone) | 20 |

### 3.6 Game System ✅ (Selesai)
- State machine: MENU → PLAYING → WIN / LOSE
- Overlay WIN: score breakdown (requirement, bonus, aesthetics, penalti)
- Overlay LOSE: penyebab gagal + retry
- 3 level dengan brief klien berbeda
- HUD: budget meter, checklist item, bonus rules, furniture selector

### 3.7 Refactor Objek ✅ (Selesai)
- `ObjectSubType` enum: SOFA, MEJA, LEMARI, RAK, KURSI, MEJA_BUNDAR, LAMPU, KARPET, KIPAS
- `SceneObject`: subType, cost, isAnimated
- glTF model storage: `gGLTFModels[16]` indexed by ObjectSubType

## 4. Level Design (Final)

| Level | Klien | Item Wajib | Budget | Bonus Rules |
|-------|-------|-------------|--------|-------------|
| 1 | Ruang Tamu | 2 sofa, 1 meja, 1 karpet | 50 | Meja di atas karpet (AABB overlap) + Sofa dekat meja (< 4 unit) |
| 2 | Ruang Makan | 1 meja bundar, 4 kursi | 60 | Kursi dekat meja (< 2.5 unit) + Kursi tidak menumpuk (> 1 unit) |
| 3 | Kamar Tidur | 1 lemari, 1 lampu, 1 karpet | 45 | Lampu dekat lemari (< 3 unit) + Pintu tidak terhalang (exclusion zone) |

## 5. Kontrol (Final)

| Tombol | Mode | Fungsi |
|--------|------|--------|
| Tab | Global | EDIT ↔ VIEW |
| W A S D | EDIT | Geser objek terpilih |
| W A S D | VIEW | Gerak target kamera |
| Space | EDIT | Place objek baru |
| [ / ] | EDIT | Select prev/next objek |
| Q | EDIT | Rotasi objek -15° |
| E | EDIT | Rotasi objek +15° |
| X / Delete | EDIT | Hapus objek terpilih |
| Mouse Drag | EDIT | Geser objek terpilih |
| Mouse Drag | VIEW | Orbit kamera (yaw/pitch) |
| 1/2/3 | VIEW | Preset kamera 1/2/3-point |
| L | VIEW | Toggle directional light |
| F | VIEW | Toggle smooth/flat shading |
| Z | VIEW | Toggle depth test |
| P | VIEW | Toggle fly-through kamera |
| Enter | Global | Submit/Mulai/Ulang |
| Esc | Global | Menu/Keluar |

## 6. Catatan Implementasi

### Tekstur
- `stb_image.h` untuk load, `gluBuild2DMipmaps()` untuk mipmap
- `GL_MODULATE` untuk gabung tekstur dengan material color
- Procedural checker texture sebagai fallback

### Shadow
- Projective shadow + stencil buffer, two-pass rendering
- `GLUT_STENCIL` di display mode, `glClearStencil(0)`
- Directional light matrix proyeksi ke y=0

### Collision
- AABB pairwise check dengan rotation-aware bounds
- 4 corner rotation: rotasi tiap titik AABB lalu hitung AABB baru
- Boundary clamp ke ±kRoomSize

### Scoring
- Requirement = gate (semua wajib terpenuhi untuk WIN)
- Bonus rules dievaluasi di `EvaluateSubmission()`
- Aesthetics: balance (center of mass) + utilization (jumlah objek)
- Penalti: counter placement gagal

### Fly-through
- 8 waypoints orbit 360° sekitar ruangan
- Smoothstep interpolation antar waypoint
- Looping, toggle via P key

### Struktur File
```
src/
├── main.cpp              — Entry point, GLUT callbacks, key handling
├── renderer.cpp/.h       — Rendering: objects, room, grid, shadows, HUD, overlay
├── scene.cpp/.h          — State, objects, collision, levels, scoring, fly-through
├── texture.cpp/.h        — Texture loading + procedural generation
├── ui.cpp/.h             — Text rendering, window title
├── gltf_loader.cpp/.h    — glTF model parser + renderer
├── json.hpp              — nlohmann/json (single-header)
└── stb_image.h           — stb_image (single-header)
```

## 7. Riwayat Pengerjaan

| # | Fitur | Status |
|---|-------|--------|
| 1 | Split arsitektur: 4+ file | ✅ Selesai |
| 2 | Game state machine + UI | ✅ Selesai |
| 3 | Refactor objek (subType, cost) | ✅ Selesai |
| 4 | Collision detection | ✅ Selesai |
| 5 | Tekstur (file + procedural) | ✅ Selesai |
| 6 | Bayangan (projective + stencil) | ✅ Selesai |
| 7 | Animasi (fly-through + kipas) | ✅ Selesai |
| 8 | Bonus Rules + Scoring | ✅ Selesai |
| 9 | glTF model loader | ✅ Selesai |
| 10 | Delete object + auto-spawn kipas | ✅ Selesai |
