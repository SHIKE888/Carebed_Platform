package com.carebed.rental.dto;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;

public record RentalStartRequest(
        @NotBlank(message = "设备编号不能为空") String deviceCode,
        @Min(value = 1, message = "最短租借时长为1小时") int expectedHours) {
}
