export const lifecycleOptions = {
    mounted() {
        this.handleResize();
        window.addEventListener('resize', this.handleResize);
        this.captureLockedDeviceFromUrl();
        const restored = this.loadSessionFromStorage();
        this.startRealtimeUpdates();
        if (restored) {
            this.refreshDashboards();
        }
    },
    beforeUnmount() {
        this.stopRealtimeUpdates();
        window.removeEventListener('resize', this.handleResize);
    }
};
