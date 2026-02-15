package com.carebed.mqttlog;

import com.carebed.auth.AuthController;
import com.carebed.auth.AuthService;
import com.carebed.auth.UserAccount;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

@RestController
@RequestMapping("/api/mqtt/logs")
public class MqttLogController {

    private final AuthService authService;
    private final MqttLogService mqttLogService;

    public MqttLogController(AuthService authService, MqttLogService mqttLogService) {
        this.authService = authService;
        this.mqttLogService = mqttLogService;
    }

    @GetMapping
    public ResponseEntity<List<MqttLogEntry>> list(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        if (account.role() != com.carebed.auth.UserRole.ADMIN) {
            return ResponseEntity.status(403).build();
        }
        return ResponseEntity.ok(mqttLogService.listLatest());
    }
}