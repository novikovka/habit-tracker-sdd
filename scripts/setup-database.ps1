param(
    [string]$PostgresBin = 'C:\Program Files\PostgreSQL\18\bin',
    [ValidatePattern('^[A-Za-z_][A-Za-z0-9_]*$')][string]$AdminUser = 'postgres',
    [ValidateRange(1,65535)][int]$Port = 5432
)
$ErrorActionPreference = 'Stop'
$psql = Join-Path $PostgresBin 'psql.exe'
if (-not (Test-Path -LiteralPath $psql)) { throw 'psql.exe не найден.' }
$previousPgPassword = $env:PGPASSWORD
$appPassword = $null

function Read-LocalPassword([string]$Prompt) {
    $secure = Read-Host $Prompt -AsSecureString
    $pointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try { return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($pointer) }
    finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($pointer); $secure.Dispose() }
}

function Invoke-LocalSql([string]$Sql, [string]$Database = 'postgres') {
    # SQL goes through stdin, never process arguments. Raw errors can contain SQL,
    # so they are consumed locally and replaced by a message without credentials.
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $psql
    $start.Arguments = "-X -q -w -h localhost -p $Port -U $AdminUser -d $Database -At -v ON_ERROR_STOP=1 -f -"
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables['PGCLIENTENCODING'] = 'UTF8'
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    try {
        [void]$process.Start()
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errTask = $process.StandardError.ReadToEndAsync()
        $sqlBytes = [Text.Encoding]::UTF8.GetBytes($Sql + "`n")
        $process.StandardInput.BaseStream.Write($sqlBytes, 0, $sqlBytes.Length)
        $process.StandardInput.BaseStream.Flush()
        $process.StandardInput.Close()
        $process.WaitForExit()
        $output = $outTask.GetAwaiter().GetResult()
        [void]$errTask.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) {
            throw 'PostgreSQL отклонил операцию. Проверьте пароль администратора, роль и права. Секреты не выводятся.'
        }
        return $output.Trim()
    } finally { $process.Dispose() }
}

try {
    $env:PGPASSWORD = Read-LocalPassword 'Текущий пароль администратора PostgreSQL'
    [void](Invoke-LocalSql 'SELECT current_user;')
    $roleExists = Invoke-LocalSql "SELECT EXISTS(SELECT 1 FROM pg_roles WHERE rolname='habit_tracker_sdd_app');"
    if ($roleExists -eq 't') {
        $privileged = Invoke-LocalSql "SELECT rolsuper OR rolcreatedb OR rolcreaterole OR rolreplication FROM pg_roles WHERE rolname='habit_tracker_sdd_app';"
        if ($privileged -ne 'f') { throw 'Существующая роль имеет лишние права. Автоматическая настройка остановлена.' }
        Write-Host 'Роль habit_tracker_sdd_app уже существует; её пароль не изменяется.'
    } else {
        $appPassword = Read-LocalPassword 'Новый пароль отдельной роли habit_tracker_sdd_app'
        if ([string]::IsNullOrEmpty($appPassword)) { throw 'Пароль новой роли не должен быть пустым.' }
        $escapedPassword = $appPassword.Replace("'", "''")
        [void](Invoke-LocalSql "SET standard_conforming_strings=on; CREATE ROLE habit_tracker_sdd_app LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOREPLICATION PASSWORD '$escapedPassword';")
        $escapedPassword = $null
        $appPassword = $null
        Write-Host 'Создана отдельная роль habit_tracker_sdd_app.'
    }
    foreach ($databaseName in @('habit_tracker_sdd','habit_tracker_sdd_test')) {
        $owner = Invoke-LocalSql "SELECT pg_get_userbyid(datdba) FROM pg_database WHERE datname='$databaseName';"
        if ($owner) {
            if ($owner -ne 'habit_tracker_sdd_app') { throw "База $databaseName принадлежит другой роли; ничего в ней не меняется." }
            Write-Host "База $databaseName уже существует; данные сохраняются."
        } else {
            [void](Invoke-LocalSql "CREATE DATABASE $databaseName OWNER habit_tracker_sdd_app ENCODING 'UTF8' TEMPLATE template0;")
            [void](Invoke-LocalSql "REVOKE ALL ON DATABASE $databaseName FROM PUBLIC; GRANT CONNECT ON DATABASE $databaseName TO habit_tracker_sdd_app;")
            Write-Host "Создана база $databaseName."
        }
    }
    $configFolder = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'HabitTrackerSDD'
    [void](New-Item -ItemType Directory -Path $configFolder -Force)
    $configPath = Join-Path $configFolder 'connection.ini'
    if (-not (Test-Path -LiteralPath $configPath)) {
        $configText = "[database]`r`nhost=localhost`r`nport=$Port`r`nname=habit_tracker_sdd`r`nuser=habit_tracker_sdd_app`r`n"
        [IO.File]::WriteAllText($configPath,$configText,(New-Object Text.UTF8Encoding($false)))
    }
    Write-Host 'Готово. Пароли не записаны в файлы. Можно запускать scripts/run.ps1.'
} finally {
    $env:PGPASSWORD = $previousPgPassword
    $appPassword = $null
    $escapedPassword = $null
}
