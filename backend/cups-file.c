/*
 * CUPS-File virtual backend
 *
 * Copyright (c) 2019 Bryan Mason <bmason@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <grp.h>

#include <cups/backend.h>

int main(int argc, char *argv[])
{
  char username[LOGIN_NAME_MAX];        // Copy of username passed through argv[2];
  char *device_uri;                     // DeviceURI
  char path[PATH_MAX];                  // Output path
  char *pwstr_buf;                      // Buffer for getpwnam_r strings
  size_t pwstr_buf_len;                 // Length of pwstr_buf
  struct passwd pwd_buf;                // Pointer to password data
  struct passwd *pwd = &pwd_buf;        // Password data
  int rv;                               // Return value
  char *u1, *u2, *p, *k;                // Some pointers for string copying & substitution
  char key[PATH_MAX];
  
  
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;
#endif 


// Make sure status message aren't buffered

  setbuf(stderr, NULL);

  //Ingore SIGPIPE

#ifdef HAVE_SIGSET
  sigset(SIGPIPE, SIG_IGN);
#elif defined(HAVE_SIGACTION)
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &action, NULL);
#else
  signal(SIGPIPE, SIG_IGN);
#endif

  if (argc == 1) {
    puts("file cups-file \"Unknown\" \"Save to File\"");
    return(CUPS_BACKEND_OK);
  } else if ( argc < 6 || argc > 7 ) {
    fprintf(stderr,
            "Usage: cups-file job-id user title copies options [file]\n");
    return CUPS_BACKEND_FAILED;
  }

  // Get the URI
  device_uri = getenv("DEVICE_URI");
  if ( !device_uri ) {
    fprintf(stderr, "ERROR: No Device URI supplied!\n");
    return CUPS_BACKEND_FAILED;
  }

  // Save the user who printed the job.
  if ( !strcmp(argv[2], "root") ) {
    fprintf(stderr, "WARN: Job submitted by root.  Setting UID to \"nobody\"\n");
    strcpy(username, "nobody");
  } else {
    strncpy(username, argv[2], LOGIN_NAME_MAX-1);
    username[LOGIN_NAME_MAX-1] = '\0';
  }

  /*
   * Get the UID/GID for this user
   */
  
  pwstr_buf_len = sysconf(_SC_GETPW_R_SIZE_MAX) * sizeof(char);
  pwstr_buf = malloc(pwstr_buf_len);
  if ( pwstr_buf == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate buffer for user information\n");
    return CUPS_BACKEND_FAILED;
  }
  
  rv = getpwnam_r(username, pwd, pwstr_buf, pwstr_buf_len, &pwd);
  if ( pwd == NULL )
  {
    if ( rv ) {
      fprintf(stderr, "ERROR: error finding entry for %s: %s\n",
              username, strerror(rv));
    } else {
      fprintf(stderr, "ERROR: couldn't fine entry for %s\n",
              username);
    }
    return CUPS_BACKEND_FAILED;
  }
  
  // Set SELinux context to that of the user who submitted the job
  
  // Drop privileges setting uid to that of the user who submitted the job

  fprintf(stderr, "DEBUG: Switching to user %s (%u:%u)\n",
         username, pwd->pw_uid, pwd->pw_gid);

  if ( setgid(pwd->pw_gid) != 0 ) {
    fprintf(stderr, "ERROR: Failed to switch to gid %u: %s\n",
            pwd->pw_gid, strerror(errno));
    return CUPS_BACKEND_FAILED;
  }
  
  if ( setgroups(0, NULL) == -1 ) {
    fprintf(stderr, "ERROR: Failed to drop supplementary groups: %s\n",
            strerror(errno));
    return CUPS_BACKEND_FAILED;
  }
  
  if ( setuid(pwd->pw_uid) != 0 ) {
    fprintf(stderr, "ERROR: Failed to switch to uid %u: %s\n",
            pwd->pw_gid, strerror(errno));
    return CUPS_BACKEND_FAILED;
  }

  // Copy the URI, substituting as necessary

  u1 = device_uri;
  p = path;

  // Skip the schema name
  while ( *u1 && *u1 != ':' ) {
    u1++;
  }

  // If we didn't find the end of the schema, that's a problem
  if ( !*u1 || *(u1 + 1) != '/' ) {
    fprintf(stderr, "ERROR: Invalid DeviceURI \"%s\"\n", device_uri);
    return CUPS_BACKEND_FAILED;
  }

  // Find the last "\" and include it in the path
  u1++;
  while ( *u1 && *u1 == '/' ) {
    u1++;
  }
  u1--;

  // Don't run off the end of the Device URI or the path
  while ( *u1 && p - path < PATH_MAX - 1 ) {
    
    // Found a substitution key delimiter
    if ( *u1 == '@' ) {
      u2 = u1 + 1;
      k = key;
      
      // Copy the key
      while ( *u1 && *u2 && *u2 != '@' && k - key < PATH_MAX - 1 ) {
        *k++ = *u2++;
      }

      // Ran off the end of the Device URI
      if ( !*u2 || k - key == PATH_MAX - 1 ) {
        fprintf(stderr, "ERROR: Invalid DeviceURI \"%s\"\n", device_uri);
        return CUPS_BACKEND_FAILED;
      }

      // Found the end of the key
      fprintf(stderr, "DEBUG: Found key \"%s\" in DeviceURI\n", key);

      /*
       * Do the substitution
       */

      // User
      if ( !strcmp(key, "user") ) {
        strcat(p, username);
        p = p + strlen(username);
      }

      // Title
      else if ( !strcmp(key, "title") ) {
        strcat(p, argv[3]);
        p = p + strlen(argv[3]);
      }

      // Job ID
      else if ( !strcmp(key, "jobid") ) {
        strcat(p, argv[1]);
        p = p + strlen(argv[1]);
      }
      
      // Invalid key; add key name (not substitution) to path
      else {
        fprintf(stderr, "WARNING: Invalid substitution key in Device URI: \"%s\"\n", key);
        strcat(p, key);
        p = p + strlen(key);
      }

      // Resume copy/substitution of the Device URI
      u1 = u2 + 1;
    } else {

      // Just copy the Device URI to the path
      *p++ = *u1++;
    }
  }

  fprintf(stderr, "DEBUG: Opening %s\n", path);
  
  // Open the file

  // Copy data to the file

  // Close the file

  system("id");

  return(CUPS_BACKEND_OK);
}
  
