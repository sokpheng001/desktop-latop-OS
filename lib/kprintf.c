/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Minimal printf-style formatter for kernel messages.
 */

#include "lib/kprintf.h"

/* Enough digits for a 64-bit value in base 10 (20 digits). */
#define NUM_BUF_SIZE 24

/* %p prints a full 64-bit address: "0x" and 16 hex digits. */
#define PTR_HEX_DIGITS 16

static kprintf_sink_fn out_sink;
static void *out_ctx;

struct fmt_state {
	kprintf_sink_fn sink;
	void *ctx;
	int count;
};

static void emit(struct fmt_state *st, char c)
{
	st->sink(c, st->ctx);
	st->count++;
}

static void emit_padded(struct fmt_state *st, const char *s, int len, int width, char pad)
{
	for (int i = len; i < width; i++)
		emit(st, pad);
	for (int i = 0; i < len; i++)
		emit(st, s[i]);
}

/* Unsigned value in base 10 or 16, with an optional leading minus sign. */
static void emit_number(struct fmt_state *st, u64 value, unsigned int base, bool negative,
			int width, char pad)
{
	static const char digits[] = "0123456789abcdef";
	char buf[NUM_BUF_SIZE];
	int len = 0;

	do {
		buf[NUM_BUF_SIZE - 1 - len] = digits[value % base];
		value /= base;
		len++;
	} while (value);

	if (negative) {
		if (pad == '0') {
			/* "-0042": the sign goes before the zero padding. */
			emit(st, '-');
			width--;
		} else {
			buf[NUM_BUF_SIZE - 1 - len] = '-';
			len++;
		}
	}
	emit_padded(st, &buf[NUM_BUF_SIZE - len], len, width, pad);
}

int kvformat(kprintf_sink_fn sink, void *ctx, const char *fmt, va_list ap)
{
	struct fmt_state st = {.sink = sink, .ctx = ctx, .count = 0};

	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			emit(&st, *fmt);
			continue;
		}
		fmt++;

		char pad = ' ';
		int width = 0;
		bool is_long = false;

		if (*fmt == '0') {
			pad = '0';
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');
		if (*fmt == 'l') {
			is_long = true;
			fmt++;
		}

		switch (*fmt) {
		case 'd': {
			s64 v = is_long ? va_arg(ap, long) : va_arg(ap, int);
			/* Negate in unsigned arithmetic so the most negative value works too. */
			u64 mag = v < 0 ? (u64)0 - (u64)v : (u64)v;
			emit_number(&st, mag, 10, v < 0, width, pad);
			break;
		}
		case 'u':
			emit_number(&st,
				    is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int),
				    10, false, width, pad);
			break;
		case 'x':
			emit_number(&st,
				    is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int),
				    16, false, width, pad);
			break;
		case 'p':
			emit(&st, '0');
			emit(&st, 'x');
			emit_number(&st, (uptr)va_arg(ap, void *), 16, false, PTR_HEX_DIGITS, '0');
			break;
		case 'c':
			emit_padded(&st, &(char){(char)va_arg(ap, int)}, 1, width, ' ');
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			int len = 0;

			if (!s)
				s = "(null)";
			while (s[len])
				len++;
			emit_padded(&st, s, len, width, ' ');
			break;
		}
		case '%':
			emit(&st, '%');
			break;
		case '\0':
			/* Format ends with a lone '%': print it and stop. */
			emit(&st, '%');
			return st.count;
		default:
			/* Unknown conversion: print it unchanged so the mistake is visible. */
			emit(&st, '%');
			emit(&st, *fmt);
			break;
		}
	}
	return st.count;
}

void kprintf_set_output(kprintf_sink_fn sink, void *ctx)
{
	out_sink = sink;
	out_ctx = ctx;
}

int kprintf(const char *fmt, ...)
{
	va_list ap;
	int n;

	if (!out_sink)
		return 0;
	va_start(ap, fmt);
	n = kvformat(out_sink, out_ctx, fmt, ap);
	va_end(ap);
	return n;
}
