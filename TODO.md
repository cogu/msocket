# Todo List

## Handling of negative return code

In msocket, when stream data processing returns an error, its internal io_task thread breaks out of the loop and terminates immediately without calling msocket_common_on_disconnected().
Can we improve this design?