param(
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location -LiteralPath $repoRoot

function Convert-PathSegment {
    param([AllowEmptyString()][string]$Segment)

    if ($Segment.Length -eq 0) { return '' }

    $value = $Segment
    $value = [regex]::Replace($value, '^P0*(\d{1,2})(?=[_\-\s]|$)', {
        param($match)
        return 'P{0:D2}' -f [int]$match.Groups[1].Value
    })
    $value = $value -replace '\s+-\s+', '_'
    $value = $value -replace '^(P\d+)-', '$1_'
    $value = $value -replace '\s+', '_'
    $value = [regex]::Replace($value, '^第_?(\d+)_?章_?', {
        param($match)
        return '第{0}章_' -f [int]$match.Groups[1].Value
    })
    $value = $value.Replace('↔', '_to_').Replace('→', '_to_').Replace('←', '_to_')
    $value = $value.Replace('×', '_x_')
    $value = $value.Replace('（', '(').Replace('）', ')')
    $value = $value.Replace('【', '(').Replace('】', ')')
    $value = $value.Replace('《', '_').Replace('》', '_')
    $value = $value -replace '[：，。、“”‘’？！；、·—]', '_'
    $value = $value.Replace('`', '_')
    $value = $value -replace '_+', '_'
    $value = $value -replace '_([.)])', '$1'
    $value = $value.Trim('_')
    return $value
}

function Convert-RepoPath {
    param([Parameter(Mandatory)][string]$Path)

    $parts = $Path.Replace('\', '/') -split '/'
    return (($parts | ForEach-Object { Convert-PathSegment $_ }) -join '/')
}

function Convert-LinkTarget {
    param([Parameter(Mandatory)][string]$Target)

    if ($Target -match '^(?:[a-zA-Z][a-zA-Z0-9+.-]*:|#)') {
        return $Target
    }

    $fragmentIndex = $Target.IndexOf('#')
    if ($fragmentIndex -ge 0) {
        $pathPart = $Target.Substring(0, $fragmentIndex)
        $fragment = $Target.Substring($fragmentIndex)
    } else {
        $pathPart = $Target
        $fragment = ''
    }

    $pathPart = $pathPart.Replace('%20', '_')
    $prefix = ''
    if ($pathPart.StartsWith('./')) {
        $prefix = './'
        $pathPart = $pathPart.Substring(2)
    }
    $normalized = Convert-RepoPath $pathPart
    return "$prefix$normalized$fragment"
}

$tracked = @(git -c core.quotePath=false ls-files)
$mapping = [ordered]@{}
foreach ($oldPath in $tracked) {
    $newPath = Convert-RepoPath $oldPath
    if ($newPath -cne $oldPath) {
        $mapping[$oldPath] = $newPath
    }
}

$targetGroups = $mapping.GetEnumerator() | Group-Object Value | Where-Object Count -gt 1
if ($targetGroups) {
    $details = $targetGroups | ForEach-Object { "$($_.Name) <= $($_.Group.Name -join ', ')" }
    throw "命名规则产生路径冲突：`n$($details -join "`n")"
}

$oldSet = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($oldPath in $mapping.Keys) { $null = $oldSet.Add($oldPath) }
foreach ($newPath in $mapping.Values) {
    if ((Test-Path -LiteralPath $newPath) -and -not $oldSet.Contains($newPath)) {
        throw "目标路径已经存在：$newPath"
    }
}

Write-Output "待重命名文件：$($mapping.Count)"
if (-not $Apply) {
    $mapping.GetEnumerator() | ForEach-Object { "$($_.Key) -> $($_.Value)" }
    exit 0
}

$textExtensions = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
@('.md', '.markdown', '.json', '.mm', '.txt', '.rst', '.c', '.h', '.css', '.js', '.yml', '.yaml') |
    ForEach-Object { $null = $textExtensions.Add($_) }

$updatedText = [ordered]@{}
foreach ($oldPath in $tracked) {
    $extension = [IO.Path]::GetExtension($oldPath)
    if (-not $textExtensions.Contains($extension)) { continue }

    $absoluteOld = Join-Path $repoRoot $oldPath
    $content = [IO.File]::ReadAllText($absoluteOld)
    $originalContent = $content

    if ($extension -in @('.md', '.markdown')) {
        $content = [regex]::Replace($content, '(?<prefix>!?\[[^\]]*\]\()(?<target>[^)]+)(?<suffix>\))', {
            param($match)
            $target = $match.Groups['target'].Value
            $converted = Convert-LinkTarget $target
            return $match.Groups['prefix'].Value + $converted + $match.Groups['suffix'].Value
        })
        $content = [regex]::Replace($content, '(?<prefix>!??\[\[)(?<target>[^\]|]+)(?<suffix>(?:\|[^\]]*)?\]\])', {
            param($match)
            $converted = Convert-LinkTarget $match.Groups['target'].Value
            return $match.Groups['prefix'].Value + $converted + $match.Groups['suffix'].Value
        })
        $content = [regex]::Replace($content, '(?<prefix>\b(?:src|href)=["''])(?<target>[^"'']+)(?<suffix>["''])', {
            param($match)
            $converted = Convert-LinkTarget $match.Groups['target'].Value
            return $match.Groups['prefix'].Value + $converted + $match.Groups['suffix'].Value
        })
    }

    foreach ($entry in $mapping.GetEnumerator()) {
        $content = $content.Replace($entry.Key, $entry.Value)
        $content = $content.Replace($entry.Key.Replace('/', '\'), $entry.Value.Replace('/', '\'))
    }
    if ($content -cne $originalContent) {
        $updatedText[$oldPath] = $content
    }
}

foreach ($entry in $mapping.GetEnumerator() | Sort-Object { ($_.Key -split '/').Count } -Descending) {
    $oldPath = $entry.Key
    $newPath = $entry.Value
    $parent = Split-Path -Parent $newPath
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        $null = New-Item -ItemType Directory -Path $parent -Force
    }
    git mv -- $oldPath $newPath
    if ($LASTEXITCODE -ne 0) { throw "git mv 失败：$oldPath -> $newPath" }
}

foreach ($entry in $updatedText.GetEnumerator()) {
    $newPath = if ($mapping.Contains($entry.Key)) { $mapping[$entry.Key] } else { $entry.Key }
    [IO.File]::WriteAllText((Join-Path $repoRoot $newPath), $entry.Value, [Text.UTF8Encoding]::new($false))
}

Write-Output "已重命名文件：$($mapping.Count)"
