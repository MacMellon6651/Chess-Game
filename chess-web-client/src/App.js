import React, { useState, useEffect, useCallback } from 'react';
import ChessBoard from './components/ChessBoard';
import Chat from './components/Chat';
import GameStatus from './components/GameStatus';
import Login from './components/Login';
import './App.css';

function App() {
  const [ws, setWs] = useState(null);
  const [connected, setConnected] = useState(false);
  const [authorized, setAuthorized] = useState(false);
  const [nick, setNick] = useState('');
  const [rating, setRating] = useState(1000);
  const [gameState, setGameState] = useState({
    fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    yourTurn: false,
    opponent: null,
    inGame: false,
    status: 'disconnected'
  });
  const [messages, setMessages] = useState([]);
  const [events, setEvents] = useState([]);

  const addMessage = (sender, text) => {
    setMessages(prev => [...prev, { sender, text, time: new Date() }]);
  };

  const handleGameEvent = (data) => {
    const eventData = data.data;
    
    switch (data.event) {
      case 'match_found':
        setGameState(prev => ({
          ...prev,
          inGame: true,
          opponent: eventData.opponent,
          fen: eventData.board?.fen || 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
          yourTurn: eventData.board?.you_are === 'white',
          status: 'playing'
        }));
        addMessage('System', `Match found! Playing against ${eventData.opponent}`);
        addMessage('System', `You are ${eventData.board?.you_are || 'white'}`);
        break;
        
      case 'move':
        setGameState(prev => ({
          ...prev,
          fen: eventData.fen,
          yourTurn: true
        }));
        addMessage('Game', `${eventData.by}: ${eventData.from} → ${eventData.to}`);
        break;
        
      case 'game_over':
        setGameState(prev => ({
          ...prev,
          inGame: false,
          status: 'game_over'
        }));
        addMessage('Game', `Game over! ${eventData.result}`);
        if (eventData.winner) {
          addMessage('Game', `Winner: ${eventData.winner}`);
        }
        // Сброс доски
        setTimeout(() => {
          setGameState(prev => ({
            ...prev,
            fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
            yourTurn: false,
            opponent: null
          }));
        }, 3000);
        break;
        
      case 'chat':
        addMessage(eventData.from, eventData.text);
        break;
        
      case 'rating_update':
        setRating(eventData.rating);
        addMessage('System', `Your rating: ${eventData.rating}`);
        break;
        
      case 'draw_offer':
        addMessage('System', `${eventData.from} offers a draw! Use "Accept Draw" button`);
        break;
        
      case 'draw_agreed':
        addMessage('System', `Draw agreed! Game ended in draw`);
        setGameState(prev => ({ ...prev, inGame: false }));
        break;
        
      case 'draw_declined':
        addMessage('System', `${eventData.by} declined the draw offer`);
        break;
        
      case 'check':
        addMessage('Game', `${eventData.side} king is in check!`);
        break;
        
      case 'opponent_disconnected':
        addMessage('System', 'Opponent disconnected! You win!');
        setGameState(prev => ({ ...prev, inGame: false }));
        break;
        
      default:
        console.log('Unknown event:', data.event);
    }
  };

  const connectWebSocket = useCallback((nickname, password) => {
    const websocket = new WebSocket('ws://localhost:18081');
    
    websocket.onopen = () => {
      console.log('WebSocket connected');
      setConnected(true);
      addMessage('System', 'Connected to server');
      
      // Отправляем auth
      websocket.send(JSON.stringify({
        type: 'auth',
        nick: nickname,
        password: password || ''
      }));
    };
    
    websocket.onmessage = (event) => {
      const data = JSON.parse(event.data);
      console.log('Received:', data);
      
      // Обработка ответа auth
      if (data.status === 'ok' && data.message === 'authorized') {
        setAuthorized(true);
        setNick(nickname);
        addMessage('System', `Welcome ${nickname}! Rating: ${rating}`);
        return;
      }
      
      if (data.status === 'error') {
        addMessage('Error', data.message);
        return;
      }
      
      if (data.type === 'event') {
        handleGameEvent(data);
      } else if (data.status === 'ok') {
        if (data.message === 'queued') {
          addMessage('System', 'Added to queue. Waiting for opponent...');
        } else if (data.message === 'left') {
          addMessage('System', 'Left queue/game');
          setGameState(prev => ({ 
            ...prev, 
            inGame: false,
            fen: 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
          }));
        } else if (data.message === 'draw offered') {
          addMessage('System', 'Draw offer sent');
        } else if (data.message === 'draw accepted') {
          addMessage('System', 'Draw accepted');
        }
      }
    };
    
    websocket.onerror = (error) => {
      console.error('WebSocket error:', error);
      addMessage('Error', 'Connection error');
    };
    
    websocket.onclose = () => {
      console.log('WebSocket disconnected');
      setConnected(false);
      setAuthorized(false);
      addMessage('System', 'Disconnected from server');
    };
    
    setWs(websocket);
    
    return () => {
      if (websocket) websocket.close();
    };
  }, [rating]);

  const sendCommand = (command) => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(command));
    } else {
      addMessage('Error', 'Not connected to server');
    }
  };

  const joinQueue = () => {
    sendCommand({ type: 'queue' });
  };

  const leaveQueue = () => {
    sendCommand({ type: 'leave' });
  };

  const makeMove = (from, to, promotion) => {
    const command = { type: 'move', from, to };
    if (promotion) command.action = promotion;
    sendCommand(command);
    setGameState(prev => ({ ...prev, yourTurn: false }));
  };

  const sendChat = (text) => {
    sendCommand({ type: 'chat', text });
  };

  const offerDraw = () => {
    sendCommand({ type: 'draw', action: 'offer' });
  };

  const acceptDraw = () => {
    sendCommand({ type: 'draw', action: 'accept' });
  };

  const declineDraw = () => {
    sendCommand({ type: 'draw', action: 'decline' });
  };

  if (!connected || !authorized) {
    return <Login onLogin={connectWebSocket} />;
  }

  return (
    <div className="app">
      <div className="game-container">
        <div className="board-container">
          <ChessBoard 
            fen={gameState.fen}
            yourTurn={gameState.yourTurn}
            onMove={makeMove}
            inGame={gameState.inGame}
          />
        </div>
        
        <div className="info-container">
          <GameStatus 
            nick={nick}
            rating={rating}
            opponent={gameState.opponent}
            yourTurn={gameState.yourTurn}
            inGame={gameState.inGame}
            onJoin={joinQueue}
            onLeave={leaveQueue}
            onOfferDraw={offerDraw}
            onAcceptDraw={acceptDraw}
            onDeclineDraw={declineDraw}
          />
          
          <Chat 
            messages={messages}
            onSendMessage={sendChat}
            inGame={gameState.inGame}
          />
        </div>
      </div>
    </div>
  );
}

export default App;