package com.carebed.admin.dto;

import java.util.List;

public record AdminAnalyticsResponse(
        List<AdminMetricPoint> usageTrend,
        List<AdminMetricPoint> revenueTrend) {
}
