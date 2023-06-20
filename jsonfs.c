/* taken from https://github.com/fntlnz/fuse-example, and modified */


#define FUSE_USE_VERSION 26
#define INIT_DIR "./mnt"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <fuse.h>
#include <json.h>

#include "jsonfs.h"

dir_file **dir_files_list = NULL;
reg_file **reg_files_list = NULL;
entry *entry_header = NULL;
struct json_object *root;


// fuse
char *get_entry_type(int inode) {
    struct reg_file *reg_entry = (struct reg_file *)reg_files_list;
    while (reg_entry != NULL) {
        if (reg_entry->inode == inode) {
            return reg_entry->type;
        }
        reg_entry = reg_entry->next;
    }

    struct dir_file *dir_entry = (struct dir_file *)dir_files_list;
    while (dir_entry != NULL) {
        if (dir_entry->inode == inode) {
            return dir_entry->type;
        }
        dir_entry = dir_entry->next;
    }

    return NULL; // No matching inode found
}

static int jsonfs_getattr(const char *path, struct stat *stbuf) {
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    // Set the attributes for the root directory
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        struct entry *current = entry_header;
        while (current != NULL) {
            if (strcmp(current->name, path + 1) == 0) {
                if (strcmp(get_entry_type(current->inode), "reg") == 0) {
                    stbuf->st_mode = S_IFREG | 0777;
                    stbuf->st_nlink = 1;
                    stbuf->st_size = current->size;
                } else if (strcmp(get_entry_type(current->inode), "dir") == 0) {
                    stbuf->st_mode = S_IFDIR | 0755;
                    stbuf->st_nlink = 2;
                }
                return res;
            }
            current = current->next;
        }

        res = -ENOENT; // No such file or directory
    }

    return res;
}

static int jsonfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi) {
    (void)offset;
    (void)fi;

    if (strcmp(path, "/") == 0) {
        filler(buf, ".", NULL, 0);
        filler(buf, "..", NULL, 0);

        struct entry *current = entry_header;
        while (current != NULL) {
            filler(buf, current->name, NULL, 0);
            current = current->next;
        }
    }

    return 0;
}

static int jsonfs_open(const char *path, struct fuse_file_info *fi) {
    (void)fi;

    struct entry *current = entry_header;
    while (current != NULL) {
        if (strcmp(current->name, path + 1) == 0) {
            if (strcmp(get_entry_type(current->inode), "reg") == 0) {
                return 0;
            } else if (strcmp(get_entry_type(current->inode), "dir") == 0) {
                return -EISDIR; // Is a directory
            }
        }
        current = current->next;
    }

    return -ENOENT; // No such file or directory
}

struct entry *get_entry_by_inode(int inode) {
    struct entry *current = entry_header;
    while (current != NULL) {
        if (current->inode == inode) {
            return current;
        }
        current = current->next;
    }
    return NULL; // No matching inode found
}

static int jsonfs_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) fi;

    struct entry *current = entry_header;
    while (current != NULL) {
        if (strcmp(current->name, path + 1) == 0) {
            if (strcmp(get_entry_type(current->inode), "reg") == 0) {
                struct reg_file *reg_entry = (struct reg_file *)get_entry_by_inode(current->inode);
                if (reg_entry != NULL) {
                    size_t len = strlen(reg_entry->data);
                    if (offset < len) {
                        if (offset + size > len)
                            size = len - offset;
                        memcpy(buf, reg_entry->data + offset, size);
                    } else {
                        size = 0;
                    }
                    return size;
                } else {
                    return -ENOENT; // No matching regular file entry found
                }
            } else if (strcmp(get_entry_type(current->inode), "dir") == 0) {
                return -EISDIR; // Is a directory
            }
        }
        current = current->next;
    }

    return -ENOENT; // No such file or directory
}

static struct fuse_operations jsonfs_operations = {
    .getattr = jsonfs_getattr,
    .readdir = jsonfs_readdir,
    .open = jsonfs_open,
    .read = jsonfs_read,
};


// jsonc
void insert_entries(struct entry **link, struct json_object *entries) {
    if (entries == NULL)
        return;

    int arr_length = json_object_array_length(entries);

    for (int i = 0; i < arr_length; i++) {
        struct json_object *entry = json_object_array_get_idx(entries, i);

        int inode = -1;
        const char *name = NULL;

        json_object_object_foreach(entry, key, val) {
            if (strcmp(key, "inode") == 0)
                inode = json_object_get_int(val);
            else if (strcmp(key, "name") == 0)
                name = json_object_get_string(val);
        }

        struct entry *new_entry = (struct entry *)malloc(sizeof(struct entry));
        new_entry->inode = inode;
        new_entry->name = strdup(name);
        new_entry->next = NULL;

		struct entry *new_entry_header = (struct entry *)malloc(sizeof(struct entry));
        new_entry_header->inode = inode;
        new_entry_header->name = strdup(name);
        new_entry_header->next = NULL;

        if (*link == NULL) {
            *link = new_entry;
        } else {
            struct entry *curr = *link;

            while (curr->next != NULL) {
                curr = curr->next;
            }

            curr->next = new_entry;
        }

        if (entry_header == NULL) { // set global entry
            entry_header = new_entry_header;
        } else {
            struct entry *curr = entry_header;

            while (curr->next != NULL) {
                curr = curr->next;
            }

            curr->next = new_entry_header;
        }
    }
}


void insert_dir_file(dir_file *file) {
    if (dir_files_list == NULL) {
        dir_files_list = (dir_file **)malloc(sizeof(dir_file *) * 2); // Allocate space for two pointers
        dir_files_list[0] = file; // Set the first element
        dir_files_list[1] = NULL; // Set the second element as NULL
    } else {
        int count = 0;
        while (dir_files_list[count] != NULL) {
            count++;
        }

        dir_files_list = (dir_file **)realloc(dir_files_list, sizeof(dir_file *) * (count + 2)); // Increase the size of the array

        dir_files_list[count] = file; // Append the new file
        dir_files_list[count + 1] = NULL; // Set the next element as NULL
    }
}

void d_entries(struct json_object *entry) {
    int inode = -1;
    const char *type = "dir";
    struct json_object *entries = NULL;

    json_object_object_foreach(entry, key, val) {
        if (strcmp(key, "inode") == 0)
            inode = json_object_get_int(val);
        else if (strcmp(key, "entries") == 0)
            entries = val;
    }

    dir_file *new_dir = (dir_file *)malloc(sizeof(dir_file));
    new_dir->inode = inode;
    new_dir->type = strdup(type);
	new_dir->entries = NULL;
    insert_entries(&(new_dir->entries), entries);
    insert_dir_file(new_dir);
}

void insert_reg_file(reg_file *file) {
    if (reg_files_list == NULL) {
        reg_files_list = (reg_file **)malloc(sizeof(reg_file *) * 2); // Allocate space for two pointers
        reg_files_list[0] = file; // Set the first element
        reg_files_list[1] = NULL; // Set the second element as NULL
    } else {
        int count = 0;
        while (reg_files_list[count] != NULL) {
            count++;
        }

        reg_files_list = (reg_file **)realloc(reg_files_list, sizeof(reg_file *) * (count + 2)); // Increase the size of the array

        reg_files_list[count] = file; // Append the new file
        reg_files_list[count + 1] = NULL; // Set the next element as NULL
    }
}

void f_entries(struct json_object *entry) {
    int inode = -1;
    const char *type = NULL;
    const char *data = NULL;

    json_object_object_foreach(entry, key, val) {
        if (strcmp(key, "inode") == 0)
            inode = json_object_get_int(val);
        else if (strcmp(key, "type") == 0)
            type = json_object_get_string(val);
        else if (strcmp(key, "data") == 0)
            data = json_object_get_string(val);
    }

    reg_file *new_reg = (reg_file *)malloc(sizeof(reg_file));
    new_reg->inode = inode;
    new_reg->type = strdup(type);
    new_reg->data = strdup(data);

    insert_reg_file(new_reg);
}

void parse_json_array(struct json_object *root) {
	int arr_length = json_object_array_length(root);

    for (int i = 0; i < arr_length; i++) {
        struct json_object *entry = json_object_array_get_idx(root, i);
		
		const char *type = NULL;
		json_object_object_foreach(entry, key, val) {
			if (strcmp(key, "type") == 0)
				type = json_object_get_string(val);
		}

		if (strcmp(type, "reg") == 0) {
            f_entries(entry);
        } else if (strcmp(type, "dir") == 0) {
			d_entries(entry);
        } else {
            fprintf(stderr, "JSON parsing error: type\n");
            exit(1);
        }
	}
}

void print_json (struct json_object * json) {
	int n = json_object_array_length(json);

	for ( int i = 0; i < n; i++ ) {
		struct json_object * obj = json_object_array_get_idx(json, i);

		printf("{\n");
		json_object_object_foreach(obj, key, val) {
			if (strcmp(key, "inode") == 0) 
				printf("   inode: %d\n", (int) json_object_get_int(val));

			if (strcmp(key, "type") == 0) 
				printf("   type: %s\n", (char *) json_object_get_string(val));

			if (strcmp(key, "name" ) == 0)
				printf("   name: %s\n", (char *) json_object_get_string(val));
			
			if (strcmp(key, "entries") == 0) {
				int array_length = json_object_array_length(val);
				printf("   # entries: %d\n", array_length);

				for (int j = 0; j < array_length; j++) {
					json_object *entry = json_object_array_get_idx(val, j);

					json_object *name;
					if (json_object_object_get_ex(entry, "name", &name) && json_object_is_type(name, json_type_string)) {
						const char *name_str = json_object_get_string(name);
						printf("      Name: %s\n", name_str);
					}
				}
			}
		}
		printf("}\n");
	}
	printf("\n");
}

void print_entry_list() {
    entry *current = entry_header;

    while (current != NULL) {
        printf("Inode: %d, Name: %s\n", current->inode, current->name);
        current = current->next;
    }
}

void print_reg_files_list() {
    reg_file **current = reg_files_list;

    while (*current != NULL) {
        printf("Inode: %d, Type: %s, Data: %s\n", (*current)->inode, (*current)->type, (*current)->data);
        current++;
    }
}

void print_dir_files_list() {
    dir_file **current = dir_files_list;

    while (*current != NULL) {
        printf("Inode: %d, Type: %s\n", (*current)->inode, (*current)->type);
        printf("Entries:\n");

        struct entry *entry = (*current)->entries;
        while (entry != NULL) {
            printf("\tInode: %d, Name: %s\n", entry->inode, entry->name);
            entry = entry->next;
        }

        current++;
    }
}


int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "./jsonfs [input_JSON_file]\n");
		return 1;
	}

	root = json_object_from_file(argv[1]);
	if (NULL == root) {
		fprintf(stderr, "Failed to parse JSON file\n");
		return 1;
	}

	dir_files_list = (dir_file **)malloc(sizeof(dir_file *));
    reg_files_list = (reg_file **)malloc(sizeof(reg_file *));
	dir_files_list = NULL;
	reg_files_list = NULL;
	entry_header = NULL;

	print_json(root);
	parse_json_array(root);
	print_entry_list();
	print_reg_files_list();
	print_dir_files_list();

    // json_object_put(root);

	char *dir = INIT_DIR;
    char *fuse_argv[] = {argv[0], dir, NULL};

	return fuse_main(argc, fuse_argv, &jsonfs_operations, NULL);
}
