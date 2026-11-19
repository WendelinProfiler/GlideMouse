$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
foreach($file in Get-ChildItem (Join-Path $root 'installer') -Filter *.ps1) {
    $tokens=$null;$parseErrors=$null
    [Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$parseErrors) | Out-Null
    if($parseErrors.Count){throw ($parseErrors | Out-String)}
}
. (Join-Path $root 'installer\Discover.ps1')
# Synthetic topology only: no real hardware or registry writes in this test.
$script:mouse='BTHENUM\{00001124-0000-1000-8000-00805F9B34FB}_VID&0001004C_PID&0323\7&TEST&0&102030405060_C00000000'
$script:adapter='USB\VID_1234&PID_5678\EXAMPLE'
$script:hub='USB\ROOT_HUB30\EXAMPLE'
function Get-PnpDevice {param([switch]$PresentOnly,$ErrorAction) [pscustomobject]@{InstanceId=$script:mouse}}
function Get-Property($Id,$Key) {
    if($Key -eq 'DEVPKEY_Device_Address'){return 9}
    if($Id -eq $script:mouse){return $script:adapter}
    if($Id -eq $script:adapter){return $script:hub}
    throw 'Unexpected topology'
}
function Get-ItemProperty {param($LiteralPath,$Name,$ErrorAction) [pscustomobject]@{LowerFilters=@('applewirelessmouse')}}
$found=@(Get-GlideMouseDevices)
if($found.Count -ne 1 -or $found[0].Address -ne '102030405060' -or $found[0].Port -ne 9 -or $found[0].Vid -ne '1234' -or $found[0].Pid -ne '5678' -or !$found[0].AppleBound){throw 'Discovery selected the wrong device'}
$script:hub='USB\VID_7777&PID_8888\EXTERNAL_HUB'
if(@(Get-GlideMouseDevices).Count -ne 0){throw 'Unsupported hub must fail closed'}
'PASS installer parsing, parameterized mouse/adapter discovery and unsupported-topology rejection; no machine changes'
