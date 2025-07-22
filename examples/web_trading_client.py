#!/usr/bin/env python3
"""
Modern Web-based Trading Client
A FastAPI application with WebSocket support for real-time trading
Requires: pip install fastapi uvicorn websockets aiofiles
"""

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
import asyncio
import websockets
import json
from datetime import datetime
from typing import Dict, List
from collections import defaultdict
import uuid

app = FastAPI()

# Global state
class TradingState:
    def __init__(self):
        self.prices: Dict[str, float] = {}
        self.portfolios: Dict[str, Dict] = {}  # user_id -> portfolio
        self.market_open = False
        self.connected_clients: Dict[str, WebSocket] = {}
        self.market_ws = None
        self.lock = asyncio.Lock()

state = TradingState()

# HTML for the trading interface
HTML_CONTENT = """
<!DOCTYPE html>
<html>
<head>
    <title>Modern Trading Client</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0a0e27;
            color: #fff;
            line-height: 1.6;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 20px 0;
            border-bottom: 1px solid #1a1f3a;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 28px;
            font-weight: 600;
        }
        
        .market-status {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .status-indicator {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #dc3545;
        }
        
        .status-indicator.open {
            background: #28a745;
        }
        
        .portfolio-info {
            display: flex;
            gap: 30px;
            margin-bottom: 30px;
        }
        
        .info-card {
            background: #151a36;
            padding: 20px;
            border-radius: 12px;
            flex: 1;
        }
        
        .info-card h3 {
            font-size: 14px;
            color: #8b92b9;
            margin-bottom: 5px;
        }
        
        .info-card .value {
            font-size: 24px;
            font-weight: 600;
        }
        
        .trading-grid {
            display: grid;
            grid-template-columns: 2fr 1fr;
            gap: 20px;
        }
        
        .stocks-panel {
            background: #151a36;
            border-radius: 12px;
            padding: 20px;
        }
        
        .stocks-panel h2 {
            font-size: 18px;
            margin-bottom: 20px;
        }
        
        .stock-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 15px;
            background: #1d2340;
            border-radius: 8px;
            margin-bottom: 10px;
            cursor: pointer;
            transition: all 0.2s;
        }
        
        .stock-item:hover {
            background: #252a4a;
            transform: translateX(5px);
        }
        
        .stock-item.selected {
            background: #2d5dff;
        }
        
        .stock-info {
            display: flex;
            flex-direction: column;
        }
        
        .stock-symbol {
            font-size: 18px;
            font-weight: 600;
        }
        
        .stock-shares {
            font-size: 14px;
            color: #8b92b9;
        }
        
        .stock-price {
            text-align: right;
        }
        
        .price-value {
            font-size: 20px;
            font-weight: 600;
        }
        
        .price-change {
            font-size: 14px;
            color: #28a745;
        }
        
        .price-change.negative {
            color: #dc3545;
        }
        
        .trading-panel {
            background: #151a36;
            border-radius: 12px;
            padding: 20px;
        }
        
        .trading-panel h2 {
            font-size: 18px;
            margin-bottom: 20px;
        }
        
        .trade-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 20px;
        }
        
        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
        }
        
        .btn-buy {
            background: #28a745;
            color: white;
        }
        
        .btn-buy:hover {
            background: #218838;
        }
        
        .btn-sell {
            background: #dc3545;
            color: white;
        }
        
        .btn-sell:hover {
            background: #c82333;
        }
        
        .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        
        .quantity-input {
            width: 100%;
            padding: 12px;
            background: #1d2340;
            border: 1px solid #2d3458;
            border-radius: 8px;
            color: white;
            font-size: 16px;
            margin-bottom: 20px;
        }
        
        .quantity-input:focus {
            outline: none;
            border-color: #2d5dff;
        }
        
        .order-history {
            margin-top: 30px;
        }
        
        .order-history h3 {
            font-size: 16px;
            margin-bottom: 15px;
        }
        
        .order-item {
            padding: 10px;
            background: #1d2340;
            border-radius: 6px;
            margin-bottom: 8px;
            font-size: 14px;
            display: flex;
            justify-content: space-between;
        }
        
        .order-time {
            color: #8b92b9;
        }
        
        .toast {
            position: fixed;
            bottom: 20px;
            right: 20px;
            padding: 16px 24px;
            background: #28a745;
            color: white;
            border-radius: 8px;
            display: none;
            animation: slideIn 0.3s ease;
        }
        
        .toast.error {
            background: #dc3545;
        }
        
        @keyframes slideIn {
            from {
                transform: translateX(100%);
            }
            to {
                transform: translateX(0);
            }
        }
        
        @media (max-width: 768px) {
            .trading-grid {
                grid-template-columns: 1fr;
            }
            
            .portfolio-info {
                flex-direction: column;
                gap: 15px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Modern Trading Client</h1>
            <div class="market-status">
                <span>Market Status:</span>
                <div class="status-indicator" id="marketStatus"></div>
                <span id="marketStatusText">Closed</span>
            </div>
        </div>
        
        <div class="portfolio-info">
            <div class="info-card">
                <h3>Cash Balance</h3>
                <div class="value" id="cashBalance">$10,000.00</div>
            </div>
            <div class="info-card">
                <h3>Portfolio Value</h3>
                <div class="value" id="portfolioValue">$0.00</div>
            </div>
            <div class="info-card">
                <h3>Total Value</h3>
                <div class="value" id="totalValue">$10,000.00</div>
            </div>
        </div>
        
        <div class="trading-grid">
            <div class="stocks-panel">
                <h2>Stocks</h2>
                <div id="stocksList"></div>
            </div>
            
            <div class="trading-panel">
                <h2>Trade</h2>
                <div id="selectedStock" style="margin-bottom: 20px; font-size: 18px;">Select a stock</div>
                <div class="trade-buttons">
                    <button class="btn btn-buy" id="buyBtn" disabled>Buy</button>
                    <button class="btn btn-sell" id="sellBtn" disabled>Sell</button>
                </div>
                <input type="number" class="quantity-input" id="quantityInput" placeholder="Enter quantity" min="1">
                
                <div class="order-history">
                    <h3>Recent Orders</h3>
                    <div id="orderHistory"></div>
                </div>
            </div>
        </div>
    </div>
    
    <div class="toast" id="toast"></div>
    
    <script>
        let ws;
        let selectedSymbol = null;
        let portfolio = {};
        let cash = 10000;
        let prices = {};
        let marketOpen = false;
        
        function connectWebSocket() {
            ws = new WebSocket(`ws://${window.location.host}/ws`);
            
            ws.onopen = () => {
                console.log('Connected to trading server');
                showToast('Connected to trading server', false);
            };
            
            ws.onmessage = (event) => {
                const data = JSON.parse(event.data);
                handleMessage(data);
            };
            
            ws.onclose = () => {
                console.log('Disconnected from server');
                showToast('Disconnected from server', true);
                setTimeout(connectWebSocket, 5000);
            };
            
            ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                showToast('Connection error', true);
            };
        }
        
        function handleMessage(data) {
            switch (data.type) {
                case 'state_update':
                    updateState(data);
                    break;
                case 'price_update':
                    updatePrice(data.symbol, data.price);
                    break;
                case 'order_confirmation':
                    handleOrderConfirmation(data);
                    break;
                case 'error':
                    showToast(data.message, true);
                    break;
            }
        }
        
        function updateState(data) {
            portfolio = data.portfolio || {};
            cash = data.cash || 10000;
            prices = data.prices || {};
            marketOpen = data.market_open || false;
            
            updateUI();
        }
        
        function updatePrice(symbol, price) {
            prices[symbol] = price;
            updateStockDisplay();
            updatePortfolioValues();
        }
        
        function updateUI() {
            updateMarketStatus();
            updateStockDisplay();
            updatePortfolioValues();
            updateTradeButtons();
        }
        
        function updateMarketStatus() {
            const indicator = document.getElementById('marketStatus');
            const text = document.getElementById('marketStatusText');
            
            if (marketOpen) {
                indicator.classList.add('open');
                text.textContent = 'Open';
            } else {
                indicator.classList.remove('open');
                text.textContent = 'Closed';
            }
        }
        
        function updateStockDisplay() {
            const stocksList = document.getElementById('stocksList');
            stocksList.innerHTML = '';
            
            Object.keys(prices).sort().forEach(symbol => {
                const price = prices[symbol];
                const shares = portfolio[symbol] || 0;
                const value = price * shares;
                
                const stockItem = document.createElement('div');
                stockItem.className = 'stock-item';
                if (symbol === selectedSymbol) {
                    stockItem.classList.add('selected');
                }
                
                stockItem.innerHTML = `
                    <div class="stock-info">
                        <div class="stock-symbol">${symbol}</div>
                        <div class="stock-shares">${shares} shares</div>
                    </div>
                    <div class="stock-price">
                        <div class="price-value">$${price.toFixed(2)}</div>
                        ${shares > 0 ? `<div class="stock-shares">Value: $${value.toFixed(2)}</div>` : ''}
                    </div>
                `;
                
                stockItem.onclick = () => selectStock(symbol);
                stocksList.appendChild(stockItem);
            });
        }
        
        function updatePortfolioValues() {
            const portfolioValue = Object.keys(portfolio).reduce((total, symbol) => {
                return total + (prices[symbol] || 0) * (portfolio[symbol] || 0);
            }, 0);
            
            document.getElementById('cashBalance').textContent = `$${cash.toFixed(2)}`;
            document.getElementById('portfolioValue').textContent = `$${portfolioValue.toFixed(2)}`;
            document.getElementById('totalValue').textContent = `$${(cash + portfolioValue).toFixed(2)}`;
        }
        
        function selectStock(symbol) {
            selectedSymbol = symbol;
            document.getElementById('selectedStock').textContent = `${symbol} - $${prices[symbol].toFixed(2)}`;
            updateStockDisplay();
            updateTradeButtons();
        }
        
        function updateTradeButtons() {
            const buyBtn = document.getElementById('buyBtn');
            const sellBtn = document.getElementById('sellBtn');
            
            buyBtn.disabled = !selectedSymbol || !marketOpen;
            sellBtn.disabled = !selectedSymbol || !marketOpen || !portfolio[selectedSymbol];
        }
        
        function executeTrade(action) {
            const quantity = parseInt(document.getElementById('quantityInput').value);
            
            if (!quantity || quantity <= 0) {
                showToast('Please enter a valid quantity', true);
                return;
            }
            
            if (!selectedSymbol) {
                showToast('Please select a stock', true);
                return;
            }
            
            ws.send(JSON.stringify({
                type: 'order',
                action: action,
                symbol: selectedSymbol,
                quantity: quantity
            }));
            
            document.getElementById('quantityInput').value = '';
        }
        
        function handleOrderConfirmation(data) {
            const orderHistory = document.getElementById('orderHistory');
            const orderItem = document.createElement('div');
            orderItem.className = 'order-item';
            
            const time = new Date().toLocaleTimeString();
            orderItem.innerHTML = `
                <span>${data.action === 'buy' ? 'Bought' : 'Sold'} ${data.quantity} ${data.symbol} @ $${data.price.toFixed(2)}</span>
                <span class="order-time">${time}</span>
            `;
            
            orderHistory.insertBefore(orderItem, orderHistory.firstChild);
            
            // Keep only last 10 orders
            while (orderHistory.children.length > 10) {
                orderHistory.removeChild(orderHistory.lastChild);
            }
            
            showToast(`Order executed: ${data.action} ${data.quantity} ${data.symbol}`, false);
        }
        
        function showToast(message, isError) {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.className = isError ? 'toast error' : 'toast';
            toast.style.display = 'block';
            
            setTimeout(() => {
                toast.style.display = 'none';
            }, 3000);
        }
        
        // Event listeners
        document.getElementById('buyBtn').onclick = () => executeTrade('buy');
        document.getElementById('sellBtn').onclick = () => executeTrade('sell');
        
        document.getElementById('quantityInput').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                if (marketOpen && selectedSymbol) {
                    executeTrade('buy');
                }
            }
        });
        
        // Connect to WebSocket
        connectWebSocket();
    </script>
</body>
</html>
"""

@app.get("/")
async def get_index():
    return HTMLResponse(content=HTML_CONTENT)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_id = str(uuid.uuid4())
    
    async with state.lock:
        state.connected_clients[client_id] = websocket
        
        # Initialize portfolio for new client
        if client_id not in state.portfolios:
            state.portfolios[client_id] = {
                "cash": 10000.0,
                "holdings": {}
            }
    
    try:
        # Send initial state
        await send_state_update(websocket, client_id)
        
        # Handle client messages
        while True:
            data = await websocket.receive_json()
            await handle_client_message(websocket, client_id, data)
            
    except WebSocketDisconnect:
        async with state.lock:
            del state.connected_clients[client_id]
    except Exception as e:
        print(f"Error handling client {client_id}: {e}")
        async with state.lock:
            if client_id in state.connected_clients:
                del state.connected_clients[client_id]

async def send_state_update(websocket: WebSocket, client_id: str):
    """Send complete state update to a client"""
    async with state.lock:
        portfolio = state.portfolios[client_id]
        await websocket.send_json({
            "type": "state_update",
            "portfolio": portfolio["holdings"],
            "cash": portfolio["cash"],
            "prices": state.prices,
            "market_open": state.market_open
        })

async def handle_client_message(websocket: WebSocket, client_id: str, data: dict):
    """Handle messages from trading clients"""
    if data["type"] == "order":
        await process_order(websocket, client_id, data)

async def process_order(websocket: WebSocket, client_id: str, order: dict):
    """Process a trading order"""
    async with state.lock:
        portfolio = state.portfolios[client_id]
        symbol = order["symbol"]
        quantity = order["quantity"]
        action = order["action"]
        
        if symbol not in state.prices:
            await websocket.send_json({
                "type": "error",
                "message": f"Unknown symbol: {symbol}"
            })
            return
        
        price = state.prices[symbol]
        
        if action == "buy":
            cost = price * quantity
            if portfolio["cash"] >= cost:
                portfolio["cash"] -= cost
                if symbol not in portfolio["holdings"]:
                    portfolio["holdings"][symbol] = 0
                portfolio["holdings"][symbol] += quantity
                
                await websocket.send_json({
                    "type": "order_confirmation",
                    "action": "buy",
                    "symbol": symbol,
                    "quantity": quantity,
                    "price": price
                })
                
                # Send order_placed event to market data server
                await send_order_to_market("buy", symbol, quantity, price)
            else:
                await websocket.send_json({
                    "type": "error",
                    "message": "Insufficient funds"
                })
                
        elif action == "sell":
            if symbol in portfolio["holdings"] and portfolio["holdings"][symbol] >= quantity:
                portfolio["holdings"][symbol] -= quantity
                if portfolio["holdings"][symbol] == 0:
                    del portfolio["holdings"][symbol]
                portfolio["cash"] += price * quantity
                
                await websocket.send_json({
                    "type": "order_confirmation",
                    "action": "sell",
                    "symbol": symbol,
                    "quantity": quantity,
                    "price": price
                })
                
                # Send order_placed event to market data server
                await send_order_to_market("sell", symbol, quantity, price)
            else:
                await websocket.send_json({
                    "type": "error",
                    "message": "Insufficient shares"
                })
    
    # Send updated state
    await send_state_update(websocket, client_id)

async def send_order_to_market(action: str, symbol: str, quantity: int, price: float):
    """Send order_placed event to the market data server"""
    if state.market_ws and not state.market_ws.closed:
        try:
            order_id = f"{action.upper()}_{symbol}_{int(datetime.now().timestamp())}"
            message = {
                "type": "order_placed",
                "order_id": order_id,
                "symbol": symbol,
                "price": price,
                "quantity": quantity,
                "action": action,
                "timestamp": datetime.now().isoformat()
            }
            await state.market_ws.send(json.dumps(message))
            print(f"Sent order_placed event to market server: {action} {quantity} {symbol} @ ${price}")
        except Exception as e:
            print(f"Failed to send order to market server: {e}")

async def connect_to_market():
    """Connect to the market data WebSocket server"""
    uri = "ws://localhost:8080/market"
    
    while True:
        try:
            print(f"Attempting to connect to market data server at {uri}")
            async with websockets.connect(uri) as websocket:
                state.market_ws = websocket
                print(f"Successfully connected to market data server")
                
                try:
                    async for message in websocket:
                        await handle_market_message(message)
                except websockets.exceptions.ConnectionClosed:
                    print("Market data connection closed")
                    
        except Exception as e:
            print(f"Market connection error: {e}")
            state.market_ws = None
            await asyncio.sleep(5)

async def handle_market_message(message: str):
    """Handle messages from the market data server"""
    try:
        data = json.loads(message)
        
        async with state.lock:
            if data["type"] == "market_open":
                state.market_open = True
                await broadcast_to_clients({
                    "type": "state_update",
                    "market_open": True
                })
                
            elif data["type"] == "market_close":
                state.market_open = False
                await broadcast_to_clients({
                    "type": "state_update",
                    "market_open": False
                })
                
            elif data["type"] == "price_update":
                symbol = data["symbol"]
                price = data["price"]
                state.prices[symbol] = price
                
                await broadcast_to_clients({
                    "type": "price_update",
                    "symbol": symbol,
                    "price": price
                })
                
    except json.JSONDecodeError:
        print(f"Failed to parse market message: {message}")

async def broadcast_to_clients(message: dict):
    """Broadcast a message to all connected clients"""
    disconnected = []
    
    for client_id, websocket in state.connected_clients.items():
        try:
            await websocket.send_json(message)
        except:
            disconnected.append(client_id)
    
    # Remove disconnected clients
    for client_id in disconnected:
        del state.connected_clients[client_id]

@app.on_event("startup")
async def startup_event():
    """Start the market data connection"""
    asyncio.create_task(connect_to_market())

if __name__ == "__main__":
    import uvicorn
    print("Starting Modern Trading Client on http://localhost:8000")
    print("Make sure the market data server is running on ws://localhost:8080/market")
    uvicorn.run(app, host="0.0.0.0", port=8000)