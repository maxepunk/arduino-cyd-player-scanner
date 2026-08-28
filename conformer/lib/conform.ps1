# conform.ps1 - shared core for the Windows launcher.
#
# Windows equivalent of conform.sh. THE TWO MUST STAY IN STEP - if you
# change a format constant in one, change it in the other.
#
# Deliberately plain PowerShell: no classes, no modules, nothing that needs
# a recent version. It has to work on a stock Windows box.

$ErrorActionPreference = 'Continue'

$InDir  = '1-PUT-YOUR-FILES-HERE'
$OutDir = '2-READY-FOR-CARD'

# Audio: AudioGeneratorWAV accepts uncompressed PCM WAV, 8/16-bit only.
$SampleRate = 22050
$Channels   = 1
$ACodec     = 'pcm_s16le'

# Image: parseBMPHeader + the render loop require 24bpp, compression 0,
# positive height (bottom-up), and width*3 divisible by 4 because there is
# no row-padding handling. 240x320 satisfies all of it.
$ImgW   = 240
$ImgH   = 320
$PixFmt = 'bgr24'

Write-Host ''
Write-Host '==============================================='
Write-Host '   Ghost Asset Conformer'
Write-Host '==============================================='
Write-Host ''

$ffmpeg = Join-Path 'tools\win' 'ffmpeg.exe'
if (-not (Test-Path $ffmpeg)) {
    Write-Host 'PROBLEM: the conversion tool is missing.'
    Write-Host ''
    Write-Host "The folder 'tools\win' should contain a file called 'ffmpeg.exe'."
    Write-Host 'If you unzipped everything together it should be there.'
    Write-Host 'Try downloading and unzipping the whole folder again.'
    exit 1
}

# Kept in step with conform.sh: verify the tool actually RUNS, not merely
# that the file exists. Otherwise every file reports "could not be
# converted", blaming the user's audio for a broken tool.
$ffprobeOk = $true
try {
    & $ffmpeg -version *> $null
    if ($LASTEXITCODE -ne 0) { $ffprobeOk = $false }
} catch {
    $ffprobeOk = $false
}
if (-not $ffprobeOk) {
    Write-Host 'PROBLEM: the conversion tool will not run on this computer.'
    Write-Host ''
    Write-Host 'Tell whoever sent you this folder that ffmpeg.exe will not'
    Write-Host 'run on your PC - they can send a different version.'
    exit 1
}

if (-not (Test-Path $InDir)) {
    Write-Host "PROBLEM: can't find the folder '$InDir'."
    Write-Host 'This file needs to stay in the same folder as it.'
    exit 1
}

New-Item -ItemType Directory -Force -Path "$OutDir\assets\audio" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\assets\images" | Out-Null

# Only clear DERIVED output. config.txt lives in $OutDir and is the user's
# to edit - wiping the whole folder would silently discard their volume.
Remove-Item "$OutDir\assets\audio\*.wav"  -ErrorAction SilentlyContinue
Remove-Item "$OutDir\assets\images\*.bmp" -ErrorAction SilentlyContinue

# Mirror cleanTokenId(): drop extension, lowercase, strip spaces and colons.
function Get-CleanName($path) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($path)
    return ($base.ToLower() -replace '[ :]', '')
}

$audioExt = @('.wav','.mp3','.m4a','.aac','.flac','.ogg','.aif','.aiff')
$imageExt = @('.png','.jpg','.jpeg','.bmp','.gif','.tif','.tiff','.webp')

$all = Get-ChildItem -Path $InDir -File -ErrorAction SilentlyContinue
$audioFiles = @($all | Where-Object { $audioExt -contains $_.Extension.ToLower() })
$imageFiles = @($all | Where-Object { $imageExt -contains $_.Extension.ToLower() })

if ($audioFiles.Count -eq 0 -and $imageFiles.Count -eq 0) {
    Write-Host "There are no files in '$InDir' yet."
    Write-Host ''
    Write-Host 'Put your ghost sounds (and pictures, if you have any) in there,'
    Write-Host 'then run this again.'
    exit 1
}

$names  = New-Object System.Collections.ArrayList
$errors = 0

function Convert-One($src, $kind, $outSub, $ext) {
    $clean = Get-CleanName $src.Name
    if ([string]::IsNullOrEmpty($clean)) {
        Write-Host "  SKIPPED  $($src.Name)  (name becomes empty)"
        $script:errors++
        return
    }

    $out = Join-Path $outSub "$clean.$ext"
    if (Test-Path $out) {
        Write-Host "  PROBLEM  $($src.Name)"
        Write-Host "           Another file is already called '$clean.$ext'."
        Write-Host "           Two files can't share a ghost name - rename one."
        $script:errors++
        return
    }

    if ($kind -eq 'audio') {
        & $ffmpeg -loglevel error -y -i $src.FullName `
            -acodec $ACodec -ar $SampleRate -ac $Channels $out 2>$null
    } else {
        $vf = "scale=${ImgW}:${ImgH}:force_original_aspect_ratio=increase,crop=${ImgW}:${ImgH}"
        & $ffmpeg -loglevel error -y -i $src.FullName `
            -vf $vf -pix_fmt $PixFmt -frames:v 1 $out 2>$null
    }

    if (-not (Test-Path $out) -or (Get-Item $out).Length -eq 0) {
        Write-Host "  PROBLEM  $($src.Name)  (could not be converted)"
        Remove-Item $out -ErrorAction SilentlyContinue
        $script:errors++
        return
    }

    Write-Host "  OK       $($src.Name)  ->  $clean.$ext"
    [void]$script:names.Add($clean)
}

if ($audioFiles.Count -gt 0) {
    Write-Host 'Sounds:'
    foreach ($f in $audioFiles) { Convert-One $f 'audio' "$OutDir\assets\audio" 'wav' }
    Write-Host ''
}

if ($imageFiles.Count -gt 0) {
    Write-Host 'Pictures:'
    foreach ($f in $imageFiles) { Convert-One $f 'image' "$OutDir\assets\images" 'bmp' }
    Write-Host ''
}

if ($names.Count -eq 0) {
    Write-Host 'Nothing was converted successfully. See the problems above.'
    exit 1
}

# Write tokens.json so nobody has to hand-edit JSON. A missing comma in a
# hand-edited file is the single most likely way to break a working card.
$uniq = $names | Sort-Object -Unique
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('{')
[void]$sb.AppendLine('  "tokens": {')
for ($i = 0; $i -lt $uniq.Count; $i++) {
    if ($i -eq $uniq.Count - 1) { [void]$sb.AppendLine("    `"$($uniq[$i])`": {}") }
    else                        { [void]$sb.AppendLine("    `"$($uniq[$i])`": {},") }
}
[void]$sb.AppendLine('  }')
[void]$sb.AppendLine('}')
Set-Content -Path (Join-Path $OutDir 'tokens.json') -Value $sb.ToString() -Encoding ASCII

Write-Host '==============================================='
Write-Host "  Done - $($uniq.Count) ghost(s) ready"
Write-Host '==============================================='
Write-Host ''
Write-Host ('  ' + ($uniq -join '  '))
Write-Host ''
Write-Host "Now copy EVERYTHING inside '$OutDir'"
Write-Host 'onto your microSD card.'
Write-Host ''
if ($errors -gt 0) {
    Write-Host "NOTE: $errors file(s) had problems - see above."
    Write-Host ''
}
