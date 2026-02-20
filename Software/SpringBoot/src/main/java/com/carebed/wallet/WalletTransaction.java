package com.carebed.wallet;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

public record WalletTransaction(
        UUID id,
        UUID userId,
        TransactionType type,
        BigDecimal amount,
        String reference,
        String orderId,
        String notes,
        Instant occurredAt) {
}
