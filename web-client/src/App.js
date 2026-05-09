import React, { useState, useCallback, useRef, useEffect } from 'react';
import { Chess } from 'chess.js';
import './App.css';

function App() {
    // Состояния (без изменений)
    const [connected, setConnected] = useState(false);
    const [authorized, setAuthorized] = useState(false);
    const [nick, setNick] = useState('');
    const [rating, setRating] = useState(1000);
    const [inGame, setInGame] = useState(false);
    const [yourTurn, setYourTurn] = useState(false);
    const [opponent, setOpponent] = useState('');
    const [fen, setFen] = useState('start');
    const [queueSize, setQueueSize] = useState(0);
    const [messages, setMessages] = useState([]);
    const [selectedSquare, setSelectedSquare] = useState(null);
    const [possibleMoves, setPossibleMoves] = useState([]);
    const [leaderboard, setLeaderboard] = useState([]);
    const [isSearching, setIsSearching] = useState(false);
    const [moveInProgress, setMoveInProgress] = useState(false);
    const wsRef = useRef(null);
    const gameRef = useRef(new Chess());
    const lastMoveRef = useRef(null);

    // Загрузка позиции из FEN (без изменений)
    useEffect(() => {
        if (fen && fen !== 'start') {
            try {
                gameRef.current.load(fen);
            } catch (e) {
            }
        } else if (fen === 'start') {
            gameRef.current = new Chess();
        }
    }, [fen]);

    // Добавление сообщения (без изменений)
    const addMessage = useCallback((from, text, isError = false) => {
        const id = Date.now() + Math.random();
        setMessages(prev => [...prev, { from, text, time: new Date(), id, isError }]);
        if (isError) {
            setTimeout(() => {
                setMessages(prev => prev.filter(msg => msg.id !== id));
            }, 5000);
        }
    }, []);

    // Отправка сообщений (без изменений)
    const send = useCallback((msg) => {
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
            wsRef.current.send(JSON.stringify(msg) + '\n');
        }
    }, []);

    // ========== ИСПРАВЛЕННАЯ ФУНКЦИЯ sendMove ==========
    // Отправка хода
    const sendMove = useCallback((from, to, promotion = 'q') => {
    if (!yourTurn || !inGame || moveInProgress) {
        return false;
    }
    
    // Защита от хода на ту же клетку
    if (from === to) {
        addMessage('Info', 'Cannot move to the same square', true);
        setSelectedSquare(null);
        setPossibleMoves([]);
        return false;
    }
    
    // Проверяем, не пытаемся ли мы пойти на клетку с нашей же фигурой
    const targetPiece = gameRef.current.get(to);
    const currentTurn = gameRef.current.turn();
    if (targetPiece && targetPiece.color === (currentTurn === 'w' ? 'w' : 'b')) {
        addMessage('Info', 'You already have a piece there!', true);
        setSelectedSquare(null);
        setPossibleMoves([]);
        return false;
    }
    
    // Проверяем легальность хода через chess.js
    const testMove = gameRef.current.move({ from, to, promotion });
    if (!testMove) {
        addMessage('Error', 'Illegal move!', true);
        setSelectedSquare(null);
        setPossibleMoves([]);
        return false;
    }
    // Отменяем тестовый ход
    gameRef.current.undo();
    
    // Дополнительная проверка: целевая клетка должна быть в списке возможных ходов
    const possibleMovesForPiece = gameRef.current.moves({ verbose: true, square: from });
    const isValidTarget = possibleMovesForPiece.some(m => m.to === to);
    if (!isValidTarget) {
        addMessage('Error', 'Illegal move! You cannot move there', true);
        setSelectedSquare(null);
        setPossibleMoves([]);
        return false;
    }
    
    // Сохраняем информацию о ходе
    lastMoveRef.current = { from, to, promotion };
    setMoveInProgress(true);
    
    // Отправляем ход на сервер
    send({ type: 'move', from, to, promotion });
    
    return true;
}, [yourTurn, inGame, moveInProgress, send, addMessage]);

    // ========== ИСПРАВЛЕННАЯ ФУНКЦИЯ onSquareClick ==========
// Обработчик клика по клетке
const onSquareClick = useCallback((square) => {

    if (!inGame) {
        addMessage('Info', 'Join queue to start game');
        return;
    }
    
    if (!yourTurn) {
        addMessage('Info', 'Wait for opponent\'s move');
        return;
    }

    if (moveInProgress) {
        addMessage('Info', 'Move in progress, please wait...');
        return;
    }

    // Если есть выбранная клетка, пытаемся сделать ход
    if (selectedSquare) {
        // Проверяем, не пытаемся ли мы пойти на ту же клетку
        if (selectedSquare === square) {
            setSelectedSquare(null);
            setPossibleMoves([]);
            addMessage('Info', 'Move cancelled');
            return;
        }
        
        // Проверяем, допустим ли ход на эту клетку
        if (!possibleMoves.includes(square)) {
            addMessage('Info', 'Illegal move! You cannot move there', true);
            setSelectedSquare(null);
            setPossibleMoves([]);
            return;
        }
        
        sendMove(selectedSquare, square);
        setSelectedSquare(null);
        setPossibleMoves([]);
        return;
    }

    // Проверяем, есть ли фигура на клетке
    const piece = gameRef.current.get(square);
    if (!piece) {
        addMessage('Info', 'No piece on this square');
        return;
    }

    // Определяем, чей сейчас ход
    const currentTurn = gameRef.current.turn();
    const isWhitePiece = piece.color === 'w';
    const isBlackPiece = piece.color === 'b';
    const isCorrectColor = (currentTurn === 'w' && isWhitePiece) || (currentTurn === 'b' && isBlackPiece);

    if (!isCorrectColor) {
        addMessage('Info', `Not your piece! ${currentTurn === 'w' ? 'White' : 'Black'} to move`);
        return;
    }

    // Выбираем фигуру и получаем возможные ходы
    setSelectedSquare(square);
    const moves = gameRef.current.moves({ verbose: true, square: square });
    const toSquares = moves.map(m => m.to);
    setPossibleMoves(toSquares);
    
    if (toSquares.length === 0) {
        addMessage('Info', 'This piece has no legal moves', true);
    } else {
    }
}, [inGame, yourTurn, moveInProgress, selectedSquare, possibleMoves, sendMove, addMessage]);

    // Сброс выбора при смене хода (без изменений)
    useEffect(() => {
        if (yourTurn && !moveInProgress) {
            setSelectedSquare(null);
            setPossibleMoves([]);
        }
    }, [yourTurn, moveInProgress]);

    // Получение символа фигуры (без изменений)
    const getPieceChar = (piece) => {
        if (!piece) return '';
        const symbols = {
            'k': '♔', 'q': '♕', 'r': '♖', 'b': '♗', 'n': '♘', 'p': '♙',
            'K': '♚', 'Q': '♛', 'R': '♜', 'B': '♝', 'N': '♞', 'P': '♟'
        };
        const key = piece.type === 'k' && piece.color === 'w' ? 'k' :
                    piece.type === 'q' && piece.color === 'w' ? 'q' :
                    piece.type === 'r' && piece.color === 'w' ? 'r' :
                    piece.type === 'b' && piece.color === 'w' ? 'b' :
                    piece.type === 'n' && piece.color === 'w' ? 'n' :
                    piece.type === 'p' && piece.color === 'w' ? 'p' :
                    piece.type === 'k' && piece.color === 'b' ? 'K' :
                    piece.type === 'q' && piece.color === 'b' ? 'Q' :
                    piece.type === 'r' && piece.color === 'b' ? 'R' :
                    piece.type === 'b' && piece.color === 'b' ? 'B' :
                    piece.type === 'n' && piece.color === 'b' ? 'N' : 'P';
        return symbols[key];
    };

    // Отрисовка доски (без изменений)
    const renderBoard = () => {
        const board = gameRef.current.board();
        const files = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];
        
        return (
            <div className="chess-board-custom">
                {board.map((row, rank) => (
                    <div key={rank} className="board-row">
                        {row.map((piece, file) => {
                            const square = files[file] + (8 - rank);
                            const isSelected = selectedSquare === square;
                            const isPossibleMove = possibleMoves.includes(square);
                            const isEven = (rank + file) % 2 === 0;
                            const isCapture = isPossibleMove && gameRef.current.get(square);
                            
                            return (
                                <div
                                    key={square}
                                    className={`board-square ${isEven ? 'white' : 'black'} 
                                        ${isSelected ? 'selected' : ''} 
                                        ${isPossibleMove ? (isCapture ? 'capture-move' : 'possible-move') : ''}`}
                                    onClick={() => onSquareClick(square)}
                                >
                                    <span className="piece">{getPieceChar(piece)}</span>
                                </div>
                            );
                        })}
                    </div>
                ))}
            </div>
        );
    };

    // Команды (без изменений)
    const joinQueue = () => {
        if (!isSearching && !inGame) {
            setIsSearching(true);
            send({ type: 'queue' });
        }
    };

    const leaveQueue = () => {
        setIsSearching(false);
        send({ type: 'leave' });
        setInGame(false);
        setYourTurn(false);
        setSelectedSquare(null);
        setPossibleMoves([]);
        setMoveInProgress(false);
        gameRef.current = new Chess();
        setFen('start');
        lastMoveRef.current = null;
    };

    const sendChat = (text) => {
        send({ type: 'chat', text });
        addMessage('me', text);
    };

    const offerDraw = () => {
        if (inGame && !moveInProgress) {
            send({ type: 'draw', action: 'offer' });
        }
    };
    
    const acceptDraw = () => {
        if (inGame) {
            send({ type: 'draw', action: 'accept' });
        }
    };
    
    const declineDraw = () => {
        if (inGame) {
            send({ type: 'draw', action: 'decline' });
        }
    };

    // Подключение WebSocket (без изменений)
    const connect = useCallback((nickname, password) => {
        const ws = new WebSocket('ws://localhost:18081');
        wsRef.current = ws;

        ws.onopen = () => {
            setConnected(true);
            setNick(nickname);
            addMessage('System', 'Connected to server');
            send({ type: 'auth', nick: nickname, password });
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);

                if (data.status === 'ok') {
                    if (data.message === 'authorized') {
                        setAuthorized(true);
                        addMessage('System', `Welcome ${nickname}!`);
                        send({ type: 'leaderboard' });
                    } else if (data.message === 'queued') {
                        addMessage('System', '✅ Searching for opponent...');
                    } else if (data.message === 'left') {
                        setIsSearching(false);
                        setInGame(false);
                        setYourTurn(false);
                        setMoveInProgress(false);
                        gameRef.current = new Chess();
                        setFen('start');
                        setSelectedSquare(null);
                        setPossibleMoves([]);
                        lastMoveRef.current = null;
                        addMessage('System', 'Left queue/game');
                    } else if (data.message === 'draw offered') {
                        addMessage('System', 'Draw offer sent');
                    } else if (data.message === 'draw accepted') {
                        addMessage('System', 'Draw accepted! Game ended');
                        setInGame(false);
                        setIsSearching(false);
                        setMoveInProgress(false);
                    }
                } else if (data.type === 'event') {
                    const ev = data.event;
                    const d = data.data;

                    if (ev === 'match_found') {
                        setIsSearching(false);
                        setInGame(true);
                        setOpponent(d.opponent);
                        const isWhite = d.board?.you_are === 'white';
                        setYourTurn(isWhite);
                        const newFen = d.board?.fen || 'start';
                        setFen(newFen);
                        gameRef.current.load(newFen);
                        setSelectedSquare(null);
                        setPossibleMoves([]);
                        setMoveInProgress(false);
                        lastMoveRef.current = null;
                        addMessage('System', `🎉 Match found! Playing vs ${d.opponent} as ${d.board?.you_are}`);
                        if (isWhite) {
                            addMessage('System', '♔ White moves first!');
                        } else {
                            addMessage('System', '♚ Black - waiting for white\'s move');
                        }
                    } else if (ev === 'move') {
                        
                        if (d.by !== nickname) {
                            try {
                                gameRef.current.load(d.fen);
                                setFen(d.fen);
                                setYourTurn(true);
                                setMoveInProgress(false);
                                setSelectedSquare(null);
                                setPossibleMoves([]);
                            } catch (e) {
                                setFen(d.fen);
                                setYourTurn(true);
                                setMoveInProgress(false);
                            }
                        } else {
                            try {
                                gameRef.current.load(d.fen);
                                setFen(d.fen);
                                setYourTurn(false);
                                setMoveInProgress(false);
                                setSelectedSquare(null);
                                setPossibleMoves([]);
                            } catch (e) {
                                setFen(d.fen);
                                setMoveInProgress(false);
                            }
                        }
                        
                        addMessage('Game', `${d.by}: ${d.from} → ${d.to}`);
                    } else if (ev === 'game_over') {
                        setInGame(false);
                        setIsSearching(false);
                        setMoveInProgress(false);
                        setYourTurn(false);
                        addMessage('Game', `🏆 Game over! ${d.result}`);
                        if (d.winner) addMessage('Game', `Winner: ${d.winner}`);
                    } else if (ev === 'chat') {
                        addMessage(d.from, d.text);
                    } else if (ev === 'rating_update') {
                        setRating(d.rating);
                        addMessage('System', `⭐ Rating updated: ${d.rating}`);
                    } else if (ev === 'draw_offer') {
                        addMessage('System', `🎲 ${d.from} offers a draw! Use buttons below.`);
                    } else if (ev === 'draw_agreed') {
                        setInGame(false);
                        setIsSearching(false);
                        setMoveInProgress(false);
                        addMessage('System', '🤝 Draw agreed!');
                    } else if (ev === 'draw_declined') {
                        addMessage('System', `❌ ${d.by} declined the draw`);
                    } else if (ev === 'queue_update') {
                        setQueueSize(d.size);
                    } else if (ev === 'check') {
                        addMessage('Game', `⚠️ ${d.side} king in check!`);
                    } else if (ev === 'leaderboard') {
                        setLeaderboard(d.players || []);
                    } else if (ev === 'opponent_disconnected') {
                        setInGame(false);
                        setIsSearching(false);
                        setMoveInProgress(false);
                        setYourTurn(false);
                        addMessage('System', '⚠️ Opponent disconnected! You win!');
                    }
                } else if (data.status === 'error') {
                    if (data.message === 'Illegal move' || data.message === 'Not your turn' || data.message === 'Move failed') {
                        addMessage('Error', data.message, true);
                        setMoveInProgress(false);
                        setYourTurn(true);
                        setSelectedSquare(null);
                        setPossibleMoves([]);
                        lastMoveRef.current = null;
                    } else {
                        addMessage('Error', data.message, true);
                        setMoveInProgress(false);
                    }
                }
            } catch (e) {
            }
        };

        ws.onclose = () => {
            setConnected(false);
            setAuthorized(false);
            setIsSearching(false);
            setInGame(false);
            setMoveInProgress(false);
            addMessage('System', 'Disconnected from server');
        };
    }, [send, addMessage]);

    // Форма входа
    const [loginNick, setLoginNick] = useState('');
    const [loginPass, setLoginPass] = useState('');

    if (!connected || !authorized) {
        return (
            <div className="login-container">
                <div className="login-box">
                    <h1>♜ CHESS GAME ♞</h1>
                    <input
                        type="text"
                        placeholder="Nickname"
                        value={loginNick}
                        onChange={(e) => setLoginNick(e.target.value)}
                        onKeyPress={(e) => e.key === 'Enter' && connect(loginNick, loginPass)}
                    />
                    <input
                        type="password"
                        placeholder="Password (optional)"
                        value={loginPass}
                        onChange={(e) => setLoginPass(e.target.value)}
                        onKeyPress={(e) => e.key === 'Enter' && connect(loginNick, loginPass)}
                    />
                    <button onClick={() => connect(loginNick, loginPass)}>▶ PLAY</button>
                </div>
            </div>
        );
    }

    return (
        <div className="app">
            <div className="game-container">
                <div className="board-container">
                    {!inGame ? (
                        <div className="waiting-board">
                            <div className={`searching-animation ${isSearching ? 'active' : ''}`}>
                                <div className="pulse-ring"></div>
                                <div className="pulse-ring delay-1"></div>
                                <div className="pulse-ring delay-2"></div>
                                <div className="chess-pieces">
                                    <span className="piece-anim">♜</span>
                                    <span className="piece-anim">♞</span>
                                    <span className="piece-anim">♝</span>
                                    <span className="piece-anim">♛</span>
                                    <span className="piece-anim">♚</span>
                                </div>
                            </div>
                            <div className="waiting-message">
                                <h2>{isSearching ? 'SEARCHING FOR OPPONENT...' : 'READY TO PLAY'}</h2>
                                <p className="queue-size">Players in queue: {queueSize}</p>
                                {!isSearching ? (
                                    <button onClick={joinQueue} className="btn-join">🎮 FIND MATCH</button>
                                ) : (
                                    <button onClick={leaveQueue} className="btn-leave">❌ CANCEL</button>
                                )}
                            </div>
                        </div>
                    ) : (
                        <>
                            <div className="game-header">
                                <div className="vs-info">
                                    <span className="you">{nick}</span>
                                    <span className="vs">VS</span>
                                    <span className="opp">{opponent}</span>
                                </div>
                            </div>
                            {renderBoard()}
                            <div className={`turn-indicator ${yourTurn && !moveInProgress ? 'your-turn' : 'waiting-turn'}`}>
                                {yourTurn && !moveInProgress ? '🎯 YOUR MOVE' : '⏳ WAITING FOR OPPONENT...'}
                                {moveInProgress && '⏳ Sending move...'}
                            </div>
                            <div className="game-buttons">
                                <button onClick={offerDraw} className="btn-draw">🤝 OFFER DRAW</button>
                                <button onClick={acceptDraw} className="btn-accept">✅ ACCEPT DRAW</button>
                                <button onClick={declineDraw} className="btn-decline">❌ DECLINE DRAW</button>
                                <button onClick={leaveQueue} className="btn-resign">🏳️ RESIGN</button>
                            </div>
                        </>
                    )}
                </div>

                <div className="info-container">
                    <div className="player-card">
                        <div className="player-avatar">👤</div>
                        <div className="player-details">
                            <div className="player-name">{nick}</div>
                            <div className="player-rating">⭐ {rating}</div>
                        </div>
                    </div>

                    <div className="leaderboard">
                        <div className="leaderboard-header">🏆 TOP PLAYERS</div>
                        <div className="leaderboard-list">
                            {leaderboard.length === 0 ? (
                                <div className="loading-leaderboard">Loading...</div>
                            ) : (
                                leaderboard.slice(0, 10).map((p, idx) => (
                                    <div key={p.id} className="leaderboard-item">
                                        <span className="rank">#{idx + 1}</span>
                                        <span className="name">{p.nickname}</span>
                                        <span className="rating">{p.rating}</span>
                                    </div>
                                ))
                            )}
                        </div>
                    </div>

                    <div className="chat-container">
                        <div className="chat-header">💬 CHAT</div>
                        <div className="chat-messages">
                            {messages.map(msg => (
                                <div key={msg.id} className={`chat-message ${msg.isError ? 'error' : ''}`}>
                                    <span className="chat-sender">{msg.from}:</span>
                                    <span className="chat-text">{msg.text}</span>
                                </div>
                            ))}
                        </div>
                        <form className="chat-form" onSubmit={(e) => {
                            e.preventDefault();
                            const input = e.target.elements.msg.value;
                            if (input.trim() && inGame) {
                                sendChat(input);
                                e.target.reset();
                            }
                        }}>
                            <input name="msg" placeholder="Type message..." disabled={!inGame} />
                            <button type="submit" disabled={!inGame}>SEND</button>
                        </form>
                    </div>
                </div>
            </div>
        </div>
    );
}

export default App;