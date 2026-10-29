/* Excecao apenas para a raiz original imutavel, cujo UID de fabrica e 1108.
 * Arquivos de chave e diretorios mutaveis continuam sujeitos a todas as checagens. */
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <stdio.h>
static int localmod_root_mount_readonly(void) {
    FILE *fp = fopen("/proc/mounts", "r");
    char line[1024], device[256], target[256], type[64], options[256];
    int found = 0;
    if (!fp) return 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %255s %63s %255s", device, target, type, options) != 4)
            continue;
        if (strcmp(target, "/") == 0)
            found = strcmp(type, "cramfs") == 0 &&
                (strcmp(options, "ro") == 0 || strncmp(options, "ro,", 3) == 0);
    }
    fclose(fp);
    return found;
}
static int localmod_readonly_factory_root(const char *path, uid_t owner) {
    struct statvfs vfs;
    struct statfs fs;
    return strcmp(path, "/") == 0 && owner == 1108 &&
        statfs(path, &fs) == 0 && (unsigned long)fs.f_type == 0x28cd3d45UL &&
        ((statvfs(path, &vfs) == 0 && (vfs.f_flag & ST_RDONLY)) ||
         localmod_root_mount_readonly());
}
