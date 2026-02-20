package com.carebed.rental;

import com.carebed.auth.AuthController;
import com.carebed.common.model.OperationResult;
import com.carebed.rental.dto.RentalResponse;
import com.carebed.rental.dto.RentalReturnRequest;
import com.carebed.rental.dto.RentalStartRequest;
import jakarta.validation.Valid;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.Optional;
import java.util.UUID;

@RestController
@RequestMapping("/api/rentals")
public class RentalController {

    private final RentalService rentalService;

    public RentalController(RentalService rentalService) {
        this.rentalService = rentalService;
    }

    @PostMapping("/start")
    public ResponseEntity<RentalResponse> startRental(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @Valid @RequestBody RentalStartRequest request) {
        return ResponseEntity.ok(rentalService.startRental(token, request));
    }

    @PostMapping("/{id}/return")
    public ResponseEntity<RentalResponse> finishRental(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @RequestBody RentalReturnRequest request) {
        return ResponseEntity.ok(rentalService.finishRental(token, id, request));
    }

    @GetMapping("/me")
    public ResponseEntity<List<RentalResponse>> myRentals(@RequestHeader(AuthController.AUTH_HEADER) String token) {
        return ResponseEntity.ok(rentalService.listUserRentals(token));
    }

    @GetMapping
    public ResponseEntity<List<RentalResponse>> allRentals(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @RequestParam(name = "status", required = false) RentalStatus status) {
        return ResponseEntity.ok(rentalService.listAll(token, Optional.ofNullable(status)));
    }

    @PostMapping("/{id}/cancel")
    public ResponseEntity<OperationResult> cancel(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id) {
        return ResponseEntity.ok(rentalService.cancelRental(token, id));
    }

    @GetMapping("/{id}")
    public ResponseEntity<RentalResponse> detail(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id) {
        return ResponseEntity.ok(rentalService.getRentalResponse(token, id));
    }
}
