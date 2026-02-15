package com.carebed.device;

import java.time.Instant;

public record DeviceEvent(
        Instant timestamp,
        String type,
        String description) {
}
