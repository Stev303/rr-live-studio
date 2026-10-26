$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$exe = Join-Path $PSScriptRoot "..\build\RrLiveStudio_artefacts\Release\Rr Live Studio.exe"
$exe = [System.IO.Path]::GetFullPath($exe)

$process = Start-Process -FilePath $exe -PassThru
$deadline = (Get-Date).AddSeconds(15)

while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $process.Refresh()
    if ($process.HasExited) { break }
    if ($process.MainWindowHandle -ne 0 -and $process.MainWindowTitle -ne "") { break }
}

Start-Sleep -Seconds 3
$process.Refresh()

Write-Output ("PID=" + $process.Id)
Write-Output ("EXITED=" + $process.HasExited)
Write-Output ("TITLE=" + $process.MainWindowTitle)
Write-Output ("RESPONDING=" + $process.Responding)

if (-not $process.HasExited) {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $screenshot = Join-Path $PSScriptRoot "..\build\smoke-screenshot.png"
    $bitmap.Save($screenshot, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
    Write-Output ("SCREENSHOT=" + [System.IO.Path]::GetFullPath($screenshot))

    $closed = $process.CloseMainWindow()
    Write-Output ("CLOSE_REQUESTED=" + $closed)
    Start-Sleep -Seconds 3
    $process.Refresh()

    if (-not $process.HasExited) {
        Write-Output "APP_DID_NOT_CLOSE_GRACEFULLY"
        Stop-Process -Id $process.Id -Force
    } else {
        Write-Output ("EXIT_CODE=" + $process.ExitCode)
    }
}
