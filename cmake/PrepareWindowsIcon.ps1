param(
    [Parameter(Mandatory = $true)]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [string]$Destination
)

$bytes = [Convert]::FromBase64String((Get-Content -Raw -LiteralPath $Source).Trim())
if ($bytes.Length -lt 6) {
    throw 'ICO data is too short.'
}

$count = [BitConverter]::ToUInt16($bytes, 4)
$pngSignature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
$entries = @()

for ($index = 0; $index -lt $count; $index++) {
    $entryOffset = 6 + (16 * $index)
    if (($entryOffset + 16) -gt $bytes.Length) {
        throw 'ICO directory is truncated.'
    }

    $size = [int][BitConverter]::ToUInt32($bytes, $entryOffset + 8)
    $offset = [int][BitConverter]::ToUInt32($bytes, $entryOffset + 12)
    if (($offset + $size) -gt $bytes.Length -or $size -lt 8) {
        continue
    }

    $isPng = $true
    for ($signatureIndex = 0; $signatureIndex -lt $pngSignature.Length; $signatureIndex++) {
        if ($bytes[$offset + $signatureIndex] -ne $pngSignature[$signatureIndex]) {
            $isPng = $false
            break
        }
    }

    if ($isPng) {
        $entries += [pscustomobject]@{
            Directory = [byte[]]$bytes[$entryOffset..($entryOffset + 15)]
            Data = [byte[]]$bytes[$offset..($offset + $size - 1)]
        }
    }
}

if ($entries.Count -eq 0) {
    throw 'ICO contains no PNG-backed images.'
}

$stream = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$entries.Count)

$nextDataOffset = 6 + (16 * $entries.Count)
foreach ($entry in $entries) {
    $writer.Write([byte[]]$entry.Directory, 0, 12)
    $writer.Write([UInt32]$nextDataOffset)
    $nextDataOffset += $entry.Data.Length
}

foreach ($entry in $entries) {
    $writer.Write([byte[]]$entry.Data)
}

$writer.Flush()
[IO.File]::WriteAllBytes($Destination, $stream.ToArray())
$writer.Dispose()
$stream.Dispose()
