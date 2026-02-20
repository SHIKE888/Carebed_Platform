package com.carebed.auth;

import com.carebed.auth.dto.AuthResponse;
import com.carebed.auth.dto.LoginRequest;
import com.carebed.auth.dto.PatientLinkRequest;
import com.carebed.auth.dto.RegisterRequest;
import com.carebed.auth.dto.UserProfileResponse;
import com.carebed.auth.persistence.UserAccountEntity;
import com.carebed.auth.persistence.UserAccountRepository;
import com.carebed.auth.persistence.UserSessionEntity;
import com.carebed.auth.persistence.UserSessionRepository;
import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.BusinessException;
import com.carebed.common.exception.ResourceNotFoundException;
import jakarta.annotation.PostConstruct;
import org.springframework.data.domain.Sort;
import org.springframework.http.HttpStatus;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.util.StringUtils;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
public class AuthService {

    private static final long SESSION_DURATION_HOURS = 12;

    private final PasswordEncoder passwordEncoder;
    private final UserAccountRepository userAccountRepository;
    private final UserSessionRepository userSessionRepository;

    public AuthService(PasswordEncoder passwordEncoder,
            UserAccountRepository userAccountRepository,
            UserSessionRepository userSessionRepository) {
        this.passwordEncoder = passwordEncoder;
        this.userAccountRepository = userAccountRepository;
        this.userSessionRepository = userSessionRepository;
    }

    @PostConstruct
    void ensureDefaultAdmin() {
        if (!userAccountRepository.existsByRole(UserRole.ADMIN)) {
            RegisterRequest adminRequest = new RegisterRequest("admin", "Admin@123", UserRole.ADMIN, "默认管理员",
                    "+8613500000000");
            register(adminRequest);
        }
    }

    @Transactional
    public AuthResponse register(RegisterRequest request) {
        userAccountRepository.findByUsernameIgnoreCase(request.username()).ifPresent(entity -> {
            throw new BadRequestException("用户名已存在");
        });
        Instant now = Instant.now();
        UUID id = UUID.randomUUID();
        UserAccountEntity entity = new UserAccountEntity();
        entity.setId(id);
        entity.setUsername(request.username());
        entity.setPasswordHash(passwordEncoder.encode(request.password()));
        entity.setRole(request.role());
        entity.setFullName(request.fullName());
        entity.setPhone(request.phone());
        entity.setLinkedPatientId(null);
        entity.setCreatedAt(now);
        entity.setUpdatedAt(now);
        UserAccountEntity saved = userAccountRepository.save(entity);
        String token = generateSession(saved);
        return new AuthResponse(token, saved.getRole(), saved.getFullName());
    }

    @Transactional
    public AuthResponse login(LoginRequest request) {
        UserAccountEntity account = userAccountRepository.findByUsernameIgnoreCase(request.username())
                .orElseThrow(() -> new ResourceNotFoundException("用户不存在"));
        if (!passwordEncoder.matches(request.password(), account.getPasswordHash())) {
            throw new BadRequestException("密码错误");
        }
        String token = generateSession(account);
        return new AuthResponse(token, account.getRole(), account.getFullName());
    }

    @Transactional
    public UserProfileResponse linkPatient(String token, PatientLinkRequest payload) {
        UserAccount account = authenticate(token);
        if (account.role() != UserRole.FAMILY) {
            throw new BadRequestException("仅陪护家属可关联患者");
        }
        UserAccountEntity entity = userAccountRepository.findById(account.id())
                .orElseThrow(() -> new ResourceNotFoundException("用户不存在"));
        entity.setLinkedPatientId(payload.patientReference());
        entity.setUpdatedAt(Instant.now());
        return toProfile(toDomain(userAccountRepository.save(entity)));
    }

    @Transactional(readOnly = true)
    public UserAccount authenticate(String token) {
        if (!StringUtils.hasText(token)) {
            throw new BadRequestException("缺少认证令牌");
        }
        Instant now = Instant.now();
        userSessionRepository.deleteByExpiresAtBefore(now.minusSeconds(60));
        UserSessionEntity session = userSessionRepository.findById(token)
                .filter(s -> s.getExpiresAt().isAfter(now))
                .orElseThrow(() -> new BusinessSessionException("登录状态已失效"));
        return toDomain(Optional.ofNullable(session.getUser())
                .orElseThrow(() -> new BusinessSessionException("用户不存在或已被删除")));
    }

    @Transactional(readOnly = true)
    public List<UserProfileResponse> listUsers(UserRole role) {
        List<UserAccountEntity> source = role == null
                ? userAccountRepository.findAll(Sort.by(Sort.Direction.ASC, "createdAt"))
                : userAccountRepository.findByRole(role);
        return source.stream()
                .sorted((left, right) -> left.getCreatedAt().compareTo(right.getCreatedAt()))
                .map(this::toDomain)
                .map(this::toProfile)
                .toList();
    }

    @Transactional(readOnly = true)
    public List<UserAccount> allAccounts() {
        return userAccountRepository.findAll().stream()
                .map(this::toDomain)
                .toList();
    }

    public void invalidateToken(String token) {
        userSessionRepository.deleteById(token);
    }

    @Transactional(readOnly = true)
    public List<UserProfileResponse> searchByPatientReference(String reference) {
        if (!StringUtils.hasText(reference)) {
            return List.of();
        }
        return userAccountRepository.findByLinkedPatientIdIgnoreCase(reference).stream()
                .map(this::toDomain)
                .map(this::toProfile)
                .toList();
    }

    private String generateSession(UserAccountEntity user) {
        String token = UUID.randomUUID().toString();
        Instant now = Instant.now();
        UserSessionEntity session = new UserSessionEntity();
        session.setToken(token);
        session.setUser(user);
        session.setIssuedAt(now);
        session.setExpiresAt(now.plus(SESSION_DURATION_HOURS, ChronoUnit.HOURS));
        userSessionRepository.save(session);
        return token;
    }

    private UserProfileResponse toProfile(UserAccount account) {
        return new UserProfileResponse(
                account.id().toString(),
                account.username(),
                account.fullName(),
                account.phone(),
                account.role(),
                account.linkedPatientId());
    }

    private UserAccount toDomain(UserAccountEntity entity) {
        return new UserAccount(
                entity.getId(),
                entity.getUsername(),
                entity.getPasswordHash(),
                entity.getRole(),
                entity.getFullName(),
                entity.getPhone(),
                entity.getLinkedPatientId(),
                entity.getCreatedAt(),
                entity.getUpdatedAt());
    }

    private static class BusinessSessionException extends BusinessException {
        BusinessSessionException(String message) {
            super(message, HttpStatus.UNAUTHORIZED);
        }
    }
}
