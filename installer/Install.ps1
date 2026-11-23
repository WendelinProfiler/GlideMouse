param([switch]$Plan,[string]$MouseAddress,[string]$UserSid,[string]$UserHome,[string]$AppleDriver)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Discover.ps1')
if ($Plan) { ConvertTo-Json -InputObject @(Get-GlideMouseDevices) -Depth 4 -Compress; exit 0 }
if (![Environment]::Is64BitProcess) { throw 'Run the 64-bit installer.' }
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Administrator rights are required.' }
if ($MouseAddress -notmatch '^[0-9A-Fa-f]{12}$' -or $UserSid -notmatch '^S-1-5-21-\d+-\d+-\d+-\d+$') { throw 'Invalid installation identity.' }
$knownHome=[string](Get-ItemProperty -LiteralPath ('HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\'+$UserSid) -Name ProfileImagePath).ProfileImagePath
if ([IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($knownHome)) -ne [IO.Path]::GetFullPath($UserHome)) { throw 'User profile does not match the selected SID.' }
$device=@(Get-GlideMouseDevices | Where-Object Address -eq $MouseAddress)
if ($device.Count -ne 1) { throw 'Pair and connect exactly one selected USB-C Magic Mouse first.' };$device=$device[0]
$root=Split-Path $PSScriptRoot
$target=Join-Path $env:ProgramFiles 'IslandMagicMouse'
$stateDir=Join-Path $env:ProgramData 'GlideMouse'
$journal=Join-Path $stateDir 'installation.json'
if ((Test-Path $journal) -or (Get-Service IslandMagicUsbBridge -ErrorAction SilentlyContinue) -or (Test-Path $target)) { throw 'An existing installation was found. It has been left unchanged; uninstall or migrate it manually first.' }
$usbHash='F340A8CF2F127BE2E6B558586B76B51BA5E8C58C462A8D9EECC1955771CB2974'
$appleHash='4B4A64722E3C5D1AC1C26C450D2C5AF74AA92BF01C3D7B6EAEDB81E00FBA5C84'
$usbSource=Join-Path $root 'dependencies\USBPcap.sys'
$usbDest=Join-Path $env:windir 'System32\drivers\USBPcap.sys'
$appleDest=Join-Path $env:windir 'System32\drivers\applewirelessmouse.sys'
function Check-Driver($Path,$Hash) {
    if([string]::IsNullOrWhiteSpace($Path)){throw 'Choose the original Apple driver package in Setup. It is not bundled.'}
    if (!(Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash) { throw "Unrecognized driver: $Path" }
    if ((Get-AuthenticodeSignature -LiteralPath $Path).Status -ne 'Valid') { throw "Invalid driver signature: $Path" }
}
Check-Driver $usbSource $usbHash
$appleExists=Test-Path $appleDest
$appleInfHash='40EBF23B4D6D18ADCC04CDFD4DAB73F32D155BEBBCFBDA4D7BBAA415C4577C71'
$appleCatHash='340EDC96F189E541DD14EA968069E819279969D1B0F8A3B78A7B5733471F74D2'
$applePackageAlreadyInstalled=$false
if($appleExists){Check-Driver $appleDest $appleHash}
if($device.DriverInf -match '^oem\d+\.inf$'){
 $installedInf=Join-Path $env:windir ('INF\'+$device.DriverInf)
 if((Test-Path $installedInf) -and (Get-FileHash $installedInf).Hash -eq $appleInfHash){$applePackageAlreadyInstalled=$true}
}
if(!$applePackageAlreadyInstalled){
 if([string]::IsNullOrWhiteSpace($AppleDriver)){throw 'Choose AppleWirelessMouse.inf from the original Apple driver package. Keep its .cat and .sys files in the same folder.'}
 $AppleDriver=[IO.Path]::ChangeExtension([IO.Path]::GetFullPath($AppleDriver),'.inf')
 $appleFolder=Split-Path $AppleDriver
 if(!(Test-Path $AppleDriver) -or (Get-FileHash $AppleDriver).Hash -ne $appleInfHash){throw 'The original supported AppleWirelessMouse.inf was not found.'}
 $catalog=Join-Path $appleFolder 'AppleWirelessMouse.cat'
 if(!(Test-Path $catalog) -or (Get-FileHash $catalog).Hash -ne $appleCatHash -or (Get-AuthenticodeSignature $catalog).Status -ne 'Valid'){throw 'The original signed Apple catalog is missing or invalid.'}
 Check-Driver (Join-Path $appleFolder 'AppleWirelessMouse.sys') $appleHash
}
$usbExists=Test-Path $usbDest
if ($usbExists) { Check-Driver $usbDest $usbHash }
# Respect pre-existing driver services; reject mismatched paths or custom filters.
foreach ($entry in @(@('USBPcap',$usbDest),@('applewirelessmouse',$appleDest))) {
    $existing=Get-ItemProperty -LiteralPath ('HKLM:\SYSTEM\CurrentControlSet\Services\'+$entry[0]) -ErrorAction SilentlyContinue
    if ($existing) {
        $image=([string]$existing.ImagePath).Replace('\SystemRoot',$env:windir).Replace('\??\','').Trim('"')
        if ($image -ine $entry[1] -or [int]$existing.Type -ne 1) { throw ('Existing driver service differs: '+$entry[0]) }
    }
}
$hubKey='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$device.Hub
$mouseKey='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$device.Instance
function Filters($Key,$Name) { @((Get-ItemProperty -LiteralPath $Key -Name $Name -ErrorAction SilentlyContinue).$Name | Where-Object {$_}) }
$upper=@(Filters $hubKey 'UpperFilters');$lower=@(Filters $mouseKey 'LowerFilters')
if (@($lower | Where-Object {$_ -ne 'applewirelessmouse'}).Count) { throw 'Another mouse filter is present. No changes made.' }
$configDir=Join-Path $UserHome 'AppData\Local\IslandMagicMouse'
$config=Join-Path $configDir 'settings.ini'
if (Test-Path $config) {
    $addressLine=@(Get-Content -LiteralPath $config | Where-Object {$_ -match '^Address='})
    if ($addressLine.Count -ne 1 -or $addressLine[0].Substring(8) -ine $device.Address) { throw 'Settings belong to a different mouse; preserved.' }
}
foreach ($file in 'IslandMagicInput.exe','IslandMagicUsbBridge.exe','GlideMouseConnection.exe','GlideMouseDriverSetup.exe') { if (!(Test-Path (Join-Path $root "bin\$file"))) { throw 'Incomplete release ZIP. Extract the whole archive before running Setup.' } }
$state=[ordered]@{Version=2;ApplePackageInstalled=$false;OriginalDriverInf=$device.DriverInf;UserSid=$UserSid;UserHome=$UserHome;Hub=$device.Hub;Mouse=$device.Instance;UpperBefore=$upper;LowerBefore=$lower;UsbFileCreated=$false;UsbServiceCreated=$false;AppleFileCreated=$false;AppleServiceCreated=$false;HubAdded=$false;MouseAdded=$false;BridgeCreated=$false;StartupCreated=$false;ScrollChanges=@();Stage='Installing'}
New-Item -ItemType Directory -Path $stateDir -Force | Out-Null
function Save-State { $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $journal -Encoding UTF8 }
Save-State
$state.NativeMouseFilterChanges=@()
try {
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    # Program Files inherits admin-only write access; never install an elevated
    # service into a user-writable directory.
    foreach ($file in 'IslandMagicInput.exe','IslandMagicUsbBridge.exe','GlideMouseConnection.exe','GlideMouseDriverSetup.exe') { Copy-Item -LiteralPath (Join-Path $root "bin\$file") -Destination (Join-Path $target $file) }
    Copy-Item -LiteralPath (Join-Path $root "bin\MagicMouse-animation.png") -Destination $target
    foreach($twin in @(Get-NativeMouseTwins $device)) {
        $old=@(Filters ('HKLM:\SYSTEM\CurrentControlSet\Enum\'+$twin) 'LowerFilters')
        if($old -notcontains 'applewirelessmouse'){continue}
        $state.NativeMouseFilterChanges+=@{Instance=$twin;HadAppleFilter=$true}
        Save-State
        & (Join-Path $target 'GlideMouseDriverSetup.exe') $twin --remove-native-filter
        if($LASTEXITCODE){throw 'Could not prepare the hidden native mouse twin for Podglow. Installation stopped.'}
    }
    Copy-Item -LiteralPath $PSScriptRoot -Destination (Join-Path $target 'installer') -Recurse
    if (!$usbExists) { Copy-Item -LiteralPath $usbSource -Destination $usbDest;$state.UsbFileCreated=$true;Save-State }
    if (!(Get-Service USBPcap -ErrorAction SilentlyContinue)) { & sc.exe create USBPcap type= kernel start= demand error= normal binPath= '\SystemRoot\System32\drivers\USBPcap.sys' | Out-Null;if ($LASTEXITCODE) { throw 'USBPcap service creation failed.' };$state.UsbServiceCreated=$true;Save-State }
    if(!$applePackageAlreadyInstalled){
        $state.ApplePackageInstalled=$true;Save-State
        & (Join-Path $target 'GlideMouseDriverSetup.exe') $device.Instance $AppleDriver --install
        $driverResult=$LASTEXITCODE
        if($driverResult -notin @(0,3010)){throw "Original signed Apple package installation failed (code $driverResult)."}
        $state.ApplePackageInstalled=$true;Save-State
    }
    if ($upper -notcontains 'USBPcap') { New-ItemProperty -LiteralPath $hubKey -Name UpperFilters -PropertyType MultiString -Value ([string[]]($upper+'USBPcap')) -Force | Out-Null;$state.HubAdded=$true;Save-State }

    $cmd='"'+(Join-Path $target 'IslandMagicUsbBridge.exe')+'" --sid='+$UserSid+' --target='+$device.Address+' --vid='+$device.Vid+' --pid='+$device.Pid+' --port='+$device.Port+' --hub="'+$device.Hub+'"'
    New-Service -Name IslandMagicUsbBridge -DisplayName 'GlideMouse bridge' -Description 'Passes only validated Magic Mouse touch reports to GlideMouse. No packet files are written.' -BinaryPathName $cmd -StartupType Automatic | Out-Null
    $state.BridgeCreated=$true;Save-State
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    if (!(Test-Path $config)) { @('[Mouse]',('Address='+$device.Address),'UseSignedUsb=1','Natural=1','PreferredNatural=1','TapToClick=0','ShowBattery=0') | Set-Content -LiteralPath $config -Encoding Unicode }
    foreach ($hid in @(Get-PnpDevice -PresentOnly | Where-Object InstanceId -like 'HID\*')) {
        $ancestor=$hid.InstanceId;$belongs=$false
        for ($depth=0;$depth -lt 8;$depth++) {
            if ($ancestor -ieq $device.Instance) {$belongs=$true;break}
            try {$ancestor=[string](Get-Property $ancestor 'DEVPKEY_Device_Parent')}catch{break}
        }
        if (!$belongs) {continue}
        $parameters='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$hid.InstanceId+'\Device Parameters'
        if (!(Test-Path $parameters)) {continue}
        foreach ($name in 'FlipFlopWheel','FlipFlopHScroll') {
            $old=Get-ItemProperty -LiteralPath $parameters -Name $name -ErrorAction SilentlyContinue
            $state.ScrollChanges+=@{Instance=$hid.InstanceId;Name=$name;HadValue=($null -ne $old);Value=$(if($old){[int]$old.$name}else{0})}
            Save-State
            New-ItemProperty -LiteralPath $parameters -Name $name -PropertyType DWord -Value 1 -Force | Out-Null
        }
    }
    $startup=Join-Path $UserHome 'AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\GlideMouse.lnk'
    if (Test-Path $startup) { throw 'Startup shortcut already exists.' }
    New-Item -ItemType Directory -Path (Split-Path $startup) -Force | Out-Null
    $shell=New-Object -ComObject WScript.Shell;$link=$shell.CreateShortcut($startup);$link.TargetPath=Join-Path $target 'IslandMagicInput.exe';$link.Arguments='--run';$link.WorkingDirectory=$target;$link.Description='GlideMouse momentum and gestures';$link.Save();$state.StartupCreated=$true;Save-State
    $uninstall='HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GlideMouse'
    New-Item $uninstall -Force | Out-Null
    New-ItemProperty $uninstall DisplayName -Value 'GlideMouse' -Force | Out-Null
    New-ItemProperty $uninstall DisplayVersion -Value '0.1.1-alpha.1' -Force | Out-Null
    New-ItemProperty $uninstall UninstallString -Value ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $target 'installer\Uninstall.ps1')+'"') -Force | Out-Null
    $state.Stage='RestartRequired';Save-State
    'Installed. Restart Windows when your work is saved. Test mode and Secure Boot were not changed.'
} catch {
    $failure=$_
    & (Join-Path $PSScriptRoot 'Uninstall.ps1') -Rollback
    throw $failure
}
