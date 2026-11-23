param([string]$BinaryDir=(Join-Path $PSScriptRoot 'build\Release'),[string]$Version='0.2.0-alpha.1')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot;$cache=Join-Path $root 'build\dependency-cache'
New-Item -ItemType Directory $cache -Force | Out-Null
$archive=Join-Path $cache 'USBPcapSetup-1.5.4.0.exe'
if(!(Test-Path $archive)){Invoke-WebRequest -UseBasicParsing 'https://github.com/desowin/usbpcap/releases/download/1.5.4.0/USBPcapSetup-1.5.4.0.exe' -OutFile $archive}
if((Get-FileHash $archive).Hash -ne '87A7EDF9BBBCF07B5F4373D9A192A6770D2FF3ADD7AA1E276E82E38582CCB622'){throw 'USBPcap upstream installer hash mismatch'}
$sevenZip=(Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
if(!$sevenZip){$sevenZip=Join-Path $env:ProgramFiles '7-Zip\7z.exe'}
if(!(Test-Path $sevenZip)){throw '7-Zip is required to extract the original driver. The upstream installer is not executed.'}
$extract=Join-Path $cache ('extracted-'+[guid]::NewGuid().ToString('N'))
& $sevenZip x $archive ('-o'+$extract) -aou -y | Out-Null;if($LASTEXITCODE){throw 'Dependency extraction failed'}
$driver=@(Get-ChildItem -LiteralPath $extract -Recurse -Filter 'USBPcap*.sys' | Where-Object {(Get-FileHash $_.FullName).Hash -eq 'F340A8CF2F127BE2E6B558586B76B51BA5E8C58C462A8D9EECC1955771CB2974'})
if(!$driver.Count){throw 'The expected signed x64 driver was not found'}
if((Get-AuthenticodeSignature $driver[0].FullName).Status -ne 'Valid'){throw 'Dependency signature verification failed'}
# Supply upstream's exact release source/PDB archive alongside its GPL driver.
$source=Join-Path $cache 'usbpcap-src-incl-pdb-1.5.4.0.7z'
if(!(Test-Path $source)){Invoke-WebRequest -UseBasicParsing 'https://github.com/desowin/usbpcap/releases/download/1.5.4.0/usbpcap-src-incl-pdb-1.5.4.0.7z' -OutFile $source}
$dist=Join-Path $root 'dist';New-Item -ItemType Directory $dist -Force | Out-Null
$package=Join-Path $dist (('GlideMouse-'+$Version+'-')+[guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory $package,(Join-Path $package 'bin'),(Join-Path $package 'installer'),(Join-Path $package 'dependencies'),(Join-Path $package 'corresponding-source') | Out-Null
foreach($name in 'IslandMagicInput.exe','IslandMagicUsbBridge.exe','GlideMouseConnection.exe','GlideMouseDriverSetup.exe'){Copy-Item -LiteralPath (Join-Path $BinaryDir $name) -Destination (Join-Path $package 'bin')}
Copy-Item -LiteralPath (Join-Path $BinaryDir 'GlideMouseSetup.exe') -Destination $package
foreach($name in 'Discover.ps1','Install.ps1','Uninstall.ps1','Invoke-Install.ps1'){Copy-Item -LiteralPath (Join-Path $root ('installer\'+$name)) -Destination (Join-Path $package 'installer')}
Copy-Item -LiteralPath (Join-Path $root "assets\MagicMouse-animation.png") -Destination (Join-Path $package "bin")
Copy-Item -LiteralPath (Join-Path $root "assets\MODEL-PROVENANCE.md") -Destination $package
Copy-Item -LiteralPath $driver[0].FullName -Destination (Join-Path $package 'dependencies\USBPcap.sys')
Copy-Item -LiteralPath $source -Destination (Join-Path $package 'corresponding-source')
foreach($name in 'README.md','RELEASE-NOTES.md','LICENSE','THIRD_PARTY_NOTICES.md'){Copy-Item -LiteralPath (Join-Path $root $name) -Destination $package}
$checksums=foreach($file in Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName){(Get-FileHash $file.FullName).Hash.ToLowerInvariant()+'  '+$file.FullName.Substring($package.Length+1).Replace('\','/')}
$checksums | Set-Content -LiteralPath (Join-Path $package 'SHA256SUMS.txt') -Encoding ASCII
$zip=Join-Path $dist ('GlideMouse-'+$Version+'-win-x64.zip')
Compress-Archive -Path (Join-Path $package '*') -DestinationPath $zip -Force
Write-Output $zip
