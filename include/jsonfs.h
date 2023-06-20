typedef struct entry {
    int inode;
    char *name;
    off_t size;

    struct entry *next;
} entry;

typedef struct reg_file {
    int inode;
    char *type;
    char *name;
    char *data;

    struct reg_file *next;
} reg_file;

typedef struct dir_file {
    int inode;
    char *type;
    struct entry *entries;

    struct dir_file *next;
} dir_file;