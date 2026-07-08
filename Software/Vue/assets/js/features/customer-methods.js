import { apiBase } from '../core/app-config.js';

export const dashboardMethods = {
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
        
        showConfirmModal(message, title = '确认', callback) {
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
            
            const buttonContainer = document.createElement('div');
            buttonContainer.style.cssText = 'display:flex;justify-content:center;gap:12px';
            
            const cancelButton = document.createElement('button');
            cancelButton.textContent = '取消';
            cancelButton.style.cssText = 'background:#f8f9fa;color:#333;border:1px solid #dee2e6;border-radius:6px;padding:10px 24px;font-size:14px;cursor:pointer';
            cancelButton.onmouseover = () => cancelButton.style.background = '#e9ecef';
            cancelButton.onmouseout = () => cancelButton.style.background = '#f8f9fa';
            cancelButton.onclick = () => {
                document.body.removeChild(overlay);
                if (callback) callback(false);
            };
            
            const okButton = document.createElement('button');
            okButton.textContent = '确定';
            okButton.style.cssText = 'background:#0d6efd;color:#fff;border:none;border-radius:6px;padding:10px 24px;font-size:14px;cursor:pointer';
            okButton.onmouseover = () => okButton.style.background = '#0b5ed7';
            okButton.onmouseout = () => okButton.style.background = '#0d6efd';
            okButton.onclick = () => {
                document.body.removeChild(overlay);
                if (callback) callback(true);
            };
            
            buttonContainer.appendChild(cancelButton);
            buttonContainer.appendChild(okButton);
            modal.appendChild(titleEl);
            modal.appendChild(messageEl);
            modal.appendChild(buttonContainer);
            overlay.appendChild(modal);
            document.body.appendChild(overlay);
        },
        
        // 设备选择相关方法
        toggleDeviceSelect() {
            this.customer.showDeviceSelect = !this.customer.showDeviceSelect;
        },

        selectDevice(device) {
            this.customer.rentalForm.deviceCode = device.deviceCode;
            this.customer.showDeviceSelect = false;
        },

        // 租借订单选择相关方法
        toggleRentalSelect() {
            this.customer.showRentalSelect = !this.customer.showRentalSelect;
        },

        selectRental(rental) {
            this.customer.repairForm.rentalId = rental.id;
            this.customer.showRentalSelect = false;
        },

        // 订单号选择相关方法
        toggleOrderSelect() {
            this.customer.showOrderSelect = !this.customer.showOrderSelect;
        },

        selectOrder(rental) {
            this.customer.disputeForm.orderId = rental.id;
            this.customer.showOrderSelect = false;
        },

        async refreshDashboards() {
            if (!this.session.token) {
                return;
            }
            if (this.isAutoRefreshing) {
                return;
            }
            this.isAutoRefreshing = true;
            try {
                const tasks = [];
                if (this.canSeeCustomer) {
                    tasks.push(this.loadCustomerDashboard());
                }
                if (this.canSeeAdmin) {
                    tasks.push(this.loadAdminDashboard());
                }
                tasks.push(this.refreshLogs());
                tasks.push(this.refreshMqttLogs());
                await Promise.all(tasks);
            } finally {
                this.isAutoRefreshing = false;
            }
        },
        async loadCustomerDashboard() {
            if (!this.session.token) {
                return;
            }
            await Promise.all([
                this.refreshCustomerDevices(),
                this.refreshCustomerRentals(),
                this.refreshWallet(),
                this.refreshCustomerMessages(),
                this.refreshCustomerRepairs()
            ]);
            this.customerLoaded = true;
        },
        async refreshCustomerDevices() {
            const data = await this.request(`${apiBase}/api/devices`);
            if (Array.isArray(data)) {
                let devices = data;
                this.customer.devices = devices;
                this.pagination.devices = 1;
                this.log(`获取到 ${data.length} 台设备`);
            }
        },
        async refreshCustomerRentals() {
            const data = await this.request(`${apiBase}/api/rentals/me`);
            if (Array.isArray(data)) {
                this.customer.rentals = data;
                this.pagination.rentals = 1;
                const activeIds = data.filter(rental => rental.status === 'ACTIVE').map(rental => rental.id);
                const preserved = this.expandedRentals.filter(id =>
                    this.customer.rentals.some(rental => rental.id === id)
                );
                this.expandedRentals = Array.from(new Set([...preserved, ...activeIds]));

                // Always recover in-progress rental from backend data after refresh/reload.
                const inProgress = data
                    .filter(rental => rental.status === 'ACTIVE' || rental.status === 'OVERDUE')
                    .sort((a, b) => new Date(b.startedAt || b.createdAt || 0) - new Date(a.startedAt || a.createdAt || 0))[0] || null;
                this.syncCurrentRental(inProgress);
            }
        },
        async refreshWallet() {
            const data = await this.request(`${apiBase}/api/wallet`);
            if (data) {
                this.customer.wallet = {
                    ...data,
                    transactions: Array.isArray(data.transactions) ? data.transactions : []
                };
                this.pagination.transactions = 1;
            }
        },
        async submitRecharge() {
            if (this.customer.rechargeAmount <= 0) {
                this.showModal('充值金额需大于0');
                return;
            }
            const data = await this.request(`${apiBase}/api/wallet/recharge`, {
                method: 'POST',
                body: JSON.stringify({ amount: this.customer.rechargeAmount })
            });
            if (data) {
                this.customer.wallet = {
                    ...data,
                    transactions: Array.isArray(data.transactions) ? data.transactions : []
                };
                this.log('充值成功');
            }
        },
        async submitDispute() {
            if (!this.customer.disputeForm.orderId || !this.customer.disputeForm.reason) {
                this.showModal('请填写争议信息');
                return;
            }
            const data = await this.request(`${apiBase}/api/wallet/disputes`, {
                method: 'POST',
                body: JSON.stringify(this.customer.disputeForm)
            });
            if (data) {
                this.customer.disputes.unshift(data);
                this.disputesLoaded = false;
                this.customer.disputeForm = { orderId: '', reason: '' };
                this.showModal('费用争议提交成功！', '提交成功');
            }
        },
        async toggleDisputeHistory() {
            if (this.showAllDisputes) {
                this.showAllDisputes = false;
                return;
            }
            if (!this.disputesLoaded) {
                this.isLoadingDisputes = true;
                try {
                    await this.refreshCustomerDisputes();
                    this.disputesLoaded = true;
                } finally {
                    this.isLoadingDisputes = false;
                }
            }
            this.showAllDisputes = true;
        },
        async refreshCustomerMessages() {
            const data = await this.request(`${apiBase}/api/messages`);
            this.customer.messages = Array.isArray(data) ? data : [];
            this.pagination.messages = 1;
        },
        async markMessageRead(id) {
            const result = await this.request(`${apiBase}/api/messages/${id}/read`, { method: 'POST' });
            if (result) {
                this.customer.messages = this.customer.messages.map(msg =>
                    msg.id === id ? { ...msg, read: true } : msg
                );
            }
        },
        async refreshCustomerRepairs() {
            const data = await this.request(`${apiBase}/api/repairs/me`);
            if (Array.isArray(data)) {
                this.customer.repairs = data;
                this.pagination.repairs = 1;
            }
        },
        triggerRepairPhotoPicker() {
            const picker = this.$refs.repairPhotoInput;
            if (picker) {
                picker.click();
            }
        },
        async submitRepairTicket() {
            if (!this.customer.repairForm.rentalId || !this.customer.repairForm.description) {
                this.showModal('请完善报修信息');
                return;
            }
            const payload = {
                rentalId: this.customer.repairForm.rentalId,
                description: this.customer.repairForm.description
            };
            const data = await this.request(`${apiBase}/api/repairs`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                await this.refreshCustomerRepairs();
                this.showModal('报修工单已提交！', '提交成功');
                this.customer.repairForm = { rentalId: '', description: '' };
            }
        },
        async startRental() {
            this.showConfirmModal(`确认发起租借吗？设备：${this.customer.rentalForm.deviceCode || '未填写'}`, '确认', async (ok) => {
                if (!ok) {
                    return;
                }
                const payload = {
                    deviceCode: this.customer.rentalForm.deviceCode,
                    expectedHours: this.customer.rentalForm.expectedHours
                };
                const data = await this.request(`${apiBase}/api/rentals/start`, {
                    method: 'POST',
                    body: JSON.stringify(payload)
                });
                if (data) {
                    await this.refreshCustomerRentals();
                    await this.refreshCustomerDevices();
                    this.log(`租借成功：${data.id}`);

                    // 直接尝试开锁
                    if (data.deviceId) {
                        this.showConfirmModal(`租借成功！是否立即为设备 ${this.customer.rentalForm.deviceCode} 开锁？`, '确认', async (unlockOk) => {
                            if (unlockOk) {
                                const unlockResult = await this.request(`${apiBase}/api/devices/${data.deviceId}/unlock`, { method: 'POST' });
                                if (unlockResult) {
                                    this.log(`已发送开锁指令：${this.customer.rentalForm.deviceCode}`);
                                    this.logMqtt(`UNLOCK -> ${this.customer.rentalForm.deviceCode}`);
                                }
                            }
                        });
                    } else {
                        this.showModal('租借成功，但未获取到设备ID，无法自动开锁。');
                    }
                }
            });
        },
        startRentalTimer() {
            // 清除现有的计时器
            this.stopRentalTimer();

            // 启动新的计时器，每秒更新一次
            this.rentalTimer = setInterval(() => {
                this.now = new Date();
            }, 1000);
        },
        stopRentalTimer() {
            if (this.rentalTimer) {
                clearInterval(this.rentalTimer);
                this.rentalTimer = null;
            }
        },
        syncCurrentRental(rental) {
            if (!rental) {
                this.currentRental = null;
                this.stopRentalTimer();
                return;
            }
            this.currentRental = {
                id: rental.id,
                deviceCode: rental.deviceCode,
                deviceId: rental.deviceId,
                startedAt: rental.startedAt || rental.createdAt || new Date().toISOString()
            };
            this.startRentalTimer();
        },
        
        formatRentalDuration(startedAt) {
            const start = new Date(startedAt);
            const now = new Date();
            const diff = now - start;
            
            const hours = Math.floor(diff / (1000 * 60 * 60));
            const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
            const seconds = Math.floor((diff % (1000 * 60)) / 1000);
            
            return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        },
        
        calculateRentalFee(startedAt) {
            const start = new Date(startedAt);
            const now = new Date();
            const diff = now - start;
            
            const hours = Math.ceil(diff / (1000 * 60 * 60));
            const feePerHour = 2; // 每小时2元
            
            return hours * feePerHour;
        },
        enterScanMode() {
            this.scanEntryRentalOption = this.rentalOption;
            this.scanEntryDeviceCode = this.customer.rentalForm.deviceCode || '';
            this.rentalOption = 'scan';
        },
        restoreAfterReturn() {
            this.currentRental = null;
            this.stopRentalTimer();
            this.lockedDeviceCode = '';
            this.rentalOption = this.scanEntryRentalOption || 'manual';
            this.customer.rentalForm.deviceCode = this.scanEntryDeviceCode || '';
            this.isTemporaryLocked = false;
        },
        
        async toggleTemporaryLock(rental) {
            if (!rental) {
                this.showModal('未找到订单信息');
                return;
            }
            
            if (this.isTemporaryLocked) {
                this.showConfirmModal('确认要为设备 ' + rental.deviceCode + ' 发送开锁指令吗？', '确认解锁', async (ok) => {
                    if (!ok) {
                        return;
                    }
                    const result = await this.request(`${apiBase}/api/devices/${rental.deviceId}/unlock`, { method: 'POST' });
                    if (result) {
                        this.isTemporaryLocked = false;
                        await this.refreshCustomerDevices();
                        this.showNotification('设备已解锁，请正常使用', 'success');
                        this.log('已发送开锁指令：' + rental.deviceCode);
                    }
                });
            } else {
                await this.refreshCustomerDevices();
                const device = this.customer.devices.find(d => d.deviceCode === rental.deviceCode);
                if (!device) {
                    this.showModal('未找到设备信息，请刷新页面后重试', '⚠️ 错误');
                } else if (device.lockStatus !== 'LOCKED') {
                    this.showModal('设备锁未闭合，请先归还床体关门后再尝试', '⚠️ 提示');
                } else {
                    this.isTemporaryLocked = true;
                    this.showNotification('✅ 临时关锁成功，计时计费继续进行中', 'success');
                }
            }
        },
        
        showNotification(message, type = 'info') {
            // 创建通知元素
            const notification = document.createElement('div');
            notification.style.position = 'fixed';
            notification.style.top = '20px';
            notification.style.right = '20px';
            notification.style.padding = '12px 16px';
            notification.style.borderRadius = '8px';
            notification.style.color = 'white';
            notification.style.fontWeight = '600';
            notification.style.zIndex = '1000';
            notification.style.transition = 'all 0.3s ease';
            notification.style.boxShadow = '0 4px 12px rgba(0, 0, 0, 0.15)';
            
            // 根据类型设置背景色
            switch (type) {
                case 'success':
                    notification.style.backgroundColor = '#22c55e';
                    break;
                case 'error':
                    notification.style.backgroundColor = '#ef4444';
                    break;
                case 'warning':
                    notification.style.backgroundColor = '#f59e0b';
                    break;
                default:
                    notification.style.backgroundColor = '#3b82f6';
            }
            
            // 设置消息内容
            notification.textContent = message;
            
            // 添加到页面
            document.body.appendChild(notification);
            
            // 3秒后移除
            setTimeout(() => {
                notification.style.opacity = '0';
                notification.style.transform = 'translateY(-20px)';
                setTimeout(() => {
                    document.body.removeChild(notification);
                }, 300);
            }, 3000);
        },
        
        async startScan() {
            if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
                this.showNotification('无法访问摄像头：浏览器不支持摄像头访问', 'error');
                return;
            }
            if (typeof jsQR === 'undefined') {
                this.showNotification('二维码解析库未加载', 'error');
                return;
            }
            this.scanning = true;
            this.$nextTick(() => {
                const video = document.getElementById('preview');
                const canvas = document.createElement('canvas');
                const ctx = canvas.getContext('2d');
                if (!video) {
                    this.showNotification('扫码组件未就绪', 'error');
                    this.scanning = false;
                    return;
                }
                const constraints = {
                    video: {
                        facingMode: { exact: 'environment' }
                    }
                };
                navigator.mediaDevices.getUserMedia(constraints)
                    .then(stream => {
                        video.srcObject = stream;
                        video.play();
                        const scanFrame = () => {
                            if (!this.scanning) return;
                            if (video.readyState === video.HAVE_ENOUGH_DATA) {
                                canvas.width = video.videoWidth;
                                canvas.height = video.videoHeight;
                                ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
                                const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
                                const code = jsQR(imageData.data, canvas.width, canvas.height);
                                if (code) {
                                    this.stopScan();
                                    const qrUrl = code.data;
                                    let deviceCode = null;
                                    if (qrUrl.startsWith('http')) {
                                        const params = new URLSearchParams(new URL(qrUrl).search);
                                        deviceCode = params.get('id');
                                    } else {
                                        deviceCode = qrUrl.trim();
                                    }
                                    if (deviceCode) {
                                        const device = this.customer.devices.find(d => d.deviceCode === deviceCode);
                                        if (device) {
                                            if (device.status === 'AVAILABLE') {
                                                this.customer.rentalForm.deviceCode = deviceCode;
                                                this.rentalOption = 'manual';
                                                this.showNotification(`扫码成功！已识别设备编号：${deviceCode}`, 'success');
                                            } else {
                                                let message = '';
                                                if (device.status === 'IN_USE') {
                                                    message = `设备 ${deviceCode} 正在使用中，无法租借`;
                                                } else if (device.status === 'MAINTENANCE') {
                                                    message = `设备 ${deviceCode} 正在维护中，无法租借`;
                                                } else if (device.status === 'OFFLINE') {
                                                    message = `设备 ${deviceCode} 离线，无法租借`;
                                                } else {
                                                    message = `设备 ${deviceCode} 状态异常，无法租借`;
                                                }
                                                this.showNotification(message, 'error');
                                            }
                                        } else {
                                            this.showNotification(`扫码成功！但未找到设备 ${deviceCode}，请检查设备是否存在`, 'error');
                                        }
                                    } else {
                                        this.showNotification('扫码失败：无法从二维码中提取设备编号', 'error');
                                    }
                                    return;
                                }
                            }
                            requestAnimationFrame(scanFrame);
                        };
                        scanFrame();
                    })
                    .catch(error => {
                        console.error('摄像头访问失败:', error);
                        this.showNotification('无法访问摄像头：' + error.message, 'error');
                        this.scanning = false;
                    });
            });
        },
        
        stopScan() {
            this.scanning = false;
            const video = document.getElementById('preview');
            if (video && video.srcObject) {
                const stream = video.srcObject;
                const tracks = stream.getTracks();
                tracks.forEach(track => track.stop());
                video.srcObject = null;
            }
        },
        
        async returnRental(id) {
            if (this.returningRentalId === id) {
                this.showModal('归还请求正在处理中，请勿重复点击。');
                return;
            }
            await this.refreshCustomerDevices();
            const rentalBefore = this.customer.rentals.find(item => item.id === id);
            const device = rentalBefore
                ? this.customer.devices.find(d => d.deviceCode === rentalBefore.deviceCode || d.id === rentalBefore.deviceId)
                : null;
            if (device && device.lockStatus && device.lockStatus !== 'LOCKED') {
                this.showModal('设备锁未闭合，请先关闭设备锁后再归还。', '⚠️ 提示');
                return;
            }
            this.showConfirmModal('确认归还该租借订单吗？', '确认', async (ok) => {
                if (!ok) {
                    return;
                }
                this.returningRentalId = id;
                try {
                    const data = await this.request(`${apiBase}/api/rentals/${id}/return`, {
                        method: 'POST',
                        body: JSON.stringify({
                            conditionNotes: '用户归还确认',
                            lockConfirmed: true
                        })
                    });
                    if (data) {
                        await this.refreshCustomerRentals();
                        await this.refreshCustomerDevices();
                        this.restoreAfterReturn();
                        this.log(`已归还订单：${data.id}`);
                        const message = rentalBefore && rentalBefore.status === 'COMPLETED'
                            ? '该订单已归还，无需重复操作。'
                            : '归还成功。';
                        this.showModal(message);
                    }
                } finally {
                    this.returningRentalId = null;
                }
            });
        },
        async unlockRental(rental) {
            if (!rental) {
                this.showModal('未找到订单信息');
                return;
            }
            if (!rental.deviceId) {
                this.showModal('该订单缺少设备编号，无法开锁');
                return;
            }
            this.showConfirmModal(`确认发送开锁指令给设备：${rental.deviceCode || rental.deviceId} 吗？`, '确认', async (ok) => {
                if (!ok) {
                    return;
                }
                const result = await this.request(`${apiBase}/api/devices/${rental.deviceId}/unlock`, { method: 'POST' });
                if (result) {
                    const target = rental.deviceCode || rental.deviceId;
                    this.log(`已发送开锁指令：${target}`);
                    this.logMqtt(`UNLOCK -> ${target}`);
                }
            });
        },
        async cancelRentalUser(id) {
            const rental = this.customer.rentals.find(item => item.id === id);
            if (!this.canCancelRental(rental)) {
                this.showModal('租借开始超过5分钟，已无法取消，如需协助请联系管理员。');
                return;
            }
            const result = await this.cancelRentalOrder(id);
            if (result) {
                await this.refreshCustomerRentals();
                this.log(`已取消订单：${id}`);
            }
        },
        async cancelRentalOrder(id) {
            return await this.request(`${apiBase}/api/rentals/${id}/cancel`, { method: 'POST' });
        },
        async refreshCustomerDisputes() {
            const data = await this.request(`${apiBase}/api/wallet/disputes/me`);
            if (Array.isArray(data)) {
                this.customer.disputes = data;
            } else {
                this.customer.disputes = [];
            }
        },
        async toggleRepairHistory() {
            if (this.showAllRepairs) {
                this.showAllRepairs = false;
                return;
            }
            this.showAllRepairs = true;
        },
};