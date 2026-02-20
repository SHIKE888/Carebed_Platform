package com.carebed.auth.dto;

import com.carebed.auth.UserRole;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.Pattern;

public record RegisterRequest(
        @NotBlank(message = "用户名不能为空") String username,
        @NotBlank(message = "密码不能为空") String password,
        @NotNull(message = "角色不能为空") UserRole role,
        @NotBlank(message = "姓名不能为空") String fullName,
        @Pattern(regexp = "^\\+?[0-9]{6,15}$", message = "手机号格式不正确") String phone) {
}
