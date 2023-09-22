# DSC ITv2 protocol

## DSC company

[DSC](https://www.dsc.com/) (Digital Security Controls) is an electronic security company founded in 1979 and acquired by Tyco International in 2002.

They offer few lines of alarm monitoring products including one called PowerSeries Neo. It consists of control panels, keypads and various modules.

## PowerSeries Neo communicators

DSC offers communication modules for integration with licensed solutions. Some of available models:

| Model    | Ethernet | RS-232 | 3G |
| -------- | -------- | ------ | -- |
| TL280E   | ✓        | ✗      | ✗  |
| TL280RE  | ✓        | ✓      | ✗  |
| TL2803GE | ✓        | ✗      | ✓  |
| 3G2080E  | ✗        | ✗      | ✓  |

## Integration solutions

There are few solutions that TL280 can integrate with:

1. PowerSeries Neo Go

   In early days DSC offered a free integration service and a free Android/iPhone app called PowerSeries Neo Go.
   There are still setup guides available: [PowerSeries Neo Go App](https://www.dsc.com/dsc-product-families/neo/PowerSeries-Neo-Go-app/9).

   Unfortunately their server seems to reply with "Server Error" only, Android app was silently removed and iPhone one was last updated in 2016.

2. PowerManage

   Visonic company (acquired by Tyco in 2011) offers PowerManage - a complete but commercial integration & management platform.

3. Control4

   [Control4](https://www.control4.com/) smart home system has an encrypted driver for DSC / TL280 integration: [DSC TL-280 Board](https://drivers.control4.com/solr/drivers/browse?q=TL-280).

4. CathexisVision

   [CathexisVision](https://cathexisvideo.com/) provides video monitoring that supports [DSC Neo ITv2 integration](https://cathexisvideo.com/technology-partners/integration/dsc-neo/) for monitoring alarms.

## Resources

Some discussions on DSC / TL280 integration:

* openHAB: [DSC Binding - Powerseries NEO ](https://community.openhab.org/t/dsc-binding-powerseries-neo/19040)
* C4 Forums: [DSC NEO Integration](https://www.c4forums.com/forums/topic/21351-dsc-neo-integration/)

## ITv2

TL280 guides and all integration resources refer to communication protocol as ITv2.
Tyco offers a paid access to their SDK (and API documentation?) for its partners only under the [Connected Partner Program](https://connectedpartnerprogram.partnerproducts.com/).

TL280 uses two communication methods:

1. Polling

   Every few seconds (10 by default) TL280 sends a HTTP request to the integration server.
   Both: requests and responses are encrypted (there is nothing human readable in HTTP data).

2. Notification

   On every event TL280 uses a custom TCP binary protocol to notify integration server.
   There is no publicly available documentation for it and communication gets encrypted after exchanging few packets.

   See [NOTIFICATION](NOTIFICATION.md) for basic [RE](https://en.wikipedia.org/wiki/Reverse_engineering)-based documentation.
