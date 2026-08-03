@ECHO === rmDOS hard disk install ===
@ECHO Booted from floppy. Target drive: C:
@ECHO.
@ECHO Step 1/3: Create active DOS partition
PARTEDIT /CREATE
@IF ERRORLEVEL 1 GOTO FAIL
@ECHO.
@ECHO Step 2/3: Format C: and install KERNEL.SYS + COMMAND.COM
FORMAT C: /S /Y
@IF ERRORLEVEL 1 GOTO FAIL
@ECHO.
@ECHO Step 3/3: Verify system files on C:
DIR C:\
@ECHO.
@ECHO INSTALL OK
@ECHO Remove the floppy and reboot to start rmDOS from the hard disk.
@GOTO END
:FAIL
@ECHO.
@ECHO INSTALL FAILED
@ECHO Check that a hard disk is attached (XT HDC) and try again.
:END
