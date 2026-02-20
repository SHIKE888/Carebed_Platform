package com.carebed.notification;

import com.carebed.auth.AuthService;
import com.carebed.auth.AuthController;
import com.carebed.auth.UserAccount;
import com.carebed.auth.UserRole;
import com.carebed.notification.dto.NotificationResponse;
import com.carebed.common.model.OperationResult;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.UUID;

@RestController
@RequestMapping("/api/messages")
public class NotificationController {

    private final NotificationService notificationService;
    private final AuthService authService;

    public NotificationController(NotificationService notificationService, AuthService authService) {
        this.notificationService = notificationService;
        this.authService = authService;
    }

    @GetMapping
    public ResponseEntity<List<NotificationResponse>> listUserMessages(
            @RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        return ResponseEntity.ok(notificationService.listUserMessages(account.id()));
    }

    @PostMapping("/{messageId}/read")
    public ResponseEntity<OperationResult> markRead(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable String messageId) {
        UserAccount account = authService.authenticate(token);
        notificationService.markRead(account.id(), UUID.fromString(messageId));
        return ResponseEntity.ok(OperationResult.of("消息已标记为已读"));
    }

    @GetMapping("/admin")
    public ResponseEntity<List<NotificationResponse>> listAdminMessages(
            @RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != UserRole.ADMIN) {
            return ResponseEntity.status(403).build();
        }
        return ResponseEntity.ok(notificationService.listAdminMessages());
    }
}
