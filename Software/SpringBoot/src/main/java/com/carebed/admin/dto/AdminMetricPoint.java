package com.carebed.admin.dto;

import java.time.Instant;

public record AdminMetricPoint(
        Instant timestamp,
        double value) {
}
