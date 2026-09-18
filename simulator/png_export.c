/**
 * @file png_export.c
 * @brief 零依赖 PNG 编码器：RGB565 -> 24位 PNG
 *
 * 采用 zlib stored block（无压缩 deflate），实现极简、无需 libpng/zlib；
 * 文件体积比压缩 PNG 大（约 w*h*3 字节），PC 工具场景够用。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png_export.h"

/* -------- CRC32（PNG 标准，多项式 0xEDB88320） -------- */
static uint32_t crc_table[256];
static int crc_ready = 0;

/**
 * @brief 初始化 CRC 查找表
 */
static void crc_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_ready = 1;
}

/**
 * @brief Adler32 校验（zlib 流尾部）
 */
static uint32_t adler32_buf(const uint8_t *buf, size_t len)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + buf[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

/**
 * @brief 大端写入 32 位
 */
static void put_be32(FILE *f, uint32_t v)
{
    fputc((int)((v >> 24) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)(v & 0xFF), f);
}

/**
 * @brief 写入一个 PNG 块（长度 + 类型 + 数据 + CRC）
 */
static void write_chunk(FILE *f, const char *type, const uint8_t *data,
                        uint32_t len)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t i;

    put_be32(f, len);
    fwrite(type, 1, 4, f);
    if (len > 0 && data != NULL) {
        fwrite(data, 1, len, f);
    }
    /* -------- CRC 覆盖 类型 + 数据 -------- */
    if (!crc_ready) {
        crc_init();
    }
    for (i = 0; i < 4; i++) {
        c = crc_table[(c ^ (uint8_t)type[i]) & 0xFF] ^ (c >> 8);
    }
    for (i = 0; i < len && data != NULL; i++) {
        c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    put_be32(f, c ^ 0xFFFFFFFFu);
}

int png_export_rgb565(const char *path, const uint16_t *px, int w, int h)
{
    static const uint8_t png_sig[8] =
        {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    FILE *f;
    uint8_t ihdr[13];
    uint8_t *raw = NULL;   /* 每行：filter 字节 0 + RGB888 */
    uint8_t *zbuf = NULL;  /* zlib 流 */
    size_t raw_len, zlen, off;
    uint32_t adler;
    int row, x;

    if (path == NULL || px == NULL || w < 1 || h < 1) {
        return -1;
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        return -1;
    }

    /* -------- 组装原始扫描线数据（filter=0） -------- */
    raw_len = (size_t)h * (1 + (size_t)w * 3);
    raw = (uint8_t *)malloc(raw_len);
    if (raw == NULL) {
        fclose(f);
        return -2;
    }
    for (row = 0; row < h; row++) {
        uint8_t *dst = raw + (size_t)row * (1 + (size_t)w * 3);
        const uint16_t *src = px + (size_t)row * w;
        *dst++ = 0;  /* filter: None */
        for (x = 0; x < w; x++) {
            uint16_t c = src[x];
            *dst++ = (uint8_t)((c >> 8) & 0xF8) | (c >> 13);  /* R */
            *dst++ = (uint8_t)((c >> 3) & 0xFC) | ((c >> 9) & 0x03);  /* G */
            *dst++ = (uint8_t)((c << 3) & 0xF8) | ((c >> 2) & 0x07);  /* B */
        }
    }

    /* -------- zlib 流：头 + stored blocks + adler32 -------- */
    zlen = 2 + raw_len + (raw_len / 65535 + 1) * 5 + 4;
    zbuf = (uint8_t *)malloc(zlen);
    if (zbuf == NULL) {
        free(raw);
        fclose(f);
        return -2;
    }
    off = 0;
    zbuf[off++] = 0x78;  /* CMF: deflate, 32K 窗口 */
    zbuf[off++] = 0x01;  /* FLG: 无字典，最低压缩级别 */
    {
        size_t sent = 0;
        while (sent < raw_len) {
            size_t block = raw_len - sent;
            if (block > 65535) {
                block = 65535;
            }
            zbuf[off++] = (sent + block >= raw_len) ? 1 : 0;  /* BFINAL */
            zbuf[off++] = (uint8_t)(block & 0xFF);            /* LEN 小端 */
            zbuf[off++] = (uint8_t)(block >> 8);
            zbuf[off++] = (uint8_t)(~block & 0xFF);           /* NLEN */
            zbuf[off++] = (uint8_t)((~block >> 8) & 0xFF);
            memcpy(zbuf + off, raw + sent, block);
            off += block;
            sent += block;
        }
    }
    adler = adler32_buf(raw, raw_len);
    zbuf[off++] = (uint8_t)(adler >> 24);
    zbuf[off++] = (uint8_t)(adler >> 16);
    zbuf[off++] = (uint8_t)(adler >> 8);
    zbuf[off++] = (uint8_t)adler;

    /* -------- 写文件 -------- */
    fwrite(png_sig, 1, 8, f);
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);  ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);  ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;   /* 位深 */
    ihdr[9] = 2;   /* 颜色类型：真彩 RGB */
    ihdr[10] = 0;  /* 压缩 */
    ihdr[11] = 0;  /* 滤波 */
    ihdr[12] = 0;  /* 隔行 */
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", zbuf, (uint32_t)off);
    write_chunk(f, "IEND", NULL, 0);

    free(raw);
    free(zbuf);
    fclose(f);
    return 0;
}
