param([string]$OutputDirectory=(Join-Path $PSScriptRoot '../../build/addon-runtime-windows'))
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$vs){throw 'MSVC and the Windows SDK are required'}
$vars=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output=[IO.Path]::GetFullPath($OutputDirectory)
$commands=@('@echo off',('call "'+$vars+'" >nul'),('cl /nologo /std:c++latest /utf-8 /EHsc /O2 /MT /DUNICODE /D_UNICODE /experimental:deterministic /pathmap:"'+$repo+'=/_/source" "'+(Join-Path $PSScriptRoot 'WindowsBackend.cpp')+'" /Fo:"'+(Join-Path $output 'WindowsBackend.obj')+'" /Fe:"'+(Join-Path $output 'GlideMouseSwitchBackend.exe')+'" /link windowsapp.lib bthprops.lib advapi32.lib'),'exit /b %errorlevel%')
$script=Join-Path $output 'build-backend.cmd'
[IO.File]::WriteAllText($script,($commands -join "`r`n"),[Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c $script
if($LASTEXITCODE){throw 'Backend compilation failed'}
