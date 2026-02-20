package com.carebed.wallet.persistence;

import com.carebed.wallet.DisputeStatus;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.UUID;

public interface WalletDisputeRepository extends JpaRepository<WalletDisputeEntity, UUID> {

    List<WalletDisputeEntity> findByStatusOrderByCreatedAtDesc(DisputeStatus status);

    List<WalletDisputeEntity> findAllByOrderByCreatedAtDesc();
}
