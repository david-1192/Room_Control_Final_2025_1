#!/usr/bin/env python3
"""
Simple Timing Comparison Script for USB vs WiFi Communication
Lab 2 - Embedded Systems Course 4100901

This script opens two threads:
1. USB thread: Opens COM port and logs received packets with timestamps
2. WiFi thread: Opens TCP connection and logs received packets with timestamps

Students must analyze the logs and calculate timing differences manually.

Usage:
    python timing_comparison.py

Requirements:
    pip install pyserial
"""

import serial
import socket
import threading
import time
import sys

# Configuration - Modify these values according to your setup
USB_PORT = "COM3"          # Change to your ST-Link COM port
ESP_IP = "10.178.166.211"   # Change to your ESP01 IP address
ESP_PORT = 2323            # Telnet port for esp-link

def usb_thread():
    """Thread that monitors USB communication and logs packets with timestamps"""
    print(f"[USB] Starting USB monitoring on {USB_PORT}")
    
    try:
        ser = serial.Serial(USB_PORT, 115200, timeout=1)
        print(f"[USB] Connected to {USB_PORT}")
        
        while True:
            data = ser.readline()
            if data:
                timestamp = time.time()
                print(f"[USB] {timestamp:.6f}: {data.decode().strip()}")
            
    except serial.SerialException as e:
        print(f"[USB] Error: {e}")
    except Exception as e:
        print(f"[USB] Unexpected error: {e}")

def wifi_thread():
    """Thread that monitors WiFi communication and logs packets with timestamps"""
    print(f"[WiFi] Starting WiFi monitoring on {ESP_IP}:{ESP_PORT}")
    
    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((ESP_IP, ESP_PORT))
            print(f"[WiFi] Connected to {ESP_IP}:{ESP_PORT}")
            
            while True:
                data = sock.recv(1024)
                if data:
                    timestamp = time.time()
                    print(f"[WiFi] {timestamp:.6f}: {data.decode().strip()}")
                else:
                    break
                    
        except socket.timeout:
            print(f"[WiFi] Connection timeout, retrying...")
            time.sleep(1)
        except socket.error as e:
            print(f"[WiFi] Socket error: {e}, retrying...")
            time.sleep(1)
        except Exception as e:
            print(f"[WiFi] Unexpected error: {e}")
            time.sleep(1)
        finally:
            try:
                sock.close()
            except:
                pass

def main():
    """Main function that starts both monitoring threads"""
    print("=" * 60)
    print("USB vs WiFi Communication Monitor")
    print("Lab 2 - Embedded Systems Course 4100901")
    print("=" * 60)
    print(f"USB Port: {USB_PORT}")
    print(f"WiFi: {ESP_IP}:{ESP_PORT}")
    print()
    print("Instructions:")
    print("1. Send the same message via both USB and WiFi")
    print("2. Compare the timestamps to calculate latency difference")
    print("3. Press Ctrl+C to stop monitoring")
    print()
    
    # Start USB monitoring thread
    usb_t = threading.Thread(target=usb_thread, daemon=True)
    usb_t.start()
    
    # Start WiFi monitoring thread  
    wifi_t = threading.Thread(target=wifi_thread, daemon=True)
    wifi_t.start()
    
    try:
        # Keep main thread alive
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n\nStopping monitoring...")
        sys.exit(0)

if __name__ == "__main__":
    main()