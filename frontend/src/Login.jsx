import React, { useState } from 'react';
import axios from 'axios';

// ✅ POINTING TO SAFE PORT 8090
const API_URL = "http://localhost:8090/api";

const Login = ({ setToken }) => {
    const [email, setEmail] = useState('');
    const [password, setPassword] = useState('');
    const [isSignup, setIsSignup] = useState(false);
    const [status, setStatus] = useState('');

    const handleSubmit = async (e) => {
        e.preventDefault();
        setStatus('Processing...');

        const endpoint = isSignup ? '/signup' : '/login';
        const payload = isSignup 
            ? { username: "User", email, password } 
            : { email, password };

        try {
            const res = await axios.post(`${API_URL}${endpoint}`, payload);

            if (isSignup) {
                setStatus('✅ Account Created! Please Login.');
                setIsSignup(false);
            } else {
                setStatus('✅ Login Successful!');
                const token = res.data.token;
                
                // 🛠️ CRITICAL FIX: Save token to Storage & State
                localStorage.setItem('token', token);
                setToken(token);
            }
        } catch (err) {
            console.error(err);
            setStatus('❌ Error: ' + (err.response?.data || "Server Offline"));
        }
    };

    return (
        <div style={{ maxWidth: '300px', margin: '50px auto', textAlign: 'center', fontFamily: 'Arial' }}>
            <h2>{isSignup ? '📝 Signup' : '🔐 Login'}</h2>
            <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
                <input 
                    type="email" 
                    placeholder="Email" 
                    value={email} 
                    onChange={e => setEmail(e.target.value)} 
                    required 
                    style={{ padding: '8px' }}
                />
                <input 
                    type="password" 
                    placeholder="Password" 
                    value={password} 
                    onChange={e => setPassword(e.target.value)} 
                    required 
                    style={{ padding: '8px' }}
                />
                <button type="submit" style={{ padding: '10px', cursor: 'pointer', backgroundColor: '#007BFF', color: 'white', border: 'none' }}>
                    {isSignup ? 'Sign Up' : 'Log In'}
                </button>
            </form>
            
            <p style={{ marginTop: '10px', color: status.includes('✅') ? 'green' : 'red' }}>{status}</p>
            
            <button 
                onClick={() => { setIsSignup(!isSignup); setStatus(''); }} 
                style={{ background: 'none', border: 'none', color: 'blue', cursor: 'pointer', textDecoration: 'underline' }}
            >
                {isSignup ? 'Already have an account? Login' : 'Need an account? Signup'}
            </button>
        </div>
    );
};

export default Login;