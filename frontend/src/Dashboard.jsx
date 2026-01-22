import React, { useState, useEffect } from 'react';
import axios from 'axios';

// ✅ POINTING TO SAFE PORT 8090
const API_URL = "http://localhost:8090/api";

const Dashboard = ({ token }) => {
    const [seats, setSeats] = useState([]);
    const [user, setUser] = useState(null);
    const [status, setStatus] = useState("Loading...");
    const [selectedSeat, setSelectedSeat] = useState(null);

    // 1. Load User Profile & Seats on Mount
    useEffect(() => {
        // 🛠️ CRITICAL FIX: Fallback to LocalStorage if prop is lost on refresh
        const activeToken = token || localStorage.getItem('token');

        if (!activeToken) {
            setStatus("❌ No Token Found. Please Login again.");
            return;
        }

        const fetchData = async () => {
            try {
                // Get User Info
                const userRes = await axios.get(`${API_URL}/profile`, {
                    headers: { Authorization: activeToken }
                });
                setUser(userRes.data.user);

                // Get Seat Grid
                fetchSeats();
            } catch (err) {
                console.error("Profile Load Error:", err);
                setStatus("Failed to load profile. (Is Backend running on 8090?)");
            }
        };
        fetchData();
        
        // POLL: Auto-refresh seats every 2 seconds
        const interval = setInterval(fetchSeats, 2000);
        return () => clearInterval(interval);
    }, [token]);

    const fetchSeats = async () => {
        try {
            const res = await axios.get(`${API_URL}/seats`);
            setSeats(res.data);
            // Only update status if it's currently "Loading..." to avoid flickering
            setStatus(prev => prev.includes("Loading") ? "Live Updates Active 🟢" : prev);
        } catch (err) {
            console.error(err);
        }
    };

    // 2. STEP 1: RESERVE (The Redis Lock)
    const handleSeatClick = async (seat) => {
        // Prevent clicking if already booked or locked
        if (seat.status !== 'AVAILABLE') return;

        setStatus(`🔐 Locking Seat ${seat.label}...`);
        try {
            await axios.post(`${API_URL}/reserve`, {
                seat_id: seat.id
            });
            
            // Optimistic Update
            setSelectedSeat(seat);
            fetchSeats(); 
            setStatus(`✅ Seat ${seat.label} Reserved! 120s to pay.`);
        } catch (err) {
            // 🛠️ CRITICAL FIX: Handle 409 Conflict gracefully
            if (err.response && err.response.status === 409) {
                setStatus("⚠️ Seat was just taken by someone else!");
                fetchSeats(); // Refresh to see the new status
            } else {
                console.error(err);
                setStatus("❌ Reservation Failed");
            }
        }
    };

    // 3. STEP 2: PAY (The RabbitMQ Queue)
    const handlePay = async () => {
        if (!selectedSeat) return;

        setStatus("💸 Sending to Payment Queue...");
        try {
            await axios.post(`${API_URL}/pay`, {
                seat_id: selectedSeat.id
            });
            
            setStatus("🎉 Payment Processing! Ticket Generating...");
            setSelectedSeat(null); // Clear selection
            fetchSeats(); // Refresh grid
        } catch (err) {
            console.error(err);
            setStatus("❌ Payment Failed");
        }
    };

    // Helper: Determine Seat Color
    const getSeatColor = (seat) => {
        if (seat.status === 'BOOKED') return '#FF6347'; // Red
        // Check if *we* have it selected (Orange)
        if (selectedSeat && selectedSeat.id === seat.id) return 'orange';
        // Note: In a real app, backend would send "RESERVED" status if locked by others
        return '#90EE90'; // Green (Available)
    };

    return (
        <div style={{ padding: '20px', fontFamily: 'Arial', maxWidth: '600px', margin: '0 auto' }}>
            <h1>🎟️ TicketMaster Dashboard</h1>
            
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '20px', borderBottom: '1px solid #ccc', paddingBottom: '10px' }}>
                <h3>👤 {user || 'Guest'}</h3>
                <span style={{ fontSize: '14px', color: 'gray' }}>{status}</span>
                <button 
                    onClick={() => { localStorage.removeItem('token'); window.location.reload(); }}
                    style={{ padding: '5px 10px', background: '#dc3545', color: 'white', border: 'none', cursor: 'pointer' }}
                >
                    Logout
                </button>
            </div>

            {/* SEAT GRID */}
            <div style={{ 
                display: 'grid', 
                gridTemplateColumns: 'repeat(5, 1fr)', 
                gap: '10px', 
                marginBottom: '30px'
            }}>
                {seats.map(seat => (
                    <button
                        key={seat.id}
                        onClick={() => handleSeatClick(seat)}
                        disabled={seat.status === 'BOOKED'}
                        style={{
                            backgroundColor: getSeatColor(seat),
                            height: '50px',
                            border: '1px solid #333',
                            borderRadius: '5px',
                            cursor: seat.status === 'AVAILABLE' ? 'pointer' : 'not-allowed',
                            fontWeight: 'bold'
                        }}
                    >
                        {seat.label}
                    </button>
                ))}
            </div>

            {/* PAYMENT SECTION (Only shows when seat reserved) */}
            {selectedSeat && (
                <div style={{ 
                    padding: '20px', 
                    border: '2px dashed orange', 
                    textAlign: 'center',
                    backgroundColor: '#fff3e0'
                }}>
                    <h2>🔶 Reserve Confirmed: {selectedSeat.label}</h2>
                    <p>Price: $50.00</p>
                    <button 
                        onClick={handlePay}
                        style={{
                            padding: '12px 24px',
                            fontSize: '18px',
                            backgroundColor: '#28a745',
                            color: 'white',
                            border: 'none',
                            borderRadius: '5px',
                            cursor: 'pointer'
                        }}
                    >
                        💳 Confirm Payment
                    </button>
                </div>
            )}
        </div>
    );
};

export default Dashboard;