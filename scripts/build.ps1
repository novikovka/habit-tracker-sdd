param(
    [string]$QtRoot = 'D:\Qt',
    [string]$PostgresBin = 'C:\Program Files\PostgreSQL\18\bin',
    [ValidatePattern('^[D-Z]:$')][string]$BuildDrive = 'H:',
    [switch]$Test,
    [switch]$Integration
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$previousPath = $env:PATH
$previousIntegration = $env:HABIT_TRACKER_RUN_DB_TESTS
$mapped = $false
try {
    # Qt 6.9.1 moc cannot write generated files through this Cyrillic source path.
    # A temporary ASCII drive alias avoids copying sources or changing the workspace.
    if (Test-Path -LiteralPath "$BuildDrive\") { throw "Диск $BuildDrive уже занят. Укажите свободный -BuildDrive." }
    & subst $BuildDrive $projectRoot
    if ($LASTEXITCODE -ne 0) { throw 'Не удалось создать временный путь сборки.' }
    $mapped = $true
    $env:PATH = "$QtRoot\6.9.1\mingw_64\bin;$QtRoot\Tools\mingw1310_64\bin;$PostgresBin;$previousPath"
    $cmake = Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe'
    $ctest = Join-Path $QtRoot 'Tools\CMake_64\bin\ctest.exe'
    $source = "$BuildDrive/"
    $output = "$BuildDrive/build/qt"
    & $cmake -S $source -B $output -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$QtRoot/Tools/Ninja/ninja.exe" `
        "-DCMAKE_CXX_COMPILER=$QtRoot/Tools/mingw1310_64/bin/g++.exe" `
        "-DCMAKE_PREFIX_PATH=$QtRoot/6.9.1/mingw_64" -DCMAKE_BUILD_TYPE=Debug
    if ($LASTEXITCODE -ne 0) { throw 'Ошибка конфигурации CMake.' }
    & $cmake --build $output --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Ошибка сборки.' }
    if ($Test -or $Integration) {
        if ($Integration) { $env:HABIT_TRACKER_RUN_DB_TESTS = '1' }
        else { $env:HABIT_TRACKER_RUN_DB_TESTS = '0' }
        & $ctest --test-dir $output --output-on-failure -V
        $testExitCode = $LASTEXITCODE
        foreach ($testLog in @('database-results.txt','window-results.txt','habit-repository-results.txt','habit-ui-results.txt','calendar-results.txt','completion-results.txt','week-ui-results.txt','statistics-results.txt','statistics-ui-results.txt')) {
            $logPath = Join-Path $output $testLog
            if (Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath -Encoding UTF8 }
        }
        if ($testExitCode -ne 0) { throw 'Проверки завершились ошибкой.' }
    }
} finally {
    $env:PATH = $previousPath
    $env:HABIT_TRACKER_RUN_DB_TESTS = $previousIntegration
    if ($mapped) { & subst $BuildDrive /D }
}
