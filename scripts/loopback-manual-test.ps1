$ErrorActionPreference = "Stop"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Threading;
using System.Runtime.InteropServices;
public static class RrLoopTest
{
    [DllImport("user32.dll")] public static extern bool SetCursorPos (int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event (uint flags, uint dx, uint dy, uint data, UIntPtr extra);

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEFORMATEX
    {
        public ushort wFormatTag;
        public ushort nChannels;
        public uint nSamplesPerSec;
        public uint nAvgBytesPerSec;
        public ushort nBlockAlign;
        public ushort wBitsPerSample;
        public ushort cbSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEHDR
    {
        public IntPtr lpData;
        public uint dwBufferLength;
        public uint dwBytesRecorded;
        public uint dwUser;
        public uint dwFlags;
        public uint dwLoops;
        public IntPtr lpNext;
        public IntPtr reserved;
    }

    [DllImport("winmm.dll")] static extern int waveOutOpen (out IntPtr h, uint device, ref WAVEFORMATEX fmt, IntPtr callback, IntPtr instance, uint flags);
    [DllImport("winmm.dll")] static extern int waveOutPrepareHeader (IntPtr h, ref WAVEHDR hdr, int size);
    [DllImport("winmm.dll")] static extern int waveOutWrite (IntPtr h, ref WAVEHDR hdr, int size);
    [DllImport("winmm.dll")] static extern int waveOutReset (IntPtr h);
    [DllImport("winmm.dll")] static extern int waveOutUnprepareHeader (IntPtr h, ref WAVEHDR hdr, int size);
    [DllImport("winmm.dll")] static extern int waveOutClose (IntPtr h);

    public static void PlayAsync (int device, int seconds, double frequency, double amplitude)
    {
        var thread = new Thread (() => Play (device, seconds, frequency, amplitude));
        thread.IsBackground = true;
        thread.Start();
    }

    static void Play (int device, int seconds, double frequency, double amplitude)
    {
        const int sampleRate = 48000;
        const int channels = 2;
        int frames = sampleRate * seconds;
        byte[] data = new byte[frames * channels * 2];
        short peak = (short) (amplitude * 32767.0);

        for (int i = 0; i < frames; ++i)
        {
            short sample = (short) (Math.Sin (2.0 * Math.PI * frequency * i / sampleRate) * peak);
            int offset = i * 4;
            data[offset] = (byte) (sample & 0xFF);
            data[offset + 1] = (byte) ((sample >> 8) & 0xFF);
            data[offset + 2] = data[offset];
            data[offset + 3] = data[offset + 1];
        }

        GCHandle pinned = GCHandle.Alloc (data, GCHandleType.Pinned);

        try
        {
            WAVEFORMATEX fmt = new WAVEFORMATEX();
            fmt.wFormatTag = 1;
            fmt.nChannels = (ushort) channels;
            fmt.nSamplesPerSec = (uint) sampleRate;
            fmt.wBitsPerSample = 16;
            fmt.nBlockAlign = (ushort) (channels * 2);
            fmt.nAvgBytesPerSec = (uint) (sampleRate * channels * 2);

            IntPtr handle;
            int openResult = waveOutOpen (out handle, (uint) device, ref fmt, IntPtr.Zero, IntPtr.Zero, 0);

            if (openResult != 0)
            {
                Console.WriteLine ("TONE_OPEN_FAILED=" + openResult);
                return;
            }

            WAVEHDR hdr = new WAVEHDR();
            hdr.lpData = pinned.AddrOfPinnedObject();
            hdr.dwBufferLength = (uint) data.Length;

            waveOutPrepareHeader (handle, ref hdr, Marshal.SizeOf (hdr));
            waveOutWrite (handle, ref hdr, Marshal.SizeOf (hdr));
            Console.WriteLine ("TONE_PLAYING device=" + device + " seconds=" + seconds);
            Thread.Sleep (seconds * 1000);
            waveOutReset (handle);
            waveOutUnprepareHeader (handle, ref hdr, Marshal.SizeOf (hdr));
            waveOutClose (handle);
            Console.WriteLine ("TONE_DONE");
        }
        finally
        {
            pinned.Free();
        }
    }
}
"@

$exe = Join-Path $PSScriptRoot "..\build\RrLiveStudio_artefacts\Release\Rr Live Studio.exe"
$exe = [System.IO.Path]::GetFullPath($exe)
$shotDir = Join-Path $PSScriptRoot "..\build\loopback-manual"
$null = New-Item -ItemType Directory -Force -Path $shotDir

$settings = Join-Path $env:APPDATA "Rr Live Studio\Rr Live Studio.settings"
$backup = "$settings.loopbacktest.bak"

if (-not (Test-Path $settings)) {
    Write-Output "SETTINGS_NOT_FOUND=$settings"
    exit 1
}

Copy-Item $settings $backup -Force
Write-Output "SETTINGS_BACKUP=$backup"

function Restore-Settings {
    if (Test-Path $backup) {
        Copy-Item $backup $settings -Force
        Remove-Item $backup -Force
        Write-Output "SETTINGS_RESTORED"
    }
}

function Save-Shot ([string] $name) {
    $rect = $window.Current.BoundingRectangle
    $bitmap = New-Object System.Drawing.Bitmap ([int]$rect.Width), ([int]$rect.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen((New-Object System.Drawing.Point ([int]$rect.X), ([int]$rect.Y)), [System.Drawing.Point]::Empty, $bitmap.Size)
    $path = Join-Path $shotDir ($name + ".png")
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose(); $bitmap.Dispose()
    Write-Output ("SCREENSHOT=" + [System.IO.Path]::GetFullPath($path))
}

function Dump-Status {
    $textCondition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Text)
    $texts = $window.FindAll([System.Windows.Automation.TreeScope]::Descendants, $textCondition)
    foreach ($text in $texts) {
        $name = $text.Current.Name
        if ($name -match "Capture aktif|kHz|terbuang|buffer|Loopback|Dropout|Overflow") {
            Write-Output ("STATUS=" + ($name -replace "`r?`n", " | "))
        }
    }
}

$process = $null
$window = $null

try {
    [xml] $doc = Get-Content $settings -Raw
    $props = $doc.SelectSingleNode("/PROPERTIES")

    foreach ($value in $props.SelectNodes("VALUE")) {
        $name = $value.GetAttribute("name")
        if ($name -eq "loopbackEndpointId")
            { $value.SetAttribute("val", "{0.0.0.00000000}.{642a9485-1b82-4265-8ae6-4ef35440cbd9}") }
        elseif ($name -eq "loopbackActive")
            { $value.SetAttribute("val", "1") }
    }

    $setup = $props.SelectSingleNode("VALUE[@name='audioDeviceState']/DEVICESETUP")
    $setup.SetAttribute("deviceType", "Windows Audio")
    $setup.SetAttribute("audioOutputDeviceName", "Speakers (High Definition Audio Device)")
    $setup.SetAttribute("audioInputDeviceName", "Microphone (High Definition Audio Device)")
    $setup.SetAttribute("audioDeviceRate", "48000.0")
    $setup.SetAttribute("audioDeviceBufferSize", "256")

    $doc.Save($settings)
    Write-Output "SETTINGS_PREPARED master=HDA@48000 loopback=VB-Cable active=1"

    $process = Start-Process -FilePath $exe -PassThru
    $deadline = (Get-Date).AddSeconds(25)

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) { break }
        if ($process.MainWindowHandle -ne 0 -and $process.MainWindowTitle -ne "") { break }
    }

    if ($process.HasExited) {
        Write-Output "APP_EXITED_DURING_STARTUP code=$($process.ExitCode)"
        throw "Aplikasi keluar saat startup"
    }

    Start-Sleep -Seconds 6

    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children,
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::NameProperty, "Rr Live Studio")))

    if ($null -eq $window) {
        Write-Output "WINDOW_NOT_FOUND"
        throw "Window tidak ditemukan"
    }

    $youtube = $window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
        (New-Object System.Windows.Automation.AndCondition(
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::NameProperty, "YouTube")),
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::RadioButton)))))

    if ($null -ne $youtube) {
        $rect = $youtube.Current.BoundingRectangle
        [RrLoopTest]::SetCursorPos([int]($rect.X + $rect.Width / 2), [int]($rect.Y + $rect.Height / 2)) | Out-Null
        Start-Sleep -Milliseconds 150
        [RrLoopTest]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
        [RrLoopTest]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Seconds 3
        Write-Output "YOUTUBE_PAGE_OPENED=True"
    } else {
        Write-Output "YOUTUBE_PAGE_OPENED=False"
    }

    Dump-Status
    Save-Shot "01-baseline-sebelum-tone"

    Write-Output "MULAI TONE 440 Hz ke waveOut device 4 (VB-Audio Virtual Cable)"
    [RrLoopTest]::PlayAsync(4, 9, 440.0, 0.25)
    Start-Sleep -Seconds 4

    Dump-Status
    Save-Shot "02-saat-tone"

    Start-Sleep -Seconds 6
    Dump-Status
    Save-Shot "03-setelah-tone"

    $process.Refresh()
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        Start-Sleep -Seconds 3
        $process.Refresh()
        if (-not $process.HasExited) {
            Write-Output "APP_DID_NOT_CLOSE_GRACEFULLY"
            Stop-Process -Id $process.Id -Force
        } else {
            Write-Output ("EXIT_CODE=" + $process.ExitCode)
        }
    }
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            Write-Output "APP_FORCED_CLOSED"
        }
    }
    Restore-Settings
}
