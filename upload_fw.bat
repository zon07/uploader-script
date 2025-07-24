@ECHO OFF
SETLOCAL EnableDelayedExpansion

REM Argument processing
SET SKIP_BOOT=0
SET SKIP_FLASH=0

:CHECK_ARGS
IF "%~1"=="" GOTO ARGS_DONE
IF /I "%~1"=="--no_boot" SET SKIP_BOOT=1
IF /I "%~1"=="--no_flash" SET SKIP_FLASH=1
SHIFT
GOTO CHECK_ARGS
:ARGS_DONE

IF %SKIP_BOOT% EQU 1 IF %SKIP_FLASH% EQU 1 (
    ECHO [INFO] Both --no_boot and --no_flash specified - skipping all operations
    EXIT /B 0
)

REM Получаем полный путь к рабочей директории
FOR %%I IN ("%~dp0.") DO SET "WORKING_DIR=%%~fI"

REM Tool paths
SET "UPLOADER=%WORKING_DIR%\mik32-uploader\mik32_upload.exe"
SET "OPENOCD_EXEC=%WORKING_DIR%\openocd\bin\openocd.exe"
SET "OPENOCD_INTERFACE=%WORKING_DIR%\mik32-uploader\openocd-scripts\interface\start-link.cfg"
SET "OPENOCD_TARGET=%WORKING_DIR%\mik32-uploader\openocd-scripts\target\mik32.cfg"

REM Firmware directory
SET "FIRMWARE_DIR=%WORKING_DIR%\fw_files"

REM Check if firmware directory exists
IF NOT EXIST "%FIRMWARE_DIR%" (
    ECHO [ERROR] Firmware directory not found: "%FIRMWARE_DIR%"
    EXIT /B 1
)

REM Fixed bootloader filename
SET "FILE1=kosvt_bootloader.hex"

REM Find main firmware file with highest version
SET "FLASH_FILE="
SET "FLASH_MAX_VERSION=0_0_0"
SET "FLASH_MAX_FILE="

FOR %%F IN ("%FIRMWARE_DIR%\kosvt_flash_*.hex") DO (
    SET "FLASH_FILENAME=%%~nF"
    REM Extract version parts after second underscore (X_Y_Z from kosvt_flash_X_Y_Z.hex)
    FOR /F "tokens=3-5 delims=_" %%A IN ("%%~nF") DO (
        SET "FLASH_VERSION=%%A_%%B_%%C"
        REM Use simple string comparison for version
        IF "%%A_%%B_%%C" GTR "!FLASH_MAX_VERSION!" (
            SET "FLASH_MAX_VERSION=%%A_%%B_%%C"
            SET "FLASH_MAX_FILE=%%F"
        )
    )
)

IF NOT DEFINED FLASH_MAX_FILE (
    ECHO [ERROR] No valid kosvt_flash_*.hex files found in "%FIRMWARE_DIR%"
    EXIT /B 1
)

SET "FILE2=!FLASH_MAX_FILE!"
ECHO [INFO] Firmware FLASH version: !FLASH_MAX_VERSION!

REM --------------------------------------------------
REM New simplified firmware loading without function
REM --------------------------------------------------

REM Load bootloader (unless --no_boot specified)
IF %SKIP_BOOT% EQU 0 (
    IF NOT EXIST "%FIRMWARE_DIR%\%FILE1%" (
        ECHO [ERROR] Bootloader file not found: "%FIRMWARE_DIR%\%FILE1%"
        EXIT /B 1
    )
    
    ECHO [STATUS] Loading bootloader...
    ECHO [DEBUG] Full path: "%FIRMWARE_DIR%\%FILE1%"
    
    "%UPLOADER%" "%FIRMWARE_DIR%\%FILE1%" ^
        --run-openocd ^
        --openocd-exec "%OPENOCD_EXEC%" ^
        --openocd-interface "%OPENOCD_INTERFACE%" ^
        --openocd-target "%OPENOCD_TARGET%" ^
        --boot-mode eeprom
    
    IF %ERRORLEVEL% NEQ 0 (
        ECHO [ERROR] Failed to load bootloader
        EXIT /B 1
    )
) ELSE (
    ECHO [INFO] Skipping bootloader (--no_boot specified^)
)


REM Load main firmware (unless --no_flash specified)
IF %SKIP_FLASH% EQU 0 (
    ECHO [STATUS] Loading main firmware version !FLASH_MAX_VERSION!...
    ECHO [DEBUG] Full path: "!FILE2!"
    
    "%UPLOADER%" "!FILE2!" ^
        --run-openocd ^
        --openocd-exec "%OPENOCD_EXEC%" ^
        --openocd-interface "%OPENOCD_INTERFACE%" ^
        --openocd-target "%OPENOCD_TARGET%" ^
        --boot-mode spifi
    
    IF %ERRORLEVEL% NEQ 0 (
        ECHO [ERROR] Failed to load main firmware
        EXIT /B 1
    )
) ELSE (
    ECHO [INFO] Skipping main firmware (--no_flash specified^)
)

ECHO [SUCCESS] Firmware upload completed successfully
EXIT /B 0