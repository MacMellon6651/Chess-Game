#include <boost/asio.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include "config.hpp"
#include "command.hpp"
#include "logging.hpp"

using boost::asio::ip::tcp;

namespace {
#ifdef _WIN32
static void setup_windows_console_utf8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(out, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(out, mode);
        }
    }
}
#endif

// Функция для обновления доски из FEN строки
void update_board_from_fen(std::array<std::array<std::string, 8>, 8>& board, const std::string& fen) {
    std::string board_part = fen;
    size_t space_pos = fen.find(' ');
    if (space_pos != std::string::npos) {
        board_part = fen.substr(0, space_pos);
    }
    
    // В FEN первая строка - это 8-я горизонталь (чёрные фигуры)
    int y = 7;  // индекс строки в доске (0-7)
    int x = 0;
    
    for (char ch : board_part) {
        if (ch == '/') {
            x = 0;
            y--;  // переходим на следующую строку вниз
        } else if (std::isdigit(ch)) {
            int empty = ch - '0';
            for (int i = 0; i < empty; ++i) {
                board[y][x] = "·";
                x++;
            }
        } else {
            // FEN: заглавные = белые, строчные = чёрные
            switch (ch) {
                case 'P': board[y][x] = u8"♙"; break;  // белая пешка
                case 'N': board[y][x] = u8"♘"; break;  // белый конь
                case 'B': board[y][x] = u8"♗"; break;  // белый слон
                case 'R': board[y][x] = u8"♖"; break;  // белая ладья
                case 'Q': board[y][x] = u8"♕"; break;  // белый ферзь
                case 'K': board[y][x] = u8"♔"; break;  // белый король
                case 'p': board[y][x] = u8"♟"; break;  // чёрная пешка
                case 'n': board[y][x] = u8"♞"; break;  // чёрный конь
                case 'b': board[y][x] = u8"♝"; break;  // чёрный слон
                case 'r': board[y][x] = u8"♜"; break;  // чёрная ладья
                case 'q': board[y][x] = u8"♛"; break;  // чёрный ферзь
                case 'k': board[y][x] = u8"♚"; break;  // чёрный король
                default: board[y][x] = "·"; break;
            }
            x++;
        }
    }
}

struct ClientUi {
    std::array<std::array<std::string, 8>, 8> board{};
    std::deque<std::string> feed;
    std::string nick;
    std::string status = "connected";
    static constexpr size_t kFeedMax = 18;

    void push_feed(const std::string& msg) {
        feed.push_back(msg);
        while (feed.size() > kFeedMax) feed.pop_front();
    }

    void reset_board() {
    // Правильная расстановка: белые внизу (ряд 0-1), чёрные вверху (ряд 6-7)
    board = {{
        // 8-я горизонталь (индекс 7) - чёрные фигуры
        {u8"♜", u8"♞", u8"♝", u8"♛", u8"♚", u8"♝", u8"♞", u8"♜"},
        // 7-я горизонталь (индекс 6) - чёрные пешки
        {u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟"},
        // 6-я горизонталь (индекс 5)
        {"·", "·", "·", "·", "·", "·", "·", "·"},
        // 5-я горизонталь (индекс 4)
        {"·", "·", "·", "·", "·", "·", "·", "·"},
        // 4-я горизонталь (индекс 3)
        {"·", "·", "·", "·", "·", "·", "·", "·"},
        // 3-я горизонталь (индекс 2)
        {"·", "·", "·", "·", "·", "·", "·", "·"},
        // 2-я горизонталь (индекс 1) - белые пешки
        {u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙"},
        // 1-я горизонталь (индекс 0) - белые фигуры
        {u8"♖", u8"♘", u8"♗", u8"♕", u8"♔", u8"♗", u8"♘", u8"♖"}
    }};
}

    // Обновление всей доски из FEN
    void update_board_from_fen(const std::string& fen) {
        ::update_board_from_fen(board, fen);
    }

    bool sq_to_idx(const std::string& sq, int& r, int& c) const {
        if (sq.size() != 2) return false;
        char file = static_cast<char>(std::tolower(static_cast<unsigned char>(sq[0])));
        char rank = sq[1];
        if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return false;
        c = file - 'a';
        r = 8 - (rank - '0');
        return true;
    }

    void apply_move(const std::string& from, const std::string& to) {
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
        if (!sq_to_idx(from, r1, c1) || !sq_to_idx(to, r2, c2)) {
            push_feed("[warn] bad move coords: " + from + " -> " + to);
            return;
        }
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = "·";
    }

    void render() const {
    // Очистка экрана и истории прокрутки
    std::cout << "\x1b[2J\x1b[H\x1b[3J";
    std::cout.flush();
    
    std::cout << "Chess Client | nick: " << nick << " | status: " << status << "\n\n";
    
    std::cout << "    a  b  c  d  e  f  g  h \n";
    std::cout << "  +------------------------+\n";
    for (int r = 7; r >= 0; --r) {
        std::cout << (1 + r) << " | ";
        for (int c = 0; c < 8; ++c) {
            std::cout << board[r][c] << "  ";
        }
        std::cout << "|\n";
    }
    std::cout << "  +------------------------+\n\n";
    
    std::cout << "Chat / Events\n";
    std::cout << "----------------------------------------\n";
    
    size_t start = feed.size() > kFeedMax ? feed.size() - kFeedMax : 0;
    for (size_t i = start; i < feed.size(); ++i) {
        std::cout << feed[i] << "\n";
    }
    
    std::cout << "----------------------------------------\n";
    std::cout << "\nCommands: /queue | /leave | /chat <text> | /move <from> <to> | /draw | /draw accept | /draw decline | /quit\n";
    std::cout << "> " << std::flush;
}
};

static void process_incoming_line(ClientUi& ui, const std::string& line) {
    try {
        auto jn = nlohmann::json::parse(line);
        if (jn.contains("status") && jn.contains("message")) {
            ui.push_feed("[" + jn.value("status", std::string("info")) + "] " + jn.value("message", std::string("")));
            return;
        }
        if (jn.value("type", std::string("")) == "event") {
            const auto event = jn.value("event", std::string(""));
            const auto data = jn.value("data", nlohmann::json::object());
            if (event == "chat") {
                ui.push_feed(data.value("from", std::string("?")) + ": " + data.value("text", std::string("")));
            } else if (event == "move") {
                const std::string from = data.value("from", std::string(""));
                const std::string to = data.value("to", std::string(""));
                const std::string by = data.value("by", std::string("?"));
                const std::string fen = data.value("fen", std::string(""));
                
                // Обновляем доску из FEN
                if (!fen.empty()) {
                    ui.update_board_from_fen(fen);
                } else {
                    // fallback: старый способ обновления
                    ui.apply_move(from, to);
                }
                ui.push_feed("[move] " + by + ": " + from + " -> " + to);
            } else if (event == "match_found") {
                ui.status = "in_game";
                ui.push_feed("[match] opponent: " + data.value("opponent", std::string("?")));
                
                // Обновляем доску из FEN, если пришла
                if (data.contains("board") && data["board"].contains("fen")) {
                    std::string fen = data["board"]["fen"];
                    ui.update_board_from_fen(fen);
                }
            } else if (event == "game_over") {
                ui.status = "menu";
                std::string result = data.value("result", std::string(""));
                if (result == "checkmate") {
                    ui.push_feed("[game] checkmate! " + data.value("winner", std::string("")) + " wins!");
                } else if (result == "stalemate") {
                    ui.push_feed("[game] stalemate! Game drawn.");
                } else if (result == "draw_agreed") {
                    ui.push_feed("[game] draw agreed!");
                } else if (result == "threefold_repetition") {
                    ui.push_feed("[game] threefold repetition! Game drawn.");
                } else if (result == "fifty_move_rule") {
                    ui.push_feed("[game] fifty move rule! Game drawn.");
                } else {
                    ui.push_feed("[game] game over: " + result);
                }
            } else if (event == "opponent_disconnected") {
                ui.status = "menu";
                ui.push_feed("[game] opponent disconnected, you win");
            } else if (event == "draw_offer") {
                ui.push_feed("[draw] offer from " + data.value("from", std::string("?")) + " (use /draw accept or /draw decline)");
            } else if (event == "draw_agreed") {
                ui.status = "menu";
                ui.push_feed("[game] draw agreed, rating unchanged");
            } else if (event == "draw_declined") {
                ui.push_feed("[draw] declined by " + data.value("by", std::string("?")));
            } else if (event == "rating_update") {
                ui.push_feed("[rating] now: " + std::to_string(data.value("rating", 0)));
            } else if (event == "check") {
                ui.push_feed("[check] " + data.value("side", std::string("")) + " king is in check!");
            } else {
                ui.push_feed("[event] " + event);
            }
            return;
        }
        ui.push_feed("[raw] " + line);
    } catch (...) {
        ui.push_feed("[raw] " + line);
    }
}

static bool drain_incoming(tcp::socket& sock, ClientUi& ui) {
    boost::system::error_code ec;
    bool got_any = false;
    while (sock.is_open() && sock.available(ec) > 0 && !ec) {
        boost::asio::streambuf buf;
        boost::asio::read_until(sock, buf, '\n', ec);
        if (ec) break;
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        if (!line.empty()) {
            process_incoming_line(ui, line);
            got_any = true;
        }
    }
    return got_any;
}

static bool send_and_process_one(tcp::socket& sock, ClientUi& ui, const nlohmann::json& payload) {
    boost::system::error_code ec;
    auto data = Protocol::serialize_json(payload);
    boost::asio::write(sock, boost::asio::buffer(data), ec);
    if (ec) return false;

    boost::asio::streambuf buf;
    boost::asio::read_until(sock, buf, '\n', ec);
    if (ec) return false;

    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    if (!line.empty()) process_incoming_line(ui, line);

    drain_incoming(sock, ui);
    return true;
}

struct InputQueue {
    std::mutex mtx;
    std::deque<std::string> q;

    void push(std::string s) {
        std::lock_guard<std::mutex> lk(mtx);
        q.push_back(std::move(s));
    }

    bool pop(std::string& out) {
        std::lock_guard<std::mutex> lk(mtx);
        if (q.empty()) return false;
        out = std::move(q.front());
        q.pop_front();
        return true;
    }
};
}  // namespace

int main() {
    init_logging();
    try {
#ifdef _WIN32
        setup_windows_console_utf8();
#endif
        auto settings = ConfigReader::load("settings.json");

        boost::asio::io_context io;
        tcp::socket sock(io);
        sock.connect(tcp::endpoint(boost::asio::ip::make_address(settings.host), settings.port));

        ClientUi ui;
        ui.reset_board();
        std::cout << "Enter nick: ";
        std::getline(std::cin, ui.nick);
        if (ui.nick.empty()) {
            std::cerr << "nick is required\n";
            return 1;
        }

        std::cout << "Enter password (optional, press Enter to skip): ";
        std::string password;
        std::getline(std::cin, password);

        if (!send_and_process_one(sock, ui, {{"type", "auth"}, {"nick", ui.nick}, {"password", password}})) {
            std::cerr << "auth failed\n";
            return 1;
        }
        ui.status = "menu";

        InputQueue input_queue;
        bool stop = false;
        std::thread input_thread([&]() {
            std::string line;
            while (!stop && std::getline(std::cin, line)) {
                input_queue.push(line);
            }
        });

        ui.push_feed("[hint] type commands in console, UI refreshes automatically");

        bool dirty = true;
        bool prompt_shown = false;
        for (;;) {
            if (drain_incoming(sock, ui)) dirty = true;

            if (dirty) {
                ui.render();
                prompt_shown = false;
                dirty = false;
            }
            if (!prompt_shown) {
                std::cout << "> " << std::flush;
                prompt_shown = true;
            }

            std::string line;
            if (!input_queue.pop(line)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                continue;
            }

            if (line == "/quit") break;
            if (line == "/queue") {
                ui.status = "waiting";
                if (!send_and_process_one(sock, ui, {{"type", "queue"}})) break;
                dirty = true;
                continue;
            }
            if (line == "/leave") {
                ui.status = "menu";
                if (!send_and_process_one(sock, ui, {{"type", "leave"}})) break;
                dirty = true;
                continue;
            }
            if (line.rfind("/chat ", 0) == 0) {
                auto text = line.substr(6);
                ui.push_feed("me: " + text);
                if (!send_and_process_one(sock, ui, {{"type", "chat"}, {"text", text}})) break;
                dirty = true;
                continue;
            }
            if (line.rfind("/move ", 0) == 0) {
                std::istringstream iss(line.substr(6));
                std::string from, to;
                iss >> from >> to;
                if (from.empty() || to.empty()) {
                    ui.push_feed("[hint] usage: /move e2 e4");
                    dirty = true;
                    continue;
                }
                // Не применяем ход локально, ждём подтверждения от сервера
                if (!send_and_process_one(sock, ui, {{"type", "move"}, {"from", from}, {"to", to}})) break;
                dirty = true;
                continue;
            }
            if (line == "/draw") {
                if (!send_and_process_one(sock, ui, {{"type", "draw"}, {"action", "offer"}})) break;
                dirty = true;
                continue;
            }
            if (line == "/draw accept") {
                if (!send_and_process_one(sock, ui, {{"type", "draw"}, {"action", "accept"}})) break;
                dirty = true;
                continue;
            }
            if (line == "/draw decline") {
                if (!send_and_process_one(sock, ui, {{"type", "draw"}, {"action", "decline"}})) break;
                dirty = true;
                continue;
            }
            ui.push_feed("[hint] unknown command");
            dirty = true;
        }

        stop = true;
        if (input_thread.joinable()) input_thread.detach();

        boost::system::error_code ignored;
        sock.shutdown(tcp::socket::shutdown_both, ignored);
        sock.close(ignored);
        return 0;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        return 1;
    }
}

///