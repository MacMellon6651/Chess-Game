import socket
import json
import pytest
import threading
import subprocess
import time
import os

ADDR = "127.0.0.1"
PORT = 18080
# Общее количество запросов в нагрузочном тесте
N = 10000

SERVER_PROC = None

def setup_module(module):
    global SERVER_PROC
    exe = os.path.join(os.path.dirname(__file__), "server.exe")
    SERVER_PROC = subprocess.Popen([exe], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.3)

def teardown_module(module):
    global SERVER_PROC
    if SERVER_PROC is not None:
        SERVER_PROC.terminate()
        try:
            SERVER_PROC.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            SERVER_PROC.kill()
    SERVER_PROC = None

# Отправка пачки ping-запросов в одном TCP-соединении
def send_burst(count):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((ADDR, PORT))
        # auth once per connection
        sock.sendall((json.dumps({"type": "auth", "nick": "stress"}) + "\n").encode())
        sock.recv(1024)
        for i in range(count):
            msg = json.dumps({"type": "ping", "id": i}) + "\n"
            sock.sendall(msg.encode())
            # ожидаем ответ pong; если сервер "умрет", recv выбросит исключение
            data = sock.recv(1024)
            assert data, "server closed connection during stress ping"

# Тест: сервер выдерживает N быстрых ping-запросов
def test_stress_load():
    send_burst(N)

# Тест: сервер работает с несколькими клиентами одновременно
def test_concurrent_clients():
    threads = []
    for _ in range(10):
        t = threading.Thread(target=send_burst, args=(200,))
        threads.append(t)
        t.start()
    
    for t in threads:
        t.join()  # ждем завершения потоков


def test_invalid_payload_does_not_crash_server():
    """
    Отправляем заведомо некорректные данные и убеждаемся,
    что сервер не падает и по‑прежнему отвечает на корректные запросы.
    """
    # шлём мусор
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((ADDR, PORT))
        sock.sendall(b"this is not json at all\n")
        # сервер может оборвать соединение или вернуть ошибку — нас устраивает любой вариант
        try:
            sock.recv(1024)
        except OSError:
            pass

    # после этого сервер все еще должен принимать обычные ping
    send_burst(50)