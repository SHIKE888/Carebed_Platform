import { apiBase } from '../core/app-config.js';

export const utilityMethods = {
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
                        alert(message);
                    }
                    return null;
                }
                return json ?? {};
            } catch (err) {
                this.log(`错误：${err.message}`);
                alert(err.message);
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
            if (!value) {
                return '未知时间';
            }
            return new Date(value).toLocaleString();
        },
        formatNumber(value) {
            if (value === null || value === undefined) {
                return '0.00';
            }
            return Number(value).toFixed(2);
        },
        displayDeviceStatus(status) {
            const labels = {
                AVAILABLE: '可用',
                IN_USE: '占用',
                MAINTENANCE: '维护',
                OFFLINE: '离线'
            };
            if (!status) {
                return '未知';
            }
            return labels[status] || status;
        },
        displayLockStatus(lockStatus, deviceStatus) {
            if (deviceStatus === 'OFFLINE') {
                return '--';
            }
            if (!lockStatus) {
                return '--';
            }
            const labels = {
                LOCKED: '锁已关',
                UNLOCKED: '锁已开'
            };
            return labels[lockStatus] || lockStatus;
        },
        displayRentalStatus(status) {
            const map = {
                ACTIVE: '进行中',
                COMPLETED: '已完成',
                OVERDUE: '已逾期',
                CANCELED: '已取消'
            };
            return map[status] || status || '未知';
        },
        shortId(value) {
            if (!value) {
                return '-';
            }
            return String(value).slice(0, 8);
        },
        rentalShortId(id) {
            return this.shortId(id);
        },
        rentalDuration(rental) {
            if (!rental || !rental.startedAt) {
                return '未开始';
            }
            const start = new Date(rental.startedAt);
            if (Number.isNaN(start.getTime())) {
                return '未开始';
            }
            const end = rental.endedAt ? new Date(rental.endedAt) : this.now;
            if (Number.isNaN(end.getTime())) {
                return '未开始';
            }
            const diffMs = Math.max(0, end.getTime() - start.getTime());
            const totalSeconds = Math.floor(diffMs / 1000);
            const days = Math.floor(totalSeconds / 86400);
            const hours = Math.floor((totalSeconds % 86400) / 3600);
            const minutes = Math.floor((totalSeconds % 3600) / 60);
            const seconds = totalSeconds % 60;
            const hh = String(hours).padStart(2, '0');
            const mm = String(minutes).padStart(2, '0');
            const ss = String(seconds).padStart(2, '0');
            if (days > 0) {
                return `${days}天 ${hh}:${mm}:${ss}`;
            }
            return `${hh}:${mm}:${ss}`;
        },
        canUnlockRental(rental) {
            if (!rental) {
                return false;
            }
            return rental.status === 'ACTIVE' || rental.status === 'OVERDUE';
        },
        canCancelRental(rental) {
            if (!rental || rental.status !== 'ACTIVE') {
                return false;
            }
            if (!rental.startedAt) {
                return true;
            }
            const startedAt = new Date(rental.startedAt);
            if (Number.isNaN(startedAt.getTime())) {
                return true;
            }
            const nowReference = this.now;
            if (!nowReference || Number.isNaN(nowReference.getTime())) {
                return true;
            }
            const elapsedMs = Math.max(0, nowReference.getTime() - startedAt.getTime());
            const fiveMinutesMs = 5 * 60 * 1000;
            return elapsedMs <= fiveMinutesMs;
        }
};
