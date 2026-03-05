import socket
import json

def start_client():
    host = '127.0.0.1'
    port = 8080

    try:
        # Создаем TCP сокет
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((host, port))
            print(f"Успешное подключение к {host}:{port}")
            print("Команды: auth, move, exit")

            while True:
                cmd_type = input("\nВведите тип команды (auth/move/exit): ").strip()
                
                if cmd_type == "exit":
                    break

                # Формируем данные в зависимости от команды
                data = {"type": cmd_type}
                
                if cmd_type == "auth":
                    data["player_id"] = int(input("Ваш Player ID (число): "))
                elif cmd_type == "move":
                    data["player_id"] = int(input("Ваш Player ID: "))
                    data["from"] = input("Откуда (например, e2): ")
                    data["to"] = input("Куда (например, e4): ")
                
                # Отправляем JSON + символ новой строки \n
                # Сервер использует async_read_until с разделителем \n
                message = json.dumps(data) + "\n"
                s.sendall(message.encode('utf-8'))

                # Ждем ответ от сервера
                response = s.recv(1024).decode('utf-8')
                print(f"Ответ сервера: {response.strip()}")

    except ConnectionRefusedError:
        print("Ошибка: Сервер не запущен!")
    except Exception as e:
        print(f"Произошла ошибка: {e}")

if __name__ == "__main__":
    start_client()