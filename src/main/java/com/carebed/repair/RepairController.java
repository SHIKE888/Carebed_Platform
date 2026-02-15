package com.carebed.repair;

import com.carebed.auth.AuthController;
import com.carebed.repair.dto.RepairTicketCreateRequest;
import com.carebed.repair.dto.RepairTicketResponse;
import com.carebed.repair.dto.RepairTicketUpdateRequest;
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
@RequestMapping("/api/repairs")
public class RepairController {

    private final RepairService repairService;

    public RepairController(RepairService repairService) {
        this.repairService = repairService;
    }

    @PostMapping
    public ResponseEntity<RepairTicketResponse> create(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @Valid @RequestBody RepairTicketCreateRequest request) {
        return ResponseEntity.ok(repairService.createTicket(token, request));
    }

    @GetMapping("/me")
    public ResponseEntity<List<RepairTicketResponse>> myTickets(
            @RequestHeader(AuthController.AUTH_HEADER) String token) {
        return ResponseEntity.ok(repairService.listMyTickets(token));
    }

    @GetMapping
    public ResponseEntity<List<RepairTicketResponse>> listAll(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @RequestParam(name = "status", required = false) RepairStatus status) {
        return ResponseEntity.ok(repairService.listAllTickets(token, Optional.ofNullable(status)));
    }

    @PostMapping("/{id}")
    public ResponseEntity<RepairTicketResponse> update(@RequestHeader(AuthController.AUTH_HEADER) String token,
            @PathVariable UUID id,
            @Valid @RequestBody RepairTicketUpdateRequest request) {
        return ResponseEntity.ok(repairService.updateTicket(token, id, request));
    }
}
