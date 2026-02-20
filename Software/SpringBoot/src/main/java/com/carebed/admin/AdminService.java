package com.carebed.admin;

import com.carebed.admin.dto.AdminAnalyticsResponse;
import com.carebed.admin.dto.AdminMetricPoint;
import com.carebed.admin.dto.AdminOverviewResponse;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.common.exception.BadRequestException;
import com.carebed.device.Device;
import com.carebed.device.DeviceService;
import com.carebed.device.DeviceStatus;
import com.carebed.repair.RepairService;
import com.carebed.repair.RepairStatus;
import com.carebed.rental.RentalRecord;
import com.carebed.rental.RentalService;
import com.carebed.rental.RentalStatus;
import com.carebed.wallet.TransactionType;
import com.carebed.wallet.WalletService;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.time.LocalDate;
import java.time.ZoneOffset;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@Service
public class AdminService {

    private final AuthService authService;
    private final DeviceService deviceService;
    private final RentalService rentalService;
    private final WalletService walletService;
    private final RepairService repairService;

    public AdminService(AuthService authService,
            DeviceService deviceService,
            RentalService rentalService,
            WalletService walletService,
            RepairService repairService) {
        this.authService = authService;
        this.deviceService = deviceService;
        this.rentalService = rentalService;
        this.walletService = walletService;
        this.repairService = repairService;
    }

    public AdminOverviewResponse overview(String token) {
        ensureAdmin(token);
        List<UserAccount> accounts = authService.allAccounts();
        List<Device> devices = deviceService.allDevices();
        List<RentalRecord> rentals = rentalService.allRentals();
        long totalUsers = accounts.size();
        long familyUsers = accounts.stream().filter(account -> account.role() == UserRole.FAMILY).count();
        long adminUsers = accounts.stream().filter(account -> account.role() == UserRole.ADMIN).count();
        long totalDevices = devices.size();
        long availableDevices = devices.stream().filter(device -> device.status() == DeviceStatus.AVAILABLE).count();
        long inUseDevices = devices.stream().filter(device -> device.status() == DeviceStatus.IN_USE).count();
        long maintenanceDevices = devices.stream().filter(device -> device.status() == DeviceStatus.MAINTENANCE)
                .count();
        long offlineDevices = devices.stream().filter(device -> device.status() == DeviceStatus.OFFLINE).count();
        long activeRentals = rentals.stream().filter(record -> record.status() == RentalStatus.ACTIVE).count();
        long completedRentals = rentals.stream().filter(record -> record.status() == RentalStatus.COMPLETED).count();
        long overdueRentals = rentals.stream().filter(record -> record.status() == RentalStatus.OVERDUE).count();
        long canceledRentals = rentals.stream().filter(record -> record.status() == RentalStatus.CANCELED).count();
        double totalRevenue = walletService.allTransactions().stream()
                .filter(transaction -> transaction.type() == TransactionType.DEBIT)
                .map(transaction -> transaction.amount().abs())
                .reduce(BigDecimal.ZERO.setScale(2, RoundingMode.HALF_UP), BigDecimal::add)
                .doubleValue();
        double totalBalance = walletService.totalBalance().doubleValue();
        long openRepairs = repairService.allTickets().stream()
                .filter(ticket -> ticket.status() == RepairStatus.OPEN || ticket.status() == RepairStatus.IN_PROGRESS)
                .count();
        long resolvedRepairs = repairService.allTickets().stream()
                .filter(ticket -> ticket.status() == RepairStatus.RESOLVED)
                .count();
        return new AdminOverviewResponse(
                totalUsers,
                familyUsers,
                adminUsers,
                totalDevices,
                availableDevices,
                inUseDevices,
                maintenanceDevices,
                offlineDevices,
                activeRentals,
                completedRentals,
                overdueRentals,
                canceledRentals,
                totalRevenue,
                totalBalance,
                openRepairs,
                resolvedRepairs);
    }

    public AdminAnalyticsResponse analytics(String token) {
        ensureAdmin(token);
        List<RentalRecord> rentals = rentalService.allRentals();
        Map<LocalDate, Long> usageByDay = rentals.stream()
                .collect(Collectors.groupingBy(record -> record.startedAt().atZone(ZoneOffset.UTC).toLocalDate(),
                        Collectors.counting()));
        List<AdminMetricPoint> usageTrend = usageByDay.entrySet().stream()
                .sorted(Map.Entry.comparingByKey())
                .map(entry -> new AdminMetricPoint(entry.getKey().atStartOfDay().toInstant(ZoneOffset.UTC),
                        entry.getValue().doubleValue()))
                .toList();
        Map<LocalDate, BigDecimal> revenueByDay = walletService.allTransactions().stream()
                .filter(transaction -> transaction.type() == TransactionType.DEBIT)
                .collect(Collectors.groupingBy(
                        transaction -> transaction.occurredAt().atZone(ZoneOffset.UTC).toLocalDate(),
                        Collectors.reducing(BigDecimal.ZERO.setScale(2, RoundingMode.HALF_UP), tx -> tx.amount().abs(),
                                BigDecimal::add)));
        List<AdminMetricPoint> revenueTrend = revenueByDay.entrySet().stream()
                .sorted(Map.Entry.comparingByKey())
                .map(entry -> new AdminMetricPoint(entry.getKey().atStartOfDay().toInstant(ZoneOffset.UTC),
                        entry.getValue().doubleValue()))
                .toList();
        return new AdminAnalyticsResponse(usageTrend, revenueTrend);
    }

    private void ensureAdmin(String token) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可访问");
        }
    }
}
