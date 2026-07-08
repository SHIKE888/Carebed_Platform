package com.carebed.auth.dto;

import jakarta.validation.constraints.NotBlank;

public record ResetPasswordRequest(
        @NotBlank(message = "用户名不能为空") String username,
        @NotBlank(message = "手机号不能为空") String phone,
        @NotBlank(message = "新密码不能为空") String newPassword) {
}
