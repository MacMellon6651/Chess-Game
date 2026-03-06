import socket
import json
import threading
import sys

def receive_messages(sock):
    """Функция, которая постоянно слушает сервер в отдельном потоке"""
    while True:
        try:
            data = sock.recv(1024).decode('utf-8')
            if not data:
                print("\n[Система] Соединение разорвано сервером.")
                break
            
            
            for line in data.strip().split('\n'):
                if line:
                    payload = json.loads(line)
                    print(f"\n[Сервер]: {json.dumps(payload, indent=2, ensure_ascii=False)}")
                    print("Введите команду: ", end="", flush=True)
                    
        except Exception as e:
            print(f"\n[Ошибка чтения]: {e}")
            break

def start_client():
    host = '127.0.0.1'
    port = 8080

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
        print(f"Подключено к {host}:{port}")
    except Exception as e:
        print(f"Не удалось подключиться: {e}")
        return

    # поток для прослушивания сервера
    threading.Thread(target=receive_messages, args=(sock,), daemon=True).start()

    print("Команды: auth, move, exit")
    
    try:
        while True:
            cmd_type = input("Введите команду: ").strip()
            
            if cmd_type == "exit":
                break
            
            if not cmd_type: continue

            data = {"type": cmd_type}
            if cmd_type == "auth":
                data["player_id"] = int(input("Ваш Player ID: "))
            elif cmd_type == "move":
                data["from"] = input("Откуда: ")
                data["to"] = input("Куда: ")
            
            # Отправляем JSON с символом переноса строки
            message = json.dumps(data) + "\n"
            sock.sendall(message.encode('utf-8'))
            
    except KeyboardInterrupt:
        print("\nВыход...")
    finally:
        sock.close()

if __name__ == "__main__":
    start_client()