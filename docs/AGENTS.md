# AGENTS.md — Ruang

## 1. Identitas Proyek

| Atribut | Nilai |
|---------|-------|
| Nama Proyek | **Ruang** |
| Mata Kuliah | Praktikum Grafika dan Teknik Interaktif |
| Program Studi | Informatika 2025/2026 |
| Repositori | `git@github.com:kemal-faza/gti-bts.git` |
| Tech Stack | C++17, OpenGL 1.x (fixed-function), GLUT (freeglut) |

## 2. Kondisi Kode Saat Ini

Semua kode berada di `main.cpp` (~893 baris). Aplikasi memiliki dua mode:

- **EDIT** (top-down orthographic): place, pilih, geser objek di grid
- **VIEW** (perspective): inspeksi 3D dengan orbit camera + preset 1/2/3-point

### Status Implementasi vs Spesifikasi Proyek

| Spesifikasi | Status | Keterangan |
|-------------|--------|------------|
| a. Primitif Drawing | ✅ | `glutSolidCube`, `gluCylinder`+`gluDisk`, `glutSolidSphere`, `GL_LINES` |
| b. Translasi & Rotasi | ✅ | `glTranslatef`/`glRotatef`, WASD geser objek, orbit camera |
| c. Proyeksi | ✅ | Ortho (edit) + Perspektif (60° FOV) |
| c. Animasi | ❌ | Belum ada |
| d. Kamera | ✅ | Preset 1/2/3-point + free orbit + ortho top-down |
| d. Depth | ✅ | `GL_DEPTH_TEST`, toggle Z |
| d. Lighting | ✅ | Directional (LIGHT0) + Point (LIGHT1), material ROUGH/GLOSSY |
| e. Tekstur | ❌ | Hanya material warna solid |
| e. Bayangan | ❌ | Belum shadow casting |
| f. Interaksi Objek | ❌ | Belum collision detection, boundary check |
| Game: Start/Win/Lose | ❌ | Belum flow game, belum UI |

### Kontrol yang Sudah Ada

| Tombol | Fungsi |
|--------|--------|
| Tab | Mode EDIT ↔ VIEW |
| WASD | Geser objek (EDIT) / gerak kamera (VIEW) |
| Q/E | Naik/turun kamera (VIEW) |
| Space | Tambah objek baru (EDIT) |
| [/] | Select objek prev/next (EDIT) |
| 1/2/3 | Preset kamera 1/2/3-point (VIEW) |
| L | Toggle directional light |
| F | Toggle smooth/flat shading |
| Z | Toggle depth test |
| Mouse drag | Geser objek (EDIT) / orbit camera (VIEW) |
| Esc | Keluar |

## 3. Masalah yang Sedang Dihadapi

### Masalah 0: Win/Lose Condition untuk Room Builder 🔴

**Konteks:** Proyek saat ini adalah scene editor / room builder — secara alami sandbox/kreatif, tidak punya win/lose condition.

**Konflik:** Spesifikasi mewajibkan Game → win/lose condition, atau Animasi → storyline. Room builder bukan game tradisional dan bukan animasi naratif.

**Opsi yang ditemukan:**

| Opsi | Deskripsi | Win | Kekurangan |
|------|-----------|-----|------------|
| 1. Interior Designer | Klien + brief + budget | Semua wajib terpasang & cost ≤ budget | Budget sebagai gate — cukup menantang? |
| 2. Time-Attack | Timer mundur + kriteria | Selesai sebelum waktu habis | Tekanan waktu vs vibe santai |
| 3. Puzzle Tetris | Semua furniture harus muat | Semua terpasang tanpa overlap | Kurang eksplorasi kreatif |

**Pertanyaan terbuka:**
- Mana yang paling cocok dengan esensi room builder?
- Ada pendekatan ke-4 yang lebih baik?
- Apakah ini lebih tepat sebagai **Animasi** (storyline dekorasi) daripada Game?

---

### Masalah 1: Implementasi Tekstur

- Semua objek masih warna solid via `glMaterialfv`
- Perlu: load gambar + mapping UV ke cube/cylinder/road
- Pertimbangan library: `stb_image.h` (single-header, nol dependensi) vs SOIL vs FreeImage
- **Pertanyaan**: bagaimana gabungkan tekstur dengan material properties (glossy/rough) yang sudah ada? `GL_MODULATE` cukup, atau perlu pendekatan lain?

### Masalah 2: Implementasi Bayangan

- Lighting sudah jalan (directional + point) tapi tidak ada shadow jatuh
- Opsi A: **projective shadow** — render ulang objek diproyeksikan ke lantai. Sederhana, cocok fixed-function, tapi hanya lantai datar. Bisa ditambah stencil buffer agar lebih presisi.
- Opsi B: **shadow mapping** — render depth dari perspektif lampu ke texture. Lebih realistis, tapi butuh shader (GLSL) untuk depth comparison — **tidak viable** di OpenGL 1.x fixed-function.
- Opsi C: **blob shadow** — ellipse gelap semi-transparan di bawah tiap objek. Sangat mudah (~10 baris), cocok sebagai fallback cepat.
- **Pertanyaan**: projective + stencil vs blob shadow — mana yang cukup untuk penilaian?

### Masalah 3: Animasi Minimal

- Spesifikasi wajibkan animasi, tapi game ini sandbox
- Rencana: kamera fly-in saat transisi level + objek lerp saat ditempatkan (jatuh dari atas)
- Alternatif: **auto-showcase fly-through** — tombol P memicu kamera terbang mengitari ruangan via waypoints (timeline eksplisit, jelas terlihat sebagai animasi)
- Alternatif: **animasi objek in-world** — kipas langit-langit berputar, jarum jam bergerak via glRotatef di-increment tiap frame
- **Pertanyaan**: apakah fly-in saja cukup, atau perlu fly-through + objek animasi untuk meyakinkan penilaian?

### Masalah 4: Collision Detection & Boundary

- Objek bisa overlap, tidak ada batas ruangan
- Pendekatan: AABB bounding box per objek, pairwise check saat placement, reject jika collision
- **Pertanyaan**: bounding cylinder pakai AABB atau cylinder-cylinder? Rotasi objek pengaruhi collision?

### Masalah 5: Game State Machine & UI

- Belum ada flow: start → playing → win/lose
- Belum ada overlay: budget, checklist, skor
- Pendekatan: GLUT bitmap font untuk teks. Alternatif: FTGL, freetype, atau render teks ke texture
- **Pertanyaan**: cukup overlay teks atau perlu layar menu terpisah?

### Masalah 6: Arsitektur Single-File

- Semua 893 baris di `main.cpp`, akan membengkak tiap tambah fitur
- Belum diputuskan kapan split dan struktur apa yang direkomendasikan

## 4. Build

**Linux:**
```bash
g++ main.cpp -o main -lGL -lGLU -lglut
```

**Windows (MSYS2 UCRT64):**
```bash
g++ main.cpp -o main.exe -lfreeglut -lopengl32 -lglu32
```
