/*
 * Copyright (c) 2022 Kontron Europe GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAGIC 'V'
#define VERSION 1
#define LEGACY_OFFSET 0x1000

struct otp_data {
	uint8_t magic;
	uint8_t version;
	char serial[15];
	struct ether_addr basemac;
	uint8_t crc8;
};

/*
 * Yep, our own ether_ntoa(). That is because the glibc one may just print
 * single digits.
 */
char *ether_ntoa(const struct ether_addr *addr)
{
	static char buf[18];
	sprintf (buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		addr->ether_addr_octet[0], addr->ether_addr_octet[1],
		addr->ether_addr_octet[2], addr->ether_addr_octet[3],
		addr->ether_addr_octet[4], addr->ether_addr_octet[5]);

	return buf;
}

void ether_add_offset(struct ether_addr *addr, int offset)
{
	uint64_t val = 0;
	int i;

	for (i = 0; i < 6; i++) {
		val <<= 8;
		val |= addr->ether_addr_octet[i];
	}
	val += offset;
	for (i = 5; i > 0; i--) {
		addr->ether_addr_octet[i] = val & 0xff;
		val >>= 8;
	}
}

/* CRC-8 (ITU-T) polynomial is 0x07, init is 0x00 */
uint8_t crc8(uint8_t *buf, size_t len)
{
	int i;
	uint8_t crc = 0x00;

	while (len--) {
		crc ^= *buf++;
		for (i = 0; i < 8; i++)
			crc = crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1;
	}
	return crc;
}

void usage(const char *progname)
{
	fprintf(stderr,
		"Usage: %s <device> [key]\n"
		"\n"
		"The key can be one of:\n"
		"  serial-number         prints the serial number\n"
		"  base-mac-address      prints the base MAC address\n"
		"  mac-address <offset>  prints the MAC address with a given offset\n",
		progname);
}

int main(int argc,char *argv[])
{
	struct otp_data vpd;
	int fd, val, ret;
	uint8_t crc;
	char *key = NULL;

	if (argc < 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (argc >= 3)
		key = argv[2];

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open()");
		return EXIT_FAILURE;
	}

	val = MTD_OTP_USER;
	ret = ioctl(fd, OTPSELECT, &val);
	if (ret < 0) {
		perror("ioctl(OTPSELECT)");
		return EXIT_FAILURE;
	}

	/* Older kernels didn't map the OTP data at the beginning. */
	ret = lseek(fd, LEGACY_OFFSET, SEEK_SET);
	if (ret < 0) {
		ret = lseek(fd, 0, SEEK_SET);
		if (ret < 0) {
			perror("lseek()");
			return EXIT_FAILURE;
		}
	}

	ret = read(fd, &vpd, sizeof(vpd));
	if (ret < 0) {
		perror("read()");
		return EXIT_FAILURE;
	} else if (ret != sizeof(vpd)) {
		fprintf(stderr, "short read (%d)\n", ret);
		return EXIT_FAILURE;
	}

	if (vpd.magic != MAGIC) {
		fprintf(stderr, "VPD magic mismatch (%d)\n", vpd.magic);
		return EXIT_FAILURE;
	}

	if (vpd.version != VERSION) {
		fprintf(stderr, "VPD version mismatch (%d)\n",
			vpd.version);
		return EXIT_FAILURE;
	}

	crc = crc8((void*)&vpd, sizeof(vpd) - 1);
	if (vpd.crc8 != crc) {
		fprintf(stderr,
			"VPD checksum mismatch (got %02Xh, expected %02Xh)\n",
			crc, vpd.crc8);
		return EXIT_FAILURE;
	}

	if (!key || !strcmp(key, "serial"))
		printf("%.*s\n", (int)sizeof(vpd.serial), vpd.serial);
	if (!key || !strcmp(key, "base-mac-address"))
		printf("%s\n", ether_ntoa(&vpd.basemac));
	if (key && !strcmp(key, "mac-address")) {
		if (argc < 4) {
			usage(argv[0]);
			return EXIT_FAILURE;
		}
		ether_add_offset(&vpd.basemac, atoi(argv[3]));
		printf("%s\n", ether_ntoa(&vpd.basemac));
	}

	close(fd);
	return EXIT_SUCCESS;
}
