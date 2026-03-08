const peerConnections = {};  
let ws = null;
let myClientId = null;

const status     = document.getElementById('status');
const loginDiv   = document.getElementById('login');
const camerasDiv = document.getElementById('cameras');

// logger on site
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

// handling and routing the messages  
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

    if(msg.type === 'camera:list')
    {
        const list = document.getElementById('cameraList');
        list.innerHTML = '';
        msg.cameras.forEach(cam => {
        const btn = document.createElement('button');
        btn.innerText = 'Watch ' + cam.device;
        btn.onclick = () => requestStream(cam.device);
        list.appendChild(btn);

        const pbBtn = document.createElement('button');
        pbBtn.innerText = 'Playback ' + cam.device;
        pbBtn.style.marginLeft = '8px';
        pbBtn.onclick = () => openPlayback(cam.device);
        list.appendChild(pbBtn);
    });
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


// ==== PLAYBACK ====

let playbackDevice = null;
let hlsPlayer = null;

// open playback panel for a camera and load available recording sessions
function openPlayback(device)
{
    playbackDevice = device;

    document.getElementById('playbackPanel').style.display = 'block';
    document.getElementById('playbackDevice').textContent  = device;
    document.getElementById('playbackStatus').textContent  = 'Loading recordings...';

    const devShort = device.replace('/dev/', '');
    fetch('http://127.0.0.1:57778/recordings?device=' + devShort)
    .then(r => r.json())
    .then(sessions => {
        if(!sessions.length)
        {
            document.getElementById('playbackStatus').textContent = 'No recordings found';
            return;
        }

        // convert YYYYMMDD to YYYY-MM-DD fpr date input element
        const fmt  = s => s.slice(0,4) + '-' + s.slice(4,6) + '-' + s.slice(6,8);
        const dates = sessions.map(s => s.startStr.slice(0,8));
        const datePicker = document.getElementById('playbackDate');
        datePicker.min = fmt(dates[0]);
        datePicker.max = fmt(dates[dates.length - 1]);
        datePicker.value = fmt(dates[dates.length - 1]);
        document.getElementById('playbackStatus').textContent = sessions.length + ' session(s) available';
    })
    .catch(() => {
        document.getElementById('playbackStatus').textContent = 'Could not load recordings';
    });
}

// Read date+time picker, comm. time with server, play
function startPlayback()
{
    const dateVal = document.getElementById('playbackDate').value; 
    const timeVal = document.getElementById('playbackTime').value; 

    if (!dateVal || !timeVal) 
    {
        document.getElementById('playbackStatus').textContent = 'Select date and time';
        return;
    }

    // Build timestamp
    const ts = dateVal.replace(/-/g, '') + '_' + timeVal.replace(':', '') + '00';
    const devShort = playbackDevice.replace('/dev/', '');
    document.getElementById('playbackStatus').textContent = 'Searching...';

    fetch('http://127.0.0.1:57778/playback?device=' + devShort + '&ts=' + ts)
    .then(r => r.json())
    .then(data => {
        if(data.error)
        {
            document.getElementById('playbackStatus').textContent = data.error;
            return;
        }
        document.getElementById('playbackStatus').textContent = 'Playing: ' + data.session;
        playHLS('http://127.0.0.1:57778' + data.playlist);
    })
    .catch(() => {
        document.getElementById('playbackStatus').textContent = 'Request failed';
    });
} 

// hands playlist URL to hls.js which fetches segment and render video
function playHLS(playlistUrl)
{
    const video = document.getElementById('playbackVideo');

    if (hlsPlayer) 
    { 
        hlsPlayer.destroy(); 
        hlsPlayer = null; 
    }

    if(Hls.isSupported())
    {
        hlsPlayer = new Hls();
        hlsPlayer.loadSource(playlistUrl);
        hlsPlayer.attachMedia(video);
        hlsPlayer.on(Hls.Events.MANIFEST_PARSED, () => video.play());
        hlsPlayer.on(Hls.Events.ERROR, (e, data) => {
            if(data.fatal)
                document.getElementById('playbackStatus').textContent = 'HLS error: ' + data.type;
        });
    }else if (video.canPlayType('application/vnd.apple.mpegurl'))
    {
        video.src = playlistUrl;
        video.play();
    }
}

// Stops playback, destroys hls.js instance, hides panel
function closePlayback()
{
    if (hlsPlayer) 
    { 
        hlsPlayer.destroy(); 
        hlsPlayer = null; 
    }
    document.getElementById('playbackPanel').style.display = 'none';
    document.getElementById('playbackVideo').src = '';
    playbackDevice = null;
}