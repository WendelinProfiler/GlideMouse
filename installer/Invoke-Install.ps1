param([string]$MouseAddress,[string]$UserSid,[string]$UserHome,[string]$AppleDriver)
$ErrorActionPreference='Stop'
$result=Join-Path $env:ProgramData 'GlideMouse\setup-result.txt'
try{
    New-Item -ItemType Directory -Path (Split-Path $result) -Force | Out-Null
    & (Join-Path $PSScriptRoot 'Install.ps1') @PSBoundParameters
    'Installed successfully. Save your work and restart Windows.' | Set-Content -LiteralPath $result -Encoding UTF8
    exit 0
}catch{
    $_.Exception.Message | Set-Content -LiteralPath $result -Encoding UTF8
    exit 1
}
