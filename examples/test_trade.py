#!/usr/bin/env python3
"""
Simple test script to send a trade order to the market server
"""

import asyncio
import websockets
import json
import datetime

async def send_test_trade():
    uri = "ws://localhost:8080/market"
    
    try:
        async with websockets.connect(uri) as websocket:
            print(f"Connected to {uri}")
            
            # Wait a moment to receive market open
            await asyncio.sleep(1)
            
            # Send a test order
            order = {
                "type": "order_placed",
                "order_id": "TEST_ORDER_001",
                "symbol": "AAPL",
                "price": 150.00,
                "quantity": 100,
                "action": "buy",
                "timestamp": datetime.datetime.now().isoformat()
            }
            
            print(f"Sending order: {order}")
            await websocket.send(json.dumps(order))
            print("Order sent successfully")
            
            # Keep connection open for a moment to see any responses
            await asyncio.sleep(2)
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    print("Test Trade Script - Sending one AAPL buy order")
    asyncio.run(send_test_trade())
    print("Done")