#!/usr/bin/env python3
"""
Interactive Python Trading Client
Connects to the WebSocket market data server and allows manual trading via keyboard.
Requires: pip install websockets aioconsole blessed
"""

import asyncio
import websockets
import json
import datetime
from collections import defaultdict
from blessed import Terminal
import sys
from concurrent.futures import ThreadPoolExecutor
import threading

class TradingClient:
    def __init__(self):
        self.term = Terminal()
        self.prices = {}
        self.portfolio = defaultdict(int)
        self.cash = 10000.0
        self.orders = []
        self.market_open = False
        self.selected_symbol = 0
        self.symbols = []
        self.websocket = None
        self.running = True
        self.input_mode = "menu"  # menu, buy, sell
        self.input_buffer = ""
        self.lock = threading.Lock()
        self.last_draw_time = 0
        self.needs_redraw = True
        self.last_prices = {}
        
    async def connect_to_market(self):
        """Connect to the market data WebSocket server"""
        uri = "ws://localhost:8080/market"
        try:
            async with websockets.connect(uri) as websocket:
                self.websocket = websocket
                print(f"Connected to {uri}")
                
                # Handle incoming messages
                async for message in websocket:
                    await self.handle_market_message(message)
                    
        except Exception as e:
            print(f"Connection error: {e}")
            self.running = False
    
    async def handle_market_message(self, message):
        """Process incoming market data messages"""
        try:
            data = json.loads(message)
            
            with self.lock:
                if data["type"] == "market_open":
                    self.market_open = True
                    
                elif data["type"] == "market_close":
                    self.market_open = False
                    
                elif data["type"] == "price_update":
                    symbol = data["symbol"]
                    new_price = data["price"]
                    
                    # Check if price actually changed
                    if symbol not in self.prices or abs(self.prices[symbol] - new_price) > 0.01:
                        self.prices[symbol] = new_price
                        self.needs_redraw = True
                        
                    if symbol not in self.symbols:
                        self.symbols.append(symbol)
                        self.symbols.sort()
                        self.needs_redraw = True
                        
                elif data["type"] == "order_placed":
                    # Could track order confirmations here
                    pass
                    
        except json.JSONDecodeError:
            print(f"Failed to parse message: {message}")
    
    async def send_order(self, order_type, symbol, quantity):
        """Send an order to the server"""
        if self.websocket and not self.websocket.closed:
            order = {
                "type": "order",
                "order_type": order_type,
                "symbol": symbol,
                "quantity": quantity,
                "price": self.prices.get(symbol, 0),
                "timestamp": datetime.datetime.now().isoformat()
            }
            
            with self.lock:
                if order_type == "buy":
                    cost = order["price"] * quantity
                    if self.cash >= cost:
                        self.cash -= cost
                        self.portfolio[symbol] += quantity
                        self.orders.append(f"Bought {quantity} {symbol} @ ${order['price']:.2f}")
                    else:
                        self.orders.append(f"Insufficient funds for {quantity} {symbol}")
                        return
                elif order_type == "sell":
                    if self.portfolio[symbol] >= quantity:
                        self.portfolio[symbol] -= quantity
                        self.cash += order["price"] * quantity
                        self.orders.append(f"Sold {quantity} {symbol} @ ${order['price']:.2f}")
                    else:
                        self.orders.append(f"Insufficient shares of {symbol}")
                        return
            
            await self.websocket.send(json.dumps(order))
    
    def draw_ui(self):
        """Draw the trading interface"""
        with self.term.fullscreen(), self.term.hidden_cursor():
            # Initial draw
            self._render_screen()
            
            while self.running:
                # Only redraw if needed or in input mode
                with self.lock:
                    if self.needs_redraw or self.input_mode != "menu":
                        self._render_screen()
                        self.needs_redraw = False
                
                # Small delay to prevent excessive CPU usage
                asyncio.run(asyncio.sleep(0.05))
    
    def _render_screen(self):
        """Render the screen content"""
        # Clear screen
        print(self.term.home + self.term.clear, end='')
        
        # Header
        print(self.term.bold("=== Python Trading Client ==="))
        print(f"Market: {'OPEN' if self.market_open else 'CLOSED'}")
        print(f"Cash: ${self.cash:.2f}")
        print()
        
        # Price display
        print(self.term.bold("Market Prices:"))
        for i, symbol in enumerate(self.symbols):
            price = self.prices.get(symbol, 0)
            shares = self.portfolio.get(symbol, 0)
            value = price * shares
            
            if i == self.selected_symbol and self.input_mode == "menu":
                print(self.term.reverse(f"  {symbol}: ${price:.2f} | Shares: {shares} | Value: ${value:.2f}"))
            else:
                print(f"  {symbol}: ${price:.2f} | Shares: {shares} | Value: ${value:.2f}")
        
        # Portfolio value
        print()
        total_value = self.cash + sum(self.prices.get(s, 0) * self.portfolio[s] for s in self.portfolio)
        print(f"Total Portfolio Value: ${total_value:.2f}")
        
        # Recent orders
        print()
        print(self.term.bold("Recent Orders:"))
        for order in self.orders[-5:]:
            print(f"  {order}")
        
        # Instructions
        print()
        print(self.term.bold("Controls:"))
        
        if self.input_mode == "menu":
            print("  ↑/↓ or j/k: Select symbol")
            print("  b: Buy shares")
            print("  s: Sell shares")
            print("  q: Quit")
        elif self.input_mode in ["buy", "sell"]:
            action = "Buy" if self.input_mode == "buy" else "Sell"
            if self.symbols:
                # Fixed input area that doesn't flicker
                print(f"\n  {self.term.bold(action + ' ' + self.symbols[self.selected_symbol])}")
                print(f"  Enter quantity: {self.term.bold(self.input_buffer)}{'_' if len(self.input_buffer) < 10 else ''}")
            print("\n  Enter: Confirm | ESC: Cancel")
    
    async def handle_input(self):
        """Handle keyboard input"""
        executor = ThreadPoolExecutor(max_workers=1)
        loop = asyncio.get_event_loop()
        
        while self.running:
            key = await loop.run_in_executor(executor, self.term.inkey, 0.1)
            
            if key:
                with self.lock:
                    if self.input_mode == "menu":
                        if key.name == 'KEY_UP' or key == 'k':
                            if self.symbols:
                                self.selected_symbol = (self.selected_symbol - 1) % len(self.symbols)
                                self.needs_redraw = True
                        elif key.name == 'KEY_DOWN' or key == 'j':
                            if self.symbols:
                                self.selected_symbol = (self.selected_symbol + 1) % len(self.symbols)
                                self.needs_redraw = True
                        elif key == 'b':
                            if self.symbols and self.market_open:
                                self.input_mode = "buy"
                                self.input_buffer = ""
                                self.needs_redraw = True
                        elif key == 's':
                            if self.symbols and self.market_open:
                                self.input_mode = "sell"
                                self.input_buffer = ""
                                self.needs_redraw = True
                        elif key == 'q':
                            self.running = False
                            
                    elif self.input_mode in ["buy", "sell"]:
                        if key.name == 'KEY_ESCAPE':
                            self.input_mode = "menu"
                            self.input_buffer = ""
                        elif key.name == 'KEY_ENTER':
                            try:
                                quantity = int(self.input_buffer)
                                if quantity > 0 and self.symbols:
                                    symbol = self.symbols[self.selected_symbol]
                                    await self.send_order(self.input_mode, symbol, quantity)
                            except ValueError:
                                self.orders.append("Invalid quantity")
                            self.input_mode = "menu"
                            self.input_buffer = ""
                        elif key.name == 'KEY_BACKSPACE':
                            self.input_buffer = self.input_buffer[:-1]
                        elif key.isdigit():
                            self.input_buffer += key
    
    async def run(self):
        """Run the trading client"""
        # Start UI in a separate thread
        ui_thread = threading.Thread(target=self.draw_ui)
        ui_thread.daemon = True
        ui_thread.start()
        
        # Run async tasks
        tasks = [
            asyncio.create_task(self.connect_to_market()),
            asyncio.create_task(self.handle_input())
        ]
        
        try:
            await asyncio.gather(*tasks)
        except KeyboardInterrupt:
            self.running = False
        finally:
            print(self.term.normal)
            print("\nTrading client stopped")

async def main():
    client = TradingClient()
    await client.run()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutdown requested")