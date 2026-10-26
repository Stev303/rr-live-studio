$ErrorActionPreference = "Stop"
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class RrInput
{
    [DllImport("user32.dll")] public static extern bool SetCursorPos (int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event (uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
"@

$exe = Join-Path $PSScriptRoot "..\build\RrLiveStudio_artefacts\Release\Rr Live Studio.exe"
$exe = [System.IO.Path]::GetFullPath($exe)
$shotDir = Join-Path $PSScriptRoot "..\build\walkthrough"
$null = New-Item -ItemType Directory -Force -Path $shotDir

$process = Start-Process -FilePath $exe -PassThru
$deadline = (Get-Date).AddSeconds(20)

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $process.Refresh()
    if ($process.HasExited) { break }
    if ($process.MainWindowHandle -ne 0 -and $process.MainWindowTitle -ne "") { break }
}

if ($process.HasExited) {
    Write-Output "APP_EXITED_DURING_STARTUP"
    exit 1
}

Start-Sleep -Seconds 4

$root = [System.Windows.Automation.AutomationElement]::RootElement
$windowCondition = New-Object System.Windows.Automation.PropertyCondition(
    [System.Windows.Automation.AutomationElement]::NameProperty, "Rr Live Studio")
$window = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $windowCondition)

if ($null -eq $window) {
    Write-Output "WINDOW_NOT_FOUND"
    Stop-Process -Id $process.Id -Force
    exit 1
}

$windowRect = $window.Current.BoundingRectangle
Write-Output ("WINDOW=" + $windowRect)

function Find-Element ([string] $name, $controlType) {
    $cond = New-Object System.Windows.Automation.AndCondition(
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::NameProperty, $name)),
        (New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ControlTypeProperty, $controlType)))
    return $window.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $cond)
}

function Invoke-Element ($element) {
    if ($null -eq $element) { return $false }

    try {
        $pattern = $element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
        $pattern.Invoke()
        return $true
    } catch { }

    try {
        $pattern = $element.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern)
        $pattern.Toggle()
        return $true
    } catch { }

    try {
        $pattern = $element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
        $pattern.Select()
        return $true
    } catch { }

    $rect = $element.Current.BoundingRectangle
    if ($rect.Width -le 0 -or $rect.Height -le 0) { return $false }

    [RrInput]::SetCursorPos([int]($rect.X + $rect.Width / 2), [int]($rect.Y + $rect.Height / 2)) | Out-Null
    Start-Sleep -Milliseconds 120
    [RrInput]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    [RrInput]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    return $true
}

function Save-Shot ([string] $name) {
    $rect = $window.Current.BoundingRectangle
    $x = [int]$rect.X
    $y = [int]$rect.Y
    $w = [int]$rect.Width
    $h = [int]$rect.Height
    $bitmap = New-Object System.Drawing.Bitmap $w, $h
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen((New-Object System.Drawing.Point $x, $y), [System.Drawing.Point]::Empty, $bitmap.Size)
    $path = Join-Path $shotDir ($name + ".png")
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
    Write-Output ("SCREENSHOT=" + [System.IO.Path]::GetFullPath($path))
}

$pages = @(
    @("Dashboard", "01-dashboard"),
    @("YouTube", "02-youtube"),
    @("Vocal Chain", "03-vocal-chain"),
    @("Effects", "04-effects"),
    @("VST3 Plugins", "05-vst3"),
    @("Sampler Pads", "06-sampler"),
    @("Mixer", "07-mixer"),
    @("Presets", "08-presets"),
    @("Settings", "09-settings")
)

foreach ($page in $pages) {
    $element = Find-Element $page[0] ([System.Windows.Automation.ControlType]::RadioButton)
    if ($null -eq $element) {
        Write-Output ("PAGE_NOT_FOUND=" + $page[0])
        continue
    }

    $ok = Invoke-Element $element
    Start-Sleep -Milliseconds 1200
    Write-Output ("PAGE=" + $page[0] + " CLICKED=" + $ok)
    Save-Shot $page[1]
}

$goLive = Find-Element "GO LIVE" ([System.Windows.Automation.ControlType]::Button)
if ($null -ne $goLive) {
    $ok = Invoke-Element $goLive
    Start-Sleep -Milliseconds 1200
    Write-Output ("GOLIVE_OPENED=" + $ok)
    Save-Shot "10-golive"

    $close = Find-Element "Tutup" ([System.Windows.Automation.ControlType]::Button)
    if ($null -ne $close) {
        $closed = Invoke-Element $close
        Start-Sleep -Milliseconds 800
        Write-Output ("GOLIVE_CLOSED=" + $closed)
    } else {
        Write-Output "GOLIVE_CLOSE_NOT_FOUND"
    }
} else {
    Write-Output "GOLIVE_BUTTON_NOT_FOUND"
}

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
