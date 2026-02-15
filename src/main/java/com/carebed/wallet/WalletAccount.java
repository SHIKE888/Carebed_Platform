package com.carebed.wallet;

import java.math.BigDecimal;
import java.time.Instant;
import java.util.UUID;

public record WalletAccount(
        UUID userId,
        BigDecimal balance,
        Instant updatedAt) {
    public WalletAccount adjust(BigDecimal delta, Instant now) {
        return new WalletAccount(userId, balance.add(delta), now);
    }
}
