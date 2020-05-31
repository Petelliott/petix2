#include "fs.h"
#include "kdebug.h"
#include <errno.h>
#include <unistd.h>
#include "sync.h"
#include "kmalloc.h"
#include <string.h>


static struct file_ops const * devices[256];

int fs_open(struct inode *in, struct file *f) {
    if (in->ftype == FT_SPECIAL) {
        const struct file_ops *fops = devices[MAJOR(in->dev)];
        if (fops == NULL) {
            return -ENXIO;
        }

        f->inode = *in;
        f->offset = 0;
        f->fops = fops;
        return fops->open(in, f);
    } else if (in->ftype == FT_REGULAR) {
        f->inode = *in;
        f->offset = 0;
        f->fops = &(in->fs->iops->reg_ops);
        return f->fops->open(in, f);
    } else if (in->ftype == FT_DIR) {
        return -EISDIR;
    } else if (in->ftype == FT_LINK) {
        panic("symlinks are not supported yet");
    }
    return -EINVAL;
}

int register_device(int major, const struct file_ops *ops) {
    kassert(major < 256 && major >= 0);
    devices[major] = ops;
    return 0;
}

off_t fs_default_lseek(struct file *f, off_t off, int whence) {
    if (whence == SEEK_SET) {
        f->offset = off;
    } else if (whence == SEEK_CUR) {
        f->offset += off;
    } else if (whence == SEEK_END) {
        f->offset = f->size + off;
    } else {
        return -EINVAL;
    }

    return f->offset;
}


petix_lock_t mount_lock;

struct mount_tree {
    char name[FILE_NAME_LEN];
    bool mountpoint;
    struct fs_inst fs;
    struct mount_tree *child;
    struct mount_tree *sibling;
};

struct mount_tree mount_root;

static int fs_getfs(const char *p, struct fs_inst **fs, const char **relpath) {

    char path[PATH_MAX];
    strncpy(path, p, PATH_MAX);
    char *save;

    struct mount_tree *node = &mount_root;

    size_t reloff;

    if (node->mountpoint) {
        *fs = &(node->fs);
        reloff = 0;
    }

    for (char *i = strtok_r(path, "/", &save); i != NULL;
         i = strtok_r(NULL, "/", &save)) {

        bool found = false;
        for (struct mount_tree *j = node->child; j != NULL; j = j->sibling) {
            if (strncmp(j->name, i, FILE_NAME_LEN) == 0) {
                found = true;
                node = j;
                break;
            }
        }

        if (!found) {
            return -ENOENT;
        }

        if (node->mountpoint) {
            *fs = &(node->fs);
            reloff = save - path;
        }
    }

    *relpath = p + reloff;
    return 0;
}

int fs_lookup(const char *p, struct inode *inode) {
    acquire_lock(&mount_lock);

    const char *p2;
    struct fs_inst *fs;
    int ret = fs_getfs(p, &fs, &p2);
    if (ret < 0) {
        release_lock(&mount_lock);
        return ret;
    }

    char path[PATH_MAX];
    strncpy(path, p2, PATH_MAX);

    fs->iops->getroot(fs, inode);

    char *save;
    for (char *i = strtok_r(path, "/", &save); i != NULL;
         i = strtok_r(NULL, "/", &save)) {

        int ret = fs->iops->lookup(inode, i, inode);
        if (ret < 0) {
            release_lock(&mount_lock);
            return ret;
        }
    }

    *inode = in

    release_lock(&mount_lock);
    return 0;
}

static void unmount_child(struct mount_tree *node) {
    if (node == NULL) {
        return;
    }

    unmount_child(node->child);
    unmount_child(node->sibling);
    kfree_sync(node);
}

int fs_mount(const char *targ, struct inode *src, const struct inode_ops *fs) {
    acquire_lock(&mount_lock);

    char path[PATH_MAX];
    strncpy(path, targ, PATH_MAX);

    char *save;

    struct mount_tree *node = &mount_root;
    for (char *i = strtok_r(path, "/", &save); i != NULL;
         i = strtok_r(NULL, "/", &save)) {


        bool insert = true;
        for (struct mount_tree *j = node->child; j != NULL; j = j->sibling) {
            if (strncmp(j->name, i, FILE_NAME_LEN) == 0) {
                node = j;
                insert = false;
            }
        }

        if (insert) {
            // allocate node
            struct mount_tree *nn = kmalloc_sync(sizeof(struct mount_tree));
            strncpy(nn->name, i, FILE_NAME_LEN);
            nn->mountpoint = false;
            nn->child = NULL;
            nn->sibling = node->child;

            node->child = nn;
            node = nn;
        }
    }

    unmount_child(node->child);
    node->mountpoint = true;
    fs_open(src, &(node->fs.file));
    node->fs.iops = fs;

    release_lock(&mount_lock);
    return 0;
}

int fs_umount(const char *targ) {
    acquire_lock(&mount_lock);

    release_lock(&mount_lock);
    return 0;
}
