@echo off
title SignBridge - Live Voice & Competition Dashboard
cd /d "%~dp0"
echo =======================================================
echo          SIGNBRIDGE SMART GLOVE VOICE BRIDGE
echo =======================================================
echo [*] Connecting to Glove on COM3...
echo [*] Laptop Speakers Activated!
echo.
python signbridge_voice_bridge.py
pause
