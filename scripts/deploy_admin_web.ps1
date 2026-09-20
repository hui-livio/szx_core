[CmdletBinding()]
param(
    [string]$StaticRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$dist = Join-Path $repoRoot "RBAC/szx_admin_web/dist"
$index = Join-Path $dist "index.html"
if (-not (Test-Path -LiteralPath $index -PathType Leaf)) {
    throw "Build the frontend first: cd RBAC/szx_admin_web; npm run build"
}
if (-not (Get-Content -LiteralPath $index -Raw).Contains('/admin/assets/')) {
    throw "The frontend must be built with base /admin/. Run npm run build again."
}
if (-not $StaticRoot) {
    $StaticRoot = Join-Path $repoRoot "build/bin/web"
}
$destination = Join-Path ([System.IO.Path]::GetFullPath($StaticRoot)) "admin"
New-Item -ItemType Directory -Path $destination -Force | Out-Null
# Copy assets before the entry point so a running service never serves an index
# whose referenced chunks have not yet been deployed. Keep old hashed chunks.
Get-ChildItem -LiteralPath $dist | Where-Object { $_.Name -ne 'index.html' } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
}
Copy-Item -LiteralPath $index -Destination (Join-Path $destination "index.html") -Force
Write-Output "Deployed admin frontend to $destination"
Write-Output "Open http://<host>:<port>/admin. Restart the service after the first deployment."
