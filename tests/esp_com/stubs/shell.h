#ifndef TEST_SHELL_H
#define TEST_SHELL_H
typedef struct { short (*write)(char *, unsigned short); } Shell;
void shellPrint(Shell *, const char *, ...);
unsigned short shellWriteString(Shell *, const char *);
#define SHELL_EXPORT_CMD(...)
#endif
