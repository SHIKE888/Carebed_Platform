import { interactionMethods } from '../../features/auth-session-methods.js?v=7';
import { dashboardMethods } from '../../features/customer-methods.js?v=7';
import { adminMethods } from '../../features/admin-methods.js?v=7';
import { utilityMethods } from '../../shared/common-methods.js?v=7';

export const methodsOptions = {
    ...interactionMethods,
    ...dashboardMethods,
    ...adminMethods,
    ...utilityMethods
};
