package com.carebed.auth.persistence;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;

import java.util.Optional;
import java.util.UUID;

public interface UserAccountRepository extends JpaRepository<UserAccountEntity, UUID> {

    Optional<UserAccountEntity> findByUsernameIgnoreCase(String username);

    boolean existsByRole(com.carebed.auth.UserRole role);

    java.util.List<UserAccountEntity> findByRole(com.carebed.auth.UserRole role);

    java.util.List<UserAccountEntity> findByLinkedPatientIdIgnoreCase(String linkedPatientId);

    Optional<UserAccountEntity> findByUsernameIgnoreCaseAndPhone(String username, String phone);

    @Query("SELECT u FROM UserAccountEntity u WHERE "
            + "(:keyword IS NULL OR LOWER(u.username) LIKE LOWER(CONCAT('%', :keyword, '%')) "
            + "OR LOWER(u.fullName) LIKE LOWER(CONCAT('%', :keyword, '%')) "
            + "OR LOWER(u.phone) LIKE LOWER(CONCAT('%', :keyword, '%')))")
    java.util.List<UserAccountEntity> searchByKeyword(@Param("keyword") String keyword);
}
