package com.carebed.wallet.dto;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.List;

public record WalletSnapshotResponse(
        BigDecimal balance,
        Instant updatedAt,
        List<WalletTransactionResponse> transactions) {
}
