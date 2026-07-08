package com.carebed.admin;

import com.carebed.activity.ActivityLogService;
import com.carebed.auth.AuthController;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.common.exception.BadRequestException;
import com.carebed.common.exception.ResourceNotFoundException;
import com.carebed.common.model.OperationResult;
import com.carebed.wallet.WalletService;
import jakarta.validation.Valid;
import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.math.BigDecimal;
import java.util.UUID;

@RestController
@RequestMapping("/api/admin/users")
public class AdminUserController {

    private final AuthService authService;
    private final WalletService walletService;
    private final ActivityLogService activityLogService;

    public AdminUserController(AuthService authService, WalletService walletService,
            ActivityLogService activityLogService) {
        this.authService = authService;
        this.walletService = walletService;
        this.activityLogService = activityLogService;
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<OperationResult> deleteUser(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id) {
        UserAccount admin = authService.authenticate(token);
        if (admin.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可操作");
        }
        authService.deleteUser(id);
        activityLogService.record(admin.id(), admin.fullName(), "ADMIN_DELETE_USER", "注销用户 " + id);
        return ResponseEntity.ok(OperationResult.of("账户已注销"));
    }

    @PostMapping("/{id}/balance")
    public ResponseEntity<OperationResult> updateBalance(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody BalanceUpdateRequest request) {
        UserAccount admin = authService.authenticate(token);
        if (admin.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可操作");
        }
        walletService.setBalance(id, request.amount());
        activityLogService.record(admin.id(), admin.fullName(), "ADMIN_SET_BALANCE",
                "设置用户 " + id + " 余额为 " + request.amount());
        return ResponseEntity.ok(OperationResult.of("余额已更新为 " + request.amount() + " 元"));
    }

    @PutMapping("/{id}/password")
    public ResponseEntity<OperationResult> updatePassword(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody PasswordUpdateRequest request) {
        UserAccount admin = authService.authenticate(token);
        if (admin.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可操作");
        }
        authService.updatePassword(id, request.newPassword());
        activityLogService.record(admin.id(), admin.fullName(), "ADMIN_SET_PASSWORD", "修改用户 " + id + " 密码");
        return ResponseEntity.ok(OperationResult.of("密码已更新"));
    }

    public record BalanceUpdateRequest(
            @DecimalMin(value = "0.00", message = "余额不能为负") BigDecimal amount) {
    }

    @PutMapping("/{id}")
    public ResponseEntity<OperationResult> updateUser(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody UserUpdateRequest request) {
        UserAccount admin = authService.authenticate(token);
        if (admin.role() != UserRole.ADMIN) {
            throw new BadRequestException("仅管理员可操作");
        }
        authService.updateUser(id, request.username(), request.fullName(), request.phone());
        activityLogService.record(admin.id(), admin.fullName(), "ADMIN_UPDATE_USER", "修改用户 " + id);
        return ResponseEntity.ok(OperationResult.of("用户信息已更新"));
    }

    public record PasswordUpdateRequest(
            @NotBlank @Size(min = 6, message = "密码至少6位") String newPassword) {
    }

    public record UserUpdateRequest(
            String username,
            String fullName,
            String phone) {
    }
}
