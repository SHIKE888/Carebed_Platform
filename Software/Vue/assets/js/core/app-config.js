export const apiBase = 'https://msas.absozero.cn';

export const defaultLoginForm = () => ({
    username: '',
    password: ''
});

export const defaultRegisterForm = () => ({
    username: '',
    password: '',
    role: 'FAMILY',
    fullName: '',
    phone: '',
    adminUsername: '',
    adminPassword: ''
});

export const defaultCustomerState = () => ({
    devices: [],
    rentals: [],
    wallet: null,
    rechargeAmount: 200,
    disputeForm: { orderId: '', reason: '' },
    disputes: [],
    messages: [],
    messageFilter: 'all',
    repairs: [],
    repairForm: { rentalId: '', description: '', photos: '' },
    rentalForm: { deviceCode: 'BED-0001', expectedHours: 4 }
});

export const defaultAdminState = () => ({
    overview: null,
    analytics: null,
    devices: [],
    deviceFilter: '',
    newDevice: { deviceCode: '', ward: '', bedNumber: '' },
    edit: { id: '', deviceCode: '', ward: '', bedNumber: '', status: 'AVAILABLE' },
    actionDeviceId: '',
    faultForm: { description: '' },
    rentals: [],
    rentalStatusFilter: '',
    disputes: [],
    disputeUpdate: { id: '', status: 'IN_REVIEW', resolution: '' },
    repairs: [],
    repairUpdate: { id: '', status: 'IN_PROGRESS', resolution: '' },
    users: [],
    adminMessages: []
});