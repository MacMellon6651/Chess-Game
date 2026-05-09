@echo off
setlocal

where openssl >nul 2>nul
if %errorlevel% neq 0 (
    echo OpenSSL is not found in PATH.
    echo Install OpenSSL and rerun this script.
    exit /b 1
)

openssl req -x509 -nodes -newkey rsa:2048 -days 365 ^
    -keyout server.key ^
    -out server.crt ^
    -subj "/C=US/ST=Dev/L=Local/O=Chess/CN=localhost"

if %errorlevel% neq 0 (
    echo Failed to generate certificate.
    exit /b 1
)

echo Generated certs\server.crt and certs\server.key
