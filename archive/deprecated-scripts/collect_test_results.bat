@echo off
REM ============================================================================
REM CYD Test Results Collector for Windows
REM This script helps organize test results for feedback submission
REM ============================================================================

echo.
echo ============================================================================
echo                    CYD TEST RESULTS COLLECTOR
echo ============================================================================
echo.

REM Set timestamp for unique folder name
set TIMESTAMP=%DATE:~-4%%DATE:~3,2%%DATE:~0,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%

REM Create results directory
set RESULTS_DIR=CYD_Test_Results_%TIMESTAMP%
mkdir "%RESULTS_DIR%" 2>nul

echo Creating results directory: %RESULTS_DIR%
echo.

REM Create subdirectories
mkdir "%RESULTS_DIR%\Detection_Phase" 2>nul
mkdir "%RESULTS_DIR%\TDD_Phase" 2>nul
mkdir "%RESULTS_DIR%\Implementation_Phase" 2>nul
mkdir "%RESULTS_DIR%\Screenshots" 2>nul
mkdir "%RESULTS_DIR%\Serial_Logs" 2>nul

REM Create hardware info file
echo ============================================================================ > "%RESULTS_DIR%\Hardware_Info.txt"
echo CYD HARDWARE TESTING INFORMATION >> "%RESULTS_DIR%\Hardware_Info.txt"
echo ============================================================================ >> "%RESULTS_DIR%\Hardware_Info.txt"
echo. >> "%RESULTS_DIR%\Hardware_Info.txt"
echo Test Date: %DATE% %TIME% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo. >> "%RESULTS_DIR%\Hardware_Info.txt"

echo Please provide the following information:
echo.

REM Collect user input
set /p CYD_MODEL="CYD Model (Single USB/Dual USB): "
set /p DISPLAY_TYPE="Display Type (ILI9341/ST7789/Unknown): "
set /p ARDUINO_VERSION="Arduino IDE Version: "
set /p ESP32_VERSION="ESP32 Core Version: "
set /p WINDOWS_VERSION="Windows Version (10/11): "
set /p TESTER_NAME="Your Name/Handle (optional): "

echo CYD Model: %CYD_MODEL% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo Display Type: %DISPLAY_TYPE% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo Arduino IDE: %ARDUINO_VERSION% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo ESP32 Core: %ESP32_VERSION% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo Windows: %WINDOWS_VERSION% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo Tester: %TESTER_NAME% >> "%RESULTS_DIR%\Hardware_Info.txt"
echo. >> "%RESULTS_DIR%\Hardware_Info.txt"

REM Create test checklist
echo ============================================================================ > "%RESULTS_DIR%\Test_Checklist.txt"
echo TEST EXECUTION CHECKLIST >> "%RESULTS_DIR%\Test_Checklist.txt"
echo ============================================================================ >> "%RESULTS_DIR%\Test_Checklist.txt"
echo. >> "%RESULTS_DIR%\Test_Checklist.txt"
echo DETECTION PHASE: >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] CYD_Model_Detector - Completed >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Hardware_Probe - Completed >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] CYD_Diagnostic_Test - Completed >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] System_Info_Collector - Completed >> "%RESULTS_DIR%\Test_Checklist.txt"
echo. >> "%RESULTS_DIR%\Test_Checklist.txt"
echo TDD PHASE (Should fail initially): >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_Hardware_Detection - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_Component_Init - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_Diagnostics - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_ILI9341_Detection - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_ST7789_Detection - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_GPIO27_Mux - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_Touch_Calibration - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo [ ] Test_RFID_Timing - Failed as expected >> "%RESULTS_DIR%\Test_Checklist.txt"
echo. >> "%RESULTS_DIR%\Test_Checklist.txt"

REM Instructions for serial logs
echo.
echo ============================================================================
echo INSTRUCTIONS FOR COLLECTING TEST RESULTS:
echo ============================================================================
echo.
echo 1. SERIAL MONITOR LOGS:
echo    - For each test sketch, save Serial Monitor output
echo    - In Arduino IDE: Edit -^> Copy All
echo    - Save as .txt file in: %RESULTS_DIR%\Serial_Logs\
echo    - Name format: [SketchName]_SerialOutput.txt
echo.
echo 2. SCREENSHOTS:
echo    - Take screenshots of Serial Monitor for important results
echo    - Take photos of hardware setup if issues occur
echo    - Save in: %RESULTS_DIR%\Screenshots\
echo.
echo 3. TEST EACH SKETCH:
echo    Detection Phase:
echo    - CYD_Model_Detector
echo    - Hardware_Probe
echo    - CYD_Diagnostic_Test
echo    - System_Info_Collector (IMPORTANT - Auto-generates report)
echo.
echo    TDD Phase (these SHOULD fail):
echo    - Test_Hardware_Detection
echo    - Test_Component_Init
echo    - Test_Diagnostics
echo    - Test_ILI9341_Detection
echo    - Test_ST7789_Detection
echo    - Test_GPIO27_Mux
echo    - Test_Touch_Calibration
echo    - Test_RFID_Timing
echo.
echo 4. AFTER ALL TESTS:
echo    - Edit Test_Checklist.txt to mark completed tests
echo    - Add any notes about specific failures or issues
echo    - Zip the entire %RESULTS_DIR% folder
echo.

REM Create a README for the results
echo Test Results Package > "%RESULTS_DIR%\README.txt"
echo =================== >> "%RESULTS_DIR%\README.txt"
echo. >> "%RESULTS_DIR%\README.txt"
echo This folder contains test results for CYD Multi-Model Compatibility testing. >> "%RESULTS_DIR%\README.txt"
echo. >> "%RESULTS_DIR%\README.txt"
echo Contents: >> "%RESULTS_DIR%\README.txt"
echo - Hardware_Info.txt: System and hardware details >> "%RESULTS_DIR%\README.txt"
echo - Test_Checklist.txt: List of tests performed >> "%RESULTS_DIR%\README.txt"
echo - Detection_Phase/: Results from hardware detection tests >> "%RESULTS_DIR%\README.txt"
echo - TDD_Phase/: Results from Test-Driven Development tests >> "%RESULTS_DIR%\README.txt"
echo - Implementation_Phase/: Results from main sketch testing >> "%RESULTS_DIR%\README.txt"
echo - Serial_Logs/: Complete serial monitor outputs >> "%RESULTS_DIR%\README.txt"
echo - Screenshots/: Visual evidence of test results >> "%RESULTS_DIR%\README.txt"
echo. >> "%RESULTS_DIR%\README.txt"
echo Generated: %DATE% %TIME% >> "%RESULTS_DIR%\README.txt"

echo.
echo Results folder created: %RESULTS_DIR%
echo Please follow the instructions above to complete testing.
echo.

REM Open the results folder
explorer "%RESULTS_DIR%"

echo Press any key to exit...
pause >nul