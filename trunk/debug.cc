#include "debug.h"

namespace playground {

bool Debug::enabled_;
int  Debug::numSyscallNames_;
const char **Debug::syscallNames_;
std::map<int, std::string> Debug::syscallNamesMap_;

Debug Debug::debug_;

Debug::Debug() {
  // Logging is disabled by default, but can be turned on by setting an
  // appropriate environment variable. Initialize this code from a global
  // constructor, so that it runs before the sandbox is turned on.
  enabled_ = !!getenv("SECCOMP_SANDBOX_DEBUGGING");

  // Read names of system calls from header files, if available. Symbolic
  // names make debugging so much nicer.
  if (enabled_) {
    static const char *filenames[] = {
      #if __WORDSIZE == 64
      "/usr/include/asm/unistd_64.h",
      #elif __WORDSIZE == 32
      "/usr/include/asm/unistd_32.h",
      #endif
      "/usr/include/asm/unistd.h",
      NULL };
    numSyscallNames_ = 0;
    for (const char **fn = filenames; *fn; ++fn) {
      FILE *fp = fopen(*fn, "r");
      if (fp) {
        std::string baseName;
        int         baseNum = -1;
        char buf[80];
        while (fgets(buf, sizeof(buf), fp)) {
          // Check if the line starts with "#define"
          static const char* whitespace = " \t\r\n";
          char *token, *save;
          token = strtok_r(buf, whitespace, &save);
          if (token && !strcmp(token, "#define")) {

            // Only parse identifiers that start with "__NR_"
            token = strtok_r(NULL, whitespace, &save);
            if (token) {
              if (strncmp(token, "__NR_", 5)) {
                continue;
              }
              std::string syscallName(token + 5);

              // Parse the value of the symbol. Try to be forgiving in what
              // we accept, as the file format might change over time.
              token = strtok_r(NULL, "\r\n", &save);
              if (token) {
                // Some values are defined relative to previous values, we
                // detect these examples by finding an earlier symbol name
                // followed by a '+' plus character.
                bool isRelative = false;
                char *base = strstr(token, baseName.c_str());
                if (baseNum >= 0 && base) {
                  base += baseName.length();
                  while (*base == ' ' || *base == '\t') {
                    ++base;
                  }
                  if (*base == '+') {
                    isRelative = true;
                    token = base;
                  }
                }

                // Skip any characters that are not part of the syscall number.
                while (*token < '0' || *token > '9') {
                  token++;
                }

                // If we now have a valid datum, enter it into our map.
                if (*token) {
                  int sysnum = atoi(token);

                  // Deal with symbols that are defined relative to earlier
                  // ones.
                  if (isRelative) {
                    sysnum += baseNum;
                  } else {
                    baseNum  = sysnum;
                    baseName = syscallName;
                  }

                  // Keep track of the highest syscall number that we know
                  // about.
                  if (sysnum >= numSyscallNames_) {
                    numSyscallNames_ = sysnum + 1;
                  }

                  syscallNamesMap_[sysnum] = syscallName;
                }
              }
            }
          }
        }
        fclose(fp);
        break;
      }
    }
    if (numSyscallNames_) {
      // We cannot make system calls at the time, when we are looking up
      // the names. So, copy them into a data structure that can be
      // accessed without having to allocated memory (i.e. no more STL).
      syscallNames_ = reinterpret_cast<const char **>(
          calloc(sizeof(char *), numSyscallNames_));
      for (std::map<int, std::string>::const_iterator iter =
               syscallNamesMap_.begin();
           iter != syscallNamesMap_.end();
           ++iter) {
        syscallNames_[iter->first] = iter->second.c_str();
      }
    }
  }
}

void Debug::message(const char* msg) {
  if (enabled_) {
    Sandbox::SysCalls sys;
    Sandbox::write(sys, 2, msg, strlen(msg));
  }
}

void Debug::syscall(int sysnum, const char* msg) {
  // This function gets called from the system call wrapper. Avoid calling
  // any library functions that themselves need system calls.
  if (enabled_) {
    const char *sysname = NULL;
    if (sysnum >= 0 && sysnum < numSyscallNames_) {
      sysname = syscallNames_[sysnum];
    }
    char unnamed[40] = "Unnamed syscall #";
    if (!sysname) {
      itoa(sysnum, strrchr(sysname = unnamed, '\000'));
    }
    char buf[strlen(sysname) + (msg ? strlen(msg) : 0) + 4];
    strcat(strcat(strcat(strcpy(buf, sysname), ": "),
                  msg ? msg : ""), "\n");
    message(buf);
  }
}

char* Debug::itoa(int n, char *s) {
  // Remember return value
  char *ret   = s;

  // Insert sign for negative numbers
  if (n < 0) {
    *s++      = '-';
    n         = -n;
  }

  // Convert to decimal (in reverse order)
  char *start = s;
  do {
    *s++      = '0' + (n % 10);
    n        /= 10;
  } while (n);
  *s--        = '\000';

  // Reverse order of digits
  while (start < s) {
    char ch   = *s;
    *s--      = *start;
    *start++  = ch;
  }

  return ret;
}

} // namespace
