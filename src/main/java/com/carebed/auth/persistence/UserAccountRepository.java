package com.carebed.auth.persistence;

import org.springframework.data.jpa.repository.JpaRepository;

import java.util.Optional;
import java.util.UUID;

public interface UserAccountRepository extends JpaRepository<UserAccountEntity, UUID> {

    Optional<UserAccountEntity> findByUsernameIgnoreCase(String username);

    boolean existsByRole(com.carebed.auth.UserRole role);

    java.util.List<UserAccountEntity> findByRole(com.carebed.auth.UserRole role);

    java.util.List<UserAccountEntity> findByLinkedPatientIdIgnoreCase(String linkedPatientId);
}
