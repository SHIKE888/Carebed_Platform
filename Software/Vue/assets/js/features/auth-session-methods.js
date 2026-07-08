import { apiBase } from '../core/app-config.js';
import { defaultLoginForm, defaultRegisterForm, defaultResetForm, defaultCustomerState, defaultAdminState } from '../core/app-config.js';

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
        switchAdminMobileTab(tab) {
            if (this.adminMobileTab === tab) {
                return;
            }
            this.adminMobileTab = tab;
            window.scrollTo({ top: 0, behavior: 'smooth' });
        },
        switchAdminDesktopTab(tab) {
            if (this.adminDesktopTab === tab) {
                return;
            }
            this.adminDesktopTab = tab;
        },
        showMobileSection(sectionKey) {
            if (!this.isMobile) {
                return true;
            }
            return this.mobileTab === sectionKey;
        },
        showAdminSection(sectionKey) {
            if (this.isMobile) {
                return this.adminMobileTab === sectionKey;
            }
            return this.adminDesktopTab === sectionKey;
        },
        paginateList(list, page, size) {
            if (!Array.isArray(list) || !list.length) {
                return [];
            }
            const start = Math.max(0, (page - 1) * size);
            return list.slice(start, start + size);
        },
        totalPages(list, pageSize) {
            if (!Array.isArray(list) || !list.length) {
                return 1;
            }
            const sz = pageSize || this.pageSize;
            return Math.max(1, Math.ceil(list.length / sz));
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
                // 只自动填充设备编号，不锁定
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
        async handleResetPassword() {
            const payload = {
                username: this.resetForm.username,
                phone: this.resetForm.phone,
                newPassword: this.resetForm.newPassword
            };
            const result = await this.request(`${apiBase}/api/auth/reset-password`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (result) {
                this.applyAuthResult(result, 'login');
                this.showModal('密码重置成功，已自动登录');
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
        displayUserRole(role) {
            const map = {
                'ADMIN': '运营管理员',
                'FAMILY': '家属用户'
            };
            return map[role] || role || '-';
        },
        displayDeviceStatus(status) {
            const map = {
                'AVAILABLE': '可用',
                'IN_USE': '使用中',
                'MAINTENANCE': '维修中',
                'OFFLINE': '离线'
            };
            return map[status] || status || '-';
        },
        displayRentalStatus(status) {
            const map = {
                'ACTIVE': '进行中',
                'COMPLETED': '已完成',
                'OVERDUE': '已逾期',
                'CANCELED': '已取消'
            };
            return map[status] || status || '-';
        },
        displayLockStatus(lockStatus, deviceStatus) {
            if (deviceStatus !== 'AVAILABLE') {
                return '不可用';
            }
            const map = {
                'LOCKED': '已锁定',
                'UNLOCKED': '已解锁'
            };
            return map[lockStatus] || '未锁定';
        },
        displayDisputeStatus(status) {
            const map = {
                'OPEN': '待处理',
                'IN_REVIEW': '处理中',
                'RESOLVED': '已解决',
                'REJECTED': '已拒绝'
            };
            return map[status] || status || '-';
        },
        disputeStatusColor(status) {
            const map = {
                'OPEN': '#dc3545',
                'IN_REVIEW': '#ffc107',
                'RESOLVED': '#28a745',
                'REJECTED': '#6c757d'
            };
            return map[status] || '#6c757d';
        },
        displayRepairStatus(status) {
            const map = {
                'OPEN': '待处理',
                'IN_PROGRESS': '处理中',
                'RESOLVED': '已解决',
                'REJECTED': '已拒绝'
            };
            return map[status] || status || '-';
        },
        repairStatusColor(status) {
            const map = {
                'OPEN': '#dc3545',
                'IN_PROGRESS': '#ffc107',
                'RESOLVED': '#28a745',
                'REJECTED': '#6c757d'
            };
            return map[status] || '#6c757d';
        },
        displayNotificationType(type) {
            const map = {
                'SYSTEM': '系统通知',
                'RENTAL': '租借通知',
                'WALLET': '钱包通知',
                'REPAIR': '维修通知',
                'REMINDER': '提醒通知',
                'PAYMENT_CHARGED': '扣款通知',
                'PAYMENT_ALERT': '支付提醒',
                'DISPUTE': '争议通知',
                'CHARGE_SUCCESS': '扣款成功',
                'CHARGE_FAILED': '扣款失败',
                'REFUND_SUCCESS': '退款成功',
                'ORDER_COMPLETED': '订单完成',
                'ORDER_CANCELLED': '订单取消',
                'DEVICE_OFFLINE': '设备离线',
                'DEVICE_ONLINE': '设备上线',
                'DEVICE_LOCKED': '设备锁定',
                'DEVICE_UNLOCKED': '设备解锁',
                'REPAIR_CREATED': '报修创建',
                'REPAIR_ASSIGNED': '维修分配',
                'REPAIR_RESOLVED': '维修完成',
                'ADMIN_MESSAGE': '管理员消息',
                'SYSTEM_ALERT': '系统告警',
                'BALANCE_LOW': '余额不足',
                'RENTAL_SUCCESS': '租借成功',
                'RENTAL_STARTED': '租借开始',
                'RENTAL_ENDED': '租借结束',
                'RENTAL_OVERDUE': '租借逾期',
                'LOCK_CONFIRMED': '锁定确认',
                'LOCK_FAILED': '锁定失败',
                'UNLOCK_CONFIRMED': '解锁确认',
                'UNLOCK_FAILED': '解锁失败',
                'REPAIR_UPDATE': '维修更新',
                'REPAIR_CANCELLED': '维修取消',
                'REPAIR_REJECTED': '维修拒绝',
                'DISPUTE_CREATED': '争议创建',
                'DISPUTE_UPDATED': '争议更新',
                'DISPUTE_RESOLVED': '争议解决',
                'DISPUTE_REJECTED': '争议拒绝',
                'TRANSACTION_COMPLETED': '交易完成',
                'TRANSACTION_FAILED': '交易失败',
                'WITHDRAWAL_REQUEST': '提现申请',
                'WITHDRAWAL_APPROVED': '提现通过',
                'WITHDRAWAL_REJECTED': '提现拒绝',
                'BONUS_EARNED': '获得奖励',
                'COUPON_ISSUED': '发放优惠券',
                'SUBSCRIPTION_EXPIRED': '订阅到期',
                'ACCOUNT_VERIFIED': '账户已验证',
                'ACCOUNT_SUSPENDED': '账户已暂停',
                'PASSWORD_CHANGED': '密码已修改',
                'EMAIL_VERIFIED': '邮箱已验证',
                'PHONE_VERIFIED': '手机已验证',
                'LOGIN_ALERT': '登录提醒',
                'LOGIN_FAILED': '登录失败',
                'SESSION_EXPIRED': '会话过期',
                'DATA_SYNCED': '数据同步完成',
                'SETTING_UPDATED': '设置已更新',
                'NOTIFICATION_ENABLED': '通知已开启',
                'NOTIFICATION_DISABLED': '通知已关闭',
                'NEW_VERSION_AVAILABLE': '新版本可用',
                'MAINTENANCE_SCHEDULED': '维护通知',
                'EMERGENCY_ALERT': '紧急告警',
                'INFO': '信息通知',
                'SUCCESS': '成功通知',
                'WARNING': '警告通知',
                'ERROR': '错误通知',
                'DEBUG': '调试信息'
            };
            return map[type] || type || '-';
        },
        displayTransactionType(type) {
            const map = {
                'RECHARGE': '充值',
                'RENTAL': '租借',
                'REFUND': '退款',
                'ADJUSTMENT': '调整',
                'DEBIT': '扣款',
                'CREDIT': '入账',
                'WITHDRAWAL': '提现',
                'FEE': '手续费'
            };
            return map[type] || type || '-';
        },
        formatNumber(value) {
            if (value == null) return '0.00';
            const num = parseFloat(value);
            if (isNaN(num)) return '0.00';
            return num.toFixed(2);
        },
        rentalShortId(id) {
            if (!id) return '-';
            const str = String(id);
            return str.length > 8 ? str.slice(0, 8) + '...' : str;
        },
        shortId(id) {
            if (!id) return '-';
            const str = String(id);
            return str.length > 8 ? str.slice(0, 8) + '...' : str;
        },
        canCancelRental(rental) {
            if (!rental || !rental.startedAt) {
                return false;
            }
            const start = new Date(rental.startedAt);
            const now = new Date();
            const diffMinutes = (now - start) / (1000 * 60);
            return diffMinutes <= 5;
        },
        rentalDuration(rental) {
            if (!rental || !rental.startedAt) {
                return '-';
            }
            const start = new Date(rental.startedAt);
            const end = rental.endedAt ? new Date(rental.endedAt) : new Date();
            const diff = end - start;
            
            const hours = Math.floor(diff / (1000 * 60 * 60));
            const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
            
            if (hours > 0) {
                return `${hours}小时${minutes}分钟`;
            } else {
                return `${minutes}分钟`;
            }
        }
};