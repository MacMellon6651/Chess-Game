import React, { useState, useRef, useEffect } from 'react';

function Chat({ messages, onSendMessage, inGame }) {
  const [input, setInput] = useState('');
  const messagesEndRef = useRef(null);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  const handleSubmit = (e) => {
    e.preventDefault();
    if (input.trim() && inGame) {
      onSendMessage(input.trim());
      setInput('');
    }
  };

  return (
    <div className="chat-container">
      <h3>Chat</h3>
      <div className="messages">
        {messages.map((msg, idx) => (
          <div key={idx} className="message">
            <span className="sender">{msg.sender}:</span>
            <span className="text">{msg.text}</span>
            <span className="time">
              {msg.time.toLocaleTimeString()}
            </span>
          </div>
        ))}
        <div ref={messagesEndRef} />
      </div>
      <form onSubmit={handleSubmit}>
        <input
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          placeholder={inGame ? "Type message..." : "Join game to chat"}
          disabled={!inGame}
        />
        <button type="submit" disabled={!inGame}>Send</button>
      </form>
    </div>
  );
}

export default Chat;