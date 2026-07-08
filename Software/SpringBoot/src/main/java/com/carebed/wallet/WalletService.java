package com.carebed.wallet;

import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.ResourceNotFoundException;
import com.carebed.rental.persistence.RentalRecordRepository;
import com.carebed.wallet.dto.DisputeRequest;
import com.carebed.wallet.dto.DisputeResponse;
import com.carebed.wallet.dto.DisputeUpdateRequest;
import com.carebed.wallet.dto.WalletSnapshotResponse;
import com.carebed.wallet.dto.WalletTransactionResponse;
import com.carebed.wallet.persistence.WalletAccountEntity;
import com.carebed.wallet.persistence.WalletAccountRepository;
import com.carebed.wallet.persistence.WalletDisputeEntity;
import com.carebed.wallet.persistence.WalletDisputeRepository;
import com.carebed.wallet.persistence.WalletTransactionEntity;
import com.carebed.wallet.persistence.WalletTransactionRepository;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
public class WalletService {

    private final WalletAccountRepository walletAccountRepository;
    private final WalletTransactionRepository walletTransactionRepository;
    private final WalletDisputeRepository walletDisputeRepository;
    private final RentalRecordRepository rentalRecordRepository;

    public WalletService(WalletAccountRepository walletAccountRepository,
            WalletTransactionRepository walletTransactionRepository,
            WalletDisputeRepository walletDisputeRepository,
            RentalRecordRepository rentalRecordRepository) {
        this.walletAccountRepository = walletAccountRepository;
        this.walletTransactionRepository = walletTransactionRepository;
        this.walletDisputeRepository = walletDisputeRepository;
        this.rentalRecordRepository = rentalRecordRepository;
    }

    @Transactional(readOnly = true)
    public WalletSnapshotResponse snapshot(UUID userId) {
        WalletAccount account = ensureAccount(userId);
        List<WalletTransactionResponse> records = walletTransactionRepository
                .findByUserIdOrderByOccurredAtDesc(userId)
                .stream()
                .map(this::toDomain)
                .map(this::toResponse)
                .toList();
        return new WalletSnapshotResponse(account.balance(), account.updatedAt(), records);
    }

    public WalletAccount ensureAccount(UUID userId) {
        return toDomain(ensureAccountEntity(userId));
    }

    @Transactional
    public WalletTransaction recharge(UUID userId, BigDecimal amount, String reference) {
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new BadRequestException("充值金额必须大于0");
        }
        WalletAccountEntity account = ensureAccountEntity(userId);
        Instant now = Instant.now();
        account.setBalance(account.getBalance().add(amount));
        account.setUpdatedAt(now);
        walletAccountRepository.save(account);
        WalletTransactionEntity entity = createTransaction(userId, TransactionType.RECHARGE, amount, reference, null,
                "用户充值", now);
        return toDomain(walletTransactionRepository.save(entity));
    }

    @Transactional
    public WalletTransaction debit(UUID userId, BigDecimal amount, String orderId, String reference) {
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new BadRequestException("扣费金额必须大于0");
        }
        WalletAccountEntity account = ensureAccountEntity(userId);
        if (account.getBalance().compareTo(amount) < 0) {
            throw new BadRequestException("余额不足，请充值");
        }
        Instant now = Instant.now();
        account.setBalance(account.getBalance().subtract(amount));
        account.setUpdatedAt(now);
        walletAccountRepository.save(account);
        WalletTransactionEntity entity = createTransaction(userId, TransactionType.DEBIT, amount.negate(), reference,
                orderId, "租借自动扣费", now);
        return toDomain(walletTransactionRepository.save(entity));
    }

    @Transactional
    public WalletTransaction refund(UUID userId, BigDecimal amount, String orderId, String reason) {
        if (amount.compareTo(BigDecimal.ZERO) <= 0) {
            throw new BadRequestException("退款金额必须大于0");
        }
        WalletAccountEntity account = ensureAccountEntity(userId);
        Instant now = Instant.now();
        account.setBalance(account.getBalance().add(amount));
        account.setUpdatedAt(now);
        walletAccountRepository.save(account);
        WalletTransactionEntity entity = createTransaction(userId, TransactionType.REFUND, amount, reason, orderId,
                "租借退款", now);
        return toDomain(walletTransactionRepository.save(entity));
    }

    @Transactional
    public DisputeResponse createDispute(UUID userId, DisputeRequest request) {
        Instant now = Instant.now();
        double orderAmount = 0.0;
        String orderId = request.orderId();
        try {
            java.util.UUID uuid = java.util.UUID.fromString(orderId);
            var rentalOpt = rentalRecordRepository.findById(uuid);
            if (rentalOpt.isPresent()) {
                orderAmount = rentalOpt.get().getAmount();
            }
        } catch (Exception ignored) {
        }
        WalletDisputeEntity entity = new WalletDisputeEntity();
        entity.setId(UUID.randomUUID());
        entity.setUserId(userId);
        entity.setOrderId(orderId);
        entity.setReason(request.reason());
        entity.setOrderAmount(orderAmount);
        entity.setRefunded(false);
        entity.setStatus(DisputeStatus.OPEN);
        entity.setResolution(null);
        entity.setCreatedAt(now);
        entity.setUpdatedAt(now);
        return toResponse(walletDisputeRepository.save(entity));
    }

    @Transactional(readOnly = true)
    public List<DisputeResponse> listDisputes(Optional<DisputeStatus> status) {
        List<WalletDisputeEntity> source = status
                .map(value -> walletDisputeRepository.findByStatusOrderByCreatedAtDesc(value))
                .orElseGet(() -> walletDisputeRepository.findAllByOrderByCreatedAtDesc());
        return source.stream().map(this::toResponse).toList();
    }

    @Transactional(readOnly = true)
    public List<DisputeResponse> listUserDisputes(UUID userId) {
        return walletDisputeRepository.findByUserIdOrderByCreatedAtDesc(userId).stream()
                .map(this::toResponse)
                .toList();
    }

    @Transactional
    public DisputeResponse updateDispute(UUID disputeId, DisputeUpdateRequest request) {
        WalletDisputeEntity entity = walletDisputeRepository.findById(disputeId)
                .orElseThrow(() -> new ResourceNotFoundException("申诉单不存在"));
        Instant now = Instant.now();
        entity.setStatus(request.status());
        entity.setResolution(request.resolution());
        entity.setUpdatedAt(now);
        return toResponse(walletDisputeRepository.save(entity));
    }

    private WalletTransactionResponse toResponse(WalletTransaction transaction) {
        return new WalletTransactionResponse(
                transaction.id().toString(),
                transaction.type(),
                transaction.amount(),
                transaction.reference(),
                transaction.orderId(),
                transaction.notes(),
                transaction.occurredAt());
    }

    private DisputeResponse toResponse(WalletDisputeEntity dispute) {
        return new DisputeResponse(
                dispute.getId().toString(),
                dispute.getUserId().toString(),
                dispute.getOrderId(),
                dispute.getReason(),
                dispute.getOrderAmount(),
                dispute.isRefunded(),
                dispute.getStatus(),
                dispute.getResolution(),
                dispute.getCreatedAt(),
                dispute.getUpdatedAt());
    }

    private WalletTransaction toDomain(WalletTransactionEntity entity) {
        return new WalletTransaction(
                entity.getId(),
                entity.getUserId(),
                entity.getType(),
                entity.getAmount(),
                entity.getReference(),
                entity.getOrderId(),
                entity.getNotes(),
                entity.getOccurredAt());
    }

    private WalletAccount toDomain(WalletAccountEntity entity) {
        return new WalletAccount(entity.getUserId(), entity.getBalance(), entity.getUpdatedAt());
    }

    private WalletDispute toDomainDispute(WalletDisputeEntity entity) {
        return new WalletDispute(
                entity.getId(),
                entity.getUserId(),
                entity.getOrderId(),
                entity.getReason(),
                entity.getOrderAmount(),
                entity.isRefunded(),
                entity.getStatus(),
                entity.getResolution(),
                entity.getCreatedAt(),
                entity.getUpdatedAt());
    }

    public BigDecimal totalBalance() {
        return walletAccountRepository.findAll().stream()
                .map(WalletAccountEntity::getBalance)
                .reduce(BigDecimal.ZERO.setScale(2, RoundingMode.HALF_UP), BigDecimal::add);
    }

    public List<WalletTransaction> allTransactions() {
        return walletTransactionRepository.findAllByOrderByOccurredAtDesc().stream()
                .map(this::toDomain)
                .toList();
    }

    public List<WalletDispute> allDisputes() {
        return walletDisputeRepository.findAll().stream()
                .map(this::toDomainDispute)
                .toList();
    }

    @Transactional
    public void disputeRefund(UUID disputeId, BigDecimal refundAmount) {
        WalletDisputeEntity dispute = walletDisputeRepository.findById(disputeId)
                .orElseThrow(() -> new ResourceNotFoundException("申诉单不存在"));
        if (dispute.getStatus() == DisputeStatus.RESOLVED || dispute.getStatus() == DisputeStatus.REJECTED) {
            throw new BadRequestException("该争议已处理，不可再退费");
        }
        if (dispute.isRefunded()) {
            throw new BadRequestException("该争议已退费，不可重复退费");
        }
        if (refundAmount.compareTo(BigDecimal.valueOf(dispute.getOrderAmount())) > 0) {
            throw new BadRequestException("退费金额不能超过订单金额 " + dispute.getOrderAmount() + " 元");
        }
        UUID userId = dispute.getUserId();
        refund(userId, refundAmount, dispute.getOrderId(), "争议退费");
        dispute.setRefunded(true);
        dispute.setStatus(DisputeStatus.RESOLVED);
        dispute.setResolution("已退费 " + refundAmount + " 元");
        dispute.setUpdatedAt(Instant.now());
        walletDisputeRepository.save(dispute);
    }

    @Transactional
    public void setBalance(UUID userId, BigDecimal newBalance) {
        WalletAccountEntity account = ensureAccountEntity(userId);
        Instant now = Instant.now();
        BigDecimal delta = newBalance.subtract(account.getBalance());
        account.setBalance(newBalance);
        account.setUpdatedAt(now);
        walletAccountRepository.save(account);
        WalletTransactionEntity entity = createTransaction(
                userId, TransactionType.ADJUSTMENT, delta, "管理员调整", null, "管理员调整余额", now);
        walletTransactionRepository.save(entity);
    }

    private WalletAccountEntity ensureAccountEntity(UUID userId) {
        return walletAccountRepository.findById(userId)
                .orElseGet(() -> {
                    WalletAccountEntity entity = new WalletAccountEntity();
                    entity.setUserId(userId);
                    entity.setBalance(BigDecimal.ZERO.setScale(2, RoundingMode.HALF_UP));
                    entity.setUpdatedAt(Instant.now());
                    return walletAccountRepository.save(entity);
                });
    }

    private WalletTransactionEntity createTransaction(UUID userId, TransactionType type, BigDecimal amount,
            String reference, String orderId, String notes, Instant occurredAt) {
        WalletTransactionEntity entity = new WalletTransactionEntity();
        entity.setId(UUID.randomUUID());
        entity.setUserId(userId);
        entity.setType(type);
        entity.setAmount(amount.setScale(2, RoundingMode.HALF_UP));
        entity.setReference(reference);
        entity.setOrderId(orderId);
        entity.setNotes(notes);
        entity.setOccurredAt(occurredAt);
        return entity;
    }
}
