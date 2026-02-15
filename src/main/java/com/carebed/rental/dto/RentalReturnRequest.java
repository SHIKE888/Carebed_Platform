package com.carebed.rental.dto;

public record RentalReturnRequest(
        String conditionNotes,
        boolean lockConfirmed) {
}
