package com.carebed.rental.persistence;

import com.carebed.rental.RentalStatus;
import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.EnumType;
import jakarta.persistence.Enumerated;
import jakarta.persistence.Id;
import jakarta.persistence.Index;
import jakarta.persistence.Table;

import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "rental_records", indexes = {
        @Index(name = "idx_rental_user", columnList = "user_id"),
        @Index(name = "idx_rental_status", columnList = "status")
})
public class RentalRecordEntity {

    @Id
    @Column(nullable = false, updatable = false, columnDefinition = "BINARY(16)")
    private UUID id;

    @Column(name = "user_id", nullable = false, columnDefinition = "BINARY(16)")
    private UUID userId;

    @Column(name = "device_id", nullable = false, columnDefinition = "BINARY(16)")
    private UUID deviceId;

    @Column(nullable = false, length = 64)
    private String deviceCode;

    @Column(nullable = false)
    private Instant startedAt;

    @Column
    private Instant endedAt;

    @Column(nullable = false)
    private Instant expectedEndAt;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 32)
    private RentalStatus status;

    @Column(nullable = false)
    private double amount;

    @Column(length = 128)
    private String patientReference;

    @Column(nullable = false)
    private boolean disputeRaised;

    @Column(nullable = false)
    private Instant createdAt;

    @Column(nullable = false)
    private Instant updatedAt;

    public UUID getId() {
        return id;
    }

    public void setId(UUID id) {
        this.id = id;
    }

    public UUID getUserId() {
        return userId;
    }

    public void setUserId(UUID userId) {
        this.userId = userId;
    }

    public UUID getDeviceId() {
        return deviceId;
    }

    public void setDeviceId(UUID deviceId) {
        this.deviceId = deviceId;
    }

    public String getDeviceCode() {
        return deviceCode;
    }

    public void setDeviceCode(String deviceCode) {
        this.deviceCode = deviceCode;
    }

    public Instant getStartedAt() {
        return startedAt;
    }

    public void setStartedAt(Instant startedAt) {
        this.startedAt = startedAt;
    }

    public Instant getEndedAt() {
        return endedAt;
    }

    public void setEndedAt(Instant endedAt) {
        this.endedAt = endedAt;
    }

    public Instant getExpectedEndAt() {
        return expectedEndAt;
    }

    public void setExpectedEndAt(Instant expectedEndAt) {
        this.expectedEndAt = expectedEndAt;
    }

    public RentalStatus getStatus() {
        return status;
    }

    public void setStatus(RentalStatus status) {
        this.status = status;
    }

    public double getAmount() {
        return amount;
    }

    public void setAmount(double amount) {
        this.amount = amount;
    }

    public String getPatientReference() {
        return patientReference;
    }

    public void setPatientReference(String patientReference) {
        this.patientReference = patientReference;
    }

    public boolean isDisputeRaised() {
        return disputeRaised;
    }

    public void setDisputeRaised(boolean disputeRaised) {
        this.disputeRaised = disputeRaised;
    }

    public Instant getCreatedAt() {
        return createdAt;
    }

    public void setCreatedAt(Instant createdAt) {
        this.createdAt = createdAt;
    }

    public Instant getUpdatedAt() {
        return updatedAt;
    }

    public void setUpdatedAt(Instant updatedAt) {
        this.updatedAt = updatedAt;
    }
}
