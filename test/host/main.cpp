// Feeds local HTML files through the real reader-mode extractor.
#include "app.h"
#include <cstdio>

static const char *gFile = nullptr;
static const char *gType = "text/html";

namespace http {
bool get(const String &, char **body, size_t *len, String *ctype, String &err, size_t) {
    FILE *f = fopen(gFile, "rb");
    if (!f) { err = "open failed"; return false; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); free(b); err = "read failed"; return false; }
    b[n] = 0;
    fclose(f);
    *body = b; *len = (size_t)n;
    if (ctype) *ctype = gType;
    return true;
}
void freeBody(char *b) { free(b); }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: reader_test <file.html> [content-type]\n"); return 2; }
    gFile = argv[1];
    if (argc > 2) gType = argv[2];
    String title, body, err;
    if (!reader::fetch("https://example.com/a/b?c=d", title, body, err)) {
        printf("FAIL: %s\n", err.c_str());
        return 1;
    }
    printf("TITLE: %s\nCHARS: %zu\n----\n%s\n", title.c_str(), body.length(),
           body.substring(0, 1400).c_str());
    return 0;
}
