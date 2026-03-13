import { apiBase, defaultLoginForm, defaultRegisterForm, defaultCustomerState, defaultAdminState } from '../core/app-config.js';

export const interactionMethods = {
        switchAuthCard(mode) {
            if (this.authCard === mode) {
                return;
            }
            this.authCard = mode;
        },
        handleResize() {
            const query = window.matchMedia ? window.matchMedia('(max-width: 720px)') : null;
            this.isMobile = query ? query.matches : window.innerWidth <= 720;
        },
        switchMobileTab(tab) {
            if (this.mobileTab === tab) {
                return;
            }
            this.mobileTab = tab;
            window.scrollTo({ top: 0, behavior: 'smooth' });
        },
        showMobileSection(sectionKey) {
            if (!this.isMobile) {
                return true;
            }
            return this.mobileTab === sectionKey;
        },
        paginateList(list, page, size) {
            if (!Array.isArray(list) || !list.length) {
                return [];
            }
            const start = Math.max(0, (page - 1) * size);
            return list.slice(start, start + size);
        },
        totalPages(list) {
            if (!Array.isArray(list) || !list.length) {
                return 1;
            }
            return Math.max(1, Math.ceil(list.length / this.pageSize));
        },
        changePage(key, delta, total) {
            const current = this.pagination[key] || 1;
            const next = Math.min(Math.max(1, current + delta), total || 1);
            this.pagination[key] = next;
        },
        captureLockedDeviceFromUrl() {
            try {
                const params = new URLSearchParams(window.location.search || '');
                const code = params.get('id') || params.get('device');
                if (!code) {
                    return;
                }
                const normalized = String(code).trim().toUpperCase();
                if (!normalized) {
                    return;
                }
                this.lockedDeviceCode = normalized;
                this.customer.rentalForm.deviceCode = normalized;
            } catch (e) {
                console.warn('parse url failed', e);
            }
        },
        saveSession() {
            try {
                localStorage.setItem('carebed-session', JSON.stringify(this.session));
            } catch (e) {
                console.warn('session persist failed', e);
            }
        },
        clearSession() {
            try {
                localStorage.removeItem('carebed-session');
            } catch (e) {
                console.warn('session clear failed', e);
            }
        },
        loadSessionFromStorage() {
            try {
                const raw = localStorage.getItem('carebed-session');
                if (!raw) {
                    return false;
                }
                const data = JSON.parse(raw);
                if (!data || !data.token) {
                    return false;
                }
                this.session = {
                    token: data.token,
                    role: data.role,
                    displayName: data.displayName
                };
                this.activeArea = this.session.role === 'ADMIN' ? 'admin' : 'customer';
                this.afterLogin(true);
                return true;
            } catch (e) {
                console.warn('session restore failed', e);
                return false;
            }
        },
        async handleLogin() {
            const payload = {
                username: this.loginForm.username,
                password: this.loginForm.password
            };
            const result = await this.request(`${apiBase}/api/auth/login`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (result) {
                this.applyAuthResult(result, 'login');
            }
        },
        async validateAdminCredentials(username, password) {
            try {
                this.log(`请求：POST ${apiBase}/api/auth/login`);
                const res = await fetch(`${apiBase}/api/auth/login`, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ username, password })
                });
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
                    this.log(`错误：${message}`);
                    alert('默认管理员账号或密码不正确');
                    return false;
                }
                return true;
            } catch (err) {
                this.log(`错误：${err.message}`);
                alert('默认管理员账号校验失败，请稍后重试');
                return false;
            }
        },
        async handleRegister() {
            if (this.registerForm.role === 'ADMIN') {
                const adminUsername = (this.registerForm.adminUsername || '').trim();
                const adminPassword = this.registerForm.adminPassword || '';
                if (!adminUsername || !adminPassword) {
                    alert('注册运营管理员账号时，请先输入默认管理员账号密码');
                    return;
                }
                const valid = await this.validateAdminCredentials(adminUsername, adminPassword);
                if (!valid) {
                    return;
                }
            }
            const payload = {
                username: this.registerForm.username,
                password: this.registerForm.password,
                role: this.registerForm.role,
                fullName: this.registerForm.fullName,
                phone: this.registerForm.phone
            };
            const result = await this.request(`${apiBase}/api/auth/register`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (result) {
                this.applyAuthResult(result, 'register');
            }
        },
        applyAuthResult(result, mode) {
            this.session.token = result.token;
            this.session.role = result.role;
            this.session.displayName = result.displayName;
            const message = mode === 'register' ? '注册成功' : '登录成功';
            this.log(`${message}，角色：${result.role}`);
            this.saveSession();
            this.afterLogin();
        },
        afterLogin(fromRestore = false) {
            this.sessionExpiredHandling = false;
            this.authCard = 'login';
            this.loginForm = defaultLoginForm();
            this.registerForm = defaultRegisterForm();
            this.activeArea = this.session.role === 'ADMIN' ? 'admin' : 'customer';
            this.customerLoaded = false;
            this.adminLoaded = false;
            this.expandedRentals = [];
            this.mobileTab = 'devices';
            if (this.lockedDeviceCode && !this.isAdmin) {
                this.customer.rentalForm.deviceCode = this.lockedDeviceCode;
            }
            if (this.canSeeCustomer) {
                this.loadCustomerDashboard();
            }
            if (this.canSeeAdmin) {
                this.loadAdminDashboard();
            }
            if (this.canSeeAdmin) {
                this.refreshMqttLogs();
            }
            this.refreshLogs();
            if (!fromRestore) {
                this.log('会话已激活');
            }
        },
        resetToLoginState(logMessage) {
            this.session = { token: null, role: null, displayName: null };
            this.clearSession();
            this.activeArea = 'customer';
            this.customerLoaded = false;
            this.adminLoaded = false;
            this.customer = defaultCustomerState();
            this.admin = defaultAdminState();
            if (this.lockedDeviceCode) {
                this.customer.rentalForm.deviceCode = this.lockedDeviceCode;
            }
            this.loginForm = defaultLoginForm();
            this.registerForm = defaultRegisterForm();
            this.authCard = 'login';
            this.expandedRentals = [];
            this.mobileTab = 'devices';
            this.logs = [];
            if (logMessage) {
                this.log(logMessage);
            }
        },
        handleSessionExpired(message) {
            if (this.sessionExpiredHandling) {
                return;
            }
            this.sessionExpiredHandling = true;
            this.resetToLoginState('登录已过期，已返回登录页');
            alert(message || '登录已过期，请重新登录');
        },
        logout() {
            this.sessionExpiredHandling = false;
            this.resetToLoginState('已退出登录');
        },
        switchArea(area) {
            if (area === 'customer' && !this.canSeeCustomer) {
                return;
            }
            if (area === 'admin' && !this.canSeeAdmin) {
                return;
            }
            this.activeArea = area;
            if (area === 'customer') {
                if (this.isMobile) {
                    this.mobileTab = 'devices';
                    window.scrollTo({ top: 0, behavior: 'smooth' });
                }
                this.loadCustomerDashboard();
            }
            if (area === 'admin') {
                this.loadAdminDashboard();
            }
        },
        toggleRentalDetails(id) {
            if (!id) {
                return;
            }
            if (this.isRentalExpanded(id)) {
                this.expandedRentals = this.expandedRentals.filter(item => item !== id);
            } else {
                this.expandedRentals = [...this.expandedRentals, id];
            }
        },
        isRentalExpanded(id) {
            return this.expandedRentals.includes(id);
        },
        updateBeijingTime() {
            const now = new Date();
            this.now = now;
            this.beijingTime = now.toLocaleString('zh-CN', {
                hour12: false,
                timeZone: 'Asia/Shanghai'
            });
        },
        startRealtimeUpdates() {
            if (this.clockTimer) {
                clearInterval(this.clockTimer);
            }
            if (this.autoRefreshTimer) {
                clearInterval(this.autoRefreshTimer);
            }
            this.updateBeijingTime();
            this.clockTimer = setInterval(() => this.updateBeijingTime(), 1000);
            this.autoRefreshTimer = setInterval(() => this.refreshDashboards(), this.refreshIntervalMs);
        },
        stopRealtimeUpdates() {
            if (this.clockTimer) {
                clearInterval(this.clockTimer);
                this.clockTimer = null;
            }
            if (this.autoRefreshTimer) {
                clearInterval(this.autoRefreshTimer);
                this.autoRefreshTimer = null;
            }
        },
};
