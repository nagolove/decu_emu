#include "machine.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct machine *
machine_init (WINDOW *game_screen)
{
	size_t sz_machine = sizeof (struct machine);
	struct machine *m = malloc (sz_machine);
	memset (m, 0, sz_machine);

	m->cpu.s = 0xffff;
	m->is_run = 0;
	m->game_screen = game_screen;
	m->timer.tv_sec = 0;
	m->timer.tv_nsec = 16 * 1000;

	return m;
}

void
machine_input (struct machine *m, int c)
{

	switch (c) {
		case 'h':
			m->cross[0] = 1;
			break;
		case 'j':
			m->cross[1] = 1;
			break;
		case 'k':
			m->cross[2] = 1;
			break;
		case 'l':
			m->cross[3] = 1;
			break;
		case 'w':
			m->button[0] = 1;
			break;
		case 'e':
			m->button[1] = 1;
			break;
		case 's':
			m->button[2] = 1;
			break;
		case 'd':
			m->button[3] = 1;
			break;
		case 'q':
			m->is_run = 0;
			break;
	}
}

static void
get_print_screen (struct machine *m, uint8_t *b)
{
	char sym[2];
	snprintf (sym, 2, "%c", m->cpu.a);

	if (m->cpu.x > MAX_WINDOW_GAME_WIDTH) {
		m->cpu.x = MAX_WINDOW_GAME_WIDTH + 1;
	}
	if (m->cpu.y >= MAX_WINDOW_GAME_HEIGHT) {
		m->cpu.y = MAX_WINDOW_GAME_HEIGHT - 1;
	}

	m->cpu.a = mvwgetch (m->game_screen, m->cpu.y + 1, m->cpu.x + 1);

	m->cpu.ip += 2;
}

static void
print_screen (struct machine *m, uint8_t *b)
{
	char sym[2];
	snprintf (sym, 2, "%c", m->cpu.a);

	if (m->cpu.x > MAX_WINDOW_GAME_WIDTH) {
		m->cpu.x = MAX_WINDOW_GAME_WIDTH + 1;
	}
	if (m->cpu.y >= MAX_WINDOW_GAME_HEIGHT) {
		m->cpu.y = MAX_WINDOW_GAME_HEIGHT - 1;
	}

	mvwaddstr (m->game_screen, m->cpu.y + 1, m->cpu.x + 1, sym);
	wrefresh (m->game_screen);

	m->cpu.ip += 2;
}

static void
set_timer (struct machine *m, uint8_t *b)
{
	uint16_t value = *(uint16_t *) &b[m->cpu.ip + 1];
	m->timer.tv_nsec = value * 1000;

	m->cpu.ip += 2;
}

static void
get_timer (struct machine *m, uint8_t *b)
{
	m->cpu.a = m->timer.tv_nsec / 1000;

	m->cpu.ip += 2;
}

static void
handle_out (struct machine *m, uint8_t *b)
{
	switch (b[m->cpu.ip + 1]) {
		case ADDR_TIMER:
			set_timer (m, b);
			break;
		case ADDR_SCREEN:
			print_screen (m, b);
			break;
		default:
			m->cpu.ip++;
			break;
	}
}

static void
get_cross (struct machine *m, uint8_t *b)
{
	uint8_t val = 0;
	val |= (m->cross[0]? 1 << 3: 0);
	val |= (m->cross[1]? 1 << 2: 0);
	val |= (m->cross[2]? 1 << 1: 0);
	val |= (m->cross[3]? 1 << 0: 0);

	m->cpu.a = val;

	m->cpu.ip += 2;
}

static void
get_buttons (struct machine *m, uint8_t *b)
{
	uint8_t val = 0;
	val |= (m->button[0]? 1 << 3: 0);
	val |= (m->button[1]? 1 << 2: 0);
	val |= (m->button[2]? 1 << 1: 0);
	val |= (m->button[3]? 1 << 0: 0);

	m->cpu.a = val;

	m->cpu.ip += 2;
}

static void
handle_in (struct machine *m, uint8_t *b)
{
	switch (b[m->cpu.ip + 1]) {
		case ADDR_TIMER:
			get_timer (m, b);
			break;
		case ADDR_SCREEN:
			get_print_screen (m, b);
			break;
		case ADDR_CROSS:
			get_cross (m, b);
			break;
		case ADDR_BUTTONS:
			get_buttons (m, b);
			break;
	}
}

uint8_t *
get_register (struct machine *m, uint8_t r, uint8_t *val)
{
	switch (r) {
		case REG_A: return &m->cpu.a;
		case REG_X: return &m->cpu.x;
		case REG_Y: return &m->cpu.y;
		case REG_ADDR: return val;
	}
}

void
handle_ld (struct machine *m, uint8_t *b)
{
	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint8_t value = 0;
	uint8_t is_addr = 0;
	if (reg0 == REG_ADDR || reg1 == REG_ADDR) {
		is_addr = 1;
	}

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;
	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] = *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] = *r1;
	else if (reg1 == REG_ADDR)
		*r0 = b[*r1];
	else
		*r0 = *r1;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;

}

static void
handle_add (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S|CPU_FLAG_C);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	uint8_t old_r0 = *r0;

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] += *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] += *r1;
	else
		*r0 += *r1;

	uint8_t r = *r0;

	if (r < old_r0) m->cpu.flags |= CPU_FLAG_C;
	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;


	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_and (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);
	uint8_t r = *r0 & *r1;

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] &= *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] &= *r1;
	else
		*r0 &= *r1;

	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	*r0 = r;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_or (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] |= *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] |= *r1;
	else
		*r0 |= *r1;

	uint8_t r = *r0;

	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	*r0 = r;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_xor (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] ^= *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] ^= *r1;
	else
		*r0 ^= *r1;

	uint8_t r = *r0;

	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_sub (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S|CPU_FLAG_C);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	uint8_t old_r0 = *r0;

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		b[*r0] -= *r1;
	else if (reg0 == REG_ADDR)
		b[*r0] -= *r1;
	else
		*r0 -= *r1;

	uint8_t r = *r0;

	if (r > old_r0) m->cpu.flags |= CPU_FLAG_C;
	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_test (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S|CPU_FLAG_C);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);

	uint8_t old_r0 = *r0;
	uint8_t r = 0;

	if (reg0 == REG_ADDR && reg1 == REG_ADDR)
		r = b[*r0] - *r1;
	else if (reg0 == REG_ADDR)
		r = b[*r0] - *r1;
	else
		r = *r0 - *r1;


	if (r > old_r0) m->cpu.flags |= CPU_FLAG_C;
	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_shl (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S|CPU_FLAG_C);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);
	uint8_t r = *r0 << *r1;

	uint8_t bit = 0x80;
	uint8_t is_c = 0;
	for (int i = 0; i < *r1; i++) {
		if (bit & *r0) {
			is_c = 1;
			break;
		}
		bit >>= 1;
	}

	if (is_c) m->cpu.flags |= CPU_FLAG_C;
	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	*r0 = r;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_shr (struct machine *m, uint8_t *b)
{
	m->cpu.flags &= ~(CPU_FLAG_Z|CPU_FLAG_S|CPU_FLAG_C);

	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x3;

	uint16_t off = 1;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + off]);
	if (reg0 == REG_ADDR)
		off++;

	uint8_t *r1 = get_register (m, reg1, &b[m->cpu.ip + off]);
	uint8_t r = *r0 << *r1;

	uint8_t bit = 0x01;
	uint8_t is_c = 0;
	for (int i = 0; i < *r1; i++) {
		if (bit & *r0) {
			is_c = 1;
			break;
		}
		bit <<= 1;
	}

	if (is_c) m->cpu.flags |= CPU_FLAG_C;
	if (r == 0) m->cpu.flags |= CPU_FLAG_Z;
	if ((int8_t) r < 0) m->cpu.flags |= CPU_FLAG_S;

	*r0 = r;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
	if (reg1 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_push (struct machine *m, uint8_t *b)
{
	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + 1]);

	m->cpu.s--;

	b[m->cpu.s] = *r0;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_pop (struct machine *m, uint8_t *b)
{
	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x3;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + 1]);

	*r0 = b[m->cpu.s];

	m->cpu.s++;

	m->cpu.ip++;

	if (reg0 == REG_ADDR)
		m->cpu.ip++;
}

static void
handle_jc (struct machine *m, uint8_t *b)
{
	uint8_t reg0 = (b[m->cpu.ip] >> 2) & 0x03;
	uint8_t reg1 = (b[m->cpu.ip] >> 0) & 0x03;

	uint8_t *r0 = get_register (m, reg0, &b[m->cpu.ip + 1]);

	uint16_t off = 0;

	if (reg0 == REG_ADDR) {
		off = *(uint16_t *) &b[m->cpu.ip + 1];
	} else {
		off = *r0;
	}

	if (reg1 == JC_C) {
		if (m->cpu.flags & CPU_FLAG_C) {
			m->cpu.ip = off;
			return;
		}
	}

	if (reg1 == JC_S) {
		if (m->cpu.flags & CPU_FLAG_S) {
			m->cpu.ip = off;
			return;
		}
	}

	if (reg1 == JC_Z) {
		if (m->cpu.flags & CPU_FLAG_Z) {
			m->cpu.ip = off;
			return;
		}
	}

	if (reg1 == JC_JMP) {
		m->cpu.ip = off;
		return;
	}

	if (reg0 == REG_ADDR)
		m->cpu.ip += 2;
	else 
		m->cpu.ip++;
}

void
machine_run (struct machine *m)
{
	uint8_t *b = &m->hex_editor->bytes[0][0];

	uint8_t opcode = b[m->cpu.ip] >> 4;

	switch (opcode) {
		case ADD:
			handle_add (m, b);
			break;
		case SUB:
			handle_sub (m, b);
			break;
		case AND:
			handle_and (m, b);
			break;
		case OR:
			handle_or (m, b);
			break;
		case XOR:
			handle_xor (m, b);
			break;
		case SHL:
			handle_shl (m, b);
			break;
		case SHR:
			handle_shr (m, b);
			break;
		case LD:
			handle_ld (m, b);
			break;
		case IN:
			handle_in (m, b);
			break;
		case NOP:
			m->cpu.ip++;
			break;
		case OUT:
			handle_out (m, b);
			break;
		case PUSH:
			handle_push (m, b);
			break;
		case POP:
			handle_pop (m, b);
			break;
		case TEST:
			handle_test (m, b);
			break;
		case JC:
			handle_jc (m, b);
			break;
		case HLT:
			m->cpu.ip++;
			m->is_run = 0;
			break;
	}

}
