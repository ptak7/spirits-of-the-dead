const net = require('net');
const stdin = process.openStdin();
const keypress = require('keypress');
const address = '192.168.8.106';
const port = 1252;

//emitt keypress events by stdin
keypress(process.stdin);

//create socket
let client = new net.Socket();

//connect to the socket with given address and port
client.connect(port, address, () => {
	console.log('Connected to the server ' + address + ':' + port + '...');
});

//listen for the keypress event
process.stdin.on('keypress', (ch, key) => {
  // console.log('got "keypress"', key);
  //console.log(key);
  client.write(JSON.stringify(key));
  if (key && key.ctrl && key.name == 'c') {
    process.stdin.pause();
    client.destroy();
  }
});

process.stdin.setRawMode(true);
process.stdin.resume();

client.on('data', (data) => {
	console.log('Received: ' + data);
});

client.on('close', () => {
	console.log('Connection closed');
});
