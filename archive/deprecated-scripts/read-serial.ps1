$port = New-Object System.IO.Ports.SerialPort
$port.PortName = "COM8"
$port.BaudRate = 115200
$port.Parity = [System.IO.Ports.Parity]::None
$port.DataBits = 8
$port.StopBits = [System.IO.Ports.StopBits]::One
$port.ReadTimeout = 1000

try {
    $port.Open()
    Write-Host "Serial port opened on COM8 at 115200 baud"
    
    $count = 0
    while ($count -lt 10) {
        try {
            $line = $port.ReadLine()
            Write-Host "Received: $line"
        } catch [System.TimeoutException] {
            Write-Host "." -NoNewline
        }
        $count++
        Start-Sleep -Milliseconds 500
    }
} finally {
    if ($port.IsOpen) {
        $port.Close()
        Write-Host "`nPort closed"
    }
}