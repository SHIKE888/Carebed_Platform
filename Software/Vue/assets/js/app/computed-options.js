export const computedOptions = {
        canSeeCustomer() {
            return this.session.role === 'FAMILY' || this.session.role === 'ADMIN';
        },
        
        // 设备选择显示文本
        selectedDeviceText() {
            if (!this.customer.rentalForm.deviceCode) {
                return '';
            }
            const device = this.customer.devices.find(d => d.deviceCode === this.customer.rentalForm.deviceCode);
            if (device) {
                return `${device.deviceCode} (${device.ward}·床位 ${device.bedNumber})`;
            }
            return this.customer.rentalForm.deviceCode;
        },
        
        // 租借订单选择显示文本
        selectedRentalText() {
            if (!this.customer.repairForm.rentalId) {
                return '';
            }
            const rental = this.customer.rentals.find(r => r.id === this.customer.repairForm.rentalId);
            if (rental) {
                return `${rental.deviceCode} · ${this.displayRentalStatus(rental.status)} · ${this.formatTime(rental.startedAt || rental.createdAt)}`;
            }
            return this.customer.repairForm.rentalId;
        },
        
        // 订单号选择显示文本
        selectedOrderText() {
            if (!this.customer.disputeForm.orderId) {
                return '';
            }
            const rental = this.customer.rentals.find(r => r.id === this.customer.disputeForm.orderId);
            if (rental) {
                return `${this.rentalShortId(rental.id)} · ${this.displayRentalStatus(rental.status)} · ${this.formatTime(rental.startedAt || rental.createdAt)}`;
            }
            return this.customer.disputeForm.orderId;
        },

        canSeeAdmin() {
            return this.session.role === 'ADMIN';
        },
        isAdmin() {
            return this.session.role === 'ADMIN';
        },
        filteredCustomerMessages() {
            const messages = Array.isArray(this.customer.messages) ? this.customer.messages : [];
            if (this.customer.messageFilter === 'unread') {
                return messages.filter(msg => !msg.read);
            }
            return messages;
        },
        pagedCustomerMessages() {
            return this.paginateList(this.filteredCustomerMessages, this.pagination.messages, this.pageSize);
        },
        pagedWalletTransactions() {
            const txns = this.customer.wallet && Array.isArray(this.customer.wallet.transactions)
                ? this.customer.wallet.transactions
                : [];
            return this.paginateList(txns, this.pagination.transactions, this.pageSize);
        },
        filteredCustomerDisputes() {
            const disputes = Array.isArray(this.customer.disputes) ? this.customer.disputes : [];
            const filter = this.customer.disputeStatusFilter;
            if (!filter) {
                return disputes;
            }
            if (filter === 'RESOLVED') {
                return disputes.filter(dispute => dispute.status === 'RESOLVED' || dispute.status === 'REJECTED');
            }
            return disputes.filter(dispute => dispute.status === filter);
        },
        filteredAdminDevices() {
            let filtered = this.admin.devices;
            
            // 按状态筛选
            if (this.admin.deviceFilter) {
                filtered = filtered.filter(device => device.status === this.admin.deviceFilter);
            }
            
            // 按搜索关键词筛选
            if (this.admin.deviceSearchKeyword && this.admin.deviceSearchKeyword.trim()) {
                const keyword = this.admin.deviceSearchKeyword.toLowerCase().trim();
                filtered = filtered.filter(device => 
                    device.deviceCode.toLowerCase().includes(keyword) || 
                    device.ward.toLowerCase().includes(keyword)
                );
            }
            
            return filtered;
        },
        filteredCustomerDevices() {
            const devices = Array.isArray(this.customer.devices) ? this.customer.devices : [];
            let filtered = devices.filter(device => device.status === 'AVAILABLE');
            
            // 按搜索关键词筛选
            if (this.customer.deviceSearchKeyword && this.customer.deviceSearchKeyword.trim()) {
                const keyword = this.customer.deviceSearchKeyword.toLowerCase().trim();
                filtered = filtered.filter(device =>
                    device.deviceCode.toLowerCase().includes(keyword) ||
                    device.ward.toLowerCase().includes(keyword)
                );
            }
            
            return filtered;
        },
        pagedCustomerDevices() {
            return this.paginateList(this.filteredCustomerDevices, this.pagination.devices, this.pageSize);
        },
        pagedCustomerRentals() {
            return this.paginateList(this.customer.rentals, this.pagination.rentals, this.pageSize);
        },
        pagedCustomerRepairs() {
            return this.paginateList(this.customer.repairs, this.pagination.repairs, this.pageSize);
        },
        filteredAdminRentals() {
            if (!this.admin.rentalStatusFilter) {
                return this.admin.rentals;
            }
            return this.admin.rentals.filter(rental => rental.status === this.admin.rentalStatusFilter);
        },
        filteredAdminDisputes() {
            const disputes = Array.isArray(this.admin.disputes) ? this.admin.disputes : [];
            if (!this.admin.disputeStatusFilter) {
                return disputes;
            }
            return disputes.filter(dispute => dispute.status === this.admin.disputeStatusFilter);
        },
        currentAdminDispute() {
            if (!this.admin.disputeUpdate || !this.admin.disputeUpdate.id) {
                return null;
            }
            return this.admin.disputes.find(dispute => dispute.id === this.admin.disputeUpdate.id) || null;
        },
        filteredAdminRepairs() {
            const repairs = Array.isArray(this.admin.repairs) ? this.admin.repairs : [];
            if (!this.admin.repairStatusFilter) {
                return repairs;
            }
            return repairs.filter(ticket => ticket.status === this.admin.repairStatusFilter);
        },
        currentAdminRepair() {
            if (!this.admin.repairUpdate || !this.admin.repairUpdate.id) {
                return null;
            }
            return this.admin.repairs.find(ticket => ticket.id === this.admin.repairUpdate.id) || null;
        },
        pagedAdminRentals() {
            return this.paginateList(this.filteredAdminRentals, this.pagination.adminRentals || 1, this.pageSize);
        },
        pagedAdminDisputes() {
            return this.paginateList(this.filteredAdminDisputes, this.pagination.adminDisputes || 1, this.pageSize);
        },
        pagedAdminRepairs() {
            return this.paginateList(this.filteredAdminRepairs, this.pagination.adminRepairs || 1, this.pageSize);
        },
        adminRentalsTotalPages() {
            return this.totalPages(this.filteredAdminRentals);
        },
        adminDisputesTotalPages() {
            return this.totalPages(this.filteredAdminDisputes);
        },
        adminRepairsTotalPages() {
            return this.totalPages(this.filteredAdminRepairs);
        },
        pagedAdminMessages() {
            return this.paginateList(this.admin.adminMessages, this.pagination.adminMessages || 1, this.pageSize);
        },
        adminMessagesTotalPages() {
            return this.totalPages(this.admin.adminMessages);
        },
        filteredAdminUsers() {
            let filtered = this.admin.users;
            if (this.admin.userSearchKeyword && this.admin.userSearchKeyword.trim()) {
                const keyword = this.admin.userSearchKeyword.toLowerCase().trim();
                filtered = filtered.filter(user => 
                    (user.fullName && user.fullName.toLowerCase().includes(keyword)) || 
                    (user.username && user.username.toLowerCase().includes(keyword)) ||
                    (user.phone && user.phone.toLowerCase().includes(keyword))
                );
            }
            return filtered;
        },
        pagedAdminUsers() {
            return this.paginateList(this.filteredAdminUsers, this.pagination.adminUsers || 1, 10);
        },
        adminUsersTotalPages() {
            return this.totalPages(this.filteredAdminUsers, 10);
        },
        tokenPreview() {
            if (!this.session.token) {
                return '未登录';
            }
            const token = this.session.token;
            if (token.length <= 15) {
                return token;
            }
            return `${token.slice(0, 6)}...${token.slice(-4)}`;
        },
        sessionHighlights() {
            const items = [];
            if (this.activeArea === 'customer') {
                if (this.customer.devices.length) {
                    items.push(`可查看设备 ${this.customer.devices.length} 台`);
                }
                if (this.customer.wallet) {
                    items.push(`余额 ${this.formatNumber(this.customer.wallet.balance)} 元`);
                }
                if (this.customer.rentals.length) {
                    items.push(`租借订单 ${this.customer.rentals.length} 个`);
                }
            } else {
                if (this.admin.devices.length) {
                    items.push(`设备总数 ${this.admin.devices.length}`);
                }
                if (this.admin.rentals.length) {
                    items.push(`租借订单 ${this.admin.rentals.length}`);
                }
                if (this.admin.users.length) {
                    items.push(`用户数 ${this.admin.users.length}`);
                }
            }
            return items;
        },
        mobileNavVisible() {
            return this.session.token && this.isMobile;
        },
        showWorkspaceCard() {
            if (this.activeArea === 'admin') {
                return true;
            }
            return !this.isMobile || this.mobileTab === 'mine';
        },
        showLogCards() {
            if (this.activeArea === 'admin') {
                return true;
            }
            return !this.isMobile || this.mobileTab === 'mine';
        },
        showTokenCard() {
            if (this.activeArea === 'admin') {
                return !this.isMobile || this.adminMobileTab === 'overview';
            }
            return !this.isMobile || this.mobileTab === 'mine';
        },
        devicesTotalPages() {
            return this.totalPages(this.filteredCustomerDevices);
        },
        rentalsTotalPages() {
            return this.totalPages(this.customer.rentals);
        },
        repairsTotalPages() {
            return this.totalPages(this.customer.repairs);
        },
        messagesTotalPages() {
            return this.totalPages(this.filteredCustomerMessages);
        },
        transactionsTotalPages() {
            const txns = this.customer.wallet && Array.isArray(this.customer.wallet.transactions)
                ? this.customer.wallet.transactions
                : [];
            return this.totalPages(txns);
        },
        filteredCustomerRepairHistory() {
            // 显示所有报修单（除非有删除标记），让用户能看到所有历史记录
            const repairs = Array.isArray(this.customer.repairs) ? this.customer.repairs : [];
            // 如需排除已删除的，可加条件：!['DELETED', '已删除'].includes(repair.status)
            return repairs;
        }
};
