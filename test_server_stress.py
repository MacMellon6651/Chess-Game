import socket
import json
import pytest
import threading

ADDR = "127.0.0.1"
PORT = 8080
N = 10000
# Тест Отправка пачки сообщений в одном соединении
def send_burst(count):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((ADDR, PORT))
        for i in range(count):
            msg = json.dumps({"type": "ping", "id": i}) + "\n"
            sock.sendall(msg.encode())
            sock.recv(1024) 

#Тест Сервер выдерживает N быстрых запросов
def test_stress_load():
    send_burst(N)

#Тест Cервер работает с несколькими клиентами одновременно
def test_concurrent_clients():
    threads = []
    for _ in range(10):
        t = threading.Thread(target=send_burst, args=(20,))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join() # Жду завершения потоков