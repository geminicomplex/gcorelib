#ifndef CAPN_DDE41F3D290D408E
#define CAPN_DDE41F3D290D408E
/* AUTO GENERATED - DO NOT EDIT */
#include <capnp_c.h>

#if CAPN_VERSION != 1
#error "version mismatch between capnp_c.h and generated code"
#endif

#ifndef capnp_nowarn
# ifdef __GNUC__
#  define capnp_nowarn __extension__
# else
#  define capnp_nowarn
# endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct String;
struct ProfilePin;
struct VecChunk;
struct SerialStim;

typedef struct {capn_ptr p;} String_ptr;
typedef struct {capn_ptr p;} ProfilePin_ptr;
typedef struct {capn_ptr p;} VecChunk_ptr;
typedef struct {capn_ptr p;} SerialStim_ptr;

typedef struct {capn_ptr p;} String_list;
typedef struct {capn_ptr p;} ProfilePin_list;
typedef struct {capn_ptr p;} VecChunk_list;
typedef struct {capn_ptr p;} SerialStim_list;

enum ProfilePin_ProfileTags {
	ProfilePin_ProfileTags_profileTagNone = 0,
	ProfilePin_ProfileTags_profileTagCclk = 1,
	ProfilePin_ProfileTags_profileTagResetB = 2,
	ProfilePin_ProfileTags_profileTagCsiB = 3,
	ProfilePin_ProfileTags_profileTagRdwrB = 4,
	ProfilePin_ProfileTags_profileTagProgramB = 5,
	ProfilePin_ProfileTags_profileTagInitB = 6,
	ProfilePin_ProfileTags_profileTagDone = 7,
	ProfilePin_ProfileTags_profileTagData = 8,
	ProfilePin_ProfileTags_profileTagGpio = 9
};

enum VecChunk_ArtixSelects {
	VecChunk_ArtixSelects_artixSelectNone = 0,
	VecChunk_ArtixSelects_artixSelectA1 = 1,
	VecChunk_ArtixSelects_artixSelectA2 = 2
};

enum SerialStim_StimTypes {
	SerialStim_StimTypes_stimTypeNone = 0,
	SerialStim_StimTypes_stimTypeRbt = 1,
	SerialStim_StimTypes_stimTypeBin = 2,
	SerialStim_StimTypes_stimTypeBit = 3,
	SerialStim_StimTypes_stimTypeDots = 4,
	SerialStim_StimTypes_stimTypeRaw = 5
};

struct String {
	capn_text string;
};

static const size_t String_word_count = 0;

static const size_t String_pointer_count = 1;

static const size_t String_struct_bytes_count = 8;

struct ProfilePin {
	int32_t dutId;
	capn_text pinName;
	capn_text compName;
	capn_text netName;
	capn_text netAlias;
	enum ProfilePin_ProfileTags tag;
	int32_t tagData;
	int32_t dutIoId;
	uint32_t numDests;
	capn_list32 destDutIds;
	String_list destPinNames;
};

static const size_t ProfilePin_word_count = 3;

static const size_t ProfilePin_pointer_count = 6;

static const size_t ProfilePin_struct_bytes_count = 72;

struct VecChunk {
	uint8_t id;
	enum VecChunk_ArtixSelects artixSelect;
	uint32_t numVecs;
	capn_data vecData;
	uint32_t vecDataSize;
	uint32_t vecDataCompressedSize;
};

static const size_t VecChunk_word_count = 2;

static const size_t VecChunk_pointer_count = 1;

static const size_t VecChunk_struct_bytes_count = 24;

struct SerialStim {
	enum SerialStim_StimTypes type;
	uint16_t numPins;
	ProfilePin_list pins;
	uint32_t numVecs;
	uint32_t numUnrolledVecs;
	uint32_t numPaddingVecs;
	uint32_t numA1VecChunks;
	uint32_t numA2VecChunks;
	VecChunk_list a1VecChunks;
	VecChunk_list a2VecChunks;
};

static const size_t SerialStim_word_count = 3;

static const size_t SerialStim_pointer_count = 3;

static const size_t SerialStim_struct_bytes_count = 48;

String_ptr new_String(struct capn_segment*);
ProfilePin_ptr new_ProfilePin(struct capn_segment*);
VecChunk_ptr new_VecChunk(struct capn_segment*);
SerialStim_ptr new_SerialStim(struct capn_segment*);

String_list new_String_list(struct capn_segment*, int len);
ProfilePin_list new_ProfilePin_list(struct capn_segment*, int len);
VecChunk_list new_VecChunk_list(struct capn_segment*, int len);
SerialStim_list new_SerialStim_list(struct capn_segment*, int len);

void read_String(struct String*, String_ptr);
void read_ProfilePin(struct ProfilePin*, ProfilePin_ptr);
void read_VecChunk(struct VecChunk*, VecChunk_ptr);
void read_SerialStim(struct SerialStim*, SerialStim_ptr);

void write_String(const struct String*, String_ptr);
void write_ProfilePin(const struct ProfilePin*, ProfilePin_ptr);
void write_VecChunk(const struct VecChunk*, VecChunk_ptr);
void write_SerialStim(const struct SerialStim*, SerialStim_ptr);

void get_String(struct String*, String_list, int i);
void get_ProfilePin(struct ProfilePin*, ProfilePin_list, int i);
void get_VecChunk(struct VecChunk*, VecChunk_list, int i);
void get_SerialStim(struct SerialStim*, SerialStim_list, int i);

void set_String(const struct String*, String_list, int i);
void set_ProfilePin(const struct ProfilePin*, ProfilePin_list, int i);
void set_VecChunk(const struct VecChunk*, VecChunk_list, int i);
void set_SerialStim(const struct SerialStim*, SerialStim_list, int i);

#ifdef __cplusplus
}
#endif
#endif
