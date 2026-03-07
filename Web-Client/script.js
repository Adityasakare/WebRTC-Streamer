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
}
