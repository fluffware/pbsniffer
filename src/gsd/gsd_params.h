
enum {
  GSD_TYPE_UINT8 ,
  GSD_TYPE_UINT16,
  GSD_TYPE_BOOLEAN,
  GSD_TYPE_VISIBLE_STRING,
  GSD_TYPE_OCTET_STRING,
  GSD_TYPE_SECTION_START,
  GSD_TYPE_SECTION_END,
};

#define GSD_MANDATORY 0x01
#define GSD_OPTIONAL 0x02
#define GSD_DEFAULT 0x04
#define GSD_GROUP 0x08
#define GSD_FROM_REV_1 0x100
#define GSD_FROM_REV_2 0x200
#define GSD_MODULAR_STATION 0x400

typedef struct GSDParams GSDParams;
struct GSDParams {
  char *name;
  unsigned int type;
  unsigned int flags;
};
