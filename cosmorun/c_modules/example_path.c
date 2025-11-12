#include "src/cosmo_libc.h"
#include "mod_path.h"

void print_separator() {
    printf("----------------------------------------\n");
}

void demo_join() {
    printf("=== Path Join Demo ===\n");

    char* result;

    result = path_join3("usr", "local", "bin");
    printf("path_join3('usr', 'local', 'bin') = %s\n", result);
    free(result);

    result = path_join4("/home", "user", "documents", "file.txt");
    printf("path_join4('/home', 'user', 'documents', 'file.txt') = %s\n", result);
    free(result);

    result = path_join2("project", "src/main.c");
    printf("path_join2('project', 'src/main.c') = %s\n", result);
    free(result);

    print_separator();
}

void demo_dirname_basename() {
    printf("=== Dirname & Basename Demo ===\n");

    const char* paths[] = {
        "/usr/local/bin/node",
        "/home/user/file.txt",
        "relative/path/to/file.c",
        "simple.txt",
        "/",
        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        char* dir = path_dirname(paths[i]);
        char* base = path_basename(paths[i]);
        printf("Path: %s\n", paths[i]);
        printf("  dirname:  %s\n", dir);
        printf("  basename: %s\n", base);
        free(dir);
        free(base);
    }

    print_separator();
}

void demo_extname() {
    printf("=== Extension Demo ===\n");

    const char* files[] = {
        "document.txt",
        "archive.tar.gz",
        "README",
        ".bashrc",
        "script.sh",
        "/path/to/config.json",
        NULL
    };

    for (int i = 0; files[i] != NULL; i++) {
        char* ext = path_extname(files[i]);
        printf("%-30s -> ext: '%s'\n", files[i], ext);
        free(ext);
    }

    print_separator();
}

void demo_normalize() {
    printf("=== Normalize Demo ===\n");

    const char* messy_paths[] = {
        "/foo/bar/../baz",
        "/usr/./local/./bin",
        "foo//bar///baz",
        "./foo/./bar",
        "foo/../bar/../baz",
        "../../../up",
        NULL
    };

    for (int i = 0; messy_paths[i] != NULL; i++) {
        char* clean = path_normalize(messy_paths[i]);
        printf("%-30s -> %s\n", messy_paths[i], clean);
        free(clean);
    }

    print_separator();
}

void demo_is_absolute() {
    printf("=== Absolute Path Check Demo ===\n");

    const char* test_paths[] = {
        "/usr/bin",
        "/",
        "relative/path",
        "./foo",
        "../bar",
        "simple.txt",
        NULL
    };

    for (int i = 0; test_paths[i] != NULL; i++) {
        int is_abs = path_is_absolute(test_paths[i]);
        printf("%-30s -> %s\n", test_paths[i],
               is_abs ? "absolute" : "relative");
    }

    print_separator();
}

void demo_resolve() {
    printf("=== Resolve Demo ===\n");

    const char* paths[] = {
        ".",
        "./foo/bar",
        "../sibling",
        "/absolute/path",
        NULL
    };

    printf("Current working directory:\n");
    char* cwd = getcwd(NULL, 0);
    printf("  %s\n\n", cwd);
    free(cwd);

    for (int i = 0; paths[i] != NULL; i++) {
        char* resolved = path_resolve(paths[i]);
        printf("%-30s -> %s\n", paths[i], resolved);
        free(resolved);
    }

    print_separator();
}

void demo_parse() {
    printf("=== Parse Demo ===\n");

    const char* paths[] = {
        "/home/user/documents/report.pdf",
        "project/src/main.c",
        "archive.tar.gz",
        "/etc/config",
        NULL
    };

    for (int i = 0; paths[i] != NULL; i++) {
        path_parse_t* parsed = path_parse(paths[i]);
        printf("Path: %s\n", paths[i]);
        printf("  root: '%s'\n", parsed->root);
        printf("  dir:  '%s'\n", parsed->dir);
        printf("  base: '%s'\n", parsed->base);
        printf("  name: '%s'\n", parsed->name);
        printf("  ext:  '%s'\n", parsed->ext);
        printf("\n");
        path_parse_free(parsed);
    }

    print_separator();
}

void demo_platform() {
    printf("=== Platform Info ===\n");

    printf("Path separator: '%c'\n", path_sep());
    printf("PATH delimiter: '%c'\n", path_delimiter());

#ifdef _WIN32
    printf("Platform: Windows\n");
#else
    printf("Platform: Unix/Linux\n");
#endif

    print_separator();
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   mod_path - Path Utilities Demo      ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");

    demo_platform();
    demo_join();
    demo_dirname_basename();
    demo_extname();
    demo_normalize();
    demo_is_absolute();
    demo_resolve();
    demo_parse();

    printf("Demo complete!\n\n");
    return 0;
}
