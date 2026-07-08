export const apiBase = 'https://msas.absozero.cn';
//export const apiBase = 'http://192.168.242.170:8081';

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

export const defaultResetForm = () => ({
    username: '',
    phone: '',
    newPassword: ''
});

export const defaultCustomerState = () => ({
        devices: [],
        rentals: [],
        wallet: null,
        rechargeAmount: 200,
        deviceSearchKeyword: '',
        disputeForm: { orderId: '', reason: '' },
        disputeStatusFilter: '',
        disputes: [],
        messages: [],
        messageFilter: 'all',
        repairs: [],
        repairForm: { rentalId: '', description: '', photos: [] },
        rentalForm: { deviceCode: '', expectedHours: 24 },
        showDeviceSelect: false,
        showRentalSelect: false,
        showOrderSelect: false
});

export const defaultAdminState = () => ({
    overview: null,
    analytics: null,
    devices: [],
    deviceFilter: '',
    deviceSearchKeyword: '',
    deviceEditModalVisible: false,
    newDevice: { deviceCode: '', ward: '', bedNumber: '' },
    edit: { id: '', deviceCode: '', ward: '', bedNumber: '', status: 'AVAILABLE' },
    actionDeviceId: '',
    faultForm: { description: '' },
    rentals: [],
    rentalStatusFilter: '',
    disputes: [],
    disputeStatusFilter: '',
    disputeEditModalVisible: false,
    disputeUpdate: { id: '', status: 'IN_REVIEW', resolution: '' },
    repairs: [],
    repairStatusFilter: '',
    repairEditModalVisible: false,
    repairUpdate: { id: '', status: 'IN_PROGRESS', resolution: '' },
    users: [],
    userSearchKeyword: '',
    userEditModalVisible: false,
    userEditForm: {
        id: '',
        username: '',
        fullName: '',
        phone: '',
        newPassword: '',
        newBalance: '',
        showResetPwd: false,
        showSetBalance: false,
        showDelete: false
    },
    adminMessages: []
});