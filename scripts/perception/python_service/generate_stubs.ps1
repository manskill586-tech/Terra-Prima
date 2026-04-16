param(
    [string]$PythonExe = "python"
)

$ErrorActionPreference = "Stop"

$serviceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$protoPath = Join-Path (Split-Path -Parent $serviceDir) "proto\\perception.proto"

if (!(Test-Path $protoPath)) {
    throw "Proto file not found: $protoPath"
}

Push-Location $serviceDir
try {
    & $PythonExe -m grpc_tools.protoc `
        -I "..\\proto" `
        --python_out "." `
        --grpc_python_out "." `
        "..\\proto\\perception.proto"
    Write-Host "gRPC stubs generated in $serviceDir"
}
finally {
    Pop-Location
}
