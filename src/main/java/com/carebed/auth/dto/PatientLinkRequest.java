package com.carebed.auth.dto;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;

public record PatientLinkRequest(
        @NotBlank(message = "住院号或床位号不能为空") @Size(max = 32, message = "编号过长") String patientReference) {
}
