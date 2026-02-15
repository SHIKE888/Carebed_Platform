package com.carebed.admin;

import com.carebed.admin.dto.AdminAnalyticsResponse;
import com.carebed.admin.dto.AdminOverviewResponse;
import com.carebed.auth.AuthController;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/admin")
public class AdminController {

    private final AdminService adminService;

    public AdminController(AdminService adminService) {
        this.adminService = adminService;
    }

    @GetMapping("/overview")
    public ResponseEntity<AdminOverviewResponse> overview(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        return ResponseEntity.ok(adminService.overview(token));
    }

    @GetMapping("/analytics")
    public ResponseEntity<AdminAnalyticsResponse> analytics(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        return ResponseEntity.ok(adminService.analytics(token));
    }
}
