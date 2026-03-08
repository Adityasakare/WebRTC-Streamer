const fs   = require('fs');
const path = require('path');
const url  = require('url');

const http = require('http');
const WebSocket = require('ws');


const streamers = new Map();    // device -> {conn, username};
const clients   = new Map();    // id     -> {conn, username};
let nextId      = 1;

const RECORDINGS_DIR = path.join(__dirname, '..', 'build', 'recordings');


const server = http.createServer((req, res) =>{
    // parse the URL
    const parsed = url.parse(req.url, true);
    const pathname = parsed.pathname;

    if(pathname === '/recordings')
    {
        const device = parsed.query.device;     // video0
        if(!device)
        {
            res.writeHead(400);
            res.end('Missing device');
            return;
        }

        if(!fs.existsSync(RECORDINGS_DIR))
        {
            res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*'});
            res.end(JSON.stringify([]));
            return;
        }
        // Read the recording direcory
        fs.readdir(RECORDINGS_DIR, (err, entries) => {
            if(err)
            {
                res.writeHead(500);
                res.end(err.message);
                return;
            }
            // filter to only device folder & parse it
            const sessions = entries
            .filter(e => e.startsWith(device + '_'))
            .map(e => {
                const m = e.match(/^(.+)_(\d{8})_(\d{6})$/); // regex - extract date amd time from folder 
                if(!m) return null;
                return {
                    dir:    e,                                      // full folder name
                    device: m[1],                                   // 'video0;
                    start:  parseTimestamp(m[2], m[3]),             // miliseconds
                    startStr: m[2] + '_' + m[3],                    // '20260308_180609'
                    playlist: '/recordings/' + e + '/playlist.m3u8' 
                };
            })
            .filter(Boolean)
            .sort((a, b) => a.start - b.start);    // Sort by start time so oldest is first

        res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*'});
        res.end(JSON.stringify(sessions));
        });
        return;
    }

    // Route /playback -> GET /playback?device=video0&ts=20290308_130609
    if(pathname === '/playback')
    {
        const device = parsed.query.device;     // video0
        const ts = parsed.query.ts;             // 20260308_180600
        if(!device || !ts)
        {
            res.writeHead(400);
            res.end('missing params');
            return;
        }

        fs.readdir(RECORDINGS_DIR, (err, entries) => {
        if(err)
        {
            res.writeHead(500);
            res.end(Error.message);
            return;
        }

        const sessions = entries
              .filter(e => e.startsWith(device + '_'))
              .map(e => {
                const m =  e.match(/^(.+)_(\d{8})_(\d{6})$/);
                if(!m) return null;
                return { dir: e, start: parseTimestamp(m[2], m[3]) };
              })
              .filter(Boolean)
              .sort((a, b) => a.start - b.start);
              
              // convert to req timestamp
              const reqTs = parseTimestamp(ts.slice(0,8), ts.slice(9,15));
             
              // find correct session - started at or before req time 
              let match = null;
              for(const s of sessions)
              {
                if(s.start <= reqTs) 
                    match = s;
                else 
                    break;
              }

              if(!match)
              {
                res.writeHead(404, { 'Access-Control-Allow-Origin': '*' });
                res.end(JSON.stringify({ error: 'No recording found for that time' }));
                return;
              }

              // check that plyalist m3u8 exits on disk
              const playlistPath = RECORDINGS_DIR + '/' + match.dir + '/playlist.m3u8';
              if (!fs.existsSync(playlistPath)) 
              {
                res.writeHead(404, { 'Access-Control-Allow-Origin': '*' });
                res.end(JSON.stringify({ error: 'Playlist not ready yet' }));
                return;
              }

              // return URL browser should use to load
              res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' });
              res.end(JSON.stringify({
                playlist: '/recordings/' + match.dir + '/playlist.m3u8',
                session:  match.dir
              }));
        });  
        return;
    }

    // hls.js req actual .m3u8 and .ts files - serves them
    if(pathname.startsWith('/recordings'))
    {
        const relative = pathname.slice('/recordings/'.length);
        const fpath = RECORDINGS_DIR + '/' + relative;

        if (!fs.existsSync(fpath)) 
        { 
            res.writeHead(404); 
            res.end('not found'); 
            return; 
        }

        // set correct content-type per file extenstion
        const ext = path.extname(fpath);
        const contentType =
            ext === '.m3u8' ? 'application/vnd.apple.mpegurl' :
            ext === '.ts'   ? 'video/MP2T' :
            ext === '.mp4'  ? 'video/mp4'  : 'application/octet-stream';

        // handle range requests 
        const stat = fs.statSync(fpath);
        const range = req.headers.range;

        if (range) {
            const parts = range.replace(/bytes=/, '').split('-');
            const start = parseInt(parts[0], 10);
            const end   = parts[1] ? parseInt(parts[1], 10) : stat.size - 1;
            res.writeHead(206, {
                'Content-Range':  'bytes ' + start + '-' + end + '/' + stat.size,
                'Accept-Ranges':  'bytes',
                'Content-Length': end - start + 1,
                'Content-Type':   contentType,
                'Access-Control-Allow-Origin': '*'
            });
            fs.createReadStream(fpath, { start, end }).pipe(res);
        } else {
            res.writeHead(200, {
                'Content-Length': stat.size,
                'Content-Type':   contentType,
                'Accept-Ranges':  'bytes',
                'Access-Control-Allow-Origin': '*'
            });
            fs.createReadStream(fpath).pipe(res);
        }
        return;
    }

    // fallthrough
    res.writeHead(404, { 'Access-Control-Allow-Origin': '*' });
    res.end('not found: ' + pathname);

});

const wss = new WebSocket.Server({ server });

server.listen(57778, () => {
    console.log('[Server] Listening on port 57778');
    console.log('[Server] Recordings dir: ' + RECORDINGS_DIR);
});

wss.on('connection', (conn) =>{
    console.log('[server] New Connection');

    conn.on('message', (data) => {
        const msg = JSON.parse(data);
        console.log('[server] type=' + msg.type);

        // Routing the requests 
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

        if(msg.type === 'streamer:offer')
        {
            const client = clients.get(parseInt(msg.clientId));
            if(client)
            {
                send(client.conn, {
                    type:   'offer',
                    device: msg.device,
                    data:   msg.data
                });
            }
        }

        if(msg.type === 'client:answer')
        {
            const streamer = streamers.get(msg.device);
            if(streamer)
            {
                send(streamer.conn, {
                    type:       'sdp',
                    device:     msg.device,
                    clientId:   conn._clientId,
                    data:       msg.data
                });
            }
        }

        if(msg.type === 'streamer:ice')
        {
            const client = clients.get(parseInt(msg.clientId));
            if(client)
            {
                send(client.conn, {
                    type:   'ice',
                    device: msg.device,
                    data:   msg.data
                });
            }
        }

        if(msg.type === 'client:ice')
        {
            console.log('[server] client:ice — routing to streamer');
            const streamer = streamers.get(msg.device);
            if(streamer)
            {
                send(streamer.conn, {
                    type:       'ice',
                    device:     msg.device,
                    clientId:   conn._clientId,
                    data:       msg.data
                });
            }
        }
    });

    conn.on('close', () => {
        // cleanup
        clients.forEach((v, k)   => { 
            if (v.conn === conn) 
                clients.delete(k); 
            });
        
            streamers.forEach((v, k) => { 
            if (v.conn === conn) 
                streamers.delete(k); 
            });
        console.log('[server] Connection closed');
    })
});


// helper function to send JSON
function send(conn, obj)
{
    conn.send(JSON.stringify(obj));
}


function parseTimestamp(dateStr, timeStr) 
{
    // dateStr = '20260308'
    // timeStr = '180609'

    const Y  = dateStr.slice(0,4);
    const M = dateStr.slice(4,6);
    const D = dateStr.slice(6,8);
    const hh = timeStr.slice(0,2);
    const mm = timeStr.slice(2,4);
    const ss = timeStr.slice(4,6);

    return new Date(Y+'-'+M+'-'+D+'T'+hh+':'+mm+':'+ss).getTime();
}
