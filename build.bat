set BOOST_PATH=D:\practice\Cpp\Chess\boost_1_90_0
set BOOST_LIB=%BOOST_PATH%\stage\lib

g++ main.cpp server.cpp command.cpp config.cpp logging.cpp chess_engine/pieces.cpp chess_engine/zobrist.cpp chess_engine/move_gen.cpp chess_engine/position.cpp chess_engine/fen.cpp -o server.exe -I"%BOOST_PATH%" -I"." -L"%BOOST_LIB%" -lboost_log-mgw13-mt-x64-1_90 -lboost_log_setup-mgw13-mt-x64-1_90 -lboost_thread -lboost_chrono -lboost_atomic -lws2_32 -lmswsock

g++ client.cpp command.cpp config.cpp logging.cpp -o client.exe -I"%BOOST_PATH%" -I"." -L"%BOOST_LIB%" -lboost_log-mgw13-mt-x64-1_90 -lboost_log_setup-mgw13-mt-x64-1_90 -lboost_thread -lboost_chrono -lboost_atomic -lws2_32 -lmswsock
