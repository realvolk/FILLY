const net = require('net');
const { spawn } = require('child_process');
const fs = require('fs');

class Session {
    constructor(socketPath = '/tmp/filly.sock') {
        this.socketPath = socketPath;
        this.conn = null;
        this.buffer = '';
    }

    async connect() {
        // Auto-start daemon
        if (!fs.existsSync(this.socketPath)) {
            spawn('filly', ['daemon', '--socket', this.socketPath], { detached: true });
            // Wait for socket
            for (let i = 0; i < 50; i++) {
                if (fs.existsSync(this.socketPath)) break;
                await new Promise(r => setTimeout(r, 50));
            }
        }
        return new Promise((resolve, reject) => {
            this.conn = net.createConnection(this.socketPath, () => resolve());
            this.conn.on('error', reject);
        });
    }

    close() {
        if (this.conn) this.conn.end();
    }

    async send(widget, params = {}) {
        const req = JSON.stringify({ widget, params }) + '\n';
        return new Promise((resolve, reject) => {
            this.conn.write(req);
            this.conn.once('data', (data) => {
                let response = data.toString().trim();
                // Skip yield lines
                while (response.startsWith('yield:')) {
                    const idx = response.indexOf('\n');
                    if (idx < 0) break;
                    response = response.slice(idx + 1).trim();
                }
                try {
                    resolve(JSON.parse(response));
                } catch (e) {
                    reject(e);
                }
            });
        });
    }

    async menu(title, message, choices) {
        return this.send('menu', { title, message, choices });
    }

    async yesno(title, message, def = true) {
        return this.send('yesno', { title, message, default: def });
    }

    async input(title, message, def = '', placeholder = '') {
        return this.send('input', { title, message, default: def, placeholder });
    }

    async password(title, message, placeholder = '') {
        return this.send('password', { title, message, placeholder });
    }

    async msg(title, message) {
        return this.send('msg', { title, message });
    }
}

module.exports = { Session };