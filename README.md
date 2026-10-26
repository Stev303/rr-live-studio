# Rr Live Studio

Aplikasi Windows standalone untuk latihan dan performa **live singing**: PC memproses vokal dan
musik, sedangkan HP menjalankan BIGO LIVE. Dibangun dengan **C++17, JUCE 8.0.10, CMake**, tampilan
dark-neon, dan mesin audio real-time.

> **Kejujuran dulu:** aplikasi ini **tidak** terintegrasi ke API BIGO LIVE, tidak mengontrol HP,
> dan tidak mengirim audio ke HP secara otomatis. Rr Live Studio hanya menyiapkan dan memproses
> output audio di PC. Pemilihan sumber audio di HP/BIGO tetap dilakukan manual. Baca
> [Routing ke HP / BIGO LIVE](#6-routing-ke-hp--bigo-live).

---

## 1. Status verifikasi

Diuji pada Windows 10 x64, build Release, 26 Oktober 2026:

| Verifikasi | Hasil nyata |
| --- | --- |
| Build Release (`cmake --build`) | Berhasil, tanpa error |
| Unit test `RrLiveStudioTests.exe` | 12 suite lulus, exit code 0 |
| Walkthrough UI otomatis 9 halaman + dialog Go Live | Semua halaman terbuka, aplikasi keluar normal |
| WASAPI loopback pada endpoint HDA asli (48 kHz) | Capture berjalan, `48.0 kHz -> 48.0 kHz`, buffer 40–55 ms, `terbuang 0` |
| Uji loopback 70 detik dengan tone 440 Hz | Buffer bertahan 41–80 ms (koreksi drift adaptif aktif), `terbuang 0`, stop/start capture berhasil |
| Capture tone 440 Hz via VB-Cable | Meter capture menyala, peak terbaca ±0,25 (tone 0,25) |
| Scan VST3 di mesin uji | 47 plugin terdeteksi; plugin bisa ditambahkan ke chain Vocal dan Master |
| Editor VST3 | Jendela editor CLA Vocals (Waves) terbuka dan tertutup tanpa crash |

Belum diverifikasi / tidak diklaim:
- Input mikrofon fisik belum diuji otomatis di sesi ini; gunakan `docs/MANUAL_TEST.md` langkah 3.
- Capture audio **YouTube yang sebenarnya** belum diuji end-to-end; yang diuji adalah mekanisme
  WASAPI loopback dengan tone pada endpoint nyata. Alur yang sama berlaku, tetapi tetap uji sendiri
  sebelum live.
- Pemutaran sampler dengan file audio nyata serta simpan/muat preset dan scene belum diuji
  otomatis; lihat `docs/MANUAL_TEST.md` langkah 8 dan 10.
- State parameter plugin VST3 setelah restart belum diuji per plugin.
- Build ASIO belum diuji karena Steinberg ASIO SDK tidak tersedia di mesin build ini.
- Tidak ada pengujian di macOS/Linux; proyek ini Windows-only.
- Tidak ada integrasi atau klaim kompatibilitas dengan BIGO LIVE.

---

## 2. Fitur yang benar-benar berjalan

Semua kontrol di bawah terhubung ke engine/persistensi nyata (tidak ada tombol hiasan). Tingkat
verifikasi tiap bagian dirangkum di bagian 1 dan di `docs/MANUAL_TEST.md`.

### Dashboard
- Meter live Mic / Music / Master dengan peak marker.
- Input Gain, Music Fader, Master Gain, Transpose.
- Mute Mic, Test Tone 1 kHz, Mulai/Hentikan Music Capture, Reset Clip, GO LIVE.
- Status engine: device, sample rate, buffer, latency, CPU, dropout, overflow capture.
- Diagram alur sinyal aktual (bukan hiasan: nilainya diambil dari engine).

### YouTube
- Browser WebView2 tertanam untuk YouTube (login/iklan/DRM mengikuti kebijakan YouTube; aplikasi
  tidak mem-bypass apa pun).
- Music bus memakai **WASAPI loopback** dari endpoint output yang dipilih, bukan mengekstrak stream
  dari WebView. Karena itu seluruh audio sistem pada endpoint tersebut ikut tertangkap.
- Audio WebView mengikuti output default Windows (atau routing per-aplikasi bila diatur). Supaya
  YouTube masuk ke music bus, arahkan output audio YouTube/Windows ke endpoint yang sama dengan
  endpoint loopback, lalu set output master Rr ke headphone/interface. Jangan memakai endpoint yang
  sama untuk loopback dan output master, karena musik bisa terdengar dua kali.
- Slider **Video Volume** mengatur volume elemen video di WebView (bukan volume music bus). Volume 0
  juga membuat loopback tidak menerima audio YouTube.
- Pilihan endpoint loopback, mulai/stop capture, meter, status sample rate/buffer/drop.

### Vocal Chain
Urutan pemrosesan aktual:

```
Input Gain -> Noise Gate -> Smart EQ (analisis) -> HPF + 5-band EQ -> De-esser
-> Compressor -> Pitch Detector -> Pitch Correction -> Harmonizer
-> Saturation -> Reverb -> Delay -> Vocal Fader
```

- Smart EQ berbasis aturan (FFT + heuristik), bukan model AI terlatih. Analisis berjalan di audio
  thread hanya sebagai FFT; keputusan dan penulisan parameter terjadi di message thread.
- Pitch correction dan harmonizer memakai deteksi pitch rule-based; key/scale dapat dipilih.
- Harmonizer: 2 voice, interval, level, dry/wet, humanize, retune, stereo width, formant preserve.
- Semua kontrol tersimpan ke preset/session.

### Effects
- Transpose musik −12..+12 semitone (phase vocoder) + bypass.
- Music Booster: input trim, bass, presence, treble, loudness, output, compressor, limiter.
- Smart Balance: ducking musik terhadap vokal (target, duck, attack, release, strength, max reduction).
- Master: gain, limiter dengan ceiling, test tone, meter.

### VST3 Plugins
- Scan folder VST3 (default `C:\Program Files\Common Files\VST3`).
- Daftar plugin, pencarian nama/vendor/format, chain Vocal dan Master masing-masing 8 slot.
- Tambah, hapus, urutkan, bypass, buka editor, dan simpan/muat state plugin.
- Latensi chain dihitung dan dipakai untuk kompensasi latency.
- **Plugin berjalan in-process.** Plugin yang crash/hang dapat menjatuhkan aplikasi. Tidak ada
  jaminan kompatibilitas semua VST3.

### Sampler Pads
- Tepat **10 pad**, masing-masing bisa memuat WAV/AIFF/FLAC/MP3 (MP3 aktif di build ini).
- Gain per pad, loop, play/stop, meter sampler, dan pemetaan ke bus sampler.
- File yang dimuat disimpan sebagai path; preset sampler menyimpan konfigurasi pad.

### Mixer
- Strip Mic, YouTube Music, Sampler, Vocal FX, Master.
- Fader, mute, solo (mic/music/sampler), input gain, reverb/delay send pada strip vokal.
- Catatan bahwa mute/solo vokal ada di strip Mic, sesuai implementasi engine.

### Presets dan Scenes
- Kategori Vocal, Music, Harmonizer, Master, Sampler, Scene.
- Simpan, muat, duplikat, rename, hapus (preset bawaan tidak bisa dihapus/di-rename).
- Format JSON `.rrpreset`; lokasi: `%APPDATA%\Rr Live Studio\Presets`.

### Settings
- Panel audio device JUCE (WASAPI Shared/Exclusive, dan ASIO bila SDK tersedia saat build).
- Sample rate, buffer, latency, CPU, dropout; pilih mode buffer.
- Pemilihan endpoint loopback + refresh + capture.
- Test tone melewati master VST dan limiter.
- Simpan pengaturan, ekspor log diagnostik, dan reset statistik audio.

### Go Live
Checklist nyata: device aktif, limiter aktif, test tone nonaktif, tidak clip, status capture,
sinyal master terdeteksi, plus dua konfirmasi manual tentang routing HP. Dialog ini juga menjelaskan
batasan integrasi BIGO secara eksplisit.

---

## 3. Kebutuhan build

- Windows 10/11 x64
- Visual Studio 2022 (Desktop development with C++) atau Build Tools + Windows SDK
- CMake ≥ 3.22, Ninja
- **JUCE 8.0.10** (tersedia di `external/JUCE`; workflow CI meng-clone versi ini otomatis)
- **Microsoft.Web.WebView2 SDK** 1.0.1901.177 (NuGet), dibutuhkan saat configure
- WebView2 Evergreen Runtime di komputer pengguna (Windows 10/11 umumnya sudah terpasang)

WebView2 SDK dapat disiapkan dengan:

```powershell
Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.1901.177
```

Atau ekstrak paket NuGet ke `external/webview2pkg/Microsoft.Web.WebView2.1.0.1901.177/`.
CMake mencari paket di lokasi tersebut.

### Build dengan script (mesin pengembangan ini)

```bat
scripts\configure.bat
scripts\build.bat
scripts\test.bat
```

`configure.bat` dan `build.bat` memakai path Visual Studio Build Tools yang terpasang di mesin ini.
Untuk mesin lain, jalankan dari **Developer PowerShell for VS 2022**:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRRLIVE_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Hasil build:
- `build/RrLiveStudio_artefacts/Release/Rr Live Studio.exe`
- `build/RrLiveStudioTests_artefacts/Release/RrLiveStudioTests.exe`

### ASIO (opsional)

Build default memakai **WASAPI** (Shared/Exclusive lewat panel device JUCE). Untuk mengaktifkan
ASIO, sediakan Steinberg ASIO SDK dan arahkan:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRRLIVE_ASIO_SDK_PATH="C:\path\ke\asio-sdk"
```

Tanpa SDK, CMake mencetak `building without ASIO (WASAPI only)`. Ini bukan klaim bahwa ASIO sudah
diuji; build ASIO belum diverifikasi di mesin ini.

---

## 4. Menjalankan

1. Buka **Settings**, pilih audio device output (mis. headphone/audio interface), sample rate, dan
   buffer. Mulai dari 256 sample @ 48 kHz; turunkan bila CPU kuat dan latency dibutuhkan.
2. Pilih **endpoint loopback** untuk music bus. Ini adalah perangkat output tempat audio YouTube
   keluar. Agar YouTube (WebView) benar-benar masuk ke endpoint itu:
   - set **output default Windows** ke endpoint tersebut (atau gunakan routing per-aplikasi), dan
   - set **output master Rr Live Studio** ke headphone/interface monitoring yang berbeda.
   Contoh: Windows default + loopback = *VB-Audio Virtual Cable*; output master = *Headphones*.
   Audio YouTube masuk ke cable (tidak terdengar), diproses, lalu keluar ke headphone. Bila loopback
   dan output master memakai device yang sama, musik akan terdengar dua kali.
   Untuk menangkap *seluruh* audio sistem, endpoint loopback cukup disamakan dengan output default.
3. Buka **YouTube**, putar lagu, klik **Mulai Music Capture**.
4. Atur **Transpose** dan Music Booster di halaman Effects bila perlu.
5. Atur vokal di **Vocal Chain** dan **Mixer**.
6. Uji level memakai **Test Tone 1 kHz** dengan level rendah, lalu reset clip.
7. Buka **GO LIVE** dan selesaikan checklist sebelum siaran.

Pengaturan window, device, dan parameter disimpan di:

```
%APPDATA%\Rr Live Studio\Rr Live Studio.settings
```

Preset dan scene: `%APPDATA%\Rr Live Studio\Presets\`.
Log diagnostik disimpan di memori (maksimal 1000 baris) dan baru ditulis ke file saat diekspor
melalui tombol **Export...** di Settings.

---

## 5. Alur audio

```
Mic --+--> Vocal Chain --> VST3 Vocal --> (delay kompensasi) --+
      |                                                        |
YouTube/System --> WASAPI loopback --> Pitch/Key --> Booster --+--> Smart Balance (duck)
      |                                                        |
Sampler (10 pad) -----------------------------------------------+
                                                               |
                                                    Bus fader/mute/solo
                                                               |
                                                   VST3 Master --> Master Gain
                                                               |
                                                   Limiter --> Output device
```

- Music bus melakukan priming ±40 ms lalu menjaga backlog; bila backlog melewati ±200 ms, data
  tertua dibuang dan counter **terbuang** naik.
- Capture dan output memakai clock yang berbeda, jadi resampler menyesuaikan rasio secara perlahan
  (±0,4% maksimum) berdasarkan tingkat backlog FIFO. Tanpa ini backlog bisa merangkak sampai batas
  drop dan memotong audio.
- Bila sample rate device berubah, capture loopback di-restart otomatis agar resampling tetap benar.
- Latensi plugin dan pitch shifter dikompensasi dengan delay pada jalur lain.

---

## 6. Routing ke HP / BIGO LIVE

Ini bagian yang paling sering disalahpahami. **Kabel USB biasa tidak mengirim audio dari PC ke HP.**
BIGO LIVE di HP memakai mikrofon HP atau sumber audio yang didukung oleh sistem operasi HP.

Pilihan yang realistis dan jujur:

1. **Speaker + mic HP** — paling sederhana. Putar output PC lewat speaker/monitor, atur volume
   rendah, dan biarkan HP menangkap suara ruangan. Gunakan headphone untuk monitoring agar tidak
   feedback. Kualitas bergantung akustik ruangan.
2. **Audio interface / mixer USB ke HP** — sebagian HP Android mendukung input audio USB (USB-C)
   melalui adapter/interface tertentu. Sambungkan output PC ke input interface, lalu pilih sumber
   audio eksternal di BIGO bila tersedia. Tidak semua HP/aplikasi mendukung ini.
3. **Virtual audio cable + perangkat capture** — berguna bila Anda memakai perangkat capture
   (mis. kamera/encoder) yang menerima audio dari PC. Rr Live Studio bisa mengeluarkan audio ke
   VB-Cable, lalu perangkat lain mengambilnya. Ini bukan integrasi langsung ke BIGO.

Langkah aman:
- Set output Rr Live Studio ke perangkat yang akan dipakai HP/perangkat capture.
- Mainkan **Test Tone**, pastikan level di meter master masuk akal dan tidak clip.
- Di HP, pilih sumber audio yang sesuai dan uji rekaman/stream percobaan.
- Matikan speaker dan mic HP bila memakai jalur eksternal untuk mencegah feedback.
- Delay monitoring tergantung buffer, driver, dan jalur virtual cable. Gunakan headphone.

Yang **tidak** dilakukan aplikasi: mengontrol HP, mengirim audio lewat jaringan ke BIGO,
membajak DRM/iklan/login YouTube, atau mengunggah rekaman ke cloud tanpa persetujuan.

---

## 7. Batasan dan catatan teknis

- **WASAPI loopback menangkap seluruh audio sistem** pada endpoint terpilih, bukan hanya tab
  YouTube. Notifikasi, suara game, dan audio aplikasi lain ikut tertangkap bila keluar di endpoint
  yang sama.
- **Endpoint virtual tertentu dapat salah clock.** Pada mesin uji, endpoint ASIOVADPRO melaporkan
  44,1 kHz/256 tetapi hanya memanggil callback ±110×/detik (±28,3k frame/detik), sehingga capture
  tampak terbuang. Endpoint HDA asli pada 48 kHz berjalan benar (187 callback/detik, 0 drop). Ini
  masalah driver/perangkat, bukan mesin audio aplikasi.
- **VST3 in-process**: tidak ada isolasi crash. Plugin 32-bit, rusak, atau tidak kompatibel akan
  muncul di daftar gagal atau dapat menjatuhkan aplikasi.
- **Smart EQ, pitch detection, dan Harmony Assistant berbasis aturan**, bukan AI/ML terlatih.
  UI dan dokumentasi menyebutnya demikian.
- **ASIO** hanya tersedia bila SDK disertakan saat build; build default WASAPI.
- Rekaman master tidak disediakan sebagai fitur; aplikasi ini memproses audio live.
- Pengujian audio otomatis terbatas pada mesin uji dan perangkat yang tersedia. Selalu uji level
  dan routing di perangkat Anda sendiri sebelum live.

---

## 8. Struktur proyek

```
CMakeLists.txt
Source/
  App/            entry point, AppState, MainComponent
  Audio/          AudioEngine, LoopbackCapture (WASAPI), BusRouter, ParamStore, SmartBalance
  DSP/            Biquad, compressor, limiter, pitch shifter/detector, harmonizer,
                  vocal chain, music booster, Smart EQ
  Sampler/        SamplerEngine (10 pad)
  VST3Host/       PluginHost (scan, chain, editor, state, latency)
  Presets/        PresetManager (JSON .rrpreset)
  AI/             HarmonyAssistant (rule-based key detection/suggestion)
  UI/             tema, widget, sidebar, top bar, halaman, dialog, diagram routing
  YouTube/        WebView2 browser
  Utilities/      DiagnosticsLog
Tests/            unit test DSP dan routing/preset
scripts/          configure/build/test, walkthrough UI
docs/             manual test dan panduan
.github/          workflow build Windows
```

---

## 9. Lisensi

Lihat [LICENSES.md](LICENSES.md). JUCE 8 bersifat dual-license (AGPLv3 atau lisensi komersial
JUCE). Jika Anda mendistribusikan aplikasi ini, pastikan kepatuhan lisensi JUCE, VST3 SDK, dan
WebView2.

## 10. Troubleshooting singkat

| Gejala | Penyebab umum | Tindakan |
| --- | --- | --- |
| Meter music diam | Endpoint loopback salah / audio keluar di device lain | Pilih endpoint tempat audio benar-benar keluar; uji dengan tone |
| `terbuang` naik terus | Driver/endpoint salah clock atau CPU overload | Ganti endpoint/device, naikkan buffer, cek CPU |
| Tidak ada suara vokal | Device input salah, mute aktif, gain terlalu rendah | Cek Settings, Mute Mic, Input Gain, meter Mic |
| Plugin tidak muncul | Folder salah / plugin 32-bit / gagal scan | Pilih folder VST3 yang benar, buka **File Gagal** |
| Aplikasi crash saat plugin dibuka | Plugin bermasalah (in-process) | Hapus plugin dari chain, gunakan plugin lain |
| WebView2 kosong | Runtime WebView2 belum terpasang | Pasang WebView2 Evergreen Runtime |
| HP tidak menerima audio | USB biasa tidak mengirim audio | Gunakan speaker+mic atau interface/mixer yang didukung |
