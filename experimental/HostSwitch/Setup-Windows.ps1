$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$folder=Join-Path $env:LOCALAPPDATA 'GlideMouseSwitch'
$app=Join-Path $folder 'app'
$ini=Join-Path $env:LOCALAPPDATA 'IslandMagicMouse/settings.ini'
$sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
if(!(Test-Path -LiteralPath $ini)){throw 'Install GlideMouse first'}
$address=(@(Get-Content -LiteralPath $ini | Where-Object {$_ -match '^Address='}) | Select-Object -First 1).Substring(8)
if($address -notmatch '^[0-9A-Fa-f]{12}$'){throw 'GlideMouse has no selected mouse'}
if(!(Test-Path -LiteralPath (Join-Path $root 'app/GlideMouseSwitch.exe'))){throw 'Extract the entire Windows Switch add-on ZIP first'}
$old=Get-CimInstance Win32_Process -Filter "Name='GlideMouseSwitch.exe'" | Where-Object {$_.ExecutablePath -eq (Join-Path $app 'GlideMouseSwitch.exe') -and $_.CommandLine -match '\sserve$'}
foreach($process in $old){Stop-Process -Id $process.ProcessId -Force}
New-Item -ItemType Directory -Path $app -Force | Out-Null
if(Test-Path -LiteralPath (Join-Path $app 'GlideMouseSwitch.dll')) {
 $backup=Join-Path $folder ('app-before-update-'+(Get-Date -Format yyyyMMdd-HHmmss));Copy-Item -LiteralPath $app -Destination $backup -Recurse
}
Copy-Item -Path (Join-Path $root 'app/*') -Destination $app -Recurse -Force
$agent=Join-Path $app 'GlideMouseSwitch.exe'
if(!(Test-Path -LiteralPath (Join-Path $folder 'settings.json'))) {
 & $agent init $address (Join-Path $app 'GlideMouseSwitchBackend.exe')
 if($LASTEXITCODE){throw 'Could not initialize the local Switch identity'}
}
$identity=& $agent identity | ConvertFrom-Json
if($LASTEXITCODE -or $identity.address -ine $address){throw 'Existing Switch identity belongs to another mouse; preserved'}
Write-Host ('Mouse address: '+$address)
Write-Host ('Windows public fingerprint: '+$identity.pin)
Write-Host 'Enter those values in the Mac app. Private keys remain on their own computers.'
Write-Host 'Your active local IPv4 addresses:'
Get-NetIPAddress -AddressFamily IPv4 | Where-Object {$_.IPAddress -notlike '127.*' -and $_.AddressState -eq 'Preferred'} | ForEach-Object {Write-Host $_.IPAddress}
$config=Get-Content (Join-Path $folder 'settings.json') -Raw | ConvertFrom-Json
if(!$config.Peer -or !$config.PeerPin) {
 $peer=Read-Host 'Mac IP address or hostname'
 $pin=Read-Host 'Mac public fingerprint (copied by the Mac app)'
 & $agent trust $peer $pin
 if($LASTEXITCODE){throw 'Peer trust was not saved'}
}
Copy-Item -LiteralPath (Join-Path $root 'Enable-WindowsAddon.ps1') -Destination $folder -Force
$enable=Join-Path $folder 'Enable-WindowsAddon.ps1'
$elevated=Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList ('-NoProfile -ExecutionPolicy Bypass -File "'+$enable+'" -Address '+$address+' -OwnerSid '+$sid)
if($elevated.ExitCode){throw 'Machine-scoped configuration was not completed'}
& (Join-Path $app 'GlideMouseSwitchBackend.exe') --prepare
if($LASTEXITCODE){throw 'The installed GlideMouse broker did not enable the direct path. Check that GlideMouse 0.2.0 and Podglow 0.1.8 are installed.'}
Add-Type @'
using System.Runtime.InteropServices;
public static class SwitchProfile {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p);
}
'@
foreach($name in @('DirectPointer','HostSwitchEnabled')){if(![SwitchProfile]::WritePrivateProfileString('Mouse',$name,'1',$ini)){throw 'Could not enable the selected mouse'}}
$start='Start-Process -FilePath '''+$agent.Replace("'","''")+''' -ArgumentList serve -WindowStyle Hidden'
$run='powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -Command "'+$start+'"'
New-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name GlideMouseSwitch -Value $run -PropertyType String -Force | Out-Null
Start-Process -FilePath $agent -ArgumentList serve -WindowStyle Hidden
Write-Host 'Ready. Ctrl + Alt + M takes the mouse here. Control + Option + M does the same on the Mac. No SSH is used.'
