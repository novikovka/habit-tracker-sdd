param(
    [string]$QtRoot = 'D:\Qt',
    [string]$PostgresBin = 'C:\Program Files\PostgreSQL\18\bin',
    [switch]$NoPasswordPrompt
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $projectRoot 'build\qt\habit_tracker.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Сначала соберите приложение по README.' }
$previousPath = $env:PATH
$previousPassword = $env:HABIT_TRACKER_DB_PASSWORD
try {
    $env:PATH = "$QtRoot\6.9.1\mingw_64\bin;$QtRoot\Tools\mingw1310_64\bin;$PostgresBin;$previousPath"
    if (-not $env:HABIT_TRACKER_DB_PASSWORD -and -not $NoPasswordPrompt) {
        $securePassword = Read-Host 'Пароль роли приложения PostgreSQL' -AsSecureString
        $passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
        try { $env:HABIT_TRACKER_DB_PASSWORD = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPointer) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer); $securePassword.Dispose() }
    }
    & $exe | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Приложение завершилось с ошибкой.' }
} finally {
    $env:PATH = $previousPath
    $env:HABIT_TRACKER_DB_PASSWORD = $previousPassword
}
