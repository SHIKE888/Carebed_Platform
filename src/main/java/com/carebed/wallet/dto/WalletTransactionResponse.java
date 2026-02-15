package com.carebed.wallet.dto;

import com.carebed.wallet.TransactionType;

import java.math.BigDecimal;
import java.time.Instant;

public record WalletTransactionResponse(
        String id,
        TransactionType type,
        BigDecimal amount,
        String reference,
        String orderId,
        String notes,
        Instant occurredAt) {
}
