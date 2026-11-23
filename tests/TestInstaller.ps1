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
$script:physical=$script:adapter
$script:virtual='USB\VID_1234&PID_5678\WPODS0123456789ABCDEF'
function Get-Property($Id,$Key) {
    if($Key -eq 'DEVPKEY_Device_Address'){return 9}
    if($Key -eq 'DEVPKEY_Device_Service' -and $Id -eq $script:physical){return 'WINUSB'}
    if($Id -eq $script:mouse){return $script:virtual}
    if($Id -eq $script:physical){return $script:hub}
    if($Id -eq $script:virtual){return 'USB\ROOT_HUB30\EMULATED'}
    throw 'Unexpected topology'
}
function Get-ItemProperty {param($LiteralPath,$Name,$ErrorAction)
    if($Name -eq 'RadioInstanceId'){return [pscustomobject]@{RadioInstanceId=$script:physical}}
    return [pscustomobject]@{LowerFilters=@('applewirelessmouse')}
}
$found=@(Get-GlideMouseDevices)
if($found.Count -ne 1 -or $found[0].Adapter -ne $script:physical -or $found[0].Hub -ne $script:hub -or $found[0].VirtualAdapter -ne $script:virtual){throw 'Podglow discovery selected its virtual hub instead of the physical adapter'}
$script:physical='USB\VID_9999&PID_5678\UNRELATED'
$rejected=$false
try{Get-GlideMouseDevices | Out-Null}catch{$rejected=$true}
if(!$rejected){throw 'Mismatched Podglow adapter must fail closed'}
$script:physical=$script:adapter
$script:hub='USB\VID_7777&PID_8888\EXTERNAL_HUB'
if(@(Get-GlideMouseDevices).Count -ne 0){throw 'Unsupported hub must fail closed'}
$script:ghost=$script:mouse.Replace('\7&TEST&0&','\7&NATIVE&0&')
$script:other=$script:mouse.Replace('\7&TEST&0&','\7&OTHER&0&')
function Get-PnpDevice {param([switch]$PresentOnly,$ErrorAction)
    @($script:mouse,$script:ghost,$script:other) | ForEach-Object {[pscustomobject]@{InstanceId=$_}}
}
function Get-Property($Id,$Key) {
    if($Key -eq 'DEVPKEY_Device_IsPresent'){return $Id -eq $script:mouse}
    if($Id -eq $script:ghost){return $script:physical}
    if($Id -eq $script:other){return 'USB\VID_9999&PID_8888\UNRELATED'}
    throw 'Unexpected topology'
}
$twins=@(Get-NativeMouseTwins ([pscustomobject]@{VirtualAdapter=$script:virtual;Adapter=$script:physical;Address='102030405060';Instance=$script:mouse}))
if($twins.Count -ne 1 -or $twins[0] -ne $script:ghost){throw 'Native-twin selection included a present or unrelated mouse'}
'PASS installer parsing, native/Podglow physical-adapter discovery, mismatched-radio and unsupported-topology rejection; no machine changes'
