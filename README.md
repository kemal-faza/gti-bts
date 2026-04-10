# Roomist (OpenGL GLUT)

Project ini adalah simulator layout arsitektur berbasis state machine (state-driven) yang bertujuan untuk mendekorasi suatu ruangan.

Aplikasi memiliki dua mode utama:

- `EDIT`: untuk membuat dan menata objek pada bidang datar.
- `VIEW`: untuk inspeksi scene 3D dengan preset kamera perspektif.

## Build dan Run

Contoh compile di Linux:

```bash
g++ main.cpp -o main -lGL -lGLU -lglut
./main
```

Contoh compile di Windows (MSYS2 UCRT64, disarankan):

1. Install MSYS2 dari https://www.msys2.org/.
2. Buka terminal `MSYS2 UCRT64`, lalu install toolchain dan freeglut:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-freeglut
```

3. Masuk ke folder project lalu compile:

```bash
g++ main.cpp -o main.exe -lfreeglut -lopengl32 -lglu32
```

4. Jalankan aplikasi:

```bash
./main.exe
```

Alternatif (MinGW-w64 standalone di CMD/PowerShell) command compile umumnya sama:

```bash
g++ main.cpp -o main.exe -lfreeglut -lopengl32 -lglu32
```

## Kontrol

### Kontrol Global

- `Esc`: keluar aplikasi.
- `Tab`: ganti mode `EDIT` <-> `VIEW`.

### Mode EDIT (Orthographic)

- `W A S D`: geser objek terpilih pada bidang X-Z.
- `Klik kiri + drag`: geser objek terpilih pada bidang X-Z.
- `Space`: tambah objek baru ke scene.
- `[` / `]`: pilih objek sebelumnya / berikutnya.

### Mode VIEW

- `1`: kamera preset 1-point perspective.
- `2`: kamera preset 2-point perspective.
- `3`: kamera preset 3-point perspective.
- `W A S D`: gerak target kamera (maju, mundur, kiri, kanan).
- `Q / E`: naik / turun target kamera.
- `Klik kiri + drag`: ubah yaw dan pitch kamera (look around).
- `L`: toggle light.
- `F`: toggle shading (flat / smooth).
- `Z`: toggle depth test.

## Struktur Data (Data Oriented)

Scene disimpan sebagai data, tidak hardcode di fungsi render:

- `SceneObject`
    - `type`: `CUBE`, `CYLINDER`, `ROAD`
    - `position`: `(x, y, z)`
    - `rotationY`
    - `material`: `ROUGH` atau `GLOSSY`

- `AppState`
    - `activeMode`: `EDIT` atau `VIEW`
    - `cameraPreset`: `ONE_POINT`, `TWO_POINT`, `THREE_POINT`, `FREE`
    - `selectedObjectIndex`
    - `smoothShading`
    - `directionalLightEnabled`
    - `depthTestEnabled`
    - state input mouse (`isDragging`, `lastMouseX`, `lastMouseY`)

## Penerapan Konsep Grafika

### 1. Proyeksi Orthographic

- Saat mode `EDIT`, matriks proyeksi memakai `glOrtho(...)`.
- Kamera dipaksa top-down (`gluLookAt` dari atas) agar cocok untuk penempatan objek pada grid.

### 2. Proyeksi Perspective

- Saat mode `VIEW`, matriks proyeksi memakai `gluPerspective(...)`.
- Preset kamera:
    - `1-point`: kamera menghadap lurus ke arah depth utama scene.
    - `2-point`: kamera dari sudut scene (horizon relatif level).
    - `3-point`: kamera tinggi menatap serong ke bawah.
- Setelah user drag mouse atau bergerak bebas, preset menjadi `FREE`.

### 3. Rendering Pipeline: Geometri

- Geometri tidak digambar hardcode satu-satu panjang.
- Semua objek dirender dengan loop `for` pada list `sceneObjects`.
- Setiap objek memakai `glPushMatrix()` / `glPopMatrix()`, lalu translasi, rotasi, material, dan draw primitive sesuai tipe.

### 4. Rendering Pipeline: Kamera

- Kamera diputuskan dari state mode + preset.
- Orthographic: top-down view untuk editing.
- Perspective: orbit camera (yaw, pitch, distance, target) untuk inspeksi dan look around.

### 5. Rendering Pipeline: Cahaya

- Dua sumber cahaya dipakai:
    - `GL_LIGHT0`: directional light (matahari), bisa di-toggle dengan `L`.
    - `GL_LIGHT1`: point light (lampu lokal) dengan attenuation.
- Posisi dan properti lampu di-set setelah view matrix aktif, sebelum geometri digambar.

### 6. Karakteristik Permukaan (Material)

- Material `ROUGH`: specular rendah, shininess kecil.
- Material `GLOSSY`: specular tinggi, shininess besar.
- Objek terpilih di mode edit diberi emission ringan agar terlihat jelas.

### 7. Algoritma Rendering

- Z-buffer dipakai melalui `GL_DEPTH_TEST`.
- Untuk kebutuhan demonstrasi, depth test bisa di-toggle dengan `Z`.
    - `ON`: occlusion benar (objek dekat menutupi objek jauh).
    - `OFF`: urutan gambar dapat terlihat salah.

### 8. Shading

- Tersedia toggle shading `GL_SMOOTH` dan `GL_FLAT` lewat tombol `F`.
- Ini memperlihatkan perbedaan visual Gouraud-like smooth interpolation vs flat per-face shading.

## Urutan Render di `Display()`

Urutan render mengikuti pipeline yang benar:

1. `glClear(...)`
2. Set projection matrix (`glOrtho` atau `gluPerspective` berdasarkan mode)
3. Set view matrix (`gluLookAt` berdasarkan mode/preset)
4. Setup lights (`GL_LIGHT0`, `GL_LIGHT1`)
5. Render geometri scene (grid, loop objek, marker lampu)
6. `glutSwapBuffers()`
