# msocket backlog

## 1. Handling of negative return code

In msocket, when stream data processing returns an error, its internal io_task thread breaks out of the loop and terminates immediately without calling msocket_common_on_disconnected().
Can we improve this design?

Review the non-EINTR readiness-error exit path in [msocket_common.c](source/msocket_common.c) as part of this work. Define consistent error notification, connection-state updates, and cleanup when the I/O thread terminates unexpectedly. Keep intentional shutdown separate and avoid duplicate disconnect callbacks.

## 2. Recoverable accept errors must not stop the listener

The readiness wait retries EINTR, but a failure of the actual accept() call returns NULL from [msocket_common.c](source/msocket_common.c). The accept loop in [msocket_server.c](source/msocket_server.c) then terminates and deletes the listener, including for EINTR, EACCES, or EPERM. Existing connections may continue, but no new connections are accepted.

- Distinguish recoverable errors from intentional shutdown and fatal listener errors.
- Retry interrupted accepts. Define a policy for permission-related failures: these are not necessarily temporary, so immediate unbounded retries could busy-loop.
- Preserve the original errno or Winsock error across failed-child cleanup if the caller uses it to classify the failure. Cleanup now checks descriptor validity; the historical close(-1) error overwrite was not established in this implementation.
- Add fault-injection tests verifying that a recoverable failure does not prevent a subsequent connection, while shutdown still terminates promptly.

This is a source-confirmed availability defect under the stated error conditions; remote exploitability has not been established.

## 3. Retry interrupted stream receives

The plain recv() path in [msocket_common.c](source/msocket_common.c) treats every nonpositive result as disconnection, including EINTR on POSIX or WSAEINTR on Windows.

- Retry interrupted receives without reporting a peer disconnect.
- Preserve the existing handling of orderly peer closure and distinguish other receive errors.
- Add an injected-interruption test verifying that the connection remains usable.

## 4. Validate readiness retries under sustained signals

The I/O readiness loop in [msocket_common.c](source/msocket_common.c) already separates actual timeouts from errors: EINTR retries, other errors exit, and only a genuine timeout advances inactivity handling. The old timeout misclassification is therefore fixed.

Retries after EINTR are immediate, however. Test CPU usage, inactivity accounting, and shutdown responsiveness under repeated interruptions before deciding whether additional pacing or deadline-based accounting is needed. A signal-storm CPU-spin vulnerability has not been demonstrated.

## 5. TCP_NODELAY should be optional

In msocket, all TCP sockets are automatically using TCP_NODELAY. This feature should be optional using a setting or config.

## 6. Remove state MSOCKET_STATE_ACCEPTING

The MSOCKET_STATE_ACCEPTING doesn't serve any purpose except for debugging. It should be removed.

## 7. Binding TCP address

Currently, TCP server listens on all interfaces. Add support for selecting what interfaces to bind to.
