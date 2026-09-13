# Coding Guidelines for msocket

## Type and Struct Conventions
- **Struct Names**: All `struct` names must end with `_tag` (e.g., `struct msocket_tag`, `struct msocket_server_tag`, `struct msocket_os_tag`).
- **Typedef Names**: The corresponding `typedef` for a struct must end with `_t` (e.g., `typedef struct msocket_tag msocket_t;`).
- **Forward Declarations in Headers**:
  - Only forward-declare the struct tag (`struct <name>_tag;`), never a forward `typedef` (e.g., do **not** write `typedef struct msocket_os_t msocket_os_t;` as a forward declaration).
  - The `typedef` is defined **only** in the header or module where the struct itself is fully declared.
  - Struct members pointing to opaque or forward-declared structs in external headers should use the struct tag pointer (e.g., `struct msocket_os_tag *os;`).
- **Usage in Source Files (`.c` / `.cpp`)**:
  - In source files where all required headers are included, **do not** use `struct <name>_tag` in the code.
  - Always use the typedef `<name>_t` instead (e.g., `msocket_server_t *server`, `msocket_os_t *os`, `msocket_t *self`).

## Callback Conventions
- **Callback Function Names**: Name callbacks with the pattern `on_<subject>_<event>` (e.g., `on_client_data`, `on_client_disconnected`).
- **Callback Signatures**: Signatures in `msocket_handler_t` pass user context first and socket second:
  `(void *arg, void *socket, ...)`

## Variable Naming Conventions
- **File-Scoped Variables (`static`)**: File-scoped variables declared using `static` must be prefixed with `m_` (short for member, e.g., `static volatile int m_running = 1;`).
- **Program-Scoped Variables (`extern`)**: Program-scoped variables intended to be accessed across compilation units using `extern` must be prefixed with `g_`.

