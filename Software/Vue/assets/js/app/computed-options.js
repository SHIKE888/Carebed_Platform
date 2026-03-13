export const computedOptions = {
        canSeeCustomer() {
            return this.session.role === 'FAMILY' || this.session.role === 'ADMIN';
        },
        canSeeAdmin() {
            return this.session.role === 'ADMIN';
        },
        isAdmin() {
            return this.session.role === 'ADMIN';
        },
        filteredCustomerMessages() {
            if (this.customer.messageFilter === 'unread') {
                return this.customer.messages.filter(msg => !msg.read);
            }
            return this.customer.messages;
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
        filteredAdminDevices() {
            if (!this.admin.deviceFilter) {
                return this.admin.devices;
            }
            return this.admin.devices.filter(device => device.status === this.admin.deviceFilter);
        },
        pagedCustomerDevices() {
            return this.paginateList(this.customer.devices, this.pagination.devices, this.pageSize);
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
            return this.session.token && this.activeArea === 'customer' && this.isMobile;
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
                return true;
            }
            return !this.isMobile || this.mobileTab === 'mine';
        },
        devicesTotalPages() {
            return this.totalPages(this.customer.devices);
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
        }
};
