param([switch]$Rollback)
$ErrorActionPreference='Stop'
$admin=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if (!$admin.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $args='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'"'
    if ($Rollback) {$args+=' -Rollback'}
    $p=Start-Process powershell.exe -Verb RunAs -WindowStyle Hidden -ArgumentList $args -Wait -PassThru
    exit $p.ExitCode
}
$journal=Join-Path $env:ProgramData 'GlideMouse\installation.json'
if (!(Test-Path $journal)) { throw 'No GlideMouse installation journal was found.' }
$state=Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
if ($state.Version -ne 2 -or $state.Hub -notmatch '^USB\\ROOT_HUB[^\r\n]+$' -or $state.Mouse -notmatch '^BTHENUM\\\{00001124-[^\r\n]+$' -or $state.UserSid -notmatch '^S-1-5-21-\d+-\d+-\d+-\d+$') { throw 'Invalid installation journal.' }
$home=[Environment]::ExpandEnvironmentVariables([string](Get-ItemProperty -LiteralPath ('HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\'+$state.UserSid)).ProfileImagePath)
if ([IO.Path]::GetFullPath($home) -ne [IO.Path]::GetFullPath($state.UserHome)) { throw 'Profile mismatch in journal.' }
$target=Join-Path $env:ProgramFiles 'IslandMagicMouse'
if ($state.StartupCreated) { Remove-Item -LiteralPath (Join-Path $home 'AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\GlideMouse.lnk') -ErrorAction SilentlyContinue }
if (Test-Path (Join-Path $target 'IslandMagicInput.exe')) { & (Join-Path $target 'IslandMagicInput.exe') --stop; Start-Sleep -Milliseconds 300 }
if ($state.BridgeCreated) { Stop-Service IslandMagicUsbBridge -ErrorAction SilentlyContinue;& sc.exe delete IslandMagicUsbBridge | Out-Null }
function Remove-OwnedFilter($Instance,$Name,$Filter) {
    $key='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$Instance
    $old=@((Get-ItemProperty -LiteralPath $key -Name $Name -ErrorAction SilentlyContinue).$Name | Where-Object {$_})
    $next=@($old | Where-Object {$_ -ne $Filter})
    if ($next.Count) { New-ItemProperty -LiteralPath $key -Name $Name -PropertyType MultiString -Value ([string[]]$next) -Force | Out-Null }
    else { Remove-ItemProperty -LiteralPath $key -Name $Name -ErrorAction SilentlyContinue }
}
if($state.ApplePackageInstalled){
 & (Join-Path $target 'GlideMouseDriverSetup.exe') $state.Mouse --restore-default
 if($LASTEXITCODE -notin @(0,3010)){throw 'Could not restore the Windows mouse driver. Registration and journal retained for recovery.'}
}
if ($state.HubAdded) { Remove-OwnedFilter $state.Hub UpperFilters USBPcap }
if ($state.MouseAdded) { Remove-OwnedFilter $state.Mouse LowerFilters applewirelessmouse }
foreach ($change in $state.ScrollChanges) {
    if ($change.Instance -notmatch '^HID\\[^\r\n]+$' -or $change.Name -notin @('FlipFlopWheel','FlipFlopHScroll')) {throw 'Invalid scroll restore entry.'}
    $key='HKLM:\SYSTEM\CurrentControlSet\Enum\'+$change.Instance+'\Device Parameters'
    if ($change.HadValue) {New-ItemProperty -LiteralPath $key -Name $change.Name -PropertyType DWord -Value ([int]$change.Value) -Force | Out-Null}
    else {Remove-ItemProperty -LiteralPath $key -Name $change.Name -ErrorAction SilentlyContinue}
}
# Mark only our newly created services for deletion. Do not stop or remove a
# loaded USB filter or force a USB/Bluetooth restart. Windows releases it at boot.
if ($state.UsbServiceCreated) { & sc.exe delete USBPcap | Out-Null }
# The original Apple driver package/service remains managed by Windows Driver Store.
Remove-Item -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GlideMouse' -ErrorAction SilentlyContinue
$state.Stage=if($Rollback){'RolledBackRestartRequired'}else{'UninstalledRestartRequired'}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $journal -Encoding UTF8
foreach ($file in 'IslandMagicInput.exe','IslandMagicUsbBridge.exe','GlideMouseConnection.exe','GlideMouseDriverSetup.exe','MagicMouse-animation.png') {
    Remove-Item -LiteralPath (Join-Path $target $file) -ErrorAction SilentlyContinue
}
foreach ($file in 'Discover.ps1','Install.ps1','Uninstall.ps1','Invoke-Install.ps1','Setup.cs') {
    Remove-Item -LiteralPath (Join-Path $target ('installer\'+$file)) -ErrorAction SilentlyContinue
}
if (Test-Path (Join-Path $target 'installer')) {if (!(Get-ChildItem -LiteralPath (Join-Path $target 'installer'))) {Remove-Item -LiteralPath (Join-Path $target 'installer')}}
if (Test-Path $target) {if (!(Get-ChildItem -LiteralPath $target)) {Remove-Item -LiteralPath $target}}
Move-Item -LiteralPath $journal -Destination (Join-Path (Split-Path $journal) 'last-uninstall.json') -Force
# Settings and driver files are deliberately retained. They are inert after the
# registrations are removed; retaining them avoids deleting loaded kernel code.
'GlideMouse disabled. Restart Windows to unload the filter. Settings retained.'
