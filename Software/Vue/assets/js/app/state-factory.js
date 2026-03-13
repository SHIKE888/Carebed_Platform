import { defaultLoginForm, defaultRegisterForm, defaultCustomerState, defaultAdminState } from '../core/app-config.js';

export function createInitialState() {
        return {
            authCard: 'login',
            loginForm: defaultLoginForm(),
            registerForm: defaultRegisterForm(),
            session: {
                token: null,
                role: null,
                displayName: null
            },
            activeArea: 'customer',
            customerLoaded: false,
            adminLoaded: false,
            enums: {
                deviceStatuses: ['AVAILABLE', 'IN_USE', 'MAINTENANCE', 'OFFLINE'],
                rentalStatuses: ['ACTIVE', 'COMPLETED', 'OVERDUE', 'CANCELED'],
                disputeStatuses: ['OPEN', 'IN_REVIEW', 'RESOLVED', 'REJECTED'],
                repairStatuses: ['OPEN', 'IN_PROGRESS', 'RESOLVED', 'REJECTED']
            },
            customer: defaultCustomerState(),
            admin: defaultAdminState(),
            logs: [],
            mqttLogs: [],
            deviceSnapshot: {},
            deviceSnapshotInitialized: false,
            beijingTime: '',
            refreshIntervalMs: 30000,
            autoRefreshTimer: null,
            clockTimer: null,
            isAutoRefreshing: false,
            now: new Date(),
            expandedRentals: [],
            lockedDeviceCode: '',
            mobileTab: 'devices',
            isMobile: false,
            sessionExpiredHandling: false,
            pageSize: 6,
            pagination: {
                devices: 1,
                rentals: 1,
                repairs: 1,
                messages: 1,
                transactions: 1
            }
        };
}
