#ifndef KTABLEFS_OPTIONS_H_
#define KTABLEFS_OPTIONS_H_

struct options {
  const char* mountdir;
  const char* datadir;
  const char* workdir;
  int show_help;
};

extern struct options options;

#endif // KTABLEFS_OPTIONS_H_