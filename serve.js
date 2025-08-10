#!/usr/bin/env node

/**
 * Simple HTTP server for local development and testing.
 * Usage: node serve.js [port]
 * Default port: 8000
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

// Default port
const PORT = process.argv[2] || 8000;

// MIME types for common file extensions
const mimeTypes = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.woff': 'font/woff',
    '.woff2': 'font/woff2',
    '.ttf': 'font/ttf',
    '.eot': 'application/vnd.ms-fontobject'
};

const server = http.createServer((req, res) => {
    // Parse URL and remove query parameters
    let filePath = req.url.split('?')[0];
    
    // Default to index.html for root path
    if (filePath === '/') {
        filePath = '/index.html';
    }
    
    // Construct full file path
    const fullPath = path.join(__dirname, filePath);
    
    // Get file extension
    const ext = path.extname(fullPath).toLowerCase();
    const contentType = mimeTypes[ext] || 'text/plain';
    
    // Add CORS headers for local development
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
    
    // Handle OPTIONS requests
    if (req.method === 'OPTIONS') {
        res.writeHead(200);
        res.end();
        return;
    }
    
    // Read and serve the file
    fs.readFile(fullPath, (err, data) => {
        if (err) {
            if (err.code === 'ENOENT') {
                // Try to serve index.html for client-side routing
                if (filePath !== '/index.html') {
                    fs.readFile(path.join(__dirname, 'index.html'), (err, data) => {
                        if (err) {
                            res.writeHead(404, { 'Content-Type': 'text/plain' });
                            res.end('404 Not Found');
                        } else {
                            res.writeHead(200, { 'Content-Type': 'text/html' });
                            res.end(data);
                        }
                    });
                } else {
                    res.writeHead(404, { 'Content-Type': 'text/plain' });
                    res.end('404 Not Found');
                }
            } else {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end('500 Internal Server Error');
            }
        } else {
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(data);
        }
    });
});

// Start the server
server.listen(PORT, (err) => {
    if (err) {
        console.error('❌ Error starting server:', err);
        process.exit(1);
    }
    
    console.log('🚀 Research Portfolio is running at:');
    console.log(`   http://localhost:${PORT}`);
    console.log(`   http://127.0.0.1:${PORT}`);
    console.log();
    console.log('📱 Test on mobile devices using your local IP:');
    console.log(`   http://[your-local-ip]:${PORT}`);
    console.log();
    console.log('⏹️  Press Ctrl+C to stop the server');
    console.log();
    
    // Try to open browser (cross-platform)
    setTimeout(() => {
        const url = `http://localhost:${PORT}`;
        const start = (process.platform === 'darwin' ? 'open' :
                      process.platform === 'win32' ? 'start' : 'xdg-open');
        exec(`${start} ${url}`, (err) => {
            if (err) {
                console.log(`🌐 Open ${url} in your browser manually`);
            }
        });
    }, 1000);
});

// Handle graceful shutdown
process.on('SIGINT', () => {
    console.log('\n🛑 Server stopped by user');
    process.exit(0);
});

process.on('uncaughtException', (err) => {
    if (err.code === 'EADDRINUSE') {
        console.log(`❌ Port ${PORT} is already in use. Try a different port:`);
        console.log(`   node serve.js ${PORT + 1}`);
    } else {
        console.error('❌ Uncaught Exception:', err);
    }
    process.exit(1);
});
