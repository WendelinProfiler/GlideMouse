param([Parameter(Mandatory)][string]$Address,[Parameter(Mandatory)][string]$OwnerSid)
$ErrorActionPreference='Stop'
if($Address -notmatch '^[A-Fa-f0-9]{12}$' -or $OwnerSid -notmatch '^S-1-5-21-[0-9-]+$'){throw 'Invalid installation identity'}
if(!(Get-Service WPodsService -ErrorAction SilentlyContinue) -or !(Get-Service IslandMagicUsbBridge -ErrorAction SilentlyContinue)){throw 'Install Podglow 0.1.8 alpha and GlideMouse 0.2.0 alpha first'}
$profile=[Environment]::ExpandEnvironmentVariables((Get-ItemPropertyValue -LiteralPath ('HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\'+$OwnerSid) -Name ProfileImagePath))
$ini=Join-Path $profile 'AppData/Local/IslandMagicMouse/settings.ini'
$configured=@(Get-Content -LiteralPath $ini | Where-Object {$_ -match '^Address='})
if($configured.Count -ne 1 -or $configured[0].Substring(8) -ine $Address){throw 'Selected mouse differs from the existing GlideMouse installation'}
$service=Get-CimInstance Win32_Service -Filter "Name='IslandMagicUsbBridge'"
if($service.PathName -notmatch [regex]::Escape('--target='+$Address) -or $service.PathName -notmatch [regex]::Escape('--sid='+$OwnerSid)){throw 'The existing broker belongs to another mouse or user'}
$key='HKLM:\SOFTWARE\GlideMouse\Handoff'
$previous=Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
$target=[Convert]::ToInt64($Address,16)
if($previous.DirectHidTarget -and $previous.DirectHidTarget -ne $target){throw 'Another direct mouse configuration already exists'}
$state=Join-Path $env:ProgramData 'GlideMouse/Switch-addon-before.json'
if(!(Test-Path -LiteralPath $state)) {
 New-Item -ItemType Directory -Path (Split-Path $state) -Force | Out-Null
 @{DirectHidTarget=$previous.DirectHidTarget;RenewalTarget=$previous.RenewalTarget;DirectOwnerSid=$previous.DirectOwnerSid} | ConvertTo-Json | Set-Content -LiteralPath $state
}
New-Item -Path $key -Force | Out-Null
foreach($name in @('DirectHidTarget','RenewalTarget')){New-ItemProperty -LiteralPath $key -Name $name -Value $target -PropertyType QWord -Force | Out-Null}
New-ItemProperty -LiteralPath $key -Name DirectOwnerSid -Value $OwnerSid -PropertyType String -Force | Out-Null
New-ItemProperty -LiteralPath $key -Name ($Address.ToUpperInvariant()+'.'+$OwnerSid) -Value 0 -PropertyType DWord -Force | Out-Null
# Only the selected user's agent on the local subnet can receive Switch TLS.
$program=Join-Path $profile 'AppData/Local/GlideMouseSwitch/app/GlideMouseSwitch.exe'
$rule='GlideMouse Switch optional add-on'
if(!(Get-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue)) {
 New-NetFirewallRule -DisplayName $rule -Direction Inbound -Program $program -Protocol TCP -LocalPort 58426 -RemoteAddress LocalSubnet -Profile Private -Action Allow | Out-Null
}
