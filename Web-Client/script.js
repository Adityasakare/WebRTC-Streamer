const peerConnections = {};  
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
        const device = msg.device;
        const pc = createPeerConnection(device);

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
        const pc = peerConnections[device];
        if (pc && msg.data) 
        {
            pc.addIceCandidate(new RTCIceCandidate(msg.data));
        }
    }

}

function requestStream(device)
{
    log('Requesting stream: ' + device);
    ws.send(JSON.stringify({type: 'client:request_stream', device}));
}

function createPeerConnection(device) 
{
    
    if (peerConnections[device]) {
        peerConnections[device].close();
    }

    const pc = new RTCPeerConnection({
        iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    peerConnections[device] = pc;

    // Create video element for this camera if it doesn't exist
    let videoEl = document.getElementById('video_' + device.replace('/dev/', ''));
    if (!videoEl) {
        const container = document.createElement('div');
        container.id = 'container_' + device.replace('/dev/', '');

        const label = document.createElement('p');
        label.innerText = device;

        videoEl = document.createElement('video');
        videoEl.id = 'video_' + device.replace('/dev/', '');
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
        log('Stream playing: ' + device);
    };

    pc.onicecandidate = (event) => {
        if (event.candidate) 
        {
            console.log('Sending ICE candidate', event.candidate);
            ws.send(JSON.stringify({
                type:   'client:ice',
                device: device,
                data:   event.candidate
            }));
        }else {
        console.log('ICE gathering complete'); 
    }
    };
    return pc;
}
