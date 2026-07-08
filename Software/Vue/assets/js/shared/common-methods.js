import { apiBase } from '../core/app-config.js';

export const utilityMethods = {
        showModal(message, title = '提示') {
            const overlay = document.createElement('div');
            overlay.style.cssText = 'position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;z-index:10000';

            const modal = document.createElement('div');
            modal.style.cssText = 'background:#fff;border-radius:12px;padding:24px;max-width:320px;width:90%;box-shadow:0 4px 20px rgba(0,0,0,0.2);text-align:center;font-family:Arial,sans-serif';

            const titleEl = document.createElement('div');
            titleEl.textContent = title;
            titleEl.style.cssText = 'font-size:18px;font-weight:bold;margin-bottom:16px;color:#333';

            const messageEl = document.createElement('div');
            messageEl.textContent = message;
            messageEl.style.cssText = 'font-size:14px;color:#666;margin-bottom:24px;line-height:1.5';

            const okButton = document.createElement('button');
            okButton.textContent = '确定';
            okButton.style.cssText = 'background:#0d6efd;color:#fff;border:none;border-radius:6px;padding:10px 32px;font-size:14px;cursor:pointer';
            okButton.onmouseover = () => okButton.style.background = '#0b5ed7';
            okButton.onmouseout = () => okButton.style.background = '#0d6efd';
            okButton.onclick = () => document.body.removeChild(overlay);

            modal.appendChild(titleEl);
            modal.appendChild(messageEl);
            modal.appendChild(okButton);
            overlay.appendChild(modal);
            document.body.appendChild(overlay);
        },

        async refreshLogs() {
            if (!this.session.token) {
                this.logs = [];
                return;
            }
            const data = await this.request(`${apiBase}/api/logs`);
            if (Array.isArray(data)) {
                this.logs = data.map(entry => {
                    const ts = entry.createdAt ? new Date(entry.createdAt) : new Date();
                    const formatted = ts.toLocaleString('zh-CN', { hour12: false });
                    const suffix = entry.details ? ` · ${entry.details}` : '';
                    const actor = entry.actorName || '系统';
                    return `[${formatted}] ${actor} · ${entry.action}${suffix}`;
                });
            }
        },
        async refreshMqttLogs() {
            if (!this.session.token || !this.canSeeAdmin) {
                this.mqttLogs = [];
                return;
            }
            const data = await this.request(`${apiBase}/api/mqtt/logs`);
            if (Array.isArray(data)) {
                this.mqttLogs = data.map(entry => {
                    const ts = entry.createdAt ? new Date(entry.createdAt) : new Date();
                    const formatted = ts.toLocaleString('zh-CN', { hour12: false });
                    const dir = entry.direction || '-';
                    const topic = entry.topic || '-';
                    const device = entry.deviceCode ? ` · ${entry.deviceCode}` : '';
                    const payload = entry.payload || '';
                    return `[${formatted}] ${dir} ${topic}${device} · ${payload}`;
                });
            }
        },
        async request(url, options = {}) {
            try {
                const headers = {
                    'Content-Type': 'application/json',
                    ...(options.headers || {})
                };
                if (this.session.token && !headers['X-Auth-Token']) {
                    headers['X-Auth-Token'] = this.session.token;
                }
                const opts = { ...options, headers };
                if (!opts.method) {
                    opts.method = opts.body ? 'POST' : 'GET';
                }
                if (opts.method === 'GET' && opts.body) {
                    delete opts.body;
                }
                this.log(`请求：${opts.method} ${url}`);
                const res = await fetch(url, opts);
                const text = await res.text();
                let json = null;
                if (text) {
                    try {
                        json = JSON.parse(text);
                    } catch (err) {
                        json = { raw: text };
                    }
                }
                if (!res.ok) {
                    const message = json && json.message
                        ? json.message
                        : (json && json.raw ? json.raw : res.statusText);
                    const isSessionExpired = res.status === 401
                        || /登录已过期|会话已过期|token.*expired|unauthorized/i.test(message || '');
                    this.log(`错误：${message}`);
                    if (isSessionExpired) {
                        this.handleSessionExpired(message);
                    } else {
                        this.showModal(`错误 ${res.status}：${message}`);
                    }
                    return null;
                }
                return json ?? {};
            } catch (err) {
                this.log(`错误：${err.message}`);
                this.showModal(err.message);
                return null;
            }
        },
        logMqtt(message) {
            const timestamp = new Date().toLocaleTimeString();
            this.mqttLogs.push(`[${timestamp}] ${message}`);
            if (this.mqttLogs.length > 30) {
                this.mqttLogs.shift();
            }
        },
        log(message) {
            const timestamp = new Date().toLocaleTimeString();
            this.logs.push(`[${timestamp}] ${message}`);
            if (this.logs.length > 200) {
                this.logs.shift();
            }
        },
        formatTime(value) {
            if (!value) return '';
            const date = new Date(value);
            if (isNaN(date.getTime())) return '';
            return date.toLocaleString('zh-CN', { hour12: false });
        },
        formatDate(value) {
            if (!value) return '';
            const date = new Date(value);
            if (isNaN(date.getTime())) return '';
            return date.toLocaleDateString('zh-CN');
        },
        formatDateTime(value) {
            if (!value) return '';
            const date = new Date(value);
            if (isNaN(date.getTime())) return '';
            return date.toLocaleString('zh-CN', {
                year: 'numeric',
                month: '2-digit',
                day: '2-digit',
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit',
                hour12: false
            });
        }
};