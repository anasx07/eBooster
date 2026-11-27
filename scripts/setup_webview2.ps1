$src = "build_msvc\webview2_test\build\native"
$dst = "third_party\webview2"

if (-not (Test-Path $dst)) {
    New-Item -ItemType Directory -Path $dst | Out-Null
}

Copy-Item -Path "$src\include" -Destination "$dst\include" -Recurse -Force
Copy-Item -Path "$src\x64" -Destination "$dst\x64" -Recurse -Force

Write-Host "WebView2 SDK staged successfully in $dst" -ForegroundColor Green
Get-ChildItem -Path $dst -Recurse | Select-Object FullName
