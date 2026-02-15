package com.carebed.repair.persistence;

import com.carebed.repair.RepairStatus;
import org.springframework.data.jpa.repository.JpaRepository;

import java.util.List;
import java.util.UUID;

public interface RepairTicketRepository extends JpaRepository<RepairTicketEntity, UUID> {

    List<RepairTicketEntity> findByUserIdOrderByCreatedAtDesc(UUID userId);

    List<RepairTicketEntity> findByStatusOrderByCreatedAtDesc(RepairStatus status);

    List<RepairTicketEntity> findAllByOrderByCreatedAtDesc();
}
