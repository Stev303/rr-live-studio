# Lisensi dan komponen pihak ketiga

Dokumen ini menjelaskan komponen pihak ketiga yang dipakai Rr Live Studio. Dokumen ini bukan
nasihat hukum. Sebelum mendistribusikan aplikasi, periksa sendiri teks lisensi lengkap pada file
yang disebut di bawah dan penuhi kewajibannya.

## 1. Kode aplikasi Rr Live Studio

Kode sumber pertama (`Source/`, `Tests/`, `scripts/`) dibuat untuk proyek ini. Tidak ada lisensi
publik terpisah yang disertakan; hak distribusi ditentukan oleh pemilik proyek. Distribusi binary
tetap harus mematuhi semua lisensi pihak ketiga di bawah.

## 2. JUCE 8.0.10

- Lokasi: `external/JUCE/`
- Lisensi: **dual-license** — AGPLv3 **atau** lisensi komersial JUCE 8.
- Teks: `external/JUCE/LICENSE.md`, https://juce.com/legal/juce-8-licence/
- Jika aplikasi didistribusikan tanpa lisensi komersial JUCE, aplikasi harus mematuhi AGPLv3
  (termasuk kewajiban menyediakan source code kepada penerima sesuai ketentuan AGPLv3).
- Proyek ini memakai modul JUCE: core, events, data_structures, graphics, gui_basics, gui_extra,
  audio_basics, audio_devices, audio_formats, audio_processors, audio_utils, dsp, webview2.

## 3. VST3 SDK (dibundel JUCE)

- Lokasi: `external/JUCE/modules/juce_audio_processors/format_types/VST3_SDK/`
- Lisensi: **Steinberg VST3 License** atau **GPLv3**, sesuai pilihan.
- Teks: `VST3_SDK/LICENSE.txt`, `VST3_License_Agreement.pdf`,
  https://www.steinberg.net/sdklicenses_vst3
- Catatan: distribusi binary yang memakai VST3 SDK tunduk pada ketentuan Steinberg, termasuk
  kewajiban lisensi komersial untuk penggunaan proprietary.

## 4. Microsoft Edge WebView2 SDK

- Lokasi: `external/webview2pkg/Microsoft.Web.WebView2.1.0.1901.177/`
- Lisensi: syarat lisensi Microsoft untuk WebView2 SDK (paket NuGet resmi Microsoft).
- Informasi: https://learn.microsoft.com/microsoft-edge/webview2/
- Runtime **WebView2 Evergreen** dipasang terpisah di komputer pengguna dan tunduk pada syarat
  distribusi Microsoft. Aplikasi ini menautkan `WebView2LoaderStatic.lib` secara statis.

## 5. Windows SDK, MSVC, dan runtime Visual C++

- Windows SDK dan compiler MSVC digunakan untuk membangun aplikasi dan tunduk pada lisensi
  Microsoft.
- Binary Release menautkan runtime MSVC sesuai pengaturan CMake/JUCE. Periksa ketentuan
  redistributable Microsoft sebelum mendistribusikan.

## 6. Steinberg ASIO SDK (opsional, tidak dibundel)

- Build default **tidak** menyertakan ASIO SDK.
- Jika Anda mengaktifkan `RRLIVE_ASIO_SDK_PATH`, lisensi Steinberg ASIO SDK berlaku.
- ASIO adalah merek dagang Steinberg Media Technologies GmbH.

## 7. Decoder audio di JUCE

- WAV/AIFF/FLAC ditangani modul `juce_audio_formats` (bagian dari JUCE).
- **MP3** diaktifkan melalui `JUCE_USE_MP3AUDIOFORMAT=1`. Header JUCE
  (`juce_MP3AudioFormat.h`) menyatakan kode MP3 dipakai "at your own risk" dan **tidak dijamin
  bebas dari pelanggaran hak pihak ketiga**, termasuk hak paten MP3 di yurisdiksi tertentu.
  Jika Anda mendistribusikan aplikasi secara komersial, tinjau implikasi lisensi MP3 terlebih
  dahulu. Menonaktifkan flag tersebut akan menghilangkan dukungan MP3.
- Dukungan Ogg Vorbis/Windows Media tidak diaktifkan secara eksplisit oleh proyek ini.

## 8. Perpustakaan sistem Windows

Aplikasi menautkan pustaka sistem Windows (ole32, oleaut32, propsys, uuid, avrt, winmm, shlwapi,
version, winhttp, setupapi bila ASIO aktif). Pustaka tersebut adalah bagian dari Windows dan
tunduk pada lisensi Microsoft.

## 9. Konten dan layanan pihak ketiga

- YouTube diakses melalui browser WebView2 biasa. Aplikasi tidak mengunduh, membajak DRM/iklan,
  atau menyimpan konten YouTube. Penggunaan tunduk pada Ketentuan Layanan YouTube.
- BIGO LIVE tidak terintegrasi dan tidak disponsori oleh proyek ini. Merek dagang milik pemiliknya
  masing-masing.
