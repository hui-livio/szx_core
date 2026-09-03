<#
.SYNOPSIS
格式化或检查仓库中受版本控制的自有 C/C++ 源码。

.DESCRIPTION
默认排除第三方、构建和生成目录。使用 -IncludeTests 后才处理 test 目录；Format 会直接改写文件，
提交前应先以 Check 或 List 审核范围。
#>

[CmdletBinding()]
param(
    [ValidateSet("Format", "Check", "List")]
    [string]$Mode = "Format",

    [string[]]$Roots = @("components", "tools", "tests"),

    [string]$ClangFormat = "",

    [switch]$IncludeTests
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$extensions = @(".c", ".cc", ".cpp", ".cxx", ".c++", ".h", ".hh", ".hpp", ".hxx", ".h++")
$excludedPathSegments = @(
    ".git",
    "build",
    "deps",
    "thirdparty",
    "third_party",
    "vendor",
    "node_modules",
    ".cache",
    "out",
    "bin",
    "obj",
    "ext"
)

function Resolve-ClangFormat {
    param([string]$RequestedPath)

    if ($RequestedPath) {
        if (-not (Test-Path -LiteralPath $RequestedPath -PathType Leaf)) {
            throw "clang-format does not exist: $RequestedPath"
        }
        return (Resolve-Path -LiteralPath $RequestedPath).Path
    }

    $fromPath = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $candidates = @(
        "C:\Program Files\LLVM\bin\clang-format.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe",
        "C:\Qt\Tools\QtCreator\bin\clang\bin\clang-format.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "clang-format was not found. Install LLVM or pass -ClangFormat <path>."
}

function Test-BusinessSourcePath {
    param([string]$RelativePath)

    # git may list a tracked file that is deleted in the working tree; do not
    # pass such paths to clang-format.
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $RelativePath) -PathType Leaf)) {
        return $false
    }

    $normalized = $RelativePath.Replace("\", "/")
    $segments = $normalized.Split("/", [System.StringSplitOptions]::RemoveEmptyEntries)
    foreach ($segment in $segments) {
        if ($excludedPathSegments -contains $segment) {
            return $false
        }
    }

    if (-not $IncludeTests -and $segments.Count -gt 0 -and $segments[0] -eq "test") {
        return $false
    }

    return $extensions -contains [System.IO.Path]::GetExtension($normalized).ToLowerInvariant()
}

$formatter = Resolve-ClangFormat -RequestedPath $ClangFormat
$selectedRoots = @($Roots | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $repoRoot $_)) })
if ($selectedRoots.Count -eq 0) {
    throw "No requested source roots exist."
}

Push-Location $repoRoot
try {
    $trackedFiles = @(& git ls-files -- $selectedRoots)
    if ($LASTEXITCODE -ne 0) {
        throw "git ls-files failed."
    }

    $sourceFiles = @(
        $trackedFiles |
            Where-Object { Test-BusinessSourcePath $_ } |
            Sort-Object -Unique
    )

    Write-Host "clang-format: $formatter"
    & $formatter --version
    Write-Host "mode: $Mode"
    Write-Host "files: $($sourceFiles.Count)"

    if ($Mode -eq "List") {
        $sourceFiles
        exit 0
    }

    $failures = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $sourceFiles.Count; ++$index) {
        $file = $sourceFiles[$index]
        if ($Mode -eq "Check") {
            & $formatter --dry-run --Werror -style=file -- $file
        }
        else {
            & $formatter -i -style=file -- $file
        }

        if ($LASTEXITCODE -ne 0) {
            $failures.Add($file)
        }

        if ((($index + 1) % 50) -eq 0 -or ($index + 1) -eq $sourceFiles.Count) {
            Write-Host "processed: $($index + 1)/$($sourceFiles.Count)"
        }
    }

    if ($failures.Count -gt 0) {
        Write-Error "clang-format failed for $($failures.Count) file(s):`n$($failures -join "`n")"
        exit 1
    }
}
finally {
    Pop-Location
}
