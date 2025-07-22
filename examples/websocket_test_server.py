#!/usr/bin/env python3
"""
Simple WebSocket server for testing the trading system example.
Requires: pip install websockets
"""

import asyncio
import websockets
import json
import random
import datetime
from typing import Set

# Global set to track all connected clients
connected_clients = set()

# Global market prices
prices = {}

async def broadcast_message(message: str, sender=None):
    """Broadcast a message to all connected clients except the sender"""
    if connected_clients:
        print(f"Broadcasting to {len(connected_clients)} clients")
        # Create list of tasks for sending to all clients
        disconnected = []
        for client in connected_clients:
            if client != sender:
                try:
                    await client.send(message)
                except websockets.exceptions.ConnectionClosed:
                    disconnected.append(client)
                except Exception as e:
                    print(f"Error sending to client: {e}")
                    disconnected.append(client)
        
        # Remove disconnected clients
        for client in disconnected:
            connected_clients.discard(client)

async def market_simulator(websocket):
    """Simulate market data feed"""
    print(f"Client connected from {websocket.remote_address}")
    connected_clients.add(websocket)
    print(f"Total active clients: {len(connected_clients)}")
    
    try:
        # Send market open to new client
        await websocket.send(json.dumps({
            "type": "market_open",
            "timestamp": datetime.datetime.now().isoformat()
        }))
        print("Sent: market_open to new client")
        
        # Just listen for incoming messages from this client
        async for message in websocket:
            try:
                data = json.loads(message)
                print(f"Received from client: {data.get('type', 'unknown')}")
                
                # Broadcast order_placed events to all clients
                if data.get('type') == 'order_placed':
                    await broadcast_message(message, sender=None)  # Send to all including sender
                    print(f"Broadcasted order: {data.get('symbol')} x{data.get('quantity')}")
                    
            except json.JSONDecodeError:
                print(f"Failed to parse message: {message}")
            except Exception as e:
                print(f"Error handling message: {e}")
            
    except websockets.exceptions.ConnectionClosed:
        print(f"Client disconnected from {websocket.remote_address}")
    except Exception as e:
        print(f"Error with client {websocket.remote_address}: {e}")
    finally:
        connected_clients.discard(websocket)
        print(f"Active clients: {len(connected_clients)}")

async def price_generator():
    """Generate price updates independently of client connections"""
    global prices
    symbols = ["AAPL", "GOOGL", "MSFT", "AMZN", "TSLA"]
    
    while True:
        await asyncio.sleep(random.uniform(0.5, 2))
        
        if connected_clients:  # Only generate if clients are connected
            # Update a random price
            symbol = random.choice(symbols)
            change = random.uniform(-2, 2)
            prices[symbol] = max(10, prices[symbol] + change)
            
            message = {
                "type": "price_update",
                "symbol": symbol,
                "price": round(prices[symbol], 2),
                "timestamp": datetime.datetime.now().isoformat()
            }
            
            message_str = json.dumps(message)
            await broadcast_message(message_str)
            print(f"Broadcast: price_update - {symbol} @ ${message['price']}")

async def main():
    print("Starting WebSocket server on ws://localhost:8080/market")
    print("Press Ctrl+C to stop")
    
    # Initialize prices
    global prices
    prices = {"AAPL": random.uniform(100, 300), 
              "GOOGL": random.uniform(100, 300),
              "MSFT": random.uniform(100, 300),
              "AMZN": random.uniform(100, 300),
              "TSLA": random.uniform(100, 300)}
    
    # Start the price generator task
    price_task = asyncio.create_task(price_generator())
    
    # Server configuration to ensure multiple connections are supported
    async with websockets.serve(
        market_simulator, 
        "localhost", 
        8080,
        max_size=10**6,  # 1MB max message size
        max_queue=100    # Queue up to 100 messages per client
    ):
        await asyncio.gather(price_task, asyncio.Future())  # run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped")