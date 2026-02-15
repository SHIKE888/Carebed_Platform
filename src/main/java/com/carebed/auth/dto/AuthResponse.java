package com.carebed.auth.dto;

import com.carebed.auth.UserRole;

public record AuthResponse(String token, UserRole role, String displayName) {
}
