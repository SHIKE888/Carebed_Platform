package com.carebed.rental;

import com.carebed.activity.ActivityLogService;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.ResourceNotFoundException;
import com.carebed.common.model.OperationResult;
import com.carebed.device.Device;
import com.carebed.device.DeviceService;
import com.carebed.device.DeviceStatus;
import com.carebed.notification.NotificationService;
import com.carebed.notification.NotificationType;
import com.carebed.rental.dto.RentalResponse;
import com.carebed.rental.dto.RentalReturnRequest;
import com.carebed.rental.dto.RentalStartRequest;
import com.carebed.rental.persistence.RentalRecordEntity;
import com.carebed.rental.persistence.RentalRecordRepository;
import com.carebed.wallet.WalletService;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.util.StringUtils;

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;
import java.time.temporal.ChronoUnit;

@Service
public class RentalService {

    private static final BigDecimal HOURLY_RATE = BigDecimal.valueOf(10);

    private final AuthService authService;
    private final DeviceService deviceService;
    private final WalletService walletService;
    private final NotificationService notificationService;
    private final RentalRecordRepository rentalRecordRepository;
    private final ActivityLogService activityLogService;

    public RentalService(AuthService authService,
            DeviceService deviceService,
            WalletService walletService,
            NotificationService notificationService,
            RentalRecordRepository rentalRecordRepository,
            ActivityLogService activityLogService) {
        this.authService = authService;
        this.deviceService = deviceService;
        this.walletService = walletService;
        this.notificationService = notificationService;
        this.rentalRecordRepository = rentalRecordRepository;
        this.activityLogService = activityLogService;
    }

    @Transactional
    public RentalResponse startRental(String token, RentalStartRequest request) {
        UserAccount account = authService.authenticate(token);
        Device device = deviceService.findByCode(request.deviceCode())
                .orElseThrow(() -> new ResourceNotFoundException("设备不存在"));
        if (device.status() != DeviceStatus.AVAILABLE) {
            throw new BadRequestException("设备当前不可用");
        }
        String patientReference = account.linkedPatientIdOpt()
                .filter(StringUtils::hasText)
                .orElse("未关联患者");
        Instant now = Instant.now();
        Instant expectedEnd = now.plus(request.expectedHours(), ChronoUnit.HOURS);
        deviceService.bindDevice(device.id(), patientReference);
        RentalRecordEntity entity = new RentalRecordEntity();
        entity.setId(UUID.randomUUID());
        entity.setUserId(account.id());
        entity.setDeviceId(device.id());
        entity.setDeviceCode(device.deviceCode());
        entity.setStartedAt(now);
        entity.setEndedAt(null);
        entity.setExpectedEndAt(expectedEnd);
        entity.setStatus(RentalStatus.ACTIVE);
        entity.setAmount(0.0);
        entity.setPatientReference(patientReference);
        entity.setDisputeRaised(false);
        entity.setCreatedAt(now);
        entity.setUpdatedAt(now);
        RentalRecordEntity saved = rentalRecordRepository.save(entity);
        notificationService.notifyUser(account.id(), NotificationType.RENTAL_SUCCESS, "租借成功",
                "已成功租借设备 " + device.deviceCode());
        notificationService.notifyAdmins(NotificationType.RENTAL_SUCCESS, "新增租借",
                account.fullName() + " 租借了设备 " + device.deviceCode());
        activityLogService.record(account.id(), account.fullName(), "RENTAL_START",
                "租借设备 " + device.deviceCode());
        return toResponse(saved);
    }

    @Transactional
    public RentalResponse finishRental(String token, UUID rentalId, RentalReturnRequest request) {
        UserAccount account = authService.authenticate(token);
        RentalRecordEntity record = getRentalEntity(rentalId);
        if (!account.role().equals(UserRole.ADMIN) && !record.getUserId().equals(account.id())) {
            throw new BadRequestException("无权操作该订单");
        }
        if (record.getStatus() != RentalStatus.ACTIVE && record.getStatus() != RentalStatus.OVERDUE) {
            throw new BadRequestException("订单状态不可归还");
        }
        if (request == null || !request.lockConfirmed()) {
            throw new BadRequestException("需确认关锁");
        }
        Instant now = Instant.now();
        Duration duration = Duration.between(record.getStartedAt(), now);
        BigDecimal fee = calculateFee(duration);
        walletService.debit(record.getUserId(), fee, record.getId().toString(), "租借费用");
        deviceService.releaseDevice(record.getDeviceId());
        record.setEndedAt(now);
        record.setStatus(RentalStatus.COMPLETED);
        record.setAmount(fee.doubleValue());
        record.setUpdatedAt(now);
        rentalRecordRepository.save(record);
        notificationService.notifyUser(record.getUserId(), NotificationType.LOCK_CONFIRMED, "关锁确认",
                "订单" + record.getId() + " 已确认归还");
        notificationService.notifyUser(record.getUserId(), NotificationType.PAYMENT_CHARGED, "扣费提醒",
                "租借费用 " + fee.setScale(2, RoundingMode.HALF_UP) + " 元已扣除");
        notificationService.notifyAdmins(NotificationType.PAYMENT_CHARGED, "订单扣费",
                "订单" + record.getId() + " 扣费" + fee.setScale(2, RoundingMode.HALF_UP) + "元");
        activityLogService.record(account.id(), account.fullName(), "RENTAL_FINISH",
                "归还订单 " + record.getId());
        return toResponse(record);
    }

    @Transactional(readOnly = true)
    public List<RentalResponse> listUserRentals(String token) {
        UserAccount account = authService.authenticate(token);
        return rentalRecordRepository.findByUserIdOrderByCreatedAtDesc(account.id()).stream()
                .map(this::toResponse)
                .toList();
    }

    @Transactional(readOnly = true)
    public List<RentalResponse> listAll(String token, Optional<RentalStatus> status) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可查看全部订单");
        }
        return status.map(value -> rentalRecordRepository.findByStatusOrderByCreatedAtDesc(value))
                .orElseGet(() -> rentalRecordRepository.findAll().stream()
                        .sorted((left, right) -> right.getCreatedAt().compareTo(left.getCreatedAt()))
                        .toList())
                .stream()
                .map(this::toResponse)
                .toList();
    }

    @Transactional
    public OperationResult cancelRental(String token, UUID rentalId) {
        UserAccount account = authService.authenticate(token);
        RentalRecordEntity record = getRentalEntity(rentalId);
        if (!account.role().equals(UserRole.ADMIN) && !record.getUserId().equals(account.id())) {
            throw new BadRequestException("无权取消该订单");
        }
        if (record.getStatus() != RentalStatus.ACTIVE) {
            throw new BadRequestException("仅未完成订单可取消");
        }
        deviceService.releaseDevice(record.getDeviceId());
        Instant now = Instant.now();
        record.setEndedAt(now);
        record.setStatus(RentalStatus.CANCELED);
        record.setUpdatedAt(now);
        rentalRecordRepository.save(record);
        notificationService.notifyUser(record.getUserId(), NotificationType.SYSTEM_ALERT, "订单取消",
                "订单" + record.getId() + " 已取消");
        activityLogService.record(account.id(), account.fullName(), "RENTAL_CANCEL",
                "取消订单 " + record.getId());
        return OperationResult.of("订单已取消");
    }

    @Transactional
    public void scanForOverdue() {
        Instant now = Instant.now();
        List<RentalRecordEntity> overdueRecords = rentalRecordRepository
                .findByExpectedEndAtBeforeAndStatus(now, RentalStatus.ACTIVE);
        overdueRecords.forEach(record -> {
            record.setStatus(RentalStatus.OVERDUE);
            record.setUpdatedAt(now);
            rentalRecordRepository.save(record);
            notificationService.notifyUser(record.getUserId(), NotificationType.RENTAL_EXPIRING, "租借即将超时",
                    "订单" + record.getId() + " 已超出预期时间");
        });
    }

    @Transactional(readOnly = true)
    public RentalResponse getRentalResponse(String token, UUID id) {
        UserAccount account = authService.authenticate(token);
        RentalRecordEntity record = getRentalEntity(id);
        if (!account.role().equals(UserRole.ADMIN) && !record.getUserId().equals(account.id())) {
            throw new BadRequestException("无权查看该订单");
        }
        return toResponse(record);
    }

    private RentalRecordEntity getRentalEntity(UUID id) {
        return rentalRecordRepository.findById(id)
                .orElseThrow(() -> new ResourceNotFoundException("订单不存在"));
    }

    public Optional<RentalRecord> findRental(UUID id) {
        return rentalRecordRepository.findById(id).map(this::toDomain);
    }

    public List<RentalRecord> allRentals() {
        return rentalRecordRepository.findAll().stream()
                .map(this::toDomain)
                .toList();
    }

    private RentalResponse toResponse(RentalRecordEntity entity) {
        RentalRecord record = toDomain(entity);
        long duration = record.durationUntil(Instant.now()).toMinutes();
        return new RentalResponse(
                record.id().toString(),
                record.userId().toString(),
                record.deviceId().toString(),
                record.deviceCode(),
                record.status(),
                record.startedAt(),
                record.getEndedAt().orElse(null),
                record.expectedEndAt(),
                record.amount(),
                duration,
                record.patientReference(),
                record.disputeRaised());
    }

    private RentalRecord toDomain(RentalRecordEntity entity) {
        return new RentalRecord(
                entity.getId(),
                entity.getUserId(),
                entity.getDeviceId(),
                entity.getDeviceCode(),
                entity.getStartedAt(),
                entity.getEndedAt(),
                entity.getExpectedEndAt(),
                entity.getStatus(),
                entity.getAmount(),
                entity.getPatientReference(),
                entity.isDisputeRaised(),
                entity.getCreatedAt(),
                entity.getUpdatedAt());
    }

    private BigDecimal calculateFee(Duration duration) {
        long totalMinutes = Math.max(duration.toMinutes(), 1);
        long hours = (totalMinutes + 59) / 60;
        return HOURLY_RATE.multiply(BigDecimal.valueOf(hours)).setScale(2, RoundingMode.HALF_UP);
    }
}
