const fs = require('fs');
const path = require('path');
const https = require('https');
const httpProxy = require('http-proxy');

const targetWs = process.env.WS_TARGET || 'ws://127.0.0.1:18081';
const listenPort = Number(process.env.WSS_PORT || 18082);
const pfxPath = process.env.SSL_PFX_PATH || path.join(__dirname, 'certs', 'localhost.pfx');
const passphrase = process.env.SSL_PFX_PASSPHRASE || 'chess-dev-pass';

if (!fs.existsSync(pfxPath)) {
  console.error(`PFX certificate not found: ${pfxPath}`);
  console.error('Run: npm run generate:wss-cert');
  process.exit(1);
}

const proxy = httpProxy.createProxyServer({
  target: targetWs,
  ws: true,
  changeOrigin: true
});

proxy.on('error', (err, req, socket) => {
  console.error('Proxy error:', err.message);
  if (socket && typeof socket.end === 'function') socket.end();
});

const server = https.createServer(
  {
    pfx: fs.readFileSync(pfxPath),
    passphrase
  },
  (_req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Chess WSS proxy is running.\n');
  }
);

server.on('upgrade', (req, socket, head) => {
  proxy.ws(req, socket, head);
});

server.listen(listenPort, '0.0.0.0', () => {
  console.log(`WSS proxy listening on wss://localhost:${listenPort}`);
  console.log(`Forwarding websocket traffic to ${targetWs}`);
});
