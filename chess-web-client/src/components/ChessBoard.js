import React, { useState, useEffect } from 'react';
import { Chessboard } from 'react-chessboard';
import { Chess } from 'chess.js';

function ChessBoard({ fen, yourTurn, onMove, inGame }) {
  const [game, setGame] = useState(() => {
    try {
      const newGame = new Chess();
      if (fen && fen !== 'start') {
        newGame.load(fen);
      }
      return newGame;
    } catch (e) {
      console.error('Invalid FEN:', fen);
      return new Chess();
    }
  });

  useEffect(() => {
    if (fen && fen !== 'start' && inGame) {
      try {
        const newGame = new Chess();
        newGame.load(fen);
        setGame(newGame);
      } catch (e) {
        console.error('Error loading FEN:', fen, e);
      }
    }
  }, [fen, inGame]);

  const onDrop = (sourceSquare, targetSquare) => {
    if (!inGame || !yourTurn) return false;
    
    try {
      const gameCopy = new Chess(game.fen());
      const move = gameCopy.move({
        from: sourceSquare,
        to: targetSquare,
        promotion: 'q'
      });
      
      if (move) {
        onMove(sourceSquare, targetSquare, 'q');
        return true;
      }
    } catch (e) {
      console.log('Invalid move:', e);
    }
    return false;
  };

  if (!inGame) {
    return (
      <div className="waiting-board">
        <div className="waiting-message">
          <h2>Waiting for opponent...</h2>
          <p>Join the queue to start a game</p>
        </div>
      </div>
    );
  }

  return (
    <div className="chess-board">
      <Chessboard 
        position={game.fen()}
        onPieceDrop={onDrop}
        boardWidth={560}
        arePiecesDraggable={yourTurn}
      />
      {!yourTurn && (
        <div className="waiting-move">
          <p>Waiting for opponent's move...</p>
        </div>
      )}
    </div>
  );
}

export default ChessBoard;