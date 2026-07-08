import { apiBase } from '../core/app-config.js';

export const adminMethods = {
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

        async loadAdminDashboard() {
            if (!this.session.token) {
                return;
            }
            await Promise.all([
                this.refreshAdminOverview(),
                this.refreshAdminAnalytics(),
                this.refreshAdminDevices(),
                this.refreshAdminRentals(),
                this.refreshDisputes(),
                this.refreshAdminRepairs(),
                this.refreshUsers(),
                this.refreshAdminMessages()
            ]);
            this.adminLoaded = true;
        },
         async refreshAdminOverview() {
             const data = await this.request(`${apiBase}/api/admin/overview`);
             if (data) {
                 this.admin.overview = data;
                 setTimeout(() => {
                     this.initAnalysisCharts();
                 }, 100);
             }
         },
        async refreshAdminAnalytics() {
            const data = await this.request(`${apiBase}/api/admin/analytics`);
            if (data) {
                this.admin.analytics = data;
            }
        },
        async refreshAdminDevices() {
            const data = await this.request(`${apiBase}/api/devices`);
            if (Array.isArray(data)) {
                if (!this.deviceSnapshotInitialized) {
                    this.deviceSnapshot = Object.fromEntries(data.map(d => [d.id, {
                        onlineStatus: d.onlineStatus,
                        lastHeartbeat: d.lastHeartbeat
                    }]));
                    this.deviceSnapshotInitialized = true;
                } else {
                    const nextSnapshot = {};
                    for (const device of data) {
                        const prev = this.deviceSnapshot[device.id];
                        if (prev) {
                            if (prev.onlineStatus !== device.onlineStatus) {
                                const tip = device.onlineStatus === 'ONLINE' ? '设备上线' : '设备离线';
                                this.logMqtt(`${tip}：${device.deviceCode}`);
                            }
                            if (prev.lastHeartbeat !== device.lastHeartbeat && device.lastHeartbeat) {
                                const lock = device.lockStatus || 'UNKNOWN';
                                const batt = device.batteryLevel ?? 'N/A';
                                this.logMqtt(`心跳 ${device.deviceCode} · 电量${batt}% · 锁${lock}`);
                            }
                        }
                        nextSnapshot[device.id] = {
                            onlineStatus: device.onlineStatus,
                            lastHeartbeat: device.lastHeartbeat
                        };
                    }
                    this.deviceSnapshot = nextSnapshot;
                }

                this.admin.devices = data;
                if (this.admin.edit.id) {
                    const current = data.find(device => device.id === this.admin.edit.id);
                    if (!current) {
                        this.closeDeviceEditModal();
                    } else {
                        this.applyDeviceEditTarget(current);
                    }
                }
            }
        },
        applyDeviceEditTarget(device) {
            this.admin.edit = {
                id: device.id,
                deviceCode: device.deviceCode,
                ward: device.ward,
                bedNumber: device.bedNumber,
                status: device.status
            };
            this.admin.actionDeviceId = device.id;
        },
        selectDeviceForEdit(device) {
            this.applyDeviceEditTarget(device);
            this.admin.deviceEditModalVisible = true;
        },
        closeDeviceEditModal() {
            this.admin.deviceEditModalVisible = false;
            this.admin.edit = { id: '', deviceCode: '', ward: '', bedNumber: '', status: 'AVAILABLE' };
        },
        async registerDevice() {
            const payload = this.admin.newDevice;
            const data = await this.request(`${apiBase}/api/devices`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                this.log(`设备已登记：${data.deviceCode}`);
                this.admin.newDevice = { deviceCode: '', ward: '', bedNumber: '' };
                await this.refreshAdminDevices();
            }
        },
        async updateDevice() {
            if (!this.admin.edit.id) {
                this.showModal('请先选择设备');
                return;
            }
            const payload = {
                ward: this.admin.edit.ward,
                bedNumber: this.admin.edit.bedNumber,
                status: this.admin.edit.status
            };
            const data = await this.request(`${apiBase}/api/devices/${this.admin.edit.id}`, {
                method: 'PUT',
                body: JSON.stringify(payload)
            });
            if (data) {
                this.log('设备信息已更新');
                this.showModal('保存成功，设备信息已更新');
                this.closeDeviceEditModal();
                await this.refreshAdminDevices();
            }
        },
        async deleteDevice() {
            if (!this.admin.edit.id) {
                this.showModal('请先选择设备');
                return;
            }
            this.showConfirmModal(`确定要删除设备 ${this.admin.edit.deviceCode || ''} 吗？此操作不可恢复。`, '确认', async (confirmed) => {
                if (!confirmed) {
                    return;
                }
                const result = await this.request(`${apiBase}/api/devices/${this.admin.edit.id}`, {
                    method: 'DELETE'
                });
                if (result) {
                    this.log('设备已删除');
                    this.closeDeviceEditModal();
                    this.admin.actionDeviceId = '';
                    await this.refreshAdminDevices();
                }
            });
        },
        async triggerReboot() {
            if (!this.admin.actionDeviceId) {
                this.showModal('请选择设备');
                return;
            }
            const result = await this.request(`${apiBase}/api/devices/${this.admin.actionDeviceId}/reboot`, { method: 'POST' });
            if (result) {
                this.showModal('重启指令已发送，请稍后查看设备状态');
                this.log('已触发设备重启');
                this.logMqtt(`REBOOT -> ${this.admin.actionDeviceId}`);
            }
        },
        async submitFault() {
            if (!this.admin.actionDeviceId) {
                this.showModal('请选择设备');
                return;
            }
            if (!this.admin.faultForm.description) {
                this.showModal('请填写故障描述');
                return;
            }
            const payload = { description: this.admin.faultForm.description };
            const data = await this.request(`${apiBase}/api/devices/${this.admin.actionDeviceId}/faults`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                this.log('故障已上报');
                this.admin.faultForm.description = '';
                await this.refreshAdminDevices();
                await this.refreshAdminRepairs();
            }
        },
        async refreshAdminRentals() {
            const data = await this.request(`${apiBase}/api/rentals`);
            if (Array.isArray(data)) {
                this.admin.rentals = data;
            }
        },
        async cancelRentalAdmin(id) {
            const result = await this.cancelRentalOrder(id);
            if (result) {
                await this.refreshAdminRentals();
                this.log(`管理员取消订单：${id}`);
            }
        },
        async refreshDisputes() {
            const data = await this.request(`${apiBase}/api/wallet/disputes`);
            if (Array.isArray(data)) {
                this.admin.disputes = data;
            }
        },
        openDisputeEditModal(dispute) {
            if (!dispute || !dispute.id) {
                return;
            }
            this.admin.disputeUpdate = {
                id: dispute.id,
                status: dispute.status || 'IN_REVIEW',
                resolution: dispute.resolution || ''
            };
            this.admin.disputeEditModalVisible = true;
        },
        closeDisputeEditModal() {
            this.admin.disputeEditModalVisible = false;
            this.admin.disputeUpdate = { id: '', status: 'IN_REVIEW', resolution: '' };
        },
        async submitDisputeUpdate() {
            if (!this.admin.disputeUpdate.id) {
                this.showModal('请选择争议单');
                return;
            }
            const payload = {
                status: this.admin.disputeUpdate.status,
                resolution: this.admin.disputeUpdate.resolution
            };
            const data = await this.request(`${apiBase}/api/wallet/disputes/${this.admin.disputeUpdate.id}`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                await this.refreshDisputes();
                this.log('已更新争议处理结果');
                this.closeDisputeEditModal();
            }
        },
        async refreshAdminRepairs() {
            const data = await this.request(`${apiBase}/api/repairs`);
            if (Array.isArray(data)) {
                this.admin.repairs = data;
            }
        },
        openRepairEditModal(ticket) {
            if (!ticket || !ticket.id) {
                return;
            }
            this.admin.repairUpdate = {
                id: ticket.id,
                status: ticket.status || 'IN_PROGRESS',
                resolution: ticket.resolution || ''
            };
            this.admin.repairEditModalVisible = true;
        },
        closeRepairEditModal() {
            this.admin.repairEditModalVisible = false;
            this.admin.repairUpdate = { id: '', status: 'IN_PROGRESS', resolution: '' };
        },
        async submitRepairUpdate() {
            if (!this.admin.repairUpdate.id) {
                this.showModal('请选择工单');
                return;
            }
            const payload = {
                status: this.admin.repairUpdate.status,
                resolution: this.admin.repairUpdate.resolution
            };
            const data = await this.request(`${apiBase}/api/repairs/${this.admin.repairUpdate.id}`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                await this.refreshAdminRepairs();
                this.log('维修工单已更新');
                this.closeRepairEditModal();
            }
        },
        async refreshUsers(page = 1, keyword = '') {
            const params = new URLSearchParams();
            if (page > 1) params.set('page', page);
            if (keyword) params.set('keyword', keyword);
            const qs = params.toString();
            const url = `${apiBase}/api/auth/users${qs ? '?' + qs : ''}`;
            const data = await this.request(url);
            if (Array.isArray(data)) {
                this.admin.users = data;
            }
        },
        async unlockDevice(deviceId) {
            const result = await this.request(`${apiBase}/api/devices/${deviceId}/unlock`, { method: 'POST' });
            if (result) {
                this.log(`开锁指令已发送: ${deviceId}`);
                this.showModal('开锁指令已发送');
            }
        },
        async unlockAllOnlineDevices() {
            this.showConfirmModal('确定要一键开锁所有在线设备吗？', '确认', async (ok) => {
                if (!ok) return;
                const result = await this.request(`${apiBase}/api/devices/unlock-all`, { method: 'POST' });
                if (result) {
                    this.log(`批量开锁: 成功${result.successCount}/${result.totalOnline}`);
                    this.showModal(`批量开锁完成: 成功 ${result.successCount} / 共 ${result.totalOnline} 台`);
                }
            });
        },
        async disputeRefund(disputeId, orderId, maxAmount) {
            const max = maxAmount || 0;
            const amountStr = prompt(`订单金额: ¥${this.formatNumber(max)}\n请输入退款金额（元），最高不超过 ${this.formatNumber(max)} 元:`);
            if (!amountStr) return;
            const refundAmount = parseFloat(amountStr);
            if (isNaN(refundAmount) || refundAmount <= 0) {
                this.showModal('请输入有效的退款金额');
                return;
            }
            if (refundAmount > max) {
                this.showModal(`退费金额不能超过订单金额 ${this.formatNumber(max)} 元`);
                return;
            }
            this.showConfirmModal(`确认向该争议订单退款 ${this.formatNumber(refundAmount)} 元吗？`, '确认退款', async (ok) => {
                if (!ok) return;
                const result = await this.request(`${apiBase}/api/wallet/disputes/${disputeId}/refund`, {
                    method: 'POST',
                    body: JSON.stringify({ refundAmount })
                });
                if (result) {
                    this.showModal(`✅ 退款成功！已退还 ${this.formatNumber(refundAmount)} 元`);
                    await this.refreshDisputes();
                    this.log(`争议退款: 单${shortId(disputeId)} 退${refundAmount}元`);
                } else {
                    this.showModal('❌ 退款失败，请稍后重试');
                }
            });
        },
        async deleteUser(userId) {
            this.showConfirmModal('确定要注销此账户吗？此操作不可恢复。', '确认', async (ok) => {
                if (!ok) return;
                const result = await this.request(`${apiBase}/api/admin/users/${userId}`, { method: 'DELETE' });
                if (result) {
                    this.showModal('✅ 账户已注销');
                    this.log(`用户注销: ${userId}`);
                    await this.refreshUsers();
                } else {
                    this.showModal('❌ 注销失败');
                }
            });
        },
        async updateUserBalance(userId) {
            const amountStr = prompt(`请输入要设置的余额（元）:`);
            if (!amountStr) return;
            const amount = parseFloat(amountStr);
            if (isNaN(amount) || amount < 0) {
                this.showModal('请输入有效的余额');
                return;
            }
            this.showConfirmModal(`确认将余额设置为 ${amount} 元吗？`, '确认修改', async (ok) => {
                if (!ok) return;
                const result = await this.request(`${apiBase}/api/admin/users/${userId}/balance`, {
                    method: 'POST',
                    body: JSON.stringify({ amount })
                });
                if (result) {
                    this.showModal(`✅ 余额已更新为 ${amount} 元`);
                    this.log(`修改余额: ${userId} -> ${amount}元`);
                } else {
                    this.showModal('❌ 余额更新失败');
                }
            });
        },
        async updateUserPassword(userId) {
            const newPassword = prompt(`请输入新密码:`);
            if (!newPassword || newPassword.length < 6) {
                this.showModal('密码长度不能少于6位');
                return;
            }
            this.showConfirmModal('确认修改该用户的密码吗？', '确认修改', async (ok) => {
                if (!ok) return;
                const result = await this.request(`${apiBase}/api/admin/users/${userId}/password`, {
                    method: 'PUT',
                    body: JSON.stringify({ newPassword })
                });
                if (result) {
                    this.showModal('✅ 密码已更新');
                    this.log(`修改密码: ${userId}`);
                } else {
                    this.showModal('❌ 密码更新失败');
                }
            });
        },
        openUserEditModal(user) {
            this.admin.userEditForm = {
                id: user.id,
                username: user.username || '',
                fullName: user.fullName || '',
                phone: user.phone || '',
                newPassword: '',
                newBalance: '',
                showResetPwd: false,
                showSetBalance: false,
                showDelete: false
            };
            this.admin.userEditModalVisible = true;
        },
        async submitUserEdit() {
            const form = this.admin.userEditForm;
            if (!form.username && !form.fullName && !form.phone) {
                this.showModal('请至少修改一项内容');
                return;
            }
            const payload = {};
            if (form.username) payload.username = form.username;
            if (form.fullName) payload.fullName = form.fullName;
            if (form.phone) payload.phone = form.phone;
            const result = await this.request(`${apiBase}/api/admin/users/${form.id}`, {
                method: 'PUT',
                body: JSON.stringify(payload)
            });
            if (result) {
                this.showModal('✅ 用户信息已更新');
                this.admin.userEditModalVisible = false;
                await this.refreshUsers();
            }
        },
        async submitUserPassword() {
            const form = this.admin.userEditForm;
            if (!form.newPassword || form.newPassword.length < 6) {
                this.showModal('密码长度不能少于6位');
                return;
            }
            const result = await this.request(`${apiBase}/api/admin/users/${form.id}/password`, {
                method: 'PUT',
                body: JSON.stringify({ newPassword: form.newPassword })
            });
            if (result) {
                this.showModal('✅ 密码已重置');
                form.showResetPwd = false;
                form.newPassword = '';
            }
        },
        async submitUserBalance() {
            const form = this.admin.userEditForm;
            const amount = parseFloat(form.newBalance);
            if (isNaN(amount) || amount < 0) {
                this.showModal('请输入有效的余额');
                return;
            }
            const result = await this.request(`${apiBase}/api/admin/users/${form.id}/balance`, {
                method: 'POST',
                body: JSON.stringify({ amount })
            });
            if (result) {
                this.showModal(`✅ 余额已更新为 ${amount} 元`);
                form.showSetBalance = false;
                form.newBalance = '';
            }
        },
        async submitUserDelete() {
            const form = this.admin.userEditForm;
            const result = await this.request(`${apiBase}/api/admin/users/${form.id}`, {
                method: 'DELETE'
            });
            if (result) {
                this.showModal('✅ 账户已注销');
                this.admin.userEditModalVisible = false;
                await this.refreshUsers();
            }
        },
         async refreshAdminMessages() {
             const data = await this.request(`${apiBase}/api/messages/admin`);
             if (Array.isArray(data)) {
                 this.admin.adminMessages = data;
             }
         },
         initAnalysisCharts() {
             if (!window.echarts || !this.admin.overview) {
                 return;
             }
             this.$nextTick(() => {
                 this.drawUserDistributionChart();
                 this.drawDeviceStatusChart();
                 this.drawRentalStatusChart();
             });
         },
         drawUserDistributionChart() {
             const chartDom = document.getElementById('userDistributionChart');
             if (!chartDom) return;

             if (!this.userChartInstance) {
                 this.userChartInstance = window.echarts.init(chartDom, null, { renderer: 'canvas' });
             }

             const overview = this.admin.overview;
             const data = [
                 { value: overview.familyUsers || 0, name: '家属用户' },
                 { value: overview.adminUsers || 0, name: '管理员' }
             ];

             const option = {
                 tooltip: {
                     trigger: 'item',
                     formatter: '{b}: {c} ({d}%)'
                 },
                 legend: {
                     orient: 'bottom',
                     data: ['家属用户', '管理员']
                 },
                 series: [{
                     type: 'pie',
                     radius: ['40%', '70%'],
                     data: data,
                     itemStyle: {
                         borderRadius: 8,
                         borderColor: '#fff',
                         borderWidth: 2
                     },
                     emphasis: {
                         itemStyle: {
                             shadowBlur: 10,
                             shadowOffsetX: 0,
                             shadowColor: 'rgba(0, 0, 0, 0.5)'
                         }
                     },
                     color: ['#3b82f6', '#8b5cf6']
                 }]
             };

             this.userChartInstance.setOption(option);
         },
         drawDeviceStatusChart() {
             const chartDom = document.getElementById('deviceStatusChart');
             if (!chartDom) return;

             if (!this.deviceChartInstance) {
                 this.deviceChartInstance = window.echarts.init(chartDom, null, { renderer: 'canvas' });
             }

             const overview = this.admin.overview;
             const data = [
                 { value: overview.availableDevices || 0, name: '可用' },
                 { value: overview.inUseDevices || 0, name: '使用中' },
                 { value: overview.maintenanceDevices || 0, name: '维修' },
                 { value: overview.offlineDevices || 0, name: '离线' }
             ];

             const option = {
                 tooltip: {
                     trigger: 'item',
                     formatter: '{b}: {c} ({d}%)'
                 },
                 legend: {
                     orient: 'bottom',
                     data: ['可用', '使用中', '维修', '离线']
                 },
                 series: [{
                     type: 'pie',
                     radius: ['40%', '70%'],
                     data: data,
                     itemStyle: {
                         borderRadius: 8,
                         borderColor: '#fff',
                         borderWidth: 2
                     },
                     emphasis: {
                         itemStyle: {
                             shadowBlur: 10,
                             shadowOffsetX: 0,
                             shadowColor: 'rgba(0, 0, 0, 0.5)'
                         }
                     },
                     color: ['#10b981', '#f59e0b', '#f97316', '#ef4444']
                 }]
             };

             this.deviceChartInstance.setOption(option);
         },
         drawRentalStatusChart() {
             const chartDom = document.getElementById('rentalStatusChart');
             if (!chartDom) return;

             if (!this.rentalChartInstance) {
                 this.rentalChartInstance = window.echarts.init(chartDom, null, { renderer: 'canvas' });
             }

             const overview = this.admin.overview;
             const data = [
                 { value: overview.activeRentals || 0, name: '进行中' },
                 { value: overview.completedRentals || 0, name: '已完成' },
                 { value: overview.overdueRentals || 0, name: '已逾期' },
                 { value: overview.canceledRentals || 0, name: '已取消' }
             ];

             const option = {
                 tooltip: {
                     trigger: 'item',
                     formatter: '{b}: {c} ({d}%)'
                 },
                 legend: {
                     orient: 'bottom',
                     data: ['进行中', '已完成', '已逾期', '已取消']
                 },
                 series: [{
                     type: 'pie',
                     radius: ['40%', '70%'],
                     data: data,
                     itemStyle: {
                         borderRadius: 8,
                         borderColor: '#fff',
                         borderWidth: 2
                     },
                     emphasis: {
                         itemStyle: {
                             shadowBlur: 10,
                             shadowOffsetX: 0,
                             shadowColor: 'rgba(0, 0, 0, 0.5)'
                         }
                     },
                     color: ['#06b6d4', '#10b981', '#f97316', '#ef4444']
                 }]
             };

             this.rentalChartInstance.setOption(option);
         },
         handleChartsResize() {
             if (this.userChartInstance) {
                 this.userChartInstance.resize();
             }
             if (this.deviceChartInstance) {
                 this.deviceChartInstance.resize();
             }
             if (this.rentalChartInstance) {
                 this.rentalChartInstance.resize();
             }
         },
         disposeCharts() {
             if (this.userChartInstance) {
                 this.userChartInstance.dispose();
                 this.userChartInstance = null;
             }
             if (this.deviceChartInstance) {
                 this.deviceChartInstance.dispose();
                 this.deviceChartInstance = null;
             }
             if (this.rentalChartInstance) {
                 this.rentalChartInstance.dispose();
                 this.rentalChartInstance = null;
             }
         }
};