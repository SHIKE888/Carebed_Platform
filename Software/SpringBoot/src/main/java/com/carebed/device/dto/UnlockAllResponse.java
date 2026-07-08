package com.carebed.device.dto;

import java.util.List;

public record UnlockAllResponse(
        int totalOnline,
        int successCount,
        List<String> failedDeviceCodes) {
}
