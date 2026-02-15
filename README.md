# CareBed Platform Backend

Spring Boot 3 backend that manages shared care bed rentals with multi-role access, device lifecycle, wallet billing, repairs, and realtime notifications.

## Getting Started

1. Install JDK 21 and Apache Maven 3.9+
2. Copy `.env` style secrets if needed (none required for the in-memory demo)
3. Compile: `mvn -DskipTests compile`
4. Run: `mvn spring-boot:run`

Default service address: `http://localhost:8080`

## Modules

- **Auth**: Family and admin registration/login, patient linkage via `X-Auth-Token` header.
- **Devices**: Register, update, bind/unbind, heartbeat, faults, remote reboot, event history.
- **Rentals**: Scan to start, bluetooth unlock simulated, billing on return, overdue scan, admin oversight.
- **Notifications**: User/admin inbox with rental, payment, repair alerts.
- **Repairs**: User photo uploads (base64 URLs), auto order/device binding, admin workflow.
- **Wallet**: Balance, recharge, auto debit, refunds, disputes with admin adjudication.
- **Admin Dashboard**: Platform overview metrics and revenue/usage trends.

## Key API Endpoints

| Area | Method | Path | Notes |
| ---- | ------ | ---- | ----- |
| Auth | POST | /api/auth/register | Multi-role signup |
| Auth | POST | /api/auth/login | Returns token for `X-Auth-Token` |
| Auth | POST | /api/auth/link-patient | Family link patient reference |
| Devices | POST | /api/devices | Register device |
| Devices | POST | /api/devices/{id}/faults | Fault reporting |
| Rentals | POST | /api/rentals/start | Requires token |
| Rentals | POST | /api/rentals/{id}/return | Ends rental, bills wallet |
| Messages | GET | /api/messages | Inbox for current user |
| Repairs | POST | /api/repairs | Submit repair ticket |
| Wallet | GET | /api/wallet | Balance + history |
| Admin | GET | /api/admin/overview | Metrics (admin token) |

## Authentication

- Use `X-Auth-Token` header on protected endpoints.
- Default bootstrap admin: username `admin`, password `Admin@123` (created at startup).

## Testing & Maintenance

- Run unit tests: `mvn test`
- Update dependencies: `mvn versions:display-dependency-updates`
- All services store data in-memory; swap with persistence layer when ready.

## Next Steps

- Integrate Bluetooth unlock bridge in `/api/rentals/start` flow.
- Replace in-memory repositories with database implementations & message broker.
- Add scheduled job for `RentalService.scanForOverdue()` via Spring Scheduling.
