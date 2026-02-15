package com.carebed.rental.persistence;

import com.carebed.rental.RentalStatus;
import org.springframework.data.jpa.repository.JpaRepository;

import java.time.Instant;
import java.util.List;
import java.util.UUID;

public interface RentalRecordRepository extends JpaRepository<RentalRecordEntity, UUID> {

    List<RentalRecordEntity> findByUserIdOrderByCreatedAtDesc(UUID userId);

    List<RentalRecordEntity> findByStatusOrderByCreatedAtDesc(RentalStatus status);

    List<RentalRecordEntity> findByStatus(RentalStatus status);

    List<RentalRecordEntity> findByExpectedEndAtBeforeAndStatus(Instant cutoff, RentalStatus status);
}
