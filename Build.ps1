param([string]$Compiler,[switch]$Package)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if ($Compiler) {
    # LLVM-MinGW is optional; MSVC + CMake is the default documented toolchain.
    $out=Join-Path $root 'build';New-Item -ItemType Directory $out -Force | Out-Null
    $common=@('-target','x86_64-w64-mingw32','-static','-std=c++23','-O1','-Wall','-Wextra','-Werror')
    $targets=@(
        @{Name='IslandMagicInput';Gui=$true;Libs=@('-luser32','-lshell32','-lgdiplus','-lbthprops','-lcfgmgr32','-ldwmapi','-lshcore','-ladvapi32','-lcfgmgr32')},
        @{Name='IslandMagicUsbBridge';Gui=$false;Libs=@('-ladvapi32','-lws2_32','-lole32','-lbthprops','-lcfgmgr32')},
        @{Name='GlideMouseConnection';Gui=$true;Libs=@('-lbthprops','-lshell32','-ladvapi32')},
        @{Name='GlideMouseDriverSetup';Gui=$false;Libs=@('-lsetupapi')}
    )
    foreach($target in $targets){$extra=@('-municode');if($target.Gui){$extra+='-mwindows'};& $Compiler @common @extra (Join-Path $root ('src\'+$target.Name+'.cpp')) -o (Join-Path $out ($target.Name+'.exe')) @($target.Libs);if($LASTEXITCODE){throw ('Build failed: '+$target.Name)}}
    foreach($test in 'Engine','TapModes','MouseUsbParser','Timing','Popup','WindowsSettings'){
        $libraries=if($test -eq 'Popup'){@('-lgdiplus','-lshell32','-ldwmapi','-lshcore','-ladvapi32','-lcfgmgr32')}elseif($test -eq 'WindowsSettings'){@('-ladvapi32','-lshell32','-lcfgmgr32')}else{@()}
        $exe=Join-Path $out ('Test'+$test+'.exe');& $Compiler @common (Join-Path $root ('tests\Test'+$test+'.cpp')) -o $exe @libraries;if($LASTEXITCODE){throw "Test build failed: $test"};& $exe;if($LASTEXITCODE){throw "Test failed: $test"}
    }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'tests\TestInstaller.ps1');if($LASTEXITCODE){throw 'Installer tests failed'}
} else {
    & cmake -S $root -B (Join-Path $root 'build') -A x64;if($LASTEXITCODE){throw 'CMake configuration failed'}
    & cmake --build (Join-Path $root 'build') --config Release;if($LASTEXITCODE){throw 'Build failed'}
    & ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure;if($LASTEXITCODE){throw 'Tests failed'}
    $out=Join-Path $root 'build\Release'
}
    $csc=Join-Path $env:windir 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    & $csc /nologo /codepage:65001 /target:winexe /platform:x64 /optimize+ /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll ('/win32manifest:'+(Join-Path $root 'installer\setup.manifest')) ('/out:'+(Join-Path $out 'GlideMouseSetup.exe')) (Join-Path $root 'installer\Setup.cs')
    if($LASTEXITCODE){throw 'Wizard build failed'}

if($Package){& (Join-Path $root 'New-Release.ps1') -BinaryDir $out}

