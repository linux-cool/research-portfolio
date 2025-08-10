#!/usr/bin/env python3
"""
Simple HTTP server for local development and testing.
Usage: python serve.py [port]
Default port: 8000
"""

import http.server
import socketserver
import sys
import os
import webbrowser
from threading import Timer

# Default port
PORT = 8000

# Parse command line arguments
if len(sys.argv) > 1:
    try:
        PORT = int(sys.argv[1])
    except ValueError:
        print(f"Invalid port number: {sys.argv[1]}")
        sys.exit(1)

# Change to the directory containing this script
os.chdir(os.path.dirname(os.path.abspath(__file__)))

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    """Custom handler to serve index.html for all paths that don't exist."""
    
    def end_headers(self):
        # Add CORS headers for local development
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()
    
    def do_GET(self):
        # Serve index.html for root path
        if self.path == '/':
            self.path = '/index.html'
        return super().do_GET()

def open_browser():
    """Open the default web browser after a short delay."""
    webbrowser.open(f'http://localhost:{PORT}')

# Start the server
try:
    with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
        print(f"🚀 Research Portfolio is running at:")
        print(f"   http://localhost:{PORT}")
        print(f"   http://127.0.0.1:{PORT}")
        print()
        print("📱 Test on mobile devices using your local IP:")
        print(f"   http://[your-local-ip]:{PORT}")
        print()
        print("⏹️  Press Ctrl+C to stop the server")
        print()
        
        # Open browser after 1 second
        Timer(1.0, open_browser).start()
        
        # Start serving
        httpd.serve_forever()
        
except KeyboardInterrupt:
    print("\n🛑 Server stopped by user")
except OSError as e:
    if e.errno == 48:  # Address already in use
        print(f"❌ Port {PORT} is already in use. Try a different port:")
        print(f"   python serve.py {PORT + 1}")
    else:
        print(f"❌ Error starting server: {e}")
    sys.exit(1)
