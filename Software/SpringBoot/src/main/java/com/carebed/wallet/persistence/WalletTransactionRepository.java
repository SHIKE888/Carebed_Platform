package com.carebed.wallet.persistence;

import com.carebed.wallet.TransactionType;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.UUID;

public interface WalletTransactionRepository extends JpaRepository<WalletTransactionEntity, UUID> {

    List<WalletTransactionEntity> findByUserIdOrderByOccurredAtDesc(UUID userId);

    List<WalletTransactionEntity> findByType(TransactionType type);

    List<WalletTransactionEntity> findAllByOrderByOccurredAtDesc();
}
