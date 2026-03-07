const http = require('http')
const server = http.createServer();
const WebSocket = require('ws');
const wss = new WebSocket.Server({ server });

server.listen(57778, () => {
    console.log('[Server] Listening on port 57778');
});

wss.on('connection', (conn) =>{
    console.log('[server] New Connection');

    conn.on('message', (data) => {
        const msg = JSON.parse(data);
        console.log('[server] Connection closed');
    });
});

