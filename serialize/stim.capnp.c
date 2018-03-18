#include "stim.capnp.h"
/* AUTO GENERATED - DO NOT EDIT */
#ifdef __GNUC__
# define capnp_unused __attribute__((unused))
# define capnp_use(x) (void) x;
#else
# define capnp_unused
# define capnp_use(x)
#endif

static const capn_text capn_val0 = {0,"",0};

String_ptr new_String(struct capn_segment *s) {
	String_ptr p;
	p.p = capn_new_struct(s, 0, 1);
	return p;
}
String_list new_String_list(struct capn_segment *s, int len) {
	String_list p;
	p.p = capn_new_list(s, len, 0, 1);
	return p;
}
void read_String(struct String *s capnp_unused, String_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->string = capn_get_text(p.p, 0, capn_val0);
}
void write_String(const struct String *s capnp_unused, String_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_set_text(p.p, 0, s->string);
}
void get_String(struct String *s, String_list l, int i) {
	String_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_String(s, p);
}
void set_String(const struct String *s, String_list l, int i) {
	String_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_String(s, p);
}

ProfilePin_ptr new_ProfilePin(struct capn_segment *s) {
	ProfilePin_ptr p;
	p.p = capn_new_struct(s, 24, 5);
	return p;
}
ProfilePin_list new_ProfilePin_list(struct capn_segment *s, int len) {
	ProfilePin_list p;
	p.p = capn_new_list(s, len, 24, 5);
	return p;
}
void read_ProfilePin(struct ProfilePin *s capnp_unused, ProfilePin_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->dutId = (int32_t) ((int32_t)capn_read32(p.p, 0));
	s->pinName = capn_get_text(p.p, 0, capn_val0);
	s->compName = capn_get_text(p.p, 1, capn_val0);
	s->netName = capn_get_text(p.p, 2, capn_val0);
	s->netAlias = capn_get_text(p.p, 3, capn_val0);
	s->tag = (enum ProfilePin_ProfileTags)(int) capn_read16(p.p, 4);
	s->tagData = (int32_t) ((int32_t)capn_read32(p.p, 8));
	s->dutIoId = (int32_t) ((int32_t)capn_read32(p.p, 12));
	s->numDestPinNames = capn_read32(p.p, 16);
	s->destPinNames.p = capn_getp(p.p, 4, 0);
}
void write_ProfilePin(const struct ProfilePin *s capnp_unused, ProfilePin_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write32(p.p, 0, (uint32_t) (s->dutId));
	capn_set_text(p.p, 0, s->pinName);
	capn_set_text(p.p, 1, s->compName);
	capn_set_text(p.p, 2, s->netName);
	capn_set_text(p.p, 3, s->netAlias);
	capn_write16(p.p, 4, (uint16_t) (s->tag));
	capn_write32(p.p, 8, (uint32_t) (s->tagData));
	capn_write32(p.p, 12, (uint32_t) (s->dutIoId));
	capn_write32(p.p, 16, s->numDestPinNames);
	capn_setp(p.p, 4, s->destPinNames.p);
}
void get_ProfilePin(struct ProfilePin *s, ProfilePin_list l, int i) {
	ProfilePin_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_ProfilePin(s, p);
}
void set_ProfilePin(const struct ProfilePin *s, ProfilePin_list l, int i) {
	ProfilePin_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_ProfilePin(s, p);
}

VecChunk_ptr new_VecChunk(struct capn_segment *s) {
	VecChunk_ptr p;
	p.p = capn_new_struct(s, 16, 1);
	return p;
}
VecChunk_list new_VecChunk_list(struct capn_segment *s, int len) {
	VecChunk_list p;
	p.p = capn_new_list(s, len, 16, 1);
	return p;
}
void read_VecChunk(struct VecChunk *s capnp_unused, VecChunk_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->id = capn_read8(p.p, 0);
	s->numVecs = capn_read32(p.p, 4);
	s->vecData = capn_get_data(p.p, 0);
	s->vecDataSize = capn_read32(p.p, 8);
	s->vecDataCompressedSize = capn_read32(p.p, 12);
}
void write_VecChunk(const struct VecChunk *s capnp_unused, VecChunk_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write8(p.p, 0, s->id);
	capn_write32(p.p, 4, s->numVecs);
	capn_setp(p.p, 0, s->vecData.p);
	capn_write32(p.p, 8, s->vecDataSize);
	capn_write32(p.p, 12, s->vecDataCompressedSize);
}
void get_VecChunk(struct VecChunk *s, VecChunk_list l, int i) {
	VecChunk_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_VecChunk(s, p);
}
void set_VecChunk(const struct VecChunk *s, VecChunk_list l, int i) {
	VecChunk_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_VecChunk(s, p);
}

SerialStim_ptr new_SerialStim(struct capn_segment *s) {
	SerialStim_ptr p;
	p.p = capn_new_struct(s, 16, 2);
	return p;
}
SerialStim_list new_SerialStim_list(struct capn_segment *s, int len) {
	SerialStim_list p;
	p.p = capn_new_list(s, len, 16, 2);
	return p;
}
void read_SerialStim(struct SerialStim *s capnp_unused, SerialStim_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	s->type = (enum SerialStim_StimTypes)(int) capn_read16(p.p, 0);
	s->numPins = capn_read16(p.p, 2);
	s->pins.p = capn_getp(p.p, 0, 0);
	s->numVecs = capn_read32(p.p, 4);
	s->numUnrolledVecs = capn_read32(p.p, 8);
	s->numVecChunks = capn_read32(p.p, 12);
	s->vecChunks.p = capn_getp(p.p, 1, 0);
}
void write_SerialStim(const struct SerialStim *s capnp_unused, SerialStim_ptr p) {
	capn_resolve(&p.p);
	capnp_use(s);
	capn_write16(p.p, 0, (uint16_t) (s->type));
	capn_write16(p.p, 2, s->numPins);
	capn_setp(p.p, 0, s->pins.p);
	capn_write32(p.p, 4, s->numVecs);
	capn_write32(p.p, 8, s->numUnrolledVecs);
	capn_write32(p.p, 12, s->numVecChunks);
	capn_setp(p.p, 1, s->vecChunks.p);
}
void get_SerialStim(struct SerialStim *s, SerialStim_list l, int i) {
	SerialStim_ptr p;
	p.p = capn_getp(l.p, i, 0);
	read_SerialStim(s, p);
}
void set_SerialStim(const struct SerialStim *s, SerialStim_list l, int i) {
	SerialStim_ptr p;
	p.p = capn_getp(l.p, i, 0);
	write_SerialStim(s, p);
}
