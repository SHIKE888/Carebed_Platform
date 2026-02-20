package com.carebed.auth.dto;

import com.carebed.auth.UserRole;

public record UserProfileResponse(
        String id,
        String username,
        String fullName,
        String phone,
        UserRole role,
        String linkedPatientReference) {
}
