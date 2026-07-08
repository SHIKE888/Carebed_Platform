package com.carebed.repair;

import com.carebed.activity.ActivityLogService;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.ResourceNotFoundException;
import com.carebed.notification.NotificationService;
import com.carebed.notification.NotificationType;
import com.carebed.repair.dto.RepairTicketCreateRequest;
import com.carebed.repair.dto.RepairTicketResponse;
import com.carebed.repair.dto.RepairTicketUpdateRequest;
import com.carebed.repair.persistence.RepairTicketEntity;
import com.carebed.repair.persistence.RepairTicketRepository;
import com.carebed.rental.RentalRecord;
import com.carebed.rental.RentalService;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

@Service
public class RepairService {

    private final AuthService authService;
    private final RentalService rentalService;
    private final NotificationService notificationService;
    private final RepairTicketRepository repairTicketRepository;
    private final ActivityLogService activityLogService;

    public RepairService(AuthService authService,
            RentalService rentalService,
            NotificationService notificationService,
            RepairTicketRepository repairTicketRepository,
            ActivityLogService activityLogService) {
        this.authService = authService;
        this.rentalService = rentalService;
        this.notificationService = notificationService;
        this.repairTicketRepository = repairTicketRepository;
        this.activityLogService = activityLogService;
    }

    @Transactional
    public RepairTicketResponse createTicket(String token, RepairTicketCreateRequest request) {
        UserAccount account = authService.authenticate(token);
        RentalRecord rental = rentalService.findRental(request.rentalId())
                .orElseThrow(() -> new ResourceNotFoundException("订单不存在"));
        if (!rental.userId().equals(account.id())) {
            throw new BadRequestException("仅订单所属用户可报修");
        }
        Instant now = Instant.now();
        RepairTicketEntity entity = new RepairTicketEntity();
        entity.setId(UUID.randomUUID());
        entity.setUserId(account.id());
        entity.setRentalId(rental.id());
        entity.setDeviceId(rental.deviceId());
        entity.setDeviceCode(rental.deviceCode());
        entity.setDescription(request.description());
        entity.setPhotos(request.photos() == null ? List.of() : List.copyOf(request.photos()));
        entity.setStatus(RepairStatus.OPEN);
        entity.setResolution(null);
        entity.setCreatedAt(now);
        entity.setUpdatedAt(now);
        RepairTicketEntity saved = repairTicketRepository.save(entity);
        notificationService.notifyAdmins(NotificationType.REPAIR_UPDATE, "新的报修单", "订单" + rental.id() + " 提交报修");
        activityLogService.record(account.id(), account.fullName(), "REPAIR_CREATE",
                "报修单 " + saved.getId());
        return toResponse(saved);
    }

    @Transactional(readOnly = true)
    public List<RepairTicketResponse> listMyTickets(String token) {
        UserAccount account = authService.authenticate(token);
        return repairTicketRepository.findByUserIdOrderByCreatedAtDesc(account.id()).stream()
                .map(this::toResponse)
                .toList();
    }

    @Transactional(readOnly = true)
    public List<RepairTicketResponse> listAllTickets(String token, Optional<RepairStatus> status) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可查看");
        }
        List<RepairTicketEntity> source = status
                .map(value -> repairTicketRepository.findByStatusOrderByCreatedAtDesc(value))
                .orElseGet(() -> repairTicketRepository.findAllByOrderByCreatedAtDesc());
        return source.stream().map(this::toResponse).toList();
    }

    @Transactional
    public RepairTicketResponse updateTicket(String token, UUID ticketId, RepairTicketUpdateRequest request) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可处理报修单");
        }
        RepairTicketEntity entity = repairTicketRepository.findById(ticketId)
                .orElseThrow(() -> new ResourceNotFoundException("报修单不存在"));
        Instant now = Instant.now();
        entity.setStatus(request.status());
        entity.setResolution(request.resolution());
        entity.setUpdatedAt(now);
        RepairTicketEntity saved = repairTicketRepository.save(entity);
        notificationService.notifyUser(saved.getUserId(), NotificationType.REPAIR_UPDATE, "报修进度更新",
                "报修单" + saved.getId() + " 已更新为 " + request.status());
        activityLogService.record(account.id(), account.fullName(), "REPAIR_UPDATE",
                "更新报修单 " + saved.getId());
        return toResponse(saved);
    }

    private RepairTicketResponse toResponse(RepairTicketEntity entity) {
        RepairTicket ticket = toDomain(entity);
        return new RepairTicketResponse(
                ticket.id().toString(),
                ticket.userId().toString(),
                ticket.rentalId().toString(),
                ticket.deviceId().toString(),
                ticket.deviceCode(),
                ticket.description(),
                ticket.photos(),
                ticket.status(),
                ticket.resolution(),
                ticket.createdAt(),
                ticket.updatedAt());
    }

    @Transactional(readOnly = true)
    public List<RepairTicket> allTickets() {
        return repairTicketRepository.findAll().stream()
                .map(this::toDomain)
                .toList();
    }

    @Transactional(readOnly = true)
    public List<RepairTicketResponse> listTicketsByIds(List<UUID> ids) {
        return repairTicketRepository.findAllById(ids).stream()
                .map(this::toResponse)
                .toList();
    }

    private RepairTicket toDomain(RepairTicketEntity entity) {
        return new RepairTicket(
                entity.getId(),
                entity.getUserId(),
                entity.getRentalId(),
                entity.getDeviceId(),
                entity.getDeviceCode(),
                entity.getDescription(),
                List.copyOf(entity.getPhotos() == null ? List.of() : entity.getPhotos()),
                entity.getStatus(),
                entity.getResolution(),
                entity.getCreatedAt(),
                entity.getUpdatedAt());
    }
}
