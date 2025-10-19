$port = New-Object System.IO.Ports.SerialPort
$port.PortName = "COM8"
$port.BaudRate = 115200
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.ReadTimeout = 500

try {
    $port.Open()
    
    # Send a character to trigger the diagnostic to start
    $port.WriteLine("")
    
    Write-Host "=== TOUCH DIAGNOSTIC MONITOR ===" -ForegroundColor Green
    Write-Host "Instructions:" -ForegroundColor Yellow
    Write-Host "1. DO NOT touch screen for first 5 seconds" -ForegroundColor Cyan
    Write-Host "2. Then PRESS AND HOLD for 5 seconds" -ForegroundColor Cyan
    Write-Host "3. Then TAP multiple times" -ForegroundColor Cyan
    Write-Host "================================`n" -ForegroundColor Green
    
    $startTime = Get-Date
    while ($true) {
        try {
            $line = $port.ReadLine()
            $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
            
            # Color code different message types
            if ($line -match "IRQ CHANGED") {
                Write-Host "[$timestamp] $line" -ForegroundColor Yellow
            } elseif ($line -match "VALUE CHANGE") {
                Write-Host "[$timestamp] $line" -ForegroundColor Cyan
            } elseif ($line -match "STATE") {
                Write-Host "[$timestamp] $line" -ForegroundColor Green
            } elseif ($line -match "STATUS") {
                Write-Host "[$timestamp] $line" -ForegroundColor Gray
            } else {
                Write-Host "$line" -ForegroundColor White
            }
        } catch [System.TimeoutException] {
            # No data
        }
        
        if ((Get-Date) - $startTime -gt [TimeSpan]::FromSeconds(40)) {
            break
        }
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
        Write-Host "`nPort closed" -ForegroundColor Green
    }
}