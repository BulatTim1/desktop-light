param(
    [Parameter(Mandatory)]
    [ValidateSet('Start', 'Stop', 'EnableStartup', 'DisableStartup', 'Status', 'Uninstall')]
    [string]$Action,
    [string]$WindhawkPath = (Join-Path $PSScriptRoot '..\.cache\windhawk')
)
$ErrorActionPreference = 'Stop'
$WindhawkPath = (Resolve-Path -LiteralPath $WindhawkPath).Path
$executable = Join-Path $WindhawkPath 'windhawk.exe'
$startupPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$startupName = 'CursorBrightness'
$startupCommand = '"' + $executable + '" -tray-only'
$taskName = 'CursorBrightness'
$taskPath = '\'
function Get-LocalStartup {
    $values = Get-ItemProperty -LiteralPath $startupPath -ErrorAction SilentlyContinue
    if ($values) {
        $property = $values.PSObject.Properties[$startupName]
        if ($property) { return $property.Value }
    }
    return $null
}
function Assert-RegistryStartupOwnership {
    $existing = Get-LocalStartup
    if ($existing -and $existing -ne $startupCommand) { throw 'Startup entry belongs to another installation; it was not changed.' }
}
function Remove-RegistryStartup {
    Assert-RegistryStartupOwnership
    if (Get-LocalStartup) { Remove-ItemProperty -LiteralPath $startupPath -Name $startupName }
}
function Get-LocalTask {
    # Listing by exact name avoids a terminating missing-task error on first use.
    $task = Get-ScheduledTask -TaskPath $taskPath -ErrorAction Stop | Where-Object { $_.TaskName -eq $taskName }
    if ($task) {
        $actions = @($task.Actions)
        if ($actions.Count -ne 1 -or $actions[0].Execute -ne $executable -or $actions[0].Arguments -ne '-tray-only') {
            throw 'The CursorBrightness task belongs to another installation; it was not changed.'
        }
    }
    return $task
}
function Disable-LocalStartup {
    Assert-RegistryStartupOwnership
    $task = Get-LocalTask
    if ($task) { Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false }
    Remove-RegistryStartup
}
switch ($Action) {
    'Start' {
        $task = Get-LocalTask
        if ($task) { Start-ScheduledTask -TaskName $taskName -TaskPath $taskPath }
        else { Start-Process -FilePath $executable -ArgumentList '-tray-only' -WorkingDirectory $WindhawkPath -WindowStyle Hidden }
    }
    'Stop' {
        # Bound the wait so a damaged shell cannot hang the control command.
        & $executable -exit -wait -timeout 30000
        if ($LASTEXITCODE) { throw "Windhawk exit failed: $LASTEXITCODE" }
    }
    'EnableStartup' {
        Assert-RegistryStartupOwnership
        $null = Get-LocalTask
        $userSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
        $actionDefinition = New-ScheduledTaskAction -Execute $executable -Argument '-tray-only' -WorkingDirectory $WindhawkPath
        $trigger = New-ScheduledTaskTrigger -AtLogOn -User $userSid
        $trigger.Delay = 'PT15S'
        $principal = New-ScheduledTaskPrincipal -UserId $userSid -LogonType Interactive -RunLevel Limited
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)
        $definition = New-ScheduledTask -Action $actionDefinition -Trigger $trigger -Principal $principal -Settings $settings -Description 'Start the local cursor-targeted brightness extension 15 seconds after this user logs in.'
        Register-ScheduledTask -TaskName $taskName -TaskPath $taskPath -InputObject $definition -Force | Out-Null
        # Keep the old entry until its replacement was registered successfully.
        Remove-RegistryStartup
        Write-Output 'Cursor brightness will start 15 seconds after login (Task Scheduler).'
    }
    'DisableStartup' { Disable-LocalStartup; Write-Output 'Login startup disabled.' }
    'Status' {
        $task = Get-LocalTask
        $info = if ($task) { Get-ScheduledTaskInfo -TaskName $taskName -TaskPath $taskPath }
        $processes = @(Get-Process windhawk -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable })
        [pscustomobject]@{
            StartupMechanism = if ($task) { 'ScheduledTask' } elseif (Get-LocalStartup) { 'LegacyRegistryRun' } else { 'Disabled' }
            TaskState = if ($task) { [string]$task.State } else { $null }
            LastStartTime = if ($info) { $info.LastRunTime } else { $null }
            LastTaskResult = if ($info) { $info.LastTaskResult } else { $null }
            LastTaskResultMeaning = if ($info -and $info.LastTaskResult -eq 0x41301) { 'Task is running (normal)' } elseif ($info -and $info.LastTaskResult -eq 0) { 'Completed successfully' } else { $null }
            Running = $processes.Count -gt 0
            ProcessIds = @($processes.Id)
            Executable = $executable
        }
    }
    'Uninstall' {
        Disable-LocalStartup
        & $executable -exit -wait -timeout 30000
        if ($LASTEXITCODE) { throw "Windhawk exit failed: $LASTEXITCODE" }
        $modConfig = Join-Path $WindhawkPath 'AppData\Engine\Mods\cursor-brightness.ini'
        if (Test-Path -LiteralPath $modConfig) {
            $config = (Get-Content -LiteralPath $modConfig -Raw) -replace '(?m)^Disabled=0\s*$', 'Disabled=1'
            Set-Content -LiteralPath $modConfig -Value $config -Encoding utf8
        }
        Write-Output 'Extension disabled and login startup removed. Project sources and portable files were retained.'
    }
}
