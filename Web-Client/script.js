let pc = null;
let currentDevice = null;
let ws = null;
let myClientId = null;

const status     = document.getElementById('status');
const loginDiv   = document.getElementById('login');
const camerasDiv = document.getElementById('cameras');

function log(msg) {
    status.innerText = msg;
    console.log(msg);
}

document.getElementById('btnConnect').onclick = () => {
    const username = document.getElementById('username').value.trim();
    if (!username) 
    { 
        log('Enter a name'); 
        return; 
    }

    const host = window.location.hostname || '127.0.0.1';
    ws = new WebSocket('ws://' + host + ':57778');

    ws.onopen = () => {
        log('Connected');
        ws.send(JSON.stringify({ type: 'client:register', username }));
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        console.log('Received:', msg.type);
        handleMessage(msg);
    };

    ws.onclose = () => log('Disconnected');
};

function handleMessage(msg) 
{
    if (msg.type === 'server:registered') 
    {
        myClientId = msg.clientId;
        log('Registered as client ' + myClientId);
        loginDiv.style.display = 'none';
        camerasDiv.style.display = 'block';
    }

    if(msg.type === 'camera:list')
    {
        const list = document.getElementById('cameraList');
        list.innerHTML = '';
        msg.cameras.forEach(cam => {
            const btn = document.createElement('button');
            btn.innerText = 'Watch ' + cam.device;
            btn.onclick = () => requestStream(cam.device);
            list.appendChild(btn);
        });
    }

    if (msg.type === 'offer') 
    {
        currentDevice = msg.device;
        createPeerConnection();

        pc.setRemoteDescription(new RTCSessionDescription(msg.data))
        .then(() => pc.createAnswer())
        .then(answer => pc.setLocalDescription(answer))
        .then(() => {
          ws.send(JSON.stringify({
              type:   'client:answer',
              device: currentDevice,
              data:   pc.localDescription
          }));
          log('Answer sent');
      });
    }

    


}

function requestStream(device)
{
    log('Requesting stream: ' + device);
    ws.send(JSON.stringify({type: 'client:request_stream', device}));
}

function createPeerConnection() 
{
    pc = new RTCPeerConnection({
        iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    pc.ontrack = (event) => {
        const video = document.getElementById('video');
        video.srcObject = event.streams[0];
        document.getElementById('stream').style.display = 'block';
        log('Stream playing');
    };

    pc.onicecandidate = (event) => {
        if (event.candidate) 
        {
            ws.send(JSON.stringify({
                type:   'client:ice',
                device: currentDevice,
                data:   event.candidate
            }));
        }
    };
}
