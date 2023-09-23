# DSC ITv2 notification

Notifications are sent to integration server using a binary TCP-based protocol.
Actual messages can be send in a raw binary format or encrypted.
Encryption algorithm is probably AES with 128 b key size.
It remains unknown how to obtain a key.

## Messages encapsulation

All ITv2 messages are encapsulated before sending.
TL280 outgoing messages are additionally prepended with 12 B string containing integration ID value (TL280 unique number).
Message gets `7e` byte prepended and `7f` byte appended.

### Outgoing packet example

```
0000   31 32 33 34 35 36 37 38 39 30 31 32 7e 04 00 01   123456789012~...
0010   00 7d 00 7f                                       .}..
```

* `31 32 33 34 35 36 37 38 39 30 31 32`: Integration ID value 123456789012
* `7e`: Start of message
* `04 00 01 00 7d 00`: Actual message
* `7e`: End of message

### Incoming packet example

```
0000   7e 04 02 01 66 1f 7f                              ~...f..
```

* `7e`: Start of message
* `04 02 01 66 1f`: Actual message
* `7e`: End of message

## Message format

* The first byte specifies message length.
* Two last bytes contain CRC-16/CCITT-FALSE checksum.

### Message examples

```
0000   08 01 00 05 02 91 00 de 3c                        ~........<
```

* `08`: Length
* `de 3c`: CRC-16 checksum

```
0000   04 02 01 66 1f                                    ~...f.
```

* `04`: Length
* `66 1f`: CRC-16 checksum
