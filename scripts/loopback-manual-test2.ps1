$ErrorActionPreference = "Stop"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Threading;
using System.Runtime.InteropServices;
public static class RrLoopTest2
{
    [DllImport("user32.dll")] public static extern bool SetCursorPos (int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event (uint flags, uint dx, uint dy, uint data, UIntPtr extra);

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEFORMATEX
    {
        public ushort wFormatTag; public ushort nChannels; public uint nSamplesPerSec;
        public uint nAvgBytesPerSec; public ushort nBlockAlign; public ushort wBitsPerSample; public ushort cbSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEHDR
    {
        public IntPtr lpData; public uint dwBufferLength; public uint dwBytesRecorded; public uint dwUser;
        public uint dwFlags; public uint dwLoops; public IntPtr lpNext; public IntPtr reserved;
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
            if (waveOutOpen (out handle, (uint) device, ref fmt, IntPtr.Zero, IntPtr.Zero, 0) != 0)
                return;

            WAVEHDR hdr = new WAVEHDR();
            hdr.lpData = pinned.AddrOfPinnedObject();
            hdr.dwBufferLength = (uint) data.Length;
            waveOutPrepareHeader (handle, ref hdr, Marshal.SizeOf (hdr));
            waveOutWrite (handle, ref hdr, Marshal.SizeOf (hdr));
            Thread.Sleep (seconds * 1000);
            waveOutReset (handle);
            waveOutUnprepareHeader (handle, ref hdr, Marshal.SizeOf (hdr));
            waveOutClose (handle);
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

function Dump-Status ([string] $tag) {
    $condition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::Text)
    $texts = $window.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition)
    foreach ($text in $texts) {
        $name = $text.Current.Name
        if ($name -match "Capture aktif|Capture berhenti|Capture nonaktif|terbuang|kHz ->|Dropout|Overflow") {
            Write-Output ("STATUS[" + $tag + "]=" + ($name -replace "`r?`n", " | "))
        }
    }
}

function Find-Button ([string] $name) {
    return $window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
        (New-Object System.Windows.Automation.AndCondition(
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::NameProperty, $name)),
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::Button)))))
}

function Find-Radio ([string] $name) {
    return $window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
        (New-Object System.Windows.Automation.AndCondition(
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::NameProperty, $name)),
            (New-Object System.Windows.Automation.PropertyCondition(
                [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
                [System.Windows.Automation.ControlType]::RadioButton)))))
}

function Click-Rect ($rect) {
    if ($null -eq $rect -or $rect.Width -le 0 -or $rect.Height -le 0) { return $false }
    [RrLoopTest2]::SetCursorPos([int]($rect.X + $rect.Width / 2), [int]($rect.Y + $rect.Height / 2)) | Out-Null
    Start-Sleep -Milliseconds 150
    [RrLoopTest2]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [RrLoopTest2]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    return $true
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
    if ($process.HasExited) { throw "Aplikasi keluar saat startup" }
    Start-Sleep -Seconds 6

    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children,
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::NameProperty, "Rr Live Studio")))
    if ($null -eq $window) { throw "Window tidak ditemukan" }

    $youtube = Find-Radio "YouTube"
    Click-Rect $youtube.Current.BoundingRectangle | Out-Null
    Start-Sleep -Seconds 3
    Dump-Status "awal"

    $wshell = New-Object -ComObject WScript.Shell
    Write-Output "MULAI TONE 20 DETIK"
    [RrLoopTest2]::PlayAsync(4, 20, 440.0, 0.25)

    foreach ($elapsed in 2,4,6,8,10,12,14,16,18,20) {
        Start-Sleep -Seconds 2
        Dump-Status ("t+" + $elapsed)
        if ($elapsed -eq 8) { Save-Shot "04-saat-tone" }
    }

    Save-Shot "04-stabilitas-akhir"

    $stopButton = Find-Button "Hentikan Capture"
    if ($null -ne $stopButton) {
        Write-Output ("STOP_CAPTURE_CLICKED=" + (Click-Rect $stopButton.Current.BoundingRectangle))
        Start-Sleep -Seconds 3
        Dump-Status "setelah-stop"
        Save-Shot "05-capture-dihentikan"

        $startButton = Find-Button "Mulai Capture"
        if ($null -ne $startButton) {
            Write-Output ("START_CAPTURE_CLICKED=" + (Click-Rect $startButton.Current.BoundingRectangle))
            Start-Sleep -Seconds 5
            Dump-Status "setelah-start-ulang"
            Save-Shot "06-capture-start-ulang"
        } else {
            Write-Output "START_CAPTURE_BUTTON_NOT_FOUND"
        }
    } else {
        Write-Output "STOP_CAPTURE_BUTTON_NOT_FOUND"
    }

    # Perubahan sample rate saat capture aktif, lewat ComboBox di Settings.
    $settingsPage = Find-Radio "Settings"
    Click-Rect $settingsPage.Current.BoundingRectangle | Out-Null
    Start-Sleep -Seconds 2

    $comboCondition = New-Object System.Windows.Automation.PropertyCondition(
        [System.Windows.Automation.AutomationElement]::ControlTypeProperty,
        [System.Windows.Automation.ControlType]::ComboBox)
    $combos = $window.FindAll([System.Windows.Automation.TreeScope]::Descendants, $comboCondition)
    foreach ($combo in $combos) {
        Write-Output ("COMBO=" + $combo.Current.Name + " RECT=" + $combo.Current.BoundingRectangle)
    }

    $rateCombo = $null
    foreach ($combo in $combos) {
        if ($combo.Current.Name -match "Hz" -and $combo.Current.Name -match "000") { $rateCombo = $combo }
    }

    if ($null -ne $rateCombo) {
        Write-Output ("RATE_COMBO=" + $rateCombo.Current.Name)
        Click-Rect $rateCombo.Current.BoundingRectangle | Out-Null
        Start-Sleep -Seconds 1
        $wshell.SendKeys("{UP}")
        Start-Sleep -Milliseconds 300
        $wshell.SendKeys("{ENTER}")
        Start-Sleep -Seconds 6
        Dump-Status "setelah-ganti-rate"
        Save-Shot "07-setelah-ganti-sample-rate"
    } else {
        Write-Output "RATE_COMBO_NOT_FOUND"
    }

    $process.Refresh()
    if (-not $process.HasExited) {
        $null = $process.CloseMainWindow()
        Start-Sleep -Seconds 3
        $process.Refresh()
        if (-not $process.HasExited) { Write-Output "APP_DID_NOT_CLOSE_GRACEFULLY"; Stop-Process -Id $process.Id -Force }
        else { Write-Output ("EXIT_CODE=" + $process.ExitCode) }
    }
}
finally {
    if ($null -ne $process) {
        $process.Refresh()
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; Write-Output "APP_FORCED_CLOSED" }
    }
    Restore-Settings
}
