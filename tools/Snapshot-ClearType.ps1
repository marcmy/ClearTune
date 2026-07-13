#requires -Version 7.4
[CmdletBinding()]
param(
    [Parameter()]
    [string] $Path = (Join-Path $PWD ("cleartype-snapshot-{0:yyyyMMdd-HHmmss}.json" -f (Get-Date)))
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not ('ClearTypeSnapshot.NativeMethods' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

namespace ClearTypeSnapshot {
    public static class NativeMethods {
        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SystemParametersInfo(
            uint uiAction,
            uint uiParam,
            ref uint pvParam,
            uint fWinIni);

        private static uint Read(uint action) {
            uint value = 0;
            if (!SystemParametersInfo(action, 0, ref value, 0)) {
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            }
            return value;
        }

        public static uint FontSmoothingEnabled() => Read(0x004A);
        public static uint FontSmoothingType() => Read(0x200A);
        public static uint FontSmoothingContrast() => Read(0x200C);
        public static uint FontSmoothingOrientation() => Read(0x2012);
    }
}
'@
}

$avalonPath = 'HKCU:\Software\Microsoft\Avalon.Graphics'
$valueNames = @(
    'GammaLevel',
    'PixelStructure',
    'ClearTypeLevel',
    'TextContrastLevel',
    'EnhancedContrastLevel',
    'GrayscaleEnhancedContrastLevel'
)

$displays = @()
if (Test-Path -LiteralPath $avalonPath) {
    $displays = Get-ChildItem -LiteralPath $avalonPath -ErrorAction Stop |
        Where-Object { $_.PSChildName -like 'DISPLAY*' } |
        Sort-Object PSChildName |
        ForEach-Object {
            $properties = Get-ItemProperty -LiteralPath $_.PSPath
            $values = [ordered]@{}
            foreach ($name in $valueNames) {
                $property = $properties.PSObject.Properties[$name]
                $values[$name] = if ($null -ne $property) { [uint32] $property.Value } else { $null }
            }
            [ordered]@{
                DisplayKey = $_.PSChildName
                Values     = $values
            }
        }
}

$snapshot = [ordered]@{
    CapturedAt = (Get-Date).ToUniversalTime().ToString('o')
    Computer   = $env:COMPUTERNAME
    User       = [Environment]::UserName
    Global     = [ordered]@{
        FontSmoothingEnabled     = [ClearTypeSnapshot.NativeMethods]::FontSmoothingEnabled()
        FontSmoothingType        = [ClearTypeSnapshot.NativeMethods]::FontSmoothingType()
        FontSmoothingContrast    = [ClearTypeSnapshot.NativeMethods]::FontSmoothingContrast()
        FontSmoothingOrientation = [ClearTypeSnapshot.NativeMethods]::FontSmoothingOrientation()
    }
    Displays   = $displays
}

$resolvedPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
$parent = Split-Path -Parent $resolvedPath
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}

$snapshot | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedPath -Encoding utf8NoBOM
Write-Host "ClearType snapshot written to: $resolvedPath"
