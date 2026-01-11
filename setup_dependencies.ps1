$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "EPUB Reader - Setting up dependencies" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

New-Item -ItemType Directory -Force -Path "libs" | Out-Null

Write-Host "Downloading miniz..." -ForegroundColor Yellow
$minizUrl = "https://github.com/richgel999/miniz/releases/download/2.1.0/miniz-2.1.0.zip"
$minizZip = "libs/miniz.zip"

if (-not (Test-Path "libs/miniz.h")) {
    Invoke-WebRequest -Uri $minizUrl -OutFile $minizZip
    Expand-Archive -Path $minizZip -DestinationPath "libs/miniz_temp" -Force
    Copy-Item "libs/miniz_temp/miniz.h" "libs/miniz.h"
    Copy-Item "libs/miniz_temp/miniz.c" "libs/miniz.c"
    Remove-Item -Recurse -Force "libs/miniz_temp"
    Remove-Item $minizZip
    Write-Host "  miniz downloaded successfully" -ForegroundColor Green
} else {
    Write-Host "  miniz already exists, skipping" -ForegroundColor Gray
}

Write-Host "Downloading stb_image..." -ForegroundColor Yellow
$stbUrl = "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
$stbPath = "libs/stb_image.h"

if (-not (Test-Path $stbPath)) {
    Invoke-WebRequest -Uri $stbUrl -OutFile $stbPath
    Write-Host "  stb_image downloaded successfully" -ForegroundColor Green
} else {
    Write-Host "  stb_image already exists, skipping" -ForegroundColor Gray
}

Write-Host "Setting up Lexbor..." -ForegroundColor Yellow
$lexborUrl = "https://github.com/lexbor/lexbor/archive/refs/tags/v2.6.0.zip"
$lexborZip = "libs/lexbor.zip"
$lexborDir = "libs/lexbor"

if (-not (Test-Path $lexborDir)) {
    Write-Host "  Downloading Lexbor..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $lexborUrl -OutFile $lexborZip
    Expand-Archive -Path $lexborZip -DestinationPath "libs" -Force
    Move-Item "libs/lexbor-2.6.0" $lexborDir
    Remove-Item $lexborZip
}

$lexborBuilt = $false
$lexborLibPaths = @(
    "$lexborDir/build/liblexbor_static.lib",
    "$lexborDir/build/Release/liblexbor_static.lib",
    "$lexborDir/build/lib/Release/liblexbor_static.lib",
    "$lexborDir/build/source/lexbor/Release/lexbor_static.lib",
    "$lexborDir/build/source/lexbor/lexbor_static.lib"
)

foreach ($path in $lexborLibPaths) {
    if (Test-Path $path) {
        $lexborBuilt = $true
        Write-Host "  Lexbor library found at: $path" -ForegroundColor Gray
        break
    }
}

if (-not $lexborBuilt) {
    Write-Host "  Building Lexbor (this may take a few minutes)..." -ForegroundColor Yellow
    
    Push-Location $lexborDir
    
    Write-Host "    Configuring..." -ForegroundColor Gray
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DLEXBOR_BUILD_SHARED=OFF -DLEXBOR_BUILD_STATIC=ON
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "Lexbor CMake configuration failed"
    }
    
    Write-Host "    Building..." -ForegroundColor Gray
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "Lexbor build failed"
    }
    
    Write-Host "    Searching for built library files..." -ForegroundColor Gray
    Get-ChildItem -Path build -Recurse -Filter "*.lib" | ForEach-Object { 
        Write-Host "      Found: $($_.FullName)" -ForegroundColor Gray
    }
    
    Pop-Location
    Write-Host "  Lexbor built successfully" -ForegroundColor Green
} else {
    Write-Host "  Lexbor already built, skipping" -ForegroundColor Gray
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "All dependencies ready!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. cmake -B build -DCMAKE_BUILD_TYPE=Release" -ForegroundColor White
Write-Host "  2. cmake --build build --config Release" -ForegroundColor White
Write-Host ""