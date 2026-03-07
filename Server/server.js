const http = require('http')
const server = http.createServer();

server.listen(57778, () => {
    console.log('[Server] Listening on port 57778');
});
