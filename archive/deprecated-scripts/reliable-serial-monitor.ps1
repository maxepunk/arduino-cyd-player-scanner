# Reliable Serial Monitor for Arduino development
# Handles reconnection and provides consistent output

param(
    [string]$Port = "COM8",
    [int]$BaudRate = 115200,
    [int]$Duration = 60  # seconds
)

function Connect-SerialPort {
    $serial = New-Object System.IO.Ports.SerialPort
    $serial.PortName = $Port
    $serial.BaudRate = $BaudRate
    $serial.Parity = [System.IO.Ports.Parity]::None
    $serial.DataBits = 8
    $serial.StopBits = [System.IO.Ports.StopBits]::One
    $serial.ReadTimeout = 1000
    $serial.WriteTimeout = 1000
    
    # Set buffer sizes
    $serial.ReadBufferSize = 4096
    $serial.WriteBufferSize = 2048
    
    # DTR/RTS handling for ESP32
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    
    return $serial
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RELIABLE SERIAL MONITOR FOR ARDUINO  " -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Port: $Port | Baud: $BaudRate | Duration: ${Duration}s" -ForegroundColor Gray
Write-Host ""

$startTime = Get-Date
$serial = Connect-SerialPort

try {
    # Try to open port
    $serial.Open()
    Write-Host "[CONNECTED] Serial port opened successfully" -ForegroundColor Green
    
    # Clear any pending data
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()
    
    # Send a newline to trigger any waiting prompts
    $serial.WriteLine("")
    
    Write-Host "[MONITORING] Listening for data..." -ForegroundColor Yellow
    Write-Host ""
    
    $noDataCount = 0
    $lastDataTime = Get-Date
    
    while ((Get-Date) - $startTime -lt [TimeSpan]::FromSeconds($Duration)) {
        try {
            if ($serial.BytesToRead -gt 0) {
                $data = $serial.ReadExisting()
                if ($data) {
                    # Reset no-data counter
                    $noDataCount = 0
                    $lastDataTime = Get-Date
                    
                    # Output with timestamp
                    $lines = $data -split "`n"
                    foreach ($line in $lines) {
                        if ($line.Trim()) {
                            $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
                            Write-Host "[$timestamp] $line"
                        }
                    }
                }
            } else {
                # No data available
                $noDataCount++
                Start-Sleep -Milliseconds 50
                
                # Show we're still listening every 2 seconds of no data
                if ($noDataCount % 40 -eq 0) {
                    $elapsed = [math]::Round(((Get-Date) - $lastDataTime).TotalSeconds, 1)
                    Write-Host "[WAITING] No data for ${elapsed}s..." -ForegroundColor DarkGray -NoNewline
                    Write-Host "`r" -NoNewline
                }
            }
        } catch [System.TimeoutException] {
            # Normal timeout, continue
        } catch {
            Write-Host "[ERROR] Read error: $_" -ForegroundColor Red
        }
    }
    
} catch {
    Write-Host "[ERROR] Failed to open serial port: $_" -ForegroundColor Red
    Write-Host "[TIP] Ensure no other program is using $Port" -ForegroundColor Yellow
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
        Write-Host ""
        Write-Host "[CLOSED] Serial port closed" -ForegroundColor Green
    }
    $serial.Dispose()
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Monitoring complete" -ForegroundColor Yellow