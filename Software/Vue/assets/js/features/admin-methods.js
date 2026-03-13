import { apiBase } from '../core/app-config.js';

export const adminMethods = {
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
                // 首次加载仅建立快照，避免一次性刷屏
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
                        this.admin.edit = { id: '', deviceCode: '', ward: '', bedNumber: '', status: 'AVAILABLE' };
                    } else {
                        this.selectDeviceForEdit(current);
                    }
                }
            }
        },
        selectDeviceForEdit(device) {
            this.admin.edit = {
                id: device.id,
                deviceCode: device.deviceCode,
                ward: device.ward,
                bedNumber: device.bedNumber,
                status: device.status
            };
            this.admin.actionDeviceId = device.id;
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
                alert('请先选择设备');
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
                await this.refreshAdminDevices();
            }
        },
        async deleteDevice() {
            if (!this.admin.edit.id) {
                alert('请先选择设备');
                return;
            }
            const confirmed = confirm(`确定要删除设备 ${this.admin.edit.deviceCode || ''} 吗？此操作不可恢复。`);
            if (!confirmed) {
                return;
            }
            const result = await this.request(`${apiBase}/api/devices/${this.admin.edit.id}`, {
                method: 'DELETE'
            });
            if (result) {
                this.log('设备已删除');
                this.admin.edit = { id: '', deviceCode: '', ward: '', bedNumber: '', status: 'AVAILABLE' };
                this.admin.actionDeviceId = '';
                await this.refreshAdminDevices();
            }
        },
        async triggerReboot() {
            if (!this.admin.actionDeviceId) {
                alert('请选择设备');
                return;
            }
            const result = await this.request(`${apiBase}/api/devices/${this.admin.actionDeviceId}/reboot`, { method: 'POST' });
            if (result) {
                this.log('已触发设备重启');
                this.logMqtt(`REBOOT -> ${this.admin.actionDeviceId}`);
            }
        },
        async submitFault() {
            if (!this.admin.actionDeviceId) {
                alert('请选择设备');
                return;
            }
            if (!this.admin.faultForm.description) {
                alert('请填写故障描述');
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
        async submitDisputeUpdate() {
            if (!this.admin.disputeUpdate.id) {
                alert('请选择争议单');
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
                this.admin.disputeUpdate.resolution = '';
            }
        },
        async refreshAdminRepairs() {
            const data = await this.request(`${apiBase}/api/repairs`);
            if (Array.isArray(data)) {
                this.admin.repairs = data;
            }
        },
        async submitRepairUpdate() {
            if (!this.admin.repairUpdate.id) {
                alert('请选择工单');
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
                this.admin.repairUpdate.resolution = '';
            }
        },
        async refreshUsers() {
            const data = await this.request(`${apiBase}/api/auth/users`);
            if (Array.isArray(data)) {
                this.admin.users = data;
            }
        },
        async refreshAdminMessages() {
            const data = await this.request(`${apiBase}/api/messages/admin`);
            if (Array.isArray(data)) {
                this.admin.adminMessages = data;
            }
        },
};
