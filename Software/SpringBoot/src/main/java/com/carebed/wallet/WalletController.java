package com.carebed.wallet;

import com.carebed.activity.ActivityLogService;
import com.carebed.auth.AuthController;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.common.model.OperationResult;
import com.carebed.notification.NotificationService;
import com.carebed.notification.NotificationType;
import com.carebed.wallet.DisputeStatus;
import com.carebed.wallet.dto.DisputeRefundRequest;
import com.carebed.wallet.dto.DisputeRequest;
import com.carebed.wallet.dto.DisputeResponse;
import com.carebed.wallet.dto.DisputeUpdateRequest;
import com.carebed.wallet.dto.RechargeRequest;
import com.carebed.wallet.dto.WalletSnapshotResponse;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.Optional;
import java.util.UUID;

@RestController
@RequestMapping("/api/wallet")
public class WalletController {

    private final WalletService walletService;
    private final AuthService authService;
    private final NotificationService notificationService;
    private final ActivityLogService activityLogService;

    public WalletController(WalletService walletService, AuthService authService,
            NotificationService notificationService,
            ActivityLogService activityLogService) {
        this.walletService = walletService;
        this.authService = authService;
        this.notificationService = notificationService;
        this.activityLogService = activityLogService;
    }

    @GetMapping
    public ResponseEntity<WalletSnapshotResponse> snapshot(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        return ResponseEntity.ok(walletService.snapshot(account.id()));
    }

    @PostMapping("/recharge")
    public ResponseEntity<WalletSnapshotResponse> recharge(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @Valid @RequestBody RechargeRequest request) {
        UserAccount account = authService.authenticate(token);
        walletService.recharge(account.id(), request.amount(), "用户充值");
        notificationService.notifyUser(account.id(), NotificationType.PAYMENT_ALERT, "充值成功",
                "已成功充值 " + request.amount() + " 元");
        activityLogService.record(account.id(), account.fullName(), "WALLET_RECHARGE",
                "充值 " + request.amount() + " 元");
        return ResponseEntity.ok(walletService.snapshot(account.id()));
    }

    @PostMapping("/disputes")
    public ResponseEntity<DisputeResponse> createDispute(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @Valid @RequestBody DisputeRequest request) {
        UserAccount account = authService.authenticate(token);
        DisputeResponse response = walletService.createDispute(account.id(), request);
        notificationService.notifyAdmins(NotificationType.PAYMENT_ALERT, "新费用争议", "订单" + request.orderId() + " 提交争议");
        activityLogService.record(account.id(), account.fullName(), "WALLET_DISPUTE",
                "提交争议 订单" + request.orderId());
        return ResponseEntity.ok(response);
    }

    @GetMapping("/disputes")
    public ResponseEntity<List<DisputeResponse>> listDisputes(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @RequestParam(name = "status", required = false) DisputeStatus status) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            return ResponseEntity.status(403).build();
        }
        return ResponseEntity.ok(walletService.listDisputes(Optional.ofNullable(status)));
    }

    @GetMapping("/disputes/me")
    public ResponseEntity<List<DisputeResponse>> myDisputes(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        return ResponseEntity.ok(walletService.listUserDisputes(account.id()));
    }

    @PostMapping("/disputes/{id}")
    public ResponseEntity<DisputeResponse> updateDispute(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody DisputeUpdateRequest request) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            return ResponseEntity.status(403).build();
        }
        DisputeResponse response = walletService.updateDispute(id, request);
        activityLogService.record(account.id(), account.fullName(), "WALLET_DISPUTE_UPDATE",
                "更新争议 " + id + " -> " + request.status());
        return ResponseEntity.ok(response);
    }

    @PostMapping("/disputes/{id}/refund")
    public ResponseEntity<OperationResult> disputeRefund(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody DisputeRefundRequest request) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            return ResponseEntity.status(403).build();
        }
        walletService.disputeRefund(id, request.refundAmount());
        activityLogService.record(account.id(), account.fullName(), "DISPUTE_REFUND",
                "争议退费 " + id + " 金额 " + request.refundAmount());
        return ResponseEntity.ok(OperationResult.of("退款成功，已退还 " + request.refundAmount() + " 元"));
    }
}
