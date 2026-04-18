import process from 'node:process';

function respond(ok, exitCode, message) {
    console.log(JSON.stringify({ ok, runtime: 'javascript', exitCode, message }));
    process.exit(exitCode);
}

const raw = process.argv.at(2) ?? '';
if (!raw) {
    respond(true, 0, 'js worker ready');
}

let req;
try {
    req = JSON.parse(raw);
} catch (err) {
    respond(false, 2, `invalid json request: ${String(err)}`);
}

const command = typeof req.command === 'string' ? req.command : 'execute';
const prompt = typeof req.prompt === 'string' ? req.prompt : '';

if (command === 'ping') {
    respond(true, 0, 'pong from javascript worker');
}
if (command === 'echo') {
    respond(true, 0, prompt);
}
if (command === 'execute') {
    respond(true, 0, `js processed: ${prompt}`);
}

respond(false, 3, `unknown command: ${command}`);
