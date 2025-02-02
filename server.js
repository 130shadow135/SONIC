const fs = require('fs');
const https = require('https');
const WebSocket = require('ws');

// Charger les certificats SSL
const server = https.createServer({
  cert: fs.readFileSync('/path/to/cert.pem'),
  key: fs.readFileSync('/path/to/key.pem')
});

// Créer un serveur WebSocket sécurisé
const wss = new WebSocket.Server({ server });

wss.on('connection', function connection(ws) {
  console.log('Un client s\'est connecté');

  ws.on('message', function incoming(message) {
    console.log('Message reçu du client: %s', message);
    ws.send('Données reçues');
  });

  ws.send('Bienvenue sur le serveur WebSocket sécurisé !');
});

server.listen(8080, () => {
  console.log('Serveur WebSocket sécurisé en écoute sur wss://localhost:8080');
});
