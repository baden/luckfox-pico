# MAVLink Integration

This version of the drone software uses the MAVLink protocol for telemetry and control over UDP.

## Configuration

- **Operator IP**: `10.8.0.11`
- **UDP Port**: `14550` (Standard GCS port)
- **System ID**: `1`
- **Component ID**: `1` (MAV_COMP_ID_AUTOPILOT1)

## Features

### Telemetry (Sent to Operator)
- **Heartbeat**: Sent every 1s (`MAV_TYPE_QUADROTOR`, `MAV_STATE_STANDBY`).
- **Attitude**: Sent every 100ms.
  - `Roll` -> Mapped from `axis_0` (Stick X)
  - `Pitch` -> Mapped from `axis_1` (Stick Y)
  - `Yaw` -> 0 (or from compass if available)
- **Compass**: Sent via `VFR_HUD` message (Heading).
- **Status**: Sent via `NAMED_VALUE_INT`:
  - `LEBIDKA` (Winch state)
  - `AKTUATOR` (Actuator state)

### Control (Received from Operator)
- **Manual Control**:
  - `MANUAL_CONTROL` message supported.
  - `RC_CHANNELS_OVERRIDE` message supported.
  - Channels mapped to internal axes (Pitch/Roll/Throttle/Yaw).
- **Commands**:
  - `MAV_CMD_COMPONENT_ARM_DISARM`: Arms/Disarms the drone.
  - `MAV_CMD_NAV_TAKEOFF`: Sets `cmd_takeoff` flag.

## Build & Run

The MAVLink v2 library is included in `common/mavlink_lib`.

```bash
mkdir -p build && cd build
cmake -DCOMPILE_FOR_RV1106_IPC=ON ..
make -j4
sudo ./src/rv1106_ipc/drone
```

## Troubleshooting

- **Warnings during compile**: You may see "taking address of packed member..." warnings. This is expected with standard MAVLink headers on GCC/ARM and generally safe on Cortex-A7 (RV1106), though unaligned access should be minimized.
- **Connection**: Ensure the operator (QGroundControl or similar) is listening on `10.8.0.11:14550`.
