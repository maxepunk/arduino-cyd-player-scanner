$port = New-Object System.IO.Ports.SerialPort
$port.PortName = "COM8"
$port.BaudRate = 115200
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.ReadTimeout = 500

try {
    $port.Open()
    Write-Host "Monitoring touch test on COM8 - Touch the screen now!" -ForegroundColor Green
    Write-Host "Press Ctrl+C to stop`n" -ForegroundColor Yellow
    
    $startTime = Get-Date
    while ($true) {
        try {
            $line = $port.ReadLine()
            $timestamp = (Get-Date).ToString("HH:mm:ss.fff")
            Write-Host "[$timestamp] $line" -ForegroundColor Cyan
        } catch [System.TimeoutException] {
            # No data, just continue
        }
        
        # Exit after 30 seconds
        if ((Get-Date) - $startTime -gt [TimeSpan]::FromSeconds(30)) {
            Write-Host "`nTimeout reached" -ForegroundColor Yellow
            break
        }
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
        Write-Host "Port closed" -ForegroundColor Green
    }
}