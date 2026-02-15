package com.carebed.wallet.persistence;

import org.springframework.data.jpa.repository.JpaRepository;

import java.util.UUID;

public interface WalletAccountRepository extends JpaRepository<WalletAccountEntity, UUID> {
}
