import React, { useState } from 'react';

function Login({ onLogin }) {
  const [nick, setNick] = useState('');
  const [password, setPassword] = useState('');

  const handleSubmit = (e) => {
    e.preventDefault();
    if (nick.trim()) {
      onLogin(nick.trim(), password);
    }
  };

  return (
    <div className="login-container">
      <div className="login-box">
        <h1>Chess Game</h1>
        <form onSubmit={handleSubmit}>
          <input
            type="text"
            placeholder="Nickname"
            value={nick}
            onChange={(e) => setNick(e.target.value)}
            required
          />
          <input
            type="password"
            placeholder="Password (optional)"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
          />
          <button type="submit">Play Chess</button>
        </form>
      </div>
    </div>
  );
}

export default Login;