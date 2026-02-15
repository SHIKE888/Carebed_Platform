package com.carebed.admin.dto;

public record AdminOverviewResponse(
        long totalUsers,
        long familyUsers,
        long adminUsers,
        long totalDevices,
        long availableDevices,
        long inUseDevices,
        long maintenanceDevices,
        long offlineDevices,
        long activeRentals,
        long completedRentals,
        long overdueRentals,
        long canceledRentals,
        double totalRevenue,
        double walletBalance,
        long openRepairs,
        long resolvedRepairs) {
}
