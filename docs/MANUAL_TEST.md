# Manual Test Checklist — Rr Live Studio

Dokumen ini dipakai untuk memverifikasi aplikasi secara nyata di Windows. Jalankan tes setelah
setiap perubahan besar pada engine, routing, atau UI. Jangan menandai lulus tanpa menjalankan
langkahnya dan melihat hasilnya.

- Build yang diuji: `build/RrLiveStudio_artefacts/Release/Rr Live Studio.exe`
- Data aplikasi: `%APPDATA%\Rr Live Studio\`
- Unit test: `build\RrLiveStudioTests_artefacts\Release\RrLiveStudioTests.exe`

## 0. Build dan unit test

1. Jalankan `scripts\configure.bat` lalu `scripts\build.bat`.
   - **Harapan:** build selesai tanpa error; exe dan tests exe dihasilkan.
2. Jalankan `scripts\test.bat`.
   - **Harapan:** semua suite lulus dan exit code 0 (`SEMUA TEST LULUS`).
3. Catat versi Windows, CPU, device audio, dan driver yang dipakai. Hasil tes hanya berlaku untuk
   konfigurasi itu.

## 1. Walkthrough UI

1. Jalankan `powershell -ExecutionPolicy Bypass -File scripts\ui-walkthrough.ps1`.
   - **Harapan:** semua 9 halaman terbuka, dialog GO LIVE terbuka/tertutup, aplikasi keluar
     normal (`EXIT_CODE=0`), screenshot tersimpan di `build/walkthrough/`.
2. Periksa screenshot: tidak ada teks terpotong parah, panel tidak saling menimpa, meter tampil.

## 2. Device audio dan status engine

1. Buka **Settings**. Pilih output device nyata (bukan virtual), sample rate 48 kHz, buffer 256.
2. Tekan **Simpan Pengaturan**, lalu lihat **Status Audio** dan top bar.
   - **Harapan:** device, rate, buffer, latency tampil masuk akal; CPU di bawah ±50%; dropout 0
     saat idle.
3. Ganti buffer ke 512 dan 128. Ulangi.
   - **Harapan:** latency berubah; engine tetap berjalan; tidak crash.
4. Tutup dan buka aplikasi.
   - **Harapan:** device dan buffer yang dipilih tetap tersimpan.

## 3. Vokal, gate, dan chain

1. Pilih input mikrofon di Settings. Berbicara/singing dengan level normal.
   - **Harapan:** meter MIC dan MASTER bergerak; gate membuka; tidak ada klik/putus.
2. Aktifkan **Test Tone 1 kHz** pada level rendah.
   - **Harapan:** meter master bergerak; tidak clip; matikan kembali sebelum tes berikutnya.
3. Di **Vocal Chain**, ubah Input Gain, HPF, De-esser, Compressor, pitch correction, harmonizer,
   reverb, delay. Nyalakan/matikan tiap toggle.
   - **Harapan:** setiap kontrol mengubah suara atau meter; tidak ada kontrol mati tanpa efek.
4. Uji **Smart EQ** (analisis). Biarkan vokal berjalan beberapa detik, terapkan, lalu undo.
   - **Harapan:** kurva EQ berubah dan bisa dikembalikan; label menjelaskan bahwa ini rule-based.
5. Uji **Harmony Assistant**.
   - **Harapan:** key/scale terdeteksi dari pitch vokal (bukan klaim AI); saran harmonizer bisa
     diterapkan; UI menyebut "rule-based".

## 4. Music bus + WASAPI loopback

Gunakan dua endpoint berbeda agar jelas: **A** = endpoint tempat musik keluar (mis. VB-Audio
Virtual Cable), **B** = output monitoring (headphone/device nyata).

1. Set output aplikasi ke B. Di **Settings → YouTube Music Bus**, pilih endpoint A. Klik
   **Mulai Capture**.
2. Putar sumber audio ke endpoint A (YouTube di browser lain, atau tone generator yang output-nya
   ke A). Untuk uji cepat, mainkan tone 440 Hz ke endpoint A.
   - **Harapan:** meter capture dan meter MUSIC bergerak; status menampilkan source rate →
     target rate, buffer sekitar 40 ms, dan **terbuang 0** pada device yang clock-nya benar.
3. Biarkan berjalan 5–10 menit.
   - **Harapan:** tidak ada dropout/overflow yang terus naik; audio tidak putus-putus. Backlog
     buffer harus bertahan di kisaran 40–80 ms (resampler mengoreksi selisih clock capture vs
     output maksimum ±0,4%). Bila `terbuang` naik terus, clock endpoint terlalu jauh berbeda atau
     CPU overload — ganti endpoint/naikkan buffer.
4. Hentikan capture, lalu mulai lagi.
   - **Harapan:** capture restart tanpa restart aplikasi.
5. Ubah sample rate device saat capture aktif.
   - **Harapan:** loopback di-restart otomatis; status menunjukkan target rate baru.
6. Uji endpoint yang diketahui bermasalah (mis. driver virtual yang salah clock).
   - **Harapan:** aplikasi tetap stabil; counter `terbuang` boleh naik. Ini masalah driver, bukan
     crash aplikasi.

Catatan: loopback menangkap **seluruh** audio sistem pada endpoint A, bukan hanya YouTube.

## 5. Pitch/Key changer dan Music Booster

1. Dengan musik berjalan, ubah Transpose 0 → +12 → −12 → 0.
   - **Harapan:** pitch berubah, tidak ada artefak ekstrem; pada 0 semitone suara kembali normal.
2. Aktifkan **Bypass** transpose.
   - **Harapan:** musik kembali ke pitch asli.
3. Ubah bass/presence/treble/loudness/output booster dan compressor.
   - **Harapan:** perubahan terdengar dan meter master merespons; limiter booster mencegah clip.
4. Uji cepat CPU: mainkan musik + vokal + harmonizer + reverb + plugin. Lihat CPU di top bar.
   - **Harapan:** audio tidak terputus pada buffer yang wajar; jika overload, naikkan buffer.

## 6. Smart Balance (ducking)

1. Nyalakan Smart Balance. Bicara/nyanyi di atas musik.
   - **Harapan:** musik mereda saat vokal aktif dan pulih saat diam; meter GR bergerak.
2. Ubah target/duck/attack/release/strength/max reduction.
   - **Harapan:** perilaku ducking berubah sesuai parameter.
3. Matikan Smart Balance.
   - **Harapan:** musik kembali ke level penuh.

## 7. VST3 hosting

**Peringatan:** plugin berjalan in-process. Uji plugin satu per satu; simpan sesi sebelum mencoba
plugin baru.

1. Buka **VST3 Plugins**, pastikan folder `C:\Program Files\Common Files\VST3`.
2. Klik **Scan Sekarang**.
   - **Harapan:** progress berjalan, status akhir menampilkan jumlah plugin terdeteksi dan jumlah
     file gagal; aplikasi tidak crash. Daftar bisa dicari.
3. Pilih satu plugin, klik **Tambah ke Vocal**, lalu **Tambah ke Master**.
   - **Harapan:** plugin muncul di chain yang benar dengan nama, vendor, format, dan latensi.
4. Uji **Bypass**, tombol **^/v** (urutkan), dan **Hapus**.
   - **Harapan:** urutan berubah, bypass mengubah suara, hapus menghilangkan plugin.
5. Klik **Editor** pada plugin yang punya editor.
   - **Harapan:** jendela editor plugin terbuka dan bisa ditutup. Jika plugin crash, catat plugin
     tersebut; itu risiko in-process, bukan bukti kompatibilitas universal.
6. Simpan sesi, tutup aplikasi, buka lagi.
   - **Harapan:** chain dan state plugin dipulihkan.
7. Buka **File Gagal** bila ada.
   - **Harapan:** daftar file gagal ditampilkan dengan penjelasan (32-bit/rusak/tidak kompatibel).

## 8. Sampler Pads

1. Pastikan ada **tepat 10 pad**.
2. Muat sample WAV pada pad 1, MP3 pada pad 2, FLAC/AIFF pada pad 3.
   - **Harapan:** nama file tampil; pad bisa diputar; meter sampler bergerak.
3. Uji **Loop**, gain per pad, dan stop.
   - **Harapan:** loop berjalan; gain mengubah level; stop menghentikan suara.
4. Uji mute/solo sampler di Mixer.
   - **Harapan:** sampler mengikuti mute/solo.
5. Simpan preset sampler, muat kembali.
   - **Harapan:** konfigurasi pad dipulihkan (path file harus masih valid).

## 9. Mixer

1. Uji fader Mic, Music, Sampler, Master dan input gain.
   - **Harapan:** level berubah halus, tanpa zipper noise.
2. Uji Mute dan Solo pada Mic/Music/Sampler.
   - **Harapan:** solo mengisolasi bus; mute mematikan bus.
3. Uji reverb/delay send di strip Vocal FX.
   - **Harapan:** efek bertambah sesuai mix.
4. Uji Reset Clip pada Master.
   - **Harapan:** indikator clip kembali normal.

## 10. Preset dan Scene

1. Buka **Presets**. Simpan preset Vocal dengan nama uji.
2. Ubah beberapa parameter, lalu muat preset tersebut.
   - **Harapan:** parameter kembali ke nilai tersimpan.
3. Uji Duplicate, Rename, Delete.
   - **Harapan:** preset bawaan tidak bisa dihapus/di-rename; preset pengguna bisa.
4. Simpan dan muat **Scene**.
   - **Harapan:** scene memuat kombinasi parameter yang tercakup (vocal, music, harmonizer,
     master, sampler).
5. Periksa folder `%APPDATA%\Rr Live Studio\Presets\`.
   - **Harapan:** file `.rrpreset` JSON valid dan bisa dibaca kembali.

## 11. Go Live dan routing HP/BIGO

1. Buka **GO LIVE**.
   - **Harapan:** checklist menampilkan status nyata (device, limiter, test tone, clip, capture,
     sinyal master). Dua konfirmasi manual harus dicentang sebelum dinyatakan siap.
2. Baca teks panduan di dialog.
   - **Harapan:** teks menyatakan aplikasi tidak terintegrasi/otomatis dengan BIGO dan tidak
     mengontrol HP.
3. Uji routing sesuai perangkat Anda (speaker + mic HP, interface/mixer USB, atau perangkat
   capture + virtual cable).
   - **Harapan:** audio terdengar di siaran/uji rekaman HP; tidak ada feedback; level tidak clip.
4. Mainkan **Test Tone** dari dialog, atur level, lalu matikan.
   - **Harapan:** tone melewati master VST dan limiter; meter output akhir bergerak.
5. Tutup dialog.
   - **Harapan:** test tone otomatis mati; tidak ada tone yang tertinggal saat live.

## 12. Persistensi, log, dan stabilitas

1. Ubah ukuran/posisi window, tutup, buka lagi.
   - **Harapan:** window dipulihkan.
2. Buka Settings → **Export...**, simpan log.
   - **Harapan:** file log berisi baris diagnostik; tidak ada data audio yang diunggah ke cloud.
3. Jalankan aplikasi 30–60 menit dengan musik + vokal + efek.
   - **Harapan:** tidak crash, tidak memory leak yang terlihat, meter tetap responsif.
4. Cek Task Manager: CPU stabil; tidak ada proses nyangkut setelah aplikasi ditutup.

## 13. Kriteria lulus

- Semua langkah kritis (2, 3, 4, 7, 11) lulus di perangkat target.
- Tidak ada klaim BIGO/ASIO/kompatibilitas VST3 yang melebihi hasil tes.
- Semua kegagalan dicatat: device, driver, plugin, langkah reproduksi, dan screenshot/log.
