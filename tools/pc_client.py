#!/usr/bin/env python3
"""
LoRa-Link PC Client
Simple CLI tool for testing LoRa communication
"""

import serial
import time
import sys
import argparse
from datetime import datetime

class LoRaClient:
    def __init__(self, port, baud=921600):
        self.port = port
        self.baud = baud
        self.ser = None
        self.stats = {
            'tx': 0,
            'rx': 0,
            'errors': 0,
            'start_time': time.time()
        }
    
    def connect(self):
        """Connect to serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            time.sleep(2)  # Wait for Arduino reset
            print(f"✓ Connected to {self.port} at {self.baud} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def send_command(self, cmd):
        """Send command to device"""
        if not self.ser:
            print("✗ Not connected")
            return False
        
        try:
            self.ser.write((cmd + '\n').encode())
            self.stats['tx'] += 1
            return True
        except Exception as e:
            print(f"✗ Send error: {e}")
            self.stats['errors'] += 1
            return False
    
    def read_response(self, timeout=2.0):
        """Read response from device"""
        if not self.ser:
            return []
        
        lines = []
        start = time.time()
        
        while time.time() - start < timeout:
            if self.ser.in_waiting:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        lines.append(line)
                        self.stats['rx'] += 1
                        print(f"[{datetime.now().strftime('%H:%M:%S')}] {line}")
                except Exception as e:
                    self.stats['errors'] += 1
        
        return lines
    
    def interactive_mode(self):
        """Interactive command mode"""
        print("\n=== LoRa-Link Interactive Mode ===")
        print("Commands:")
        print("  ping          - Send PING")
        print("  send <text>   - Send message")
        print("  profile <n>   - Switch profile")
        print("  stats         - Show statistics")
        print("  rssi          - Show signal quality")
        print("  quit/exit     - Exit")
        print("===================================\n")
        
        while True:
            try:
                cmd = input("> ").strip()
                
                if cmd.lower() in ['quit', 'exit']:
                    break
                
                if cmd == 'stats':
                    self.show_stats()
                    continue
                
                if cmd:
                    self.send_command(cmd)
                    self.read_response(timeout=2.0)
                
            except KeyboardInterrupt:
                print("\n\nInterrupted by user")
                break
            except Exception as e:
                print(f"Error: {e}")
    
    def show_stats(self):
        """Show statistics"""
        uptime = time.time() - self.stats['start_time']
        print(f"\n=== Statistics ===")
        print(f"Uptime: {uptime:.0f}s")
        print(f"TX: {self.stats['tx']}")
        print(f"RX: {self.stats['rx']}")
        print(f"Errors: {self.stats['errors']}")
        print(f"==================\n")
    
    def close(self):
        """Close serial connection"""
        if self.ser:
            self.ser.close()
            print("✓ Connection closed")

def main():
    parser = argparse.ArgumentParser(description='LoRa-Link PC Client')
    parser.add_argument('--port', default='COM17', help='Serial port (default: COM17)')
    parser.add_argument('--baud', type=int, default=921600, help='Baud rate (default: 921600)')
    parser.add_argument('--cmd', help='Single command to execute')
    
    args = parser.parse_args()
    
    client = LoRaClient(args.port, args.baud)
    
    if not client.connect():
        return 1
    
    try:
        if args.cmd:
            # Single command mode
            client.send_command(args.cmd)
            client.read_response(timeout=3.0)
        else:
            # Interactive mode
            client.interactive_mode()
    finally:
        client.show_stats()
        client.close()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
