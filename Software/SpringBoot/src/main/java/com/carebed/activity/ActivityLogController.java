package com.carebed.activity;

import com.carebed.activity.dto.ActivityLogResponse;
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
@RequestMapping("/api/logs")
public class ActivityLogController {

    private final AuthService authService;
    private final ActivityLogService activityLogService;

    public ActivityLogController(AuthService authService, ActivityLogService activityLogService) {
        this.authService = authService;
        this.activityLogService = activityLogService;
    }

    @GetMapping
    public ResponseEntity<List<ActivityLogResponse>> listLogs(
            @RequestHeader(AuthController.AUTH_HEADER) String token) {
        UserAccount account = authService.authenticate(token);
        return ResponseEntity.ok(activityLogService.listFor(account));
    }
}
