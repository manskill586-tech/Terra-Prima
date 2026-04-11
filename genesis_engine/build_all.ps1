param(
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"

Write-Host "== Building SIM Core =="
& cmake -S $Root -B $BuildDir
& cmake --build $BuildDir --config $Config

Write-Host "== Building GDExtension =="
$GodotCppPath = Join-Path $Root "renderer\\addons\\godot-cpp"
if (!(Test-Path $GodotCppPath)) {
  Write-Host "godot-cpp not found. Initializing submodule..."
  & git submodule update --init --recursive "$Root\\renderer\\addons\\godot-cpp"
}

Push-Location (Join-Path $Root "renderer\\addons\\genesis_bridge")
& scons platform=windows target=template_debug
& scons platform=windows target=template_release
Pop-Location

Write-Host "Done."
