package com.carebed.auth;

import com.carebed.auth.dto.AuthResponse;
import com.carebed.auth.dto.LoginRequest;
import com.carebed.auth.dto.PatientLinkRequest;
import com.carebed.auth.dto.RegisterRequest;
import com.carebed.auth.dto.UserProfileResponse;
import jakarta.validation.Valid;
import org.springframework.http.HttpHeaders;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

@RestController
@RequestMapping("/api/auth")
public class AuthController {

    public static final String AUTH_HEADER = "X-Auth-Token";

    private final AuthService authService;

    public AuthController(AuthService authService) {
        this.authService = authService;
    }

    @PostMapping("/register")
    public ResponseEntity<AuthResponse> register(@RequestBody @Valid RegisterRequest request) {
        return ResponseEntity.ok(authService.register(request));
    }

    @PostMapping("/login")
    public ResponseEntity<AuthResponse> login(@RequestBody @Valid LoginRequest request) {
        return ResponseEntity.ok(authService.login(request));
    }

    @PostMapping("/link-patient")
    public ResponseEntity<UserProfileResponse> linkPatient(@RequestBody @Valid PatientLinkRequest request,
            @RequestHeader(name = AUTH_HEADER) String token) {
        UserProfileResponse profile = authService.linkPatient(token, request);
        return ResponseEntity.ok(profile);
    }

    @GetMapping("/users")
    public ResponseEntity<List<UserProfileResponse>> listUsers(
            @RequestHeader(name = AUTH_HEADER, required = false) String token,
            @RequestParam(name = "role", required = false) UserRole role) {
        if (token != null) {
            authService.authenticate(token);
        }
        return ResponseEntity.ok(authService.listUsers(role));
    }

    @GetMapping("/patient-links")
    public ResponseEntity<List<UserProfileResponse>> queryByPatient(@RequestParam("reference") String reference) {
        return ResponseEntity.ok(authService.searchByPatientReference(reference));
    }
}
