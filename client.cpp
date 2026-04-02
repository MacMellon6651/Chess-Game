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
        board = {{
            {u8"♜", u8"♞", u8"♝", u8"♛", u8"♚", u8"♝", u8"♞", u8"♜"},
            {u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟", u8"♟"},
            {"·", "·", "·", "·", "·", "·", "·", "·"},
            {"·", "·", "·", "·", "·", "·", "·", "·"},
            {"·", "·", "·", "·", "·", "·", "·", "·"},
            {"·", "·", "·", "·", "·", "·", "·", "·"},
            {u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙", u8"♙"},
            {u8"♖", u8"♘", u8"♗", u8"♕", u8"♔", u8"♗", u8"♘", u8"♖"}
        }};
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

    static std::string pad_right(const std::string& s, size_t width) {
        if (s.size() >= width) return s.substr(0, width);
        return s + std::string(width - s.size(), ' ');
    }

    void render() const {
        std::cout << "\x1b[2J\x1b[H";
        std::cout << "Chess Client | nick: " << nick << " | status: " << status << "\n\n";
    
        
        std::cout << "    a  b  c  d  e  f  g  h \n";
        std::cout << "  +------------------------+\n";
        for (int r = 0; r < 8; ++r) {
            std::cout << (8 - r) << " | ";
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
        
        
        size_t empty_lines = kFeedMax - (feed.size() - start);
        for (size_t i = 0; i < empty_lines; ++i) {
            std::cout << "\n";
        }
        
        std::cout << "----------------------------------------\n";
        std::cout << "\nCommands: /queue | /leave | /chat <text> | /move <from> <to> | /draw | /draw accept | /draw decline | /quit\n";
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
                ui.apply_move(from, to);
                ui.push_feed("[move] " + by + ": " + from + " -> " + to);
            } else if (event == "match_found") {
                ui.status = "in_game";
                ui.push_feed("[match] opponent: " + data.value("opponent", std::string("?")));
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

        if (!send_and_process_one(sock, ui, {{"type", "auth"}, {"nick", ui.nick}})) {
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
                ui.apply_move(from, to);
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

