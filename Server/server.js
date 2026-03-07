const http = require('http')
const server = http.createServer();
const WebSocket = require('ws');
const wss = new WebSocket.Server({ server });

const streamers = new Map();    // device -> {conn, username};
const clients   = new Map();    // id     -> {conn, username};
let nextId      = 1;

server.listen(57778, () => {
    console.log('[Server] Listening on port 57778');
});

wss.on('connection', (conn) =>{
    console.log('[server] New Connection');

    conn.on('message', (data) => {
        const msg = JSON.parse(data);
        console.log('[server] type=' + msg.type);

        if(msg.type === 'streamer:register')
        {
            streamers.set(msg.device, {conn, username: msg.username});
            console.log('[server] Streamer registered device=' + msg.device);
            send(conn, {type: 'server:registered'});
        }

        if(msg.type === 'client:register')
        {
            const id = nextId++;
            clients.set(id, {conn, username: msg.username});
            conn._clientId = id;
            console.log('[server] CLient registred: id=' + id);
            send(conn, {type: 'server:registered', clientId: id});

            // send current camera list to the client
            const list = [];
            streamers.forEach((v, device) => list.push({device, username: v.username}));
            send(conn, {type: 'camera:list', cameras: list});
        }

        if(msg.type === 'client:request_stream')
        {
            const streamer = streamers.get(msg.device);
            if(streamer)
            {
                console.log('[server] Client ' + conn._clientId + ' requested ' + msg.device);
                send(streamer.conn, {
                    type:       'signal',
                    device:     msg.device,
                    clientId:   conn._clientId
                });
            }
        }
    });

    conn.on('close', () => {
        console.log('[server] Connection closed');
    })
});


// helper function to send JSON
function send(conn, obj)
{
    conn.send(JSON.stringify(obj));
}