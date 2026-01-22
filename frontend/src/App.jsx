import React, { useState, useEffect } from 'react';
import Login from './Login';
import Dashboard from './Dashboard';

function App() {
  // 1. Initialize Token from LocalStorage (so you stay logged in on refresh)
  const [token, setToken] = useState(localStorage.getItem('token'));

  return (
    <div>
      {/* 2. LOGIC: If no token, show Login. If token exists, show Dashboard. */}
      {!token ? (
        /* 🚨 THIS WAS MISSING: Passing the function down */
        <Login setToken={setToken} />
      ) : (
        <Dashboard token={token} />
      )}
    </div>
  );
}

export default App;