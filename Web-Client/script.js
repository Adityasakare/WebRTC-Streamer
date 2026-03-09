const peerConnections = {};  // Key: clientId + '_' + device
let ws = null;
let myClientId = null;

const status     = document.getElementById('status');
const loginDiv   = document.getElementById('login');
const camerasDiv  = document.getElementById('cameras');

function log(msg) {
    status.innerText = msg;
    console.log(msg);
}

document.getElementById('btnConnect').onclick = () => {
    const username = document.getElementById('username').value.trim();
    if (!username) { 
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

    if (msg.type === 'offer') 
    {
        const device = msg.device;
        // Use unique key per client per device
        const connKey = myClientId + '_' + device;
        const pc = createPeerConnection(connKey, device);

        pc.setRemoteDescription(new RTCSessionDescription(msg.data))
        .then(() => pc.createAnswer())
        .then(answer => pc.setLocalDescription(answer))
        .then(() => {
            ws.send(JSON.stringify({
                type:   'client:answer',
                device: device,
                data:   pc.localDescription
            }));
            log('Answer sent for ' + device);
        });
    }

    if (msg.type === 'ice') 
    {
        const device = msg.device;
        // Try all peer connections for this device
        Object.keys(peerConnections).forEach(key => {
            if (key.endsWith('_' + device)) {
                const pc = peerConnections[key];
                if (pc && msg.data) {
                    pc.addIceCandidate(new RTCIceCandidate(msg.data));
                }
            }
        });
    }

    if (msg.type === 'camera:list')
    {
        const cameras = msg.cameras;
        const list = document.getElementById('cameraList');
        list.innerHTML = '';
        
        cameras.forEach(camera => {
            const btn = document.createElement('button');
            btn.innerText = camera;
            btn.onclick = () => requestStream(camera);
            list.appendChild(btn);
        });
        
        log('Found ' + cameras.length + ' cameras');
    }
}

function requestStream(device)
{
    log('Requesting stream: ' + device);
    ws.send(JSON.stringify({type: 'client:request_stream', device}));
}

function createPeerConnection(connKey, device) 
{
    // Close existing connection for this specific client+device
    if (peerConnections[connKey]) {
        peerConnections[connKey].close();
    }

    const pc = new RTCPeerConnection({
        iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    peerConnections[connKey] = pc;

    // Create video element for this camera if it doesn't exist
    let videoEl = document.getElementById('video_' + connKey.replace(/[\/\-]/g, '_'));
    if (!videoEl) {
        const container = document.createElement('div');
        container.id = 'container_' + connKey.replace(/[\/\-]/g, '_');

        const label = document.createElement('p');
        label.innerText = device + ' (' + myClientId + ')';

        videoEl = document.createElement('video');
        videoEl.id = 'video_' + connKey.replace(/[\/\-]/g, '_');
        videoEl.autoplay = true;
        videoEl.muted = true;
        videoEl.setAttribute('playsinline', '');
        videoEl.width = 640;
        videoEl.height = 480;
        videoEl.controls = true;

        container.appendChild(label);
        container.appendChild(videoEl);
        document.getElementById('streams').appendChild(container);
    }

    pc.ontrack = (event) => {
        videoEl.srcObject = event.streams[0];
        log('Stream playing: ' + device + ' for ' + myClientId);
    };

    pc.onicecandidate = (event) => {
        if (event.candidate) {
            console.log('Sending ICE candidate for ' + connKey);
            ws.send(JSON.stringify({
                type:   'client:ice',
                device: device,
                data:   event.candidate
            }));
        } else {
            console.log('ICE gathering complete for ' + connKey); 
        }
    };

    return pc;
}
