import { apiBase } from '../core/app-config.js';

export const dashboardMethods = {
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
                if (this.lockedDeviceCode && !this.isAdmin) {
                    const matchIndex = devices.findIndex(d => (d.deviceCode || '').toUpperCase() === this.lockedDeviceCode);
                    if (matchIndex > 0) {
                        const [match] = devices.splice(matchIndex, 1);
                        devices = [match, ...devices];
                    }
                }
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
            }
        },
        async refreshWallet() {
            const data = await this.request(`${apiBase}/api/wallet`);
            if (data) {
                this.customer.wallet = data;
                this.pagination.transactions = 1;
            }
        },
        async submitRecharge() {
            if (this.customer.rechargeAmount <= 0) {
                alert('充值金额需大于0');
                return;
            }
            const data = await this.request(`${apiBase}/api/wallet/recharge`, {
                method: 'POST',
                body: JSON.stringify({ amount: this.customer.rechargeAmount })
            });
            if (data) {
                this.customer.wallet = data;
                this.log('充值成功');
            }
        },
        async submitDispute() {
            if (!this.customer.disputeForm.orderId || !this.customer.disputeForm.reason) {
                alert('请填写争议信息');
                return;
            }
            const data = await this.request(`${apiBase}/api/wallet/disputes`, {
                method: 'POST',
                body: JSON.stringify(this.customer.disputeForm)
            });
            if (data) {
                this.customer.disputes.unshift(data);
                this.customer.disputeForm = { orderId: '', reason: '' };
                this.log('已提交费用争议');
            }
        },
        async refreshCustomerMessages() {
            const data = await this.request(`${apiBase}/api/messages`);
            if (Array.isArray(data)) {
                this.customer.messages = data;
                this.pagination.messages = 1;
            }
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
        async submitRepairTicket() {
            if (!this.customer.repairForm.rentalId || !this.customer.repairForm.description) {
                alert('请完善报修信息');
                return;
            }
            const photos = this.customer.repairForm.photos
                ? this.customer.repairForm.photos.split(',').map(item => item.trim()).filter(Boolean)
                : [];
            const payload = {
                rentalId: this.customer.repairForm.rentalId,
                description: this.customer.repairForm.description,
                photos
            };
            const data = await this.request(`${apiBase}/api/repairs`, {
                method: 'POST',
                body: JSON.stringify(payload)
            });
            if (data) {
                await this.refreshCustomerRepairs();
                this.log('报修工单已创建');
                this.customer.repairForm = { rentalId: '', description: '', photos: '' };
            }
        },
        async startRental() {
            const ok = confirm(`确认发起租借吗？设备：${this.customer.rentalForm.deviceCode || '未填写'}`);
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
                alert('租借成功，请到订单界面查看');
            }
        },
        async returnRental(id) {
            const ok = confirm('确认归还该租借订单吗？');
            if (!ok) {
                return;
            }
            await this.refreshCustomerDevices();
            const rental = this.customer.rentals.find(item => item.id === id);
            const device = rental
                ? this.customer.devices.find(d => d.deviceCode === rental.deviceCode || d.id === rental.deviceId)
                : null;
            if (device && device.lockStatus && device.lockStatus !== 'LOCKED') {
                alert('请先关闭设备锁，并等待同步后再归还。');
                return;
            }
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
                this.log(`已归还订单：${data.id}`);
            }
        },
        async unlockRental(rental) {
            if (!rental) {
                alert('未找到订单信息');
                return;
            }
            if (!rental.deviceId) {
                alert('该订单缺少设备编号，无法开锁');
                return;
            }
            const ok = confirm(`确认发送开锁指令给设备：${rental.deviceCode || rental.deviceId} 吗？`);
            if (!ok) {
                return;
            }
            const result = await this.request(`${apiBase}/api/devices/${rental.deviceId}/unlock`, { method: 'POST' });
            if (result) {
                const target = rental.deviceCode || rental.deviceId;
                this.log(`已发送开锁指令：${target}`);
                this.logMqtt(`UNLOCK -> ${target}`);
                alert('开锁指令已发送，请稍候查看设备状态。');
            }
        },
        async cancelRentalUser(id) {
            const rental = this.customer.rentals.find(item => item.id === id);
            if (!this.canCancelRental(rental)) {
                alert('租借开始超过5分钟，已无法取消，如需协助请联系管理员。');
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
};
