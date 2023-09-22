# DSC ITv2 notification

Notifications are sent to integration server using a binary TCP-based protocol.
Actual messages can be send in a raw binary format or encrypted.
Encryption algorithm is probably AES with 128 b key size.
It remains unknown how to obtain a key.

## Messages encapsulation

All ITv2 messages are encapsulated before sending.
Message gets `7e` byte prepended and `7f` byte appended.

### Incoming packet example

```
0000   7e 04 02 01 66 1f 7f                              ~...f..
```

* `7e`: Start of message
* `04 02 01 66 1f`: Actual message
* `7e`: End of message
