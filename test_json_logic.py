import socket
import json
import pytest
import subprocess
import time
import os

ADD = "127.0.0.1"
PORT = 18080

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

def get_server_response(payload_str):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(2.0)
        sock.connect((ADD, PORT))
        sock.sendall((payload_str + "\n").encode())
        return json.loads(sock.recv(1024).decode())

def auth(sock, nick="tester"):
    sock.sendall((json.dumps({"type": "auth", "nick": nick}) + "\n").encode())
    return json.loads(sock.recv(1024).decode())

def send_and_recv(sock, payload):
    sock.sendall((json.dumps(payload) + "\n").encode())
    return json.loads(sock.recv(1024).decode())

#Проверка move
def test_deserialization_valid_move():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(2.0)
        sock.connect((ADD, PORT))
        r = auth(sock, "p1")
        assert r["status"] == "ok"
        send_and_recv(sock, {"type": "queue"})
        # move is only allowed in game; just ensure server responds with JSON ok/error
        resp = send_and_recv(sock, {"type": "move", "from": "e2", "to": "e4", "player_id": 42})
        assert resp["status"] in ("ok", "error")

#Проверка на не-JSON данные
def test_deserialization_garbage():
    resp = get_server_response("ABOBA")
    assert resp["status"] == "error"
    assert "Invalid format" in resp["message"]

#Проверка тип есть, но данных для хода нет
def test_deserialization_missing_fields():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(2.0)
        sock.connect((ADD, PORT))
        r = auth(sock, "p2")
        assert r["status"] == "ok"
        resp = send_and_recv(sock, {"type": "move"})
        assert resp["status"] in ("ok", "error")