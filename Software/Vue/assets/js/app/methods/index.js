import { interactionMethods } from '../../features/auth-session-methods.js';
import { dashboardMethods } from '../../features/customer-methods.js';
import { adminMethods } from '../../features/admin-methods.js';
import { utilityMethods } from '../../shared/common-methods.js';

export const methodsOptions = {
    ...interactionMethods,
    ...dashboardMethods,
    ...adminMethods,
    ...utilityMethods
};
