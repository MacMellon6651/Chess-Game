import socket
import json
import pytest

ADD = "127.0.0.1"
PORT = 8080

def get_server_response(payload_str):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(2.0)
        sock.connect((ADD, PORT))
        sock.sendall((payload_str + "\n").encode())
        return json.loads(sock.recv(1024).decode())


#Проверка move
def test_deserialization_valid_move():
    payload = {"type": "move", "from": "e2", "to": "e4", "player_id": 42}
    resp = get_server_response(json.dumps(payload))
    assert resp["status"] == "ok"

#Проверка на не-JSON данные
def test_deserialization_garbage():
    resp = get_server_response("ABOBA")
    assert resp["status"] == "error"
    assert "Invalid format" in resp["message"]

#Проверка тип есть, но данных для хода нет
def test_deserialization_missing_fields():
    payload = {"type": "move"} 
    resp = get_server_response(json.dumps(payload))
    assert resp["status"] == "ok"