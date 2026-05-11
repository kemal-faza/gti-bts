# PLAN - GTI BTS: Interior Designer Simulator

## 1. Analisis Kode Saat Ini vs Spesifikasi Proyek

| Spesifikasi | Status | Catatan |
|-------------|--------|---------|
| a. Primitif Drawing | ✅ | `glutSolidCube`, `gluCylinder`+`gluDisk`, `glutSolidSphere`, `GL_LINES` |
| b. Translasi & Rotasi | ✅ | `glTranslatef`/`glRotatef`, WASD move, orbit camera |
| c. Proyeksi | ✅ | Ortho (top-down edit) + Perspektif (60° FOV) |
| c. Animasi | ❌ | Belum ada sistem animasi |
| d. Kamera | ✅ | 3 preset (1/2/3-point) + free orbit + ortho top-down |
| d. Depth | ✅ | GL_DEPTH_TEST toggle (Z key) |
| d. Lighting | ✅ | Directional (LIGHT0) + Point (LIGHT1) + material properties |
| e. Tekstur | ❌ | Belum ada texture mapping |
| e. Bayangan | ❌ | Belum ada shadow technique |
| f. Interaksi Antar Objek | ❌ | Belum ada collision detection / interaksi |
| Game: Start/Win/Lose | ❌ | Masih scene editor biasa |

## 2. Konsep Game: Interior Designer Simulator

Pemain berperan sebagai desainer interior. Setiap level = proyek dari klien berbeda.

### Alur Gameplay
1. **Start Condition**: Ruangan kosong + brief klien berisi:
   - Item wajib yang harus ditempatkan
   - Budget maksimal
   - Aturan khusus (optional)
2. **Gameplay**: Pemain menempatkan furniture (cube, cylinder, road) di dalam ruangan
3. **Win Condition**: Semua item wajib terpenuhi + total cost ≤ budget
4. **Lose Condition**: Total cost melebihi budget

### Jenis Objek / Furniture (Mapping dari yang sudah ada)

| ObjectType Saat Ini | Furniture | Cost (contoh) |
|---------------------|-----------|---------------|
| CUBE (glossy) | Meja / Lemari / Rak | 15 |
| CUBE (rough) | Sofa / Kursi | 10 |
| CYLINDER | Meja Bundar / Lampu | 8 |
| ROAD | Karpet / Tikar | 5 |

> **Catatan**: ObjectType dan material akan diperluas/direname sesuai perabotan.

### Scoring System (Satisfaction Score 0-100)

Final formula akan dievaluasi lebih lanjut, berikut draft awalnya:

#### A. Requirement Fulfillment (50%)
- Item wajib yang terpasang = lolos
- Semua item wajib harus terpasang untuk win
- Requirement fulfillment = (itemWajibTerpasang / totalItemWajib) × 50
- Catatan: item opsional (benda tambahan tidak wajib, misal vas atau jam) berbeda dari Bonus Rules di bawah. Item opsional tidak mempengaruhi skor — hanya variasi visual.

#### B. Penalti (mengurangi score, bukan prevent)
- ~~Collision detection~~ → akan dicegah sistem, bukan scoring
- Setiap percobaan place di posisi invalid = -poin kecil

#### C. Budget (sebagai gate, bukan scoring)
- Total cost ≤ budget → lanjut
- Total cost > budget → **LOSE**
- Tidak ada reward dari sisa budget (agar pemain tetap termotivasi "berbelanja" tanpa merasa dihukum)

#### D. Aesthetics Heuristic (untuk menambah depth, bisa dipertimbangkan nanti)
- Keseimbangan kiri-kanan
- Jarak minimum antar objek
- Utilisasi ruangan

#### E. Bonus Objectives — Functional Rules (Opsional, Tidak Mempengaruhi Win/Lose)

Setiap level memiliki 1-3 aturan fungsional opsional yang menambah skor jika terpenuhi. Rules ini **tidak wajib** — pemain tetap menang meskipun tidak ada bonus yang terpenuhi.

Contoh rules:
- **Kursi dekat meja**: jarak antar pusat objek < 3 unit
- **Karpet di bawah meja**: AABB karpet overlap dengan AABB meja
- **Lampu dalam jangkauan**: jarak lampu ke furniture terdekat < 5 unit
- **Pintu tidak terhalang**: tidak ada objek dalam exclusion zone 2×2 unit di area pintu
- **Tidak overlap**: semua objek bebas collision antar sesama (sudah di-prevent oleh collision system, tinggal dicek)

Tiap bonus terpenuhi = +10 skor. Maksimal bonus per level: 3 rules.
Penalti placement invalid = -5 per percobaan (minimal 0, dihitung dari 5 percobaan pertama).
Total skor: requirement (max 50) + bonus (max 30) + aesthetics (max 20) - penalti.

## 3. Yang Harus Ditambahkan / Diubah

### 3.1 Tekstur (e)
- Load gambar 2D dengan **stb_image.h** (single-header, nol dependensi eksternal)
- Generate mipmaps dengan **gluBuild2DMipmaps()** (built-in GLU, sudah di-link, nol dependensi baru — kualitas tekstur jauh lebih baik dari glTexImage2D biasa)
- Texture coordinates untuk tiap face cube, cylinder, dan road
- Gabung tekstur + material properties dengan **GL_MODULATE** (multiply tekstur × material color). Alternatif: GL_DECAL untuk efek berbeda tiap material type.
- Procedural texture (checker/noise via CPU) sebagai fallback jika tidak ada file aset gambar

### 3.2 Bayangan (e)
- **Projective shadow + stencil buffer**: render objek kedua kali via matrix proyeksi ke lantai, gunakan stencil buffer agar shadow tidak overflow keluar area lantai. 100% fixed-function OpenGL 1.x.
- **Shadow mapping tidak viable** — butuh shader (GLSL) untuk depth comparison, tidak tersedia di fixed-function pipeline. Coret dari pertimbangan.
- **Blob shadow** sebagai fallback cepat jika waktu terbatas: ellipse gelap semi-transparan di bawah tiap objek, scaled berdasarkan posisi Y.
- Catatan: bayangan hanya jatuh ke lantai datar (tidak ke dinding/objek lain) dengan metode projective.
- **InitGL perlu diupdate**: `glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL)` dan aktifkan stencil test via `glEnable(GL_STENCIL_TEST)` + `glClearStencil(0)`.

### 3.3 Animasi (c)
- **Auto-showcase fly-through**: tombol P (di VIEW mode) memicu kamera terbang otomatis mengitari ruangan via waypoints yang di-lerp. Loop sampai tombol P ditekan lagi. Ada timeline eksplisit (waypoints) — jelas terlihat sebagai animasi oleh dosen.
- **Satu objek animasi in-world**: kipas langit-langit berputar via `glRotatef(angle, 0,1,0)` dengan angle di-increment tiap frame di fungsi Idle. Implementasi ~10 baris.
- Kipas = CYLINDER dengan flag `isAnimated = true` di SceneObject. Posisi Y diset ke langit-langit (~4.0f). Tidak perlu ObjectType baru — cukup properti boolean di struct.
- Kipas **tidak ditempatkan pemain**, melainkan di-spawn otomatis saat level dimulai.
- Kombinasi dua pendekatan ini solid untuk memenuhi spesifikasi: animasi kamera (fly-through) + animasi objek (kipas).
- Implementasi: `glutTimerFunc(16, ...)` untuk ~60fps update loop.

### 3.4 Interaksi Antar Objek (f)
- **AABB (Axis-Aligned Bounding Box)**: tiap objek punya min/max XZ. Cek pairwise overlap saat placement — tolak jika collision.
- **Padding 10-15%** pada AABB untuk mengakomodasi objek yang dirotasi (tanpa perlu implementasi OBB penuh).
- Objek silindris (meja bundar, lampu): pakai **bounding cylinder khusus** — cek `distance(centerA, centerB) < rA + rB`.
- Boundary ruangan: clamp posisi objek ke min/max XZ saat drag/placement.
- OBB/SAT dan spatial hashing dipertimbangkan tapi **overkill** untuk proyek ini.

### 3.5 Bonus Objectives / Functional Rules System

- Dikerjakan **setelah** core features (tekstur, shadow, animasi, collision) selesai. Prioritas rendah — jika waktu terbatas bisa dilewati.
- Struktur data tiap level: `std::vector<BonusRule>` dengan:
  - `ruleType`: enum (PROXIMITY, OVERLAP, EXCLUSION_ZONE, WITHIN_RANGE)
  - `targetObject`: tipe objek yang diharapkan (misal: CHAIR)
  - `anchorObject`: tipe objek acuan (misal: TABLE) — null untuk EXCLUSION_ZONE
  - `threshold`: jarak/area maksimal dalam unit world
  - `description`: teks untuk ditampilkan di HUD
- Evaluasi: jalankan semua rules saat pemain tekan Enter (submit). Rules yang terpenuhi ditampilkan checklist hijau di HUD.
- Rules tidak mempengaruhi kemampuan pemain menempatkan objek — hanya scoring.

### 3.6 Game System
- State machine: START → PLAYING → WIN / LOSE
- Setelah WIN: tampilkan overlay score, opsi "Next Level" (Enter) atau "Main Menu" (Esc)
- Setelah LOSE: tampilkan overlay penyebab gagal, opsi "Retry" (Enter) atau "Main Menu" (Esc)
- Level terakhir (3) → WIN menampilkan "Congratulations! All rooms complete."
- Layar start (brief klien)
- UI overlay: budget meter, checklist item, skor, bonus completion
- Transisi antar level (minimal 3 level dengan klien berbeda)

### 3.7 Refactor Objek
- Rename/mapping ObjectType ke furniture yang relevan
- Tambah data: cost, name, client requirements
- Tambah properti `ObjectSubType subType` (enum) atau string `label` di SceneObject untuk membedakan varian dalam satu ObjectType — misal CYLINDER bisa jadi MEJA_BUNDAR, LAMPU, atau KIPAS.
- Tambah flag `bool isAnimated` di SceneObject untuk menandai objek yang punya animasi looping (misal: kipas).

## 4. Level Design (Draft)

| Level | Klien | Item Wajib | Budget | Bonus Rules (Opsional) | Catatan |
|-------|-------|-------------|--------|------------------------|---------|
| 1 | Ruang Tamu | 2 sofa (cube rough), 1 meja (cube glossy), 1 karpet (road) | 50 | 1. Karpet di bawah meja (AABB overlap)<br>2. Sofa tidak jauh dari meja (jarak < 4 unit) | Tutorial |
| 2 | Ruang Makan | 1 meja bundar (cyl), 4 kursi (cube rough) | 60 | 1. Kursi mengelilingi meja (jarak < 2.5 unit)<br>2. Kursi saling berjarak > 1 unit (tidak menumpuk) | Lebih banyak objek |
| 3 | Kamar Tidur | 1 lemari (cube glossy), 1 lampu (cyl), 1 karpet (road) | 45 | 1. Lampu dekat lemari (< 3 unit)<br>2. Lemari tidak menghalangi pintu (exclusion zone) | Budget ketat |

## 5. Kontrol (Rencana)

| Tombol | Fungsi |
|--------|--------|
| Tab | Mode EDIT ↔ VIEW (sudah ada) |
| WASD | Geser objek / kamera (sudah ada) |
| Space | Place objek baru di posisi kursor (ubah dari AddNewObjectInEditMode) |
| [/] | Select prev/next objek (sudah ada) |
| Q/E | Naik/turunkan objek (belum, perlu ditambah di edit mode) |
| Mouse Drag | Pindah objek (edit) / rotasi kamera (view) (sudah ada) |
| L | Toggle directional light (sudah ada) |
| F | Toggle smooth/flat shading (sudah ada) |
| Z | Toggle depth test (sudah ada) |
| P | Auto-showcase fly-through (VIEW mode) (baru) |
| Enter | Submit / selesai dekorasi (baru) |

## 6. Catatan Implementasi

- **Tekstur**: `stb_image.h` untuk load + `gluBuild2DMipmaps()` untuk mipmap (built-in GLU). `GL_MODULATE` untuk gabung tekstur dengan material color.
- **Shadow**: Projective shadow + stencil buffer. Glut display mode perlu `GLUT_STENCIL`. Shadow mapping **tidak viable** di OpenGL 1.x tanpa shaders.
- **Animasi**: `glutTimerFunc(16, ...)` untuk update loop ~60fps. Fly-through waypoints: simpan array posisi kamera (Vec3), lerp antar waypoint di timer callback.
- **Collision**: AABB dengan padding 10-15% untuk rotasi. Bounding cylinder khusus untuk objek silindris.
- **Split arsitektur sebelum tambah fitur**: bagi `main.cpp` menjadi 4 file — `main.cpp` + `renderer.cpp/.h` + `scene.cpp/.h` + `ui.cpp/.h`. Build command diupdate.
- **UI**: GLUT bitmap fonts untuk HUD (budget, checklist, skor). Opsi upgrade ke `stb_truetype.h` (satu keluarga dengan stb_image) jika ingin font lebih bagus tanpa dependensi baru.
- **State game**: enum `GameState { MENU, PLAYING, WIN, LOSE }` ditambahkan di AppState.
- **Layar menu**: cukup overlay quad gelap + teks GLUT bitmap, tidak perlu scene terpisah.
- **Bonus rules**: diimplementasi setelah semua spesifikasi wajib (a-f) terpenuhi. Jika waktu sempit, cukup 1-2 rule paling simpel (proximity check antar dua objek).

## 7. Urutan Pengerjaan

Prioritas berdasarkan dependensi antar fitur dan spesifikasi wajib:

| Urutan | Fitur | Alasan |
|--------|-------|--------|
| 1 | Split arsitektur: 4 file | Harus sebelum tambah fitur besar agar struktur kode rapi |
| 2 | Game state machine + UI dasar | Dibutuhkan untuk flow start/win/lose dan overlay |
| 3 | 3.7 Refactor objek (cost, name, subType) | Prasyarat untuk game system dan scoring |
| 4 | 3.4 Collision detection (f) | Prasyarat untuk "tidak overlap" dan validasi placement |
| 5 | 3.1 Tekstur (e) | Independen, bisa dikerjakan paralel dengan shadow |
| 6 | 3.2 Bayangan (e) | Independen, bisa dikerjakan paralel dengan tekstur |
| 7 | 3.3 Animasi (c) | Timer-based, bisa dikerjakan setelah view mode stabil |
| 8 | 3.5 Bonus Objectives | Prioritas rendah — hanya jika waktu masih tersisa |
