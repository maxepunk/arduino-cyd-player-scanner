# CYD Test Results Collector PowerShell Script
# This script automates the collection and organization of test results
# Run with: .\Collect-TestResults.ps1

param(
    [string]$ArduinoPath = "$env:USERPROFILE\Documents\Arduino",
    [string]$ComPort = "COM3",
    [switch]$AutoDetectPort
)

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "                    CYD TEST RESULTS COLLECTOR (PowerShell)" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""

# Create timestamp for unique folder
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultsDir = "CYD_Test_Results_$timestamp"

# Create directory structure
Write-Host "Creating results directory structure..." -ForegroundColor Yellow
$dirs = @(
    $resultsDir,
    "$resultsDir\Detection_Phase",
    "$resultsDir\TDD_Phase",
    "$resultsDir\Implementation_Phase",
    "$resultsDir\Serial_Logs",
    "$resultsDir\Screenshots",
    "$resultsDir\Sketches"
)

foreach ($dir in $dirs) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

# Auto-detect Arduino COM port if requested
if ($AutoDetectPort) {
    Write-Host "Detecting Arduino COM port..." -ForegroundColor Yellow
    $ports = Get-WmiObject Win32_SerialPort | Where-Object { 
        $_.Description -match "USB Serial|Arduino|CH340|CP210|FTDI" 
    }
    
    if ($ports) {
        $ComPort = $ports[0].DeviceID
        Write-Host "Detected Arduino on port: $ComPort" -ForegroundColor Green
    } else {
        Write-Host "No Arduino detected. Using default: $ComPort" -ForegroundColor Red
    }
}

# Collect system information
Write-Host "Collecting system information..." -ForegroundColor Yellow

$sysInfo = @{
    TestDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    ComputerName = $env:COMPUTERNAME
    WindowsVersion = (Get-WmiObject Win32_OperatingSystem).Caption
    WindowsBuild = (Get-WmiObject Win32_OperatingSystem).BuildNumber
    ArduinoPath = $ArduinoPath
    ComPort = $ComPort
    PowerShellVersion = $PSVersionTable.PSVersion.ToString()
}

# Prompt for user input
Write-Host ""
Write-Host "Please provide the following information:" -ForegroundColor Cyan

$userInfo = @{
    CYDModel = Read-Host "CYD Model (Single USB/Dual USB)"
    DisplayType = Read-Host "Display Type (ILI9341/ST7789/Unknown)"
    ArduinoVersion = Read-Host "Arduino IDE Version"
    ESP32Version = Read-Host "ESP32 Core Version"
    TesterName = Read-Host "Your Name/Handle (optional)"
    Notes = Read-Host "Any initial notes (optional)"
}

# Save hardware information
$hardwareInfo = @"
================================================================================
CYD HARDWARE TESTING INFORMATION
================================================================================

TEST INFORMATION:
-----------------
Test Date: $($sysInfo.TestDate)
Computer: $($sysInfo.ComputerName)
Windows: $($sysInfo.WindowsVersion) (Build $($sysInfo.WindowsBuild))
COM Port: $($sysInfo.ComPort)

HARDWARE DETAILS:
-----------------
CYD Model: $($userInfo.CYDModel)
Display Type: $($userInfo.DisplayType)

SOFTWARE VERSIONS:
------------------
Arduino IDE: $($userInfo.ArduinoVersion)
ESP32 Core: $($userInfo.ESP32Version)
PowerShell: $($sysInfo.PowerShellVersion)

TESTER:
-------
Name: $($userInfo.TesterName)
Notes: $($userInfo.Notes)

================================================================================
"@

$hardwareInfo | Out-File -FilePath "$resultsDir\Hardware_Info.txt" -Encoding UTF8

# Create test checklist
$testChecklist = @"
================================================================================
TEST EXECUTION CHECKLIST
================================================================================

DETECTION PHASE:
----------------
[ ] CYD_Model_Detector - Upload, run, save output
[ ] Hardware_Probe - Upload, run, save output
[ ] CYD_Diagnostic_Test - Upload, run, save output
[ ] System_Info_Collector - Upload, run, save COMPLETE output

TDD PHASE (These should FAIL initially):
-----------------------------------------
[ ] Test_Hardware_Detection - Record failure mode
[ ] Test_Component_Init - Record failure mode
[ ] Test_Diagnostics - Record failure mode
[ ] Test_ILI9341_Detection - Record failure mode
[ ] Test_ST7789_Detection - Record failure mode
[ ] Test_GPIO27_Mux - Record failure mode
[ ] Test_Touch_Calibration - Record failure mode
[ ] Test_RFID_Timing - Record failure mode

IMPLEMENTATION PHASE:
---------------------
[ ] CYD_Multi_Compatible - Main sketch test
[ ] Component initialization times recorded
[ ] All features tested (Display, Touch, RFID, SD, Audio)
[ ] Error conditions tested
[ ] Performance metrics collected

NOTES:
------
(Add any observations or issues here)

================================================================================
"@

$testChecklist | Out-File -FilePath "$resultsDir\Test_Checklist.txt" -Encoding UTF8

# Create a JSON manifest for automated processing
$manifest = @{
    Version = "1.0"
    TestDate = $sysInfo.TestDate
    System = $sysInfo
    Hardware = $userInfo
    Tests = @{
        Detection = @{}
        TDD = @{}
        Implementation = @{}
    }
    Status = "In Progress"
}

$manifest | ConvertTo-Json -Depth 10 | Out-File -FilePath "$resultsDir\manifest.json" -Encoding UTF8

# Function to capture serial output (if Arduino CLI is installed)
function Capture-SerialOutput {
    param(
        [string]$SketchName,
        [int]$Duration = 10
    )
    
    Write-Host "Attempting to capture serial output for $SketchName..." -ForegroundColor Yellow
    
    $outputFile = "$resultsDir\Serial_Logs\${SketchName}_Serial.txt"
    
    # Check if Arduino CLI is available
    if (Get-Command arduino-cli -ErrorAction SilentlyContinue) {
        Write-Host "Arduino CLI found. Monitoring $ComPort for $Duration seconds..." -ForegroundColor Green
        
        # Start monitoring
        $process = Start-Process -FilePath "arduino-cli" `
            -ArgumentList "monitor -p $ComPort -b 115200" `
            -RedirectStandardOutput $outputFile `
            -PassThru `
            -WindowStyle Hidden
        
        Start-Sleep -Seconds $Duration
        Stop-Process -Id $process.Id -Force
        
        Write-Host "Output saved to: $outputFile" -ForegroundColor Green
    } else {
        Write-Host "Arduino CLI not found. Please manually save Serial Monitor output." -ForegroundColor Yellow
        Write-Host "Save as: $outputFile" -ForegroundColor Yellow
    }
}

# Create test scripts for each phase
Write-Host ""
Write-Host "Creating test execution scripts..." -ForegroundColor Yellow

# Detection phase script
$detectionScript = @"
# Run these tests in order and save Serial Monitor output

Write-Host "DETECTION PHASE TESTS" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Upload and run: CYD_Model_Detector"
Write-Host "2. Save Serial Monitor output to: Serial_Logs\CYD_Model_Detector.txt"
Write-Host ""
Write-Host "3. Upload and run: Hardware_Probe"
Write-Host "4. Save Serial Monitor output to: Serial_Logs\Hardware_Probe.txt"
Write-Host ""
Write-Host "5. Upload and run: CYD_Diagnostic_Test"
Write-Host "6. Save Serial Monitor output to: Serial_Logs\CYD_Diagnostic_Test.txt"
Write-Host ""
Write-Host "7. Upload and run: System_Info_Collector"
Write-Host "8. Save COMPLETE output to: Serial_Logs\System_Info_Collector.txt"
Write-Host ""
Read-Host "Press Enter when detection phase is complete"
"@

$detectionScript | Out-File -FilePath "$resultsDir\Run_Detection_Tests.ps1" -Encoding UTF8

# Create summary report template
$summaryTemplate = @"
================================================================================
TEST RESULTS SUMMARY
================================================================================

OVERALL STATUS: [IN PROGRESS / COMPLETE / ISSUES FOUND]

DETECTION PHASE:
----------------
✓/✗ Model Detection: [Result]
✓/✗ Hardware Probe: [Result]
✓/✗ Diagnostic Test: [Result]
✓/✗ System Info: [Result]

TDD PHASE:
----------
✓/✗ All tests failed as expected (good for TDD)
Issues noted: [List any unexpected behaviors]

IMPLEMENTATION PHASE:
---------------------
✓/✗ Main sketch compiles
✓/✗ Upload successful
✓/✗ Initialization complete
✓/✗ All components detected

FUNCTIONALITY:
--------------
✓/✗ Display works
✓/✗ Touch responsive
✓/✗ RFID reads cards
✓/✗ SD card accessible
✓/✗ Audio plays

PERFORMANCE:
------------
Boot time: _____ ms
Display init: _____ ms
Touch response: _____ ms
RFID read time: _____ ms

ISSUES/BUGS:
------------
1. [Issue description]
2. [Issue description]

RECOMMENDATIONS:
----------------
[Any suggestions for improvements]

================================================================================
"@

$summaryTemplate | Out-File -FilePath "$resultsDir\SUMMARY_TEMPLATE.txt" -Encoding UTF8

# Display final instructions
Write-Host ""
Write-Host "============================================================================" -ForegroundColor Green
Write-Host "SETUP COMPLETE! Results directory created: $resultsDir" -ForegroundColor Green
Write-Host "============================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "NEXT STEPS:" -ForegroundColor Cyan
Write-Host "1. Run each test sketch and save Serial Monitor output" -ForegroundColor White
Write-Host "2. Save outputs to: $resultsDir\Serial_Logs\" -ForegroundColor White
Write-Host "3. Take screenshots of important results" -ForegroundColor White
Write-Host "4. Update Test_Checklist.txt as you complete each test" -ForegroundColor White
Write-Host "5. Fill out SUMMARY_TEMPLATE.txt with final results" -ForegroundColor White
Write-Host "6. Zip the entire folder when complete" -ForegroundColor White
Write-Host ""

# Open results folder
Write-Host "Opening results folder..." -ForegroundColor Yellow
Start-Process explorer.exe -ArgumentList $resultsDir

# Create a quick helper function for the session
function Save-SerialOutput {
    param([string]$TestName)
    
    $clipBoard = Get-Clipboard -Raw
    if ($clipBoard) {
        $outputFile = "$resultsDir\Serial_Logs\${TestName}_Serial.txt"
        $clipBoard | Out-File -FilePath $outputFile -Encoding UTF8
        Write-Host "Saved clipboard content to: $outputFile" -ForegroundColor Green
    } else {
        Write-Host "No content in clipboard. Copy Serial Monitor output first." -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "TIP: After copying Serial Monitor output, use this command:" -ForegroundColor Yellow
Write-Host "Save-SerialOutput -TestName 'TestNameHere'" -ForegroundColor Cyan
Write-Host ""
Write-Host "Script complete. Good luck with testing!" -ForegroundColor Green