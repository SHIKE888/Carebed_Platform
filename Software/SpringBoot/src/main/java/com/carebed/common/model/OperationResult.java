package com.carebed.common.model;

public record OperationResult(String message) {
    public static OperationResult of(String message) {
        return new OperationResult(message);
    }
}
