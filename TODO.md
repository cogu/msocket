# Todo List

## Handling of negative return code

In msocket, when stream data processing returns an error, its internal io_task thread breaks out of the loop and terminates immediately without calling msocket_common_on_disconnected().
Can we improve this design?

## TCP_NODELAY should be optional

In msocket, all TCP sockets are automatically using TCP_NODELAY. This feature should be optional using a setting or config.

## Remove state MSOCKET_STATE_ACCEPTING

The MSOCKET_STATE_ACCEPTING doesn't serve any purpose except for debugging. It should be removed