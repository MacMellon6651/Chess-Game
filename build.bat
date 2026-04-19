@echo off
set BOOST_PATH=D:\practice\Cpp\Chess\boost_1_90_0
set BOOST_LIB=%BOOST_PATH%\stage\lib
set SQLITE_PATH=D:\practice\Cpp\Chess\sqlite

echo Building Chess Server with SQLite...

REM Компилируем SQLite как C
gcc -c sqlite/sqlite3.c -o sqlite3.o -I"%SQLITE_PATH%" -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1

if %errorlevel% neq 0 (
    echo SQLite compilation failed!
    pause
    exit /b %errorlevel%
)

REM Компилируем сервер
g++ -std=c++17 ^
    main.cpp ^
    server.cpp ^
    command.cpp ^
    config.cpp ^
    logging.cpp ^
    database.cpp ^
    websocket_server.cpp ^
    chess_engine/pieces.cpp ^
    chess_engine/zobrist.cpp ^
    chess_engine/move_gen.cpp ^
    chess_engine/position.cpp ^
    chess_engine/fen.cpp ^
    sqlite3.o ^
    -o server.exe ^
    -I"%BOOST_PATH%" ^
    -I"%SQLITE_PATH%" ^
    -I"." ^
    -L"%BOOST_LIB%" ^
    -lboost_log-mgw13-mt-x64-1_90 ^
    -lboost_log_setup-mgw13-mt-x64-1_90 ^
    -lboost_thread ^
    -lboost_chrono ^
    -lboost_atomic ^
    -lws2_32 -lmswsock

if %errorlevel% neq 0 (
    echo Server build failed!
    pause
    exit /b %errorlevel%
)

echo Building Chess Client...

g++ -std=c++17 ^
    client.cpp ^
    command.cpp ^
    config.cpp ^
    logging.cpp ^
    -o client.exe ^
    -I"%BOOST_PATH%" ^
    -I"." ^
    -L"%BOOST_LIB%" ^
    -lboost_log-mgw13-mt-x64-1_90 ^
    -lboost_log_setup-mgw13-mt-x64-1_90 ^
    -lboost_thread ^
    -lboost_chrono ^
    -lboost_atomic ^
    -lws2_32 -lmswsock

echo Build complete!
pause