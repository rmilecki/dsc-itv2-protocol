// SPDX-License-Identifier: GPL-2.0
#include <arpa/inet.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "crc16.h"

#define PROTOCOL_VERSION	0x0211

#define MSG_START_OF_FRAME	0x7e
#define MSG_END_OF_FRAME	0x7f

struct itv2_info {
	int		sockfd;
	char		integration_id[12];
	uint16_t	firmware_version;
	int		seq;
	int		debug;
};

struct msg_head {
	uint8_t length;
	uint16_t type;
} __attribute ((packed));

struct msg_foot {
	uint16_t crc;
} __attribute ((packed));

struct msg_inform {
	uint8_t unk1;
	uint8_t unk2;
	uint8_t seq;
	uint8_t unk3;
	uint8_t unk4;
	uint8_t unk5;
	uint16_t firmware_version;
	uint16_t protocol_version;
	uint8_t unk6;
	uint8_t unk7;
	uint8_t unk8;
	uint8_t unk9;
	uint8_t unk10;
	uint8_t unk11;
	uint8_t unk12;
} __attribute ((packed));

struct msg_inform_reply {
	uint8_t unk1;
	uint8_t unk2;
	uint8_t seq;
	uint8_t unused;
} __attribute ((packed));

/*
 * Setup
 */

static int connect_to_server(struct itv2_info *info, const char *host, int port)
{
	struct sockaddr_in serv_addr = {};

	if ((info->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Failed to create socket: %d\n", errno);
		return errno;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
		fprintf(stderr, "Failed to parse address \"%s\": %d\n", host, errno);
		return errno;
	}

	if (connect(info->sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "Failed to connect to server: %d\n", errno);
		return errno;
	}

	return 0;
}

/*
 * Messages handling
 */

static int send_data(struct itv2_info *info, const void *buf, size_t len)
{
	size_t bytes;
	size_t sent;

	for (sent = 0; sent < len; sent += bytes) {
		bytes = write(info->sockfd, (uint8_t *)buf + sent, len - sent);
		if (bytes <= 0) {
			fprintf(stderr, "Failed to send the last %zd bytes: %zd\n", len - sent, bytes);
			return bytes ? bytes : -EIO;
		}
	}

	return 0;
}

static int msg_send(struct itv2_info *info, const void *msg, size_t len)
{
	const uint8_t *data = msg;
	static uint8_t *buf;
	static size_t size;
	size_t offset;
	size_t total;
	int err;
	int i;

	total = 0;
	total += sizeof(uint8_t); /* start of frame */
	total += len;
	for (i = 0; i < len; i++) {
		if (data[i] == 0x7d || data[i] == 0x7e || data[i] == 0x7f) {
			total++;
		}
	}
	total += sizeof(uint8_t); /* end of frame */

	if (total > size) {
		buf = realloc(buf, total);
		if (!buf) {
			return -ENOMEM;
		}
		size = total;
	}

	offset = 0;

	buf[offset++] = MSG_START_OF_FRAME;
	for (i = 0; i < len; i++) {
		switch (data[i]) {
		case 0x7d:
			buf[offset++] = 0x7d;
			buf[offset++] = 0x00;
			break;
		case 0x7e:
			buf[offset++] = 0x7d;
			buf[offset++] = 0x01;
			break;
		case 0x7f:
			buf[offset++] = 0x7d;
			buf[offset++] = 0x02;
			break;
		default:
			buf[offset++] = data[i];
			break;
		}
	}
	buf[offset++] = MSG_END_OF_FRAME;

	if (offset != total) {
		fprintf(stderr, "Failed to calculate packet size\n");
		return -EINVAL;
	}

	if (info->debug) {
		printf("Sending:");
		for (i = 0; i < offset; i++) {
			printf(" %02x", buf[i]);
		}
		printf("\n");
	}

	err = send_data(info, info->integration_id, sizeof(info->integration_id));
	if (err)
		return err;

	err = send_data(info, buf, offset);
	if (err)
		return err;

	return 0;
}

static int msg_build_and_send(struct itv2_info *info, uint16_t type, const void *content, size_t len)
{
	struct msg_head *head;
	struct msg_foot *foot;
	static size_t size;
	static void *buf;
	size_t total;
	uint16_t crc;

	total = 0;
	total += sizeof(*head);
	total += len;
	total += sizeof(*foot);

	if (total > size) {
		buf = realloc(buf, total);
		if (!buf) {
			return -ENOMEM;
		}
		size = total;
	}

	head = buf;
	head->length = sizeof(*head) - 1 + len + sizeof(*foot);
	head->type = htons(type);

	memcpy((uint8_t *)buf + sizeof(*head), content, len);

	foot = (void *)((uint8_t *)buf + sizeof(*head) + len);
	crc = 0xffff;
	crc = crc16(crc, head, sizeof(*head));
	crc = crc16(crc, content, len);
	foot->crc = htons(crc);

	return msg_send(info, buf, total);
}

static int read_message(struct itv2_info *info, uint16_t *type, void **content, size_t *len)
{
	struct msg_head *head;
	struct msg_foot *foot;
	size_t received;
	uint16_t crc;
	uint8_t *buf = NULL;
	size_t size = 0;
	int offset;
	void *end;
	int i;

	received = 0;
	do {
		size_t bytes;

		if (received + 0x40 > size) {
			size += 0x40;
			buf = realloc(buf, size);
			if (!buf) {
				return -ENOMEM;
			}
		}

		bytes = read(info->sockfd, buf + received, size - received);
		if (bytes <= 0) {
			fprintf(stderr, "Failed to read from socket: %zd\n", bytes);
			return bytes;
		}

		received += bytes;

		end = memchr(buf, 0x7f, received);
	} while (!end);

	if (info->debug) {
		printf("Received:");
		for (i = 0; i < received; i++) {
			printf(" %02x", buf[i]);
		}
		printf("\n");
	}

	if (buf[0] != 0x7e) {
		fprintf(stderr, "Failed to parse server message (incorrect first byte: %02x)\n", buf[0]);
		return -EINVAL;
	}

	offset = 0;
	for (i = 1; i < received - 1; i++) {
		switch (buf[i]) {
		case 0x7d:
			switch (buf[i + 1]) {
			case 0x00:
				buf[offset++] = 0x7d;
				break;
			case 0x01:
				buf[offset++] = 0x7e;
				break;
			case 0x02:
				buf[offset++] = 0x7f;
				break;
			}
			break;
		default:
			buf[offset++] = buf[i];
		}
	}

	head = (void *)buf;
	if (offset - 1 != head->length) {
		fprintf(stderr, "Failed to verify message length (declared: %02x calculated: %02x)\n", head->length, offset - 1);
		return -ENOMEM;
	}

	foot = (void *)(buf + offset - 2);
	crc = 0xffff;
	crc = crc16(crc, buf, offset - 2);
	if (crc != ntohs(foot->crc)) {
		fprintf(stderr, "Failed to verify CRC16 (received: %0x4 calculated: %04x)\n", ntohs(foot->crc), crc);
		return -ENOMEM;
	}

	*type = ntohs(head->type);
	*len = offset - sizeof(*head) - sizeof(*foot);
	*content = memmove(buf, buf + sizeof(*head), *len);

	return 0;
}

/*
 * Client
 */

static int send_inform(struct itv2_info *info)
{
	struct msg_inform inform = {
		.unk1 = 0x06,
		.unk2 = 0x0a,
		.seq = ++info->seq,
		.unk3 = 0x02,
		.unk4 = 0x03,
		.unk5 = 0x2c,
		.firmware_version = htons(info->firmware_version),
		.protocol_version = htons(PROTOCOL_VERSION),
		.unk6 = 0x02,
		.unk7 = 0x00,
		.unk8 = 0x02,
		.unk9 = 0x00,
		.unk10 = 0x00,
		.unk11 = 0x01,
		.unk12 = 0x01,
	};
	void *content;
	uint16_t type;
	size_t len;
	int err;

	err = msg_build_and_send(info, 0x0000, &inform, sizeof(inform));
	if (err)
		return err;

	err = read_message(info, &type, &content, &len);
	if (err)
		return err;

	if (type != 0x0100) {
		fprintf(stderr, "Unexpected Inform message response: %04x\n", type);
		return -EPROTO;
	}

	return 0;
}

static int handle_inform(struct itv2_info *info, const void *content, size_t len)
{
	const struct msg_inform *inform = content;
	struct msg_inform_reply reply;

	if (len != sizeof(*inform)) {
		fprintf(stderr, "Failed to parse Inform message (length: %02zx expected: %02zx)\n", len, sizeof(*inform));
		return -EPROTO;
	}

	reply.unk1 = 0x05;
	reply.unk2 = 0x02;
	reply.seq = inform->seq;

	return msg_build_and_send(info, 0x0102, &reply, sizeof(reply));
}

static int send_empty_listen(struct itv2_info *info, uint16_t reqtype)
{
	void *content;
	uint16_t type;
	size_t len;
	int cont;
	int err;

	err = msg_build_and_send(info, reqtype, NULL, 0);
	if (err)
		return err;
	if (info->debug) {
		printf("\n");
	}

	cont = 1;
	while (cont) {
		err = read_message(info, &type, &content, &len);
		if (err)
			return err;

		switch (type) {
		case 0x0200:
			err = handle_inform(info, content, len);
			if (err) {
				fprintf(stderr, "Failed to handle server's Inform: %d\n", err);
				return err;
			}
			break;
		case 0x0201:
			cont = 0;
			break;
		default:
			fprintf(stderr, "Received unsupported message type: %04x\n", type);
		}
		if (info->debug) {
			printf("\n");
		}
	}

	return 0;
}

static int send_enc_req(struct itv2_info *info)
{
	fprintf(stderr, "Support for encryption missing\n");

	return -ENOTSUP;
}

/**************************************************
 * Start
 **************************************************/

static void usage() {
	printf("Usage:\n");
	printf("notification [options] host port\n");
	printf("\n");
	printf("Options:\n");
	printf("-i num\t\t\tintegration ID\n");
	printf("-f x.y\t\t\tfirmware version\n");
	printf("-d\t\t\tdebug mode\n");
}

int main(int argc, char **argv) {
	struct itv2_info info = {
		.integration_id = "123456789012",
		.firmware_version = 0x0411,
	};
	uint8_t fwver[2];
	int err;
	int c;

	while ((c = getopt(argc, argv, "i:f:d")) != -1) {
		switch (c) {
		case 'i':
			memcpy(info.integration_id, optarg, sizeof(info.integration_id));
			break;
		case 'f':
			if (sscanf(optarg, "%hhu.%hhu", &fwver[0], &fwver[1]) != 2) {
				usage();
				fprintf(stderr, "\n");
				fprintf(stderr, "Failed to parse firmware version \"%s\"\n", optarg);
				return -EINVAL;
			}
			info.firmware_version = (fwver[0] << 8) | fwver[1];
			break;
		case 'd':
			info.debug = 1;
			break;
		}
	}

	if (argc - optind != 2) {
		usage();
		fprintf(stderr, "\n");
		fprintf(stderr, "Missing host / port\n");
		return -EINVAL;
	}

	err = connect_to_server(&info, argv[argc - 2], strtol(argv[argc - 1], NULL, 0));
	if (err) {
		fprintf(stderr, "Failed to connect to the integration server: %d\n", err);
		return errno;
	}

	/* Send Inform and read reply */
	err = send_inform(&info);
	if (err)
		return err;
	if (info.debug) {
		printf("\n");
	}

	/* Let server send its requests and handle them */
	err = send_empty_listen(&info, 0x0001);
	if (err)
		return err;

	/* Request encryption */
	err = send_enc_req(&info);
	if (err)
		return err;

	return 0;
}
