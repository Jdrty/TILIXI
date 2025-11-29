## Boot Sequence Placeholders

### Micro SD Card Mounting
- **Status**: Placeholder
- **Location**: `src/boot/boot_sequence.c::boot_mount_sd()`
- **Description**: SD card mounting is currently a placeholder.
- **TODO**: 
  - buy a microsd
  - format microsd
  - connect micro SD card hardware
  - implement SD card initialization using ESP32 SD library
  - add filesystem mounting
  - add error handling for missing SD card

### Initial Processes
- **Status**: Placeholder
- **Location**: `src/boot/boot_sequence.c::boot_init_processes()`
- **Description**: no initial system processes are started during boot yet.
- **TODO**:
  - implement process startup logic
  - add process dependency management
  - add process health monitoring

### Event Loop/Sequencer
- **Status**: Partial
- **Location**: `src/boot/boot_sequence.c::boot_start_event_loop()`
- **Description**: event loop exists in `main_esp32.cpp::loop()`, but boot sequence just verifies readiness.
- **TODO**:
  - formalize event loop architecture
  - add event loop monitoring/health checks
  - implement proper sequencer for scheduled tasks

### Emergency TTY
- **Status**: Placeholder
- **Location**: `src/boot/boot_sequence.c::boot_emergency_tty()`
- **Description**: emergency TTY is called when boot fails, but is not implemented yet.
- **TODO**:
  - implement minimal terminal for emergency recovery
  - add basic command set for system recovery
  - add system diagnostics commands
  - add recovery/repair utilities
  - allow system repair and reboot from emergency TTY

### Desktop Environment
- **Status**: Placeholder
- **Location**: `src/boot/boot_sequence.c::boot_start_desktop()`
- **Description**: desktop environment is not implemented yet. system currently goes to main event loop.
- **TODO**:
  - design desktop environment architecture
  - implement window manager
  - add desktop UI components
  - add application launcher
  - add system tray/status bar

## System Components

### Filesystem
- **Status**: Not Implemented
- **Description**: filesystem operations (cd, ls, etc.) are placeholders.
- **TODO**:
  - implement filesystem abstraction layer
  - add file operations (read, write, delete)
  - add directory operations
  - integrate with SD card when available

### Process Pipelining
- **Status**: Partial
- **Location**: `src/process/process_script.c`
- **Description**: basic pipeline structure exists, but full pipe wiring between processes is not implemented.
- **TODO**:
  - implement full pipe creation and wiring
  - add process-to-process communication
  - add pipe buffering
  - test multi-process pipelines

### Terminal Display Rendering
- **Status**: Partial
- **Description**: terminal buffer exists, but rendering to TFT display is not implemented.
- **TODO**:
  - implement terminal buffer to TFT rendering
  - add font rendering for terminal
  - add scrolling support
  - add cursor display

## Hardware Integration

### Keyboard Input
- **Status**: Partial
- **Description**: keyboard input works on PC, ESP32 implementation may need work.
- **TODO**:
  - verify ESP32 keyboard input
  - add keyboard hotplug support
  - add keyboard layout configuration

### TFT Display
- **Status**: Working
- **Description**: TFT display initialization works, but could use more features.
- **TODO**:
  - add display backlight control
  - optimize display refresh
  - add display power management, to add power management, or not to add power management, that is the question..

## Future Enhancements

### Process Management
- add process priority scheduling improvements
- add process resource limits
- add process communication (IPC)
- add process monitoring/debugging tools

### Terminal Features
- add terminal tabs/windows
- add terminal themes
- add terminal history search
- add command completion

### System Services
- add system logging service
- add configuration management
- add system update mechanism
- add backup/restore functionality

