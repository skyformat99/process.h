/*
   The latest version of this library is available on GitHub;
   https://github.com/sheredom/process.h
*/

/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   For more information, please refer to <http://unlicense.org/>
*/

#ifndef SHEREDOM_PROCESS_H_INCLUDED
#define SHEREDOM_PROCESS_H_INCLUDED

#include <stdio.h>

#if defined(_MSC_VER)
#if defined(_M_IX86)
#define _X86_
#endif

#if defined(_M_AMD64)
#define _AMD64_
#endif

#pragma warning(push, 1)
#include <IntSafe.h>
#include <WinDef.h>
#include <handleapi.h>
#include <io.h>
#include <malloc.h>
#include <namedpipeapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#pragma warning(pop)
#endif

#if defined(__clang__) || defined(__GNUC__)
#define process_pure __attribute__((pure))
#define process_weak __attribute__((weak))
#elif defined(_MSC_VER)
#define process_pure
#define process_weak __inline
#else
#error Non clang, non gcc, non MSVC compiler found!
#endif

struct process_s {
  FILE *stdin_file;
  FILE *stdout_file;
  FILE *stderr_file;

#if defined(_MSC_VER)
  HANDLE hProcess;
#endif
};

/// @brief Create a process.
/// @param command_line An array of strings for the command line to execute for
/// this process. The last element must be NULL to signify the end of the array.
/// @param out_process The newly created process.
/// @return On success 0 is returned.
process_pure process_weak int
process_create(const char *const command_line[],
               struct process_s *const out_process);

/// @brief Get the standard input file for a process.
/// @param process The process to query.
/// @return The file for standard input of the process.
///
/// The file returned can be written to by the parent process to feed data to
/// the standard input of the process.
process_pure process_weak FILE *process_stdin(struct process_s process);

/// @brief Get the standard output file for a process.
/// @param process The process to query.
/// @return The file for standard output of the process.
///
/// The file returned can be written to by the parent process to read data to
/// the standard output of the process.
process_pure process_weak FILE *process_stdout(struct process_s process);

/// @brief Get the standard error file for a process.
/// @param process The process to query.
/// @return The file for standard error of the process.
///
/// The file returned can be written to by the parent process to read data to
/// the standard error of the process.
process_pure process_weak FILE *process_stderr(struct process_s process);

/// @brief Wait for a process to finish execution.
/// @param process The process to wait for.
/// @param out_return_code The return code of the returned process (can be
/// NULL).
/// @return On success 0 is returned.
process_pure process_weak int process_join(struct process_s process,
                                           int *const out_return_code);

/// @brief Destroy a previously created process.
/// @param process The process to destroy.
///
/// If the process to be destroyed had not finished execution, it may out live
/// the parent process.
process_pure process_weak int process_destroy(struct process_s process);

int process_create(const char *const commandLine[],
                   struct process_s *const out_process) {
  PROCESS_INFORMATION processInfo;
  STARTUPINFO startInfo;
  SECURITY_ATTRIBUTES saAttr = {sizeof(SECURITY_ATTRIBUTES), 0, 1};
  int fd;
  HANDLE rd, wr;
  char *commandLineCombined;
  size_t len;
  int i, j;
  const DWORD startFUseStdHandles = 0x00000100;
  const DWORD handleFlagInherit = 0x00000001;

  memset(&startInfo, 0, sizeof(startInfo));
  startInfo.dwFlags = startFUseStdHandles;

  if (!CreatePipe(&rd, &wr, &saAttr, 0)) {
    return -1;
  }

  if (!SetHandleInformation(wr, handleFlagInherit, 0)) {
    return -1;
  }

  fd = _open_osfhandle((intptr_t)wr, 0);

  if (-1 != fd) {
    out_process->stdin_file = _fdopen(fd, "wb");

    if (0 == out_process->stdin_file) {
      return -1;
    }
  }

  startInfo.hStdInput = rd;

  if (!CreatePipe(&rd, &wr, &saAttr, 0)) {
    return -1;
  }

  if (!SetHandleInformation(rd, handleFlagInherit, 0)) {
    return -1;
  }

  fd = _open_osfhandle((intptr_t)rd, 0);

  if (-1 != fd) {
    out_process->stdout_file = _fdopen(fd, "rb");

    if (0 == out_process->stdout_file) {
      return -1;
    }
  }

  startInfo.hStdOutput = wr;

  if (!CreatePipe(&rd, &wr, &saAttr, 0)) {
    return -1;
  }

  if (!SetHandleInformation(rd, handleFlagInherit, 0)) {
    return -1;
  }

  fd = _open_osfhandle((intptr_t)rd, 0);

  if (-1 != fd) {
    out_process->stderr_file = _fdopen(fd, "rb");

    if (0 == out_process->stderr_file) {
      return -1;
    }
  }

  startInfo.hStdError = wr;

  // Combine commandLine together into a single string
  len = 0;
  for (i = 0; commandLine[i]; i++) {
    // For the ' ' between items and trailing '\0'
    len++;

    for (j = 0; '\0' != commandLine[i][j]; j++) {
      len++;
    }
  }

  commandLineCombined = (char *)_malloca(len);

  if (!commandLineCombined) {
    return -1;
  }

  // Gonna re-use len to store the write index into commandLineCombined
  len = 0;

  for (i = 0; commandLine[i]; i++) {
    if (0 != i) {
      commandLineCombined[len++] = ' ';
    }

    for (j = 0; '\0' != commandLine[i][j]; j++) {
      commandLineCombined[len++] = commandLine[i][j];
    }
  }

  commandLineCombined[len] = '\0';

  if (!CreateProcess(NULL,
                     (LPSTR)commandLineCombined, // command line
                     NULL,                       // process security attributes
                     NULL,       // primary thread security attributes
                     TRUE,       // handles are inherited
                     0,          // creation flags
                     NULL,       // use parent's environment
                     NULL,       // use parent's current directory
                     &startInfo, // STARTUPINFO pointer
                     &processInfo)) {
    return -1;
  }

  out_process->hProcess = processInfo.hProcess;

  // We don't need to keep the handle of the primary thread in the called
  // process.
  CloseHandle(processInfo.hThread);

  return 0;
}

FILE *process_stdin(struct process_s process) { return process.stdin_file; }

FILE *process_stdout(struct process_s process) { return process.stdout_file; }

FILE *process_stderr(struct process_s process) { return process.stderr_file; }

int process_join(struct process_s process, int *const out_return_code) {
  const DWORD infinite = 0xFFFFFFFF;

  fclose(process.stdin_file);

  WaitForSingleObject(process.hProcess, infinite);

  if (out_return_code) {
    if (!GetExitCodeProcess(process.hProcess, (LPDWORD)out_return_code)) {
      return -1;
    }
  }

  return 0;
}

int process_destroy(struct process_s process) {
  fclose(process.stdin_file);
  fclose(process.stdout_file);
  fclose(process.stderr_file);

  CloseHandle(process.hProcess);

  return 0;
}

#endif /* SHEREDOM_PROCESS_H_INCLUDED */
