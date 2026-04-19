import React from 'react';

function GameStatus({ 
  nick, rating, opponent, yourTurn, inGame, 
  onJoin, onLeave, onOfferDraw, onAcceptDraw, onDeclineDraw 
}) {
  return (
    <div className="game-status">
      <div className="player-info">
        <h3>Player: {nick}</h3>
        <p>Rating: {rating}</p>
      </div>
      
      {inGame && (
        <div className="game-info">
          <h3>Opponent: {opponent}</h3>
          <p className={yourTurn ? 'your-turn' : 'opponent-turn'}>
            {yourTurn ? 'Your turn!' : 'Opponent\'s turn...'}
          </p>
          <div className="game-actions">
            <button onClick={onOfferDraw}>Offer Draw</button>
          </div>
        </div>
      )}
      
      <div className="queue-actions">
        {!inGame ? (
          <button onClick={onJoin} className="join-btn">
            Join Queue
          </button>
        ) : (
          <button onClick={onLeave} className="leave-btn">
            Leave Game
          </button>
        )}
      </div>
    </div>
  );
}

export default GameStatus;